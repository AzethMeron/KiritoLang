#ifndef KIRITO_STDLIB_NET_HPP
#define KIRITO_STDLIB_NET_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef KIRITO_ENABLE_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"
#include "net_compat.hpp"

namespace kirito {

// A TCP socket. Wraps an OS socket handle (POSIX fd or Winsock SOCKET), closed automatically when
// the value is collected. There is no global state; you create a Socket and operate on it.
class SocketVal : public NativeClass<SocketVal> {
public:
    static constexpr const char* kTypeName = "Socket";
    netcompat::socket_t fd = netcompat::kInvalidSocket;
    bool closed = false;

    SocketVal() {
        netcompat::startup();
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (!netcompat::isValid(fd))
            throw KiritoError("socket() failed: " + netcompat::lastError());
    }
    explicit SocketVal(netcompat::socket_t existing) : fd(existing) {}
    ~SocketVal() override { closeFd(); }

    void closeFd() {
        if (netcompat::isValid(fd) && !closed) { netcompat::closeSocket(fd); closed = true; }
    }

    static int64_t asInt(KiritoVM& vm, Handle h) {
        const Object& o = vm.arena().deref(h);
        if (o.kind() != ValueKind::Integer) throw KiritoError("expected an Integer");
        return static_cast<const IntVal&>(o).value();
    }
    static const std::string& asStr(KiritoVM& vm, Handle h) {
        const Object& o = vm.arena().deref(h);
        if (o.kind() != ValueKind::String) throw KiritoError("expected a String");
        return static_cast<const StrVal&>(o).value();
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
};

namespace net {

// Resolve host:port into a sockaddr (IPv4). Returns the first usable address.
inline bool resolve(const std::string& host, int port, sockaddr_in& out) {
    netcompat::startup();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return false;
    out = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    ::freeaddrinfo(res);
    return true;
}

inline void sendAll(netcompat::socket_t fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        long long n = netcompat::sendBytes(fd, data.data() + sent, data.size() - sent);
        if (n <= 0) throw KiritoError("send failed: " + netcompat::lastError());
        sent += static_cast<std::size_t>(n);
    }
}

inline std::string recvAll(netcompat::socket_t fd) {
    std::string out;
    char buf[4096];
    while (true) {
        long long n = netcompat::recvBytes(fd, buf, sizeof(buf));
        if (n < 0) throw KiritoError("recv failed: " + netcompat::lastError());
        if (n == 0) break;
        out.append(buf, static_cast<std::size_t>(n));
    }
    return out;
}

// Parse an http://host[:port]/path URL.
struct Url {
    std::string host, path;
    int port = 80;
    bool tls = false;
};
inline Url parseUrl(const std::string& url) {
    Url u;
    std::string rest;
    if (url.compare(0, 7, "http://") == 0) {
        rest = url.substr(7);
        u.port = 80;
    } else if (url.compare(0, 8, "https://") == 0) {
        rest = url.substr(8);
        u.port = 443;
        u.tls = true;
    } else {
        throw KiritoError("URL must start with http:// or https://");
    }
    std::size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    u.path = slash == std::string::npos ? "/" : rest.substr(slash);
    std::size_t colon = hostport.find(':');
    if (colon == std::string::npos) {
        u.host = hostport;
    } else {
        u.host = hostport.substr(0, colon);
        u.port = std::stoi(hostport.substr(colon + 1));
    }
    return u;
}

// Perform the raw HTTP exchange (send request bytes, read the full response). Plain TCP by
// default; over TLS when built with KIRITO_ENABLE_TLS (links OpenSSL). Defined below.
std::string httpExchange(const Url& u, const std::string& request);

// Send one HTTP/1.1 request and return {status, body, headers} as a Dict. We send Connection:
// close so the server ends the body by closing the socket (no chunked-decoding needed), and a
// User-Agent since some hosts (e.g. rule34) reject requests without one. HTTP/1.1 is required by
// many modern servers (HTTP/1.0 gets a 426 Upgrade Required).
inline Handle httpRequest(KiritoVM& vm, const std::string& method, const std::string& url,
                          const std::string& body, const std::string& contentType) {
    Url u = parseUrl(url);
    std::string req = method + " " + u.path + " HTTP/1.1\r\nHost: " + u.host +
                      "\r\nUser-Agent: kirito/1.0\r\nAccept: */*\r\nConnection: close\r\n";
    if (!body.empty())
        req += "Content-Type: " + contentType + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
    req += "\r\n" + body;
    std::string raw = httpExchange(u, req);

    // split status line / headers / body
    std::size_t headerEnd = raw.find("\r\n\r\n");
    std::string head = headerEnd == std::string::npos ? raw : raw.substr(0, headerEnd);
    std::string responseBody = headerEnd == std::string::npos ? "" : raw.substr(headerEnd + 4);
    int status = 0;
    auto headers = std::make_unique<DictVal>();
    std::size_t pos = 0;
    bool first = true;
    RootScope rs(vm);
    Handle headersH = rs.add(vm.alloc(std::move(headers)));
    auto& hdrDict = static_cast<DictVal&>(vm.arena().deref(headersH));
    while (pos < head.size()) {
        std::size_t eol = head.find("\r\n", pos);
        std::string line = head.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        if (first) {
            std::size_t sp = line.find(' ');
            if (sp != std::string::npos) status = std::atoi(line.c_str() + sp + 1);
            first = false;
        } else if (!line.empty()) {
            std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::size_t vs = line.find_first_not_of(' ', colon + 1);
                std::string val = vs == std::string::npos ? "" : line.substr(vs);
                hdrDict.set(vm.arena(), rs.add(vm.makeString(key)), rs.add(vm.makeString(val)));
            }
        }
        if (eol == std::string::npos) break;
        pos = eol + 2;
    }
    auto result = std::make_unique<DictVal>();
    Handle resultH = rs.add(vm.alloc(std::move(result)));
    auto& res = static_cast<DictVal&>(vm.arena().deref(resultH));
    res.set(vm.arena(), rs.add(vm.makeString("status")), vm.makeInt(status));
    res.set(vm.arena(), rs.add(vm.makeString("body")), rs.add(vm.makeString(responseBody)));
    res.set(vm.arena(), rs.add(vm.makeString("headers")), headersH);
    return resultH;
}

// Open a connected TCP socket to the URL's host:port, or throw.
inline netcompat::socket_t dialTcp(const Url& u) {
    sockaddr_in addr{};
    if (!resolve(u.host, u.port, addr)) throw KiritoError("could not resolve host '" + u.host + "'");
    netcompat::socket_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!netcompat::isValid(fd)) throw KiritoError("socket() failed");
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        netcompat::closeSocket(fd);
        throw KiritoError("could not connect to " + u.host + ": " + netcompat::lastError());
    }
    return fd;
}

#ifdef KIRITO_ENABLE_TLS
// TLS transport via OpenSSL (compiled in only when KIRITO_ENABLE_TLS is defined; links -lssl -lcrypto).
inline std::string httpExchange(const Url& u, const std::string& request) {
    netcompat::socket_t fd = dialTcp(u);
    if (!u.tls) {
        std::string raw;
        try { net::sendAll(fd, request); raw = net::recvAll(fd); }
        catch (...) { netcompat::closeSocket(fd); throw; }
        netcompat::closeSocket(fd);
        return raw;
    }
    static bool inited = [] { SSL_library_init(); SSL_load_error_strings(); return true; }();
    (void)inited;
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { netcompat::closeSocket(fd); throw KiritoError("SSL_CTX_new failed"); }
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, static_cast<int>(fd));
    SSL_set_tlsext_host_name(ssl, u.host.c_str());  // SNI
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl); SSL_CTX_free(ctx); netcompat::closeSocket(fd);
        throw KiritoError("TLS handshake with " + u.host + " failed");
    }
    std::string raw;
    try {
        std::size_t sent = 0;
        while (sent < request.size()) {
            int n = SSL_write(ssl, request.data() + sent, static_cast<int>(request.size() - sent));
            if (n <= 0) throw KiritoError("SSL_write failed");
            sent += static_cast<std::size_t>(n);
        }
        char buf[4096];
        int n;
        while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0) raw.append(buf, static_cast<std::size_t>(n));
    } catch (...) {
        SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); netcompat::closeSocket(fd);
        throw;
    }
    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); netcompat::closeSocket(fd);
    return raw;
}
#else
// Default build: plain TCP only. https:// URLs are rejected with an actionable message.
inline std::string httpExchange(const Url& u, const std::string& request) {
    if (u.tls)
        throw KiritoError("https requires building with KIRITO_ENABLE_TLS (OpenSSL); use http:// otherwise");
    netcompat::socket_t fd = dialTcp(u);
    std::string raw;
    try { net::sendAll(fd, request); raw = net::recvAll(fd); }
    catch (...) { netcompat::closeSocket(fd); throw; }
    netcompat::closeSocket(fd);
    return raw;
}
#endif

}  // namespace net

inline Handle SocketVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto bind = [&](const char* nm, NativeFn fn) {
        return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
    };
    auto sock = [](KiritoVM& vm, Handle self) -> SocketVal& {
        return static_cast<SocketVal&>(vm.arena().deref(self));
    };
    if (name == "connect")
        return bind("connect", [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = sock(vm, self);
            sockaddr_in addr{};
            if (!net::resolve(asStr(vm, a[0]), static_cast<int>(asInt(vm, a[1])), addr))
                throw KiritoError("could not resolve host");
            if (::connect(s.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                throw KiritoError("connect failed: " + netcompat::lastError());
            return vm.none();
        });
    if (name == "bind")
        return bind("bind", [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = sock(vm, self);
            int yes = 1;
            ::setsockopt(s.fd, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<const char*>(&yes), sizeof(yes));
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<uint16_t>(asInt(vm, a[1])));
            ::inet_pton(AF_INET, asStr(vm, a[0]).c_str(), &addr.sin_addr);
            if (::bind(s.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                throw KiritoError("bind failed: " + netcompat::lastError());
            return vm.none();
        });
    if (name == "listen")
        return bind("listen", [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            int backlog = a.empty() ? 16 : static_cast<int>(asInt(vm, a[0]));
            if (::listen(sock(vm, self).fd, backlog) != 0)
                throw KiritoError("listen failed: " + netcompat::lastError());
            return vm.none();
        });
    if (name == "accept")
        return bind("accept", [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            netcompat::socket_t c = ::accept(sock(vm, self).fd, nullptr, nullptr);
            if (!netcompat::isValid(c)) throw KiritoError("accept failed: " + netcompat::lastError());
            return vm.alloc(std::make_unique<SocketVal>(c));
        });
    if (name == "send")
        return bind("send", [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& data = asStr(vm, a[0]);
            net::sendAll(sock(vm, self).fd, data);
            return vm.makeInt(static_cast<int64_t>(data.size()));
        });
    if (name == "recv")
        return bind("recv", [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = a.empty() ? 4096 : static_cast<std::size_t>(asInt(vm, a[0]));
            std::vector<char> buf(n);
            long long got = netcompat::recvBytes(sock(vm, self).fd, buf.data(), n);
            if (got < 0) throw KiritoError("recv failed: " + netcompat::lastError());
            return vm.makeString(std::string(buf.data(), static_cast<std::size_t>(got)));
        });
    if (name == "recv_all")
        return bind("recv_all", [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.makeString(net::recvAll(sock(vm, self).fd));
        });
    if (name == "close" || name == "_exit_")
        return bind("close", [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            sock(vm, self).closeFd();
            return vm.none();
        });
    if (name == "_enter_")
        return bind("_enter_", [self](KiritoVM&, std::span<const Handle>) { return self; });
    return Object::getAttr(vm, self, name);
}

class NetModule : public NativeModule {
public:
    std::string name() const override { return "net"; }
    void setup(ModuleBuilder& m) override {
        m.fn("Socket", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.alloc(std::make_unique<SocketVal>());
        });
        m.fn("http_get", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return net::httpRequest(vm, "GET", SocketVal::asStr(vm, a[0]), "", "");
        });
        m.fn("http_post", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string ct = a.size() > 2 ? SocketVal::asStr(vm, a[2]) : "text/plain";
            return net::httpRequest(vm, "POST", SocketVal::asStr(vm, a[0]), SocketVal::asStr(vm, a[1]), ct);
        });
    }
};

}  // namespace kirito

#endif
