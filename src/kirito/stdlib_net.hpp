#ifndef KIRITO_STDLIB_NET_HPP
#define KIRITO_STDLIB_NET_HPP

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef KIRITO_ENABLE_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#endif

#include "builtins.hpp"
#include "collections.hpp"
#include "deflate.hpp"
#include "native.hpp"
#include "net_compat.hpp"
#include "stdlib_json.hpp"

namespace kirito {

// The native-binding idiom below re-uses `vm`/`self` as bound-method lambda parameters that
// intentionally shadow the enclosing getAttr/setup `vm`/`self` (same VM, by design). Silence
// -Wshadow for these mechanical bindings; it stays active in the evaluator/parser/lexer core.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

// A TCP socket. Wraps an OS socket handle (POSIX fd or Winsock SOCKET), closed automatically when
// the value is collected. There is no global state; you create a Socket and operate on it.
class SocketVal : public NativeClass<SocketVal> {
public:
    static constexpr const char* kTypeName = "Socket";
    std::vector<std::string> inspectMembers() const override {
        return {"connect(host, port)", "bind(host, port)", "listen(backlog)", "accept() -> Socket", "send(data) -> Integer", "recv(size) -> String", "recvall() -> String", "settimeout(seconds)", "close()"};
    }
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

    static int64_t asInt(KiritoVM& vm, Handle h) { return argInt(vm, h, "argument"); }
    static const std::string& asStr(KiritoVM& vm, Handle h) { return argString(vm, h, "argument"); }

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

// Apply a send/recv timeout (seconds) so a stalled peer raises instead of hanging forever.
inline void setTimeout(netcompat::socket_t fd, double seconds) {
    if (seconds <= 0) return;
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(seconds * 1000.0);
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = static_cast<long>(seconds);
    tv.tv_usec = static_cast<long>((seconds - static_cast<double>(tv.tv_sec)) * 1e6);
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
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

// --- URL helpers (urllib.parse style) --------------------------------------------------------
// Percent-encode all but the RFC 3986 "unreserved" set (Python urllib.parse.quote default).
inline std::string percentEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}
// Decode %XX escapes (and '+' as space, matching query-string decoding). Malformed escapes pass
// through literally.
inline std::string percentDecode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = val(s[i + 1]), lo = val(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out += static_cast<char>(hi * 16 + lo); i += 2; continue; }
            out += s[i];
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

// A general URL split (any scheme), unlike parseUrl which is HTTP-specific.
struct UrlParts { std::string scheme, host, port, path, query, fragment; };
inline UrlParts splitUrl(const std::string& url) {
    UrlParts p;
    std::string rest = url;
    std::size_t sc = rest.find("://");
    if (sc != std::string::npos) { p.scheme = rest.substr(0, sc); rest = rest.substr(sc + 3); }
    std::size_t hash = rest.find('#');
    if (hash != std::string::npos) { p.fragment = rest.substr(hash + 1); rest = rest.substr(0, hash); }
    std::size_t q = rest.find('?');
    if (q != std::string::npos) { p.query = rest.substr(q + 1); rest = rest.substr(0, q); }
    std::size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    p.path = slash == std::string::npos ? "" : rest.substr(slash);
    std::size_t colon = hostport.find(':');
    if (colon == std::string::npos) { p.host = hostport; }
    else { p.host = hostport.substr(0, colon); p.port = hostport.substr(colon + 1); }
    return p;
}

// Resolve a possibly-relative Location against the request URL (absolute, root-relative, or relative).
inline std::string resolveUrl(const std::string& base, const std::string& loc) {
    if (loc.compare(0, 7, "http://") == 0 || loc.compare(0, 8, "https://") == 0) return loc;
    UrlParts b = splitUrl(base);
    std::string origin = b.scheme + "://" + b.host + (b.port.empty() ? "" : ":" + b.port);
    if (!loc.empty() && loc[0] == '/') return origin + loc;
    // relative to the base path's directory
    std::string dir = b.path;
    std::size_t slash = dir.find_last_of('/');
    dir = slash == std::string::npos ? "/" : dir.substr(0, slash + 1);
    return origin + dir + loc;
}

inline std::string base64Encode(const std::string& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) { out += T[(val >> bits) & 0x3F]; bits -= 6; }
    }
    if (bits > -6) out += T[((val << 8) >> (bits + 8)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

inline std::string asciiLower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// Decode a chunked transfer-encoded body.
inline std::string dechunk(const std::string& body) {
    std::string out;
    std::size_t i = 0;
    while (i < body.size()) {
        std::size_t eol = body.find("\r\n", i);
        if (eol == std::string::npos) break;
        std::string sizeHex = body.substr(i, eol - i);
        std::size_t semi = sizeHex.find(';');
        if (semi != std::string::npos) sizeHex = sizeHex.substr(0, semi);
        long sz = std::strtol(sizeHex.c_str(), nullptr, 16);
        i = eol + 2;
        if (sz <= 0) break;
        if (i + static_cast<std::size_t>(sz) > body.size()) { out.append(body, i, std::string::npos); break; }
        out.append(body, i, static_cast<std::size_t>(sz));
        i += static_cast<std::size_t>(sz) + 2;  // data + trailing CRLF
    }
    return out;
}

// Strip the gzip wrapper and inflate the DEFLATE payload.
inline std::string gunzip(const std::string& s) {
    if (s.size() < 18 || static_cast<unsigned char>(s[0]) != 0x1f ||
        static_cast<unsigned char>(s[1]) != 0x8b)
        throw KiritoError("invalid gzip data");
    unsigned char flg = static_cast<unsigned char>(s[3]);
    std::size_t i = 10;
    if (flg & 4) {  // FEXTRA
        std::size_t xlen = static_cast<unsigned char>(s[i]) |
                           (static_cast<unsigned char>(s[i + 1]) << 8);
        i += 2 + xlen;
    }
    if (flg & 8) { while (i < s.size() && s[i] != 0) ++i; ++i; }   // FNAME
    if (flg & 16) { while (i < s.size() && s[i] != 0) ++i; ++i; }  // FCOMMENT
    if (flg & 2) i += 2;                                           // FHCRC
    if (i + 8 > s.size()) throw KiritoError("truncated gzip data");
    return deflate::inflate(s.substr(i, s.size() - i - 8));
}

inline std::string decodeBody(const std::string& body, const std::string& enc) {
    if (enc == "gzip" || enc == "x-gzip") return gunzip(body);
    if (enc == "deflate") {
        try { return deflate::inflate(body); }
        catch (...) { return deflate::inflate(body.size() >= 2 ? body.substr(2) : body); }
    }
    return body;
}

// A parsed HTTP response.
struct HttpResult {
    int status = 0;
    std::string reason;
    std::vector<std::pair<std::string, std::string>> headers;  // original case, in order
    std::string body;
    std::string finalUrl;

    std::string header(const std::string& name) const {  // case-insensitive lookup
        std::string lc = asciiLower(name);
        for (const auto& [k, v] : headers) if (asciiLower(k) == lc) return v;
        return "";
    }
};

// Parse a raw HTTP/1.1 response: status line, headers, and the entity body (de-chunked and
// decompressed as the response's own headers direct).
inline HttpResult parseRaw(const std::string& raw) {
    HttpResult r;
    std::size_t headerEnd = raw.find("\r\n\r\n");
    std::string head = headerEnd == std::string::npos ? raw : raw.substr(0, headerEnd);
    std::string body = headerEnd == std::string::npos ? "" : raw.substr(headerEnd + 4);
    std::size_t pos = 0;
    bool first = true;
    while (pos < head.size()) {
        std::size_t eol = head.find("\r\n", pos);
        std::string line = head.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        if (first) {
            std::size_t sp = line.find(' ');
            if (sp != std::string::npos) {
                r.status = std::atoi(line.c_str() + sp + 1);
                std::size_t sp2 = line.find(' ', sp + 1);
                if (sp2 != std::string::npos) r.reason = line.substr(sp2 + 1);
            }
            first = false;
        } else if (!line.empty()) {
            std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::size_t vs = line.find_first_not_of(' ', colon + 1);
                r.headers.emplace_back(key, vs == std::string::npos ? "" : line.substr(vs));
            }
        }
        if (eol == std::string::npos) break;
        pos = eol + 2;
    }
    if (asciiLower(r.header("transfer-encoding")).find("chunked") != std::string::npos)
        body = dechunk(body);
    std::string enc = asciiLower(r.header("content-encoding"));
    if (!enc.empty()) {
        try { body = decodeBody(body, enc); } catch (...) { /* leave body as-is on decode failure */ }
    }
    r.body = std::move(body);
    return r;
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

// Send request bytes and read the full response. Plain TCP by default; TLS (with optional peer
// verification) when built with KIRITO_ENABLE_TLS. `timeout` (seconds, 0 = none) bounds send/recv.
std::string httpExchange(const Url& u, const std::string& request, double timeout, bool verify);

#ifdef KIRITO_ENABLE_TLS
inline std::string httpExchange(const Url& u, const std::string& request, double timeout, bool verify) {
    netcompat::socket_t fd = dialTcp(u);
    setTimeout(fd, timeout);
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
    if (verify) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx);
    }
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, static_cast<int>(fd));
    SSL_set_tlsext_host_name(ssl, u.host.c_str());  // SNI
    if (verify) {
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        SSL_set1_host(ssl, u.host.c_str());
    }
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl); SSL_CTX_free(ctx); netcompat::closeSocket(fd);
        throw KiritoError("TLS handshake with " + u.host + " failed");
    }
    if (verify && SSL_get_verify_result(ssl) != X509_V_OK) {
        SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); netcompat::closeSocket(fd);
        throw KiritoError("TLS certificate verification failed for " + u.host +
                          " (pass verify=False to skip)");
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
inline std::string httpExchange(const Url& u, const std::string& request, double timeout, bool verify) {
    (void)verify;
    if (u.tls)
        throw KiritoError("https requires building with KIRITO_ENABLE_TLS (OpenSSL); use http:// otherwise");
    netcompat::socket_t fd = dialTcp(u);
    setTimeout(fd, timeout);
    std::string raw;
    try { net::sendAll(fd, request); raw = net::recvAll(fd); }
    catch (...) { netcompat::closeSocket(fd); throw; }
    netcompat::closeSocket(fd);
    return raw;
}
#endif

}  // namespace net

// A rich HTTP response value (requests-style). Exposes status/reason/ok/url/text/headers/cookies
// attributes, a json()/raiseforstatus()/header() method set, and Dict-style ["status"]/["body"]
// indexing for convenience.
class ResponseVal : public NativeClass<ResponseVal> {
public:
    static constexpr const char* kTypeName = "Response";
    std::vector<std::string> inspectMembers() const override {
        return {"status: Integer", "statuscode: Integer", "reason: String", "ok: Bool", "url: String", "text: String", "body: String", "content: String", "headers: Dict", "cookies: Dict", "json() -> Any", "header(name, default) -> String", "raiseforstatus()"};
    }
    int status = 0;
    std::string reason, url, body;
    Handle headersH{}, cookiesH{};  // Dicts (headers in original case; cookies name->value)

    void children(std::vector<Handle>& out) const override { out.push_back(headersH); out.push_back(cookiesH); }
    std::string str(StringifyCtx&) const override { return "<Response [" + std::to_string(status) + "]>"; }
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
};

// A persistent HTTP session: keeps a cookie jar and default headers across requests (requests.Session
// style). `headers` and `cookies` are mutable Dicts; the verb methods merge them into each request
// and fold any Set-Cookie back into the jar.
class SessionVal : public NativeClass<SessionVal> {
public:
    static constexpr const char* kTypeName = "Session";
    std::vector<std::string> inspectMembers() const override {
        return {"headers: Dict", "cookies: Dict", "get(url, opts) -> Response", "post(url, opts) -> Response", "put(url, opts) -> Response", "delete(url, opts) -> Response", "patch(url, opts) -> Response", "head(url, opts) -> Response", "options(url, opts) -> Response", "request(method, url, opts) -> Response"};
    }
    Handle headersH{}, cookiesH{};
    void children(std::vector<Handle>& out) const override { out.push_back(headersH); out.push_back(cookiesH); }
    std::string str(StringifyCtx&) const override { return "<Session>"; }
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
};

namespace net {

// Build a Response value from a parsed result + accumulated cookie jar.
inline Handle makeResponse(KiritoVM& vm, const HttpResult& r, Handle cookiesH) {
    RootScope rs(vm);
    Dict hd(vm);
    for (const auto& [k, v] : r.headers) hd.set(k, val(vm, v));
    auto resp = std::make_unique<ResponseVal>();
    resp->status = r.status;
    resp->reason = r.reason;
    resp->url = r.finalUrl;
    resp->body = r.body;
    resp->headersH = rs.add(hd.build());
    resp->cookiesH = cookiesH;
    return vm.alloc(std::move(resp));
}

}  // namespace net

// Read an optional key from an options Dict; returns none() if absent / not a Dict.
inline Handle netOpt(KiritoVM& vm, Handle opts, const char* key) {
    const Object& o = vm.arena().deref(opts);
    if (o.kind() != ValueKind::Dict) return vm.none();
    Handle k = vm.makeString(key);
    const Handle* p = static_cast<const DictVal&>(o).find(vm.arena(), k);
    return p ? *p : vm.none();
}

// The core HTTP driver: build the request from `opts`, follow redirects, accumulate cookies, and
// return a Response. `opts` is a Dict (or none) with keys: headers, params, data, json, auth,
// timeout, allowredirects, maxredirects, verify, cookies.
inline Handle netRequest(KiritoVM& vm, const std::string& method0, const std::string& url0, Handle opts) {
    RootScope rs(vm);
    auto isStr = [&](Handle h) { return vm.arena().deref(h).kind() == ValueKind::String; };
    auto asStr = [&](Handle h) { return static_cast<const StrVal&>(vm.arena().deref(h)).value(); };

    // body + content type from json= / data=
    std::string body, contentType;
    Handle jsonH = netOpt(vm, opts, "json");
    Handle dataH = netOpt(vm, opts, "data");
    if (vm.arena().deref(jsonH).kind() != ValueKind::None) {
        std::unordered_set<const Object*> active;
        json::write(vm, jsonH, body, active);
        contentType = "application/json";
    } else if (vm.arena().deref(dataH).kind() != ValueKind::None) {
        const Object& d = vm.arena().deref(dataH);
        if (d.kind() == ValueKind::Dict) {
            for (const auto& [k, v] : Value(vm, dataH).pairs()) {
                if (!body.empty()) body += "&";
                body += net::percentEncode(k.str()) + "=" + net::percentEncode(v.str());
            }
            contentType = "application/x-www-form-urlencoded";
        } else {
            body = Value(vm, dataH).str();
            contentType = "text/plain";
        }
    }

    // multipart/form-data file upload (files = {field: content} or {field: [filename, content]})
    Handle filesH = netOpt(vm, opts, "files");
    if (vm.arena().deref(filesH).kind() == ValueKind::Dict) {
        const std::string boundary = "----KiritoFormBoundary7MA4YWxkTrZu0gW";
        std::string mp;
        for (const auto& [k, v] : Value(vm, filesH).pairs()) {
            std::string field = k.str(), filename = k.str(), content;
            const Object& vo = vm.arena().deref(v.handle());
            if (vo.kind() == ValueKind::List) {
                const ListVal& l = static_cast<const ListVal&>(vo);
                if (l.elems.size() >= 2) { filename = vm.stringify(l.elems[0]); content = vm.stringify(l.elems[1]); }
            } else {
                content = v.str();
            }
            mp += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + field +
                  "\"; filename=\"" + filename + "\"\r\nContent-Type: application/octet-stream\r\n\r\n" +
                  content + "\r\n";
        }
        mp += "--" + boundary + "--\r\n";
        body = mp;
        contentType = "multipart/form-data; boundary=" + boundary;
    }

    // query params appended to the URL
    std::string url = url0;
    Handle paramsH = netOpt(vm, opts, "params");
    if (vm.arena().deref(paramsH).kind() == ValueKind::Dict) {
        std::string qs;
        for (const auto& [k, v] : Value(vm, paramsH).pairs()) {
            if (!qs.empty()) qs += "&";
            qs += net::percentEncode(k.str()) + "=" + net::percentEncode(v.str());
        }
        if (!qs.empty()) url += (url.find('?') == std::string::npos ? "?" : "&") + qs;
    }

    // assembled (case-insensitively keyed) request headers, with defaults
    std::vector<std::pair<std::string, std::string>> hdrs;
    auto setHdr = [&](const std::string& k, const std::string& v) {
        std::string lc = net::asciiLower(k);
        for (auto& h : hdrs) if (net::asciiLower(h.first) == lc) { h.second = v; return; }
        hdrs.emplace_back(k, v);
    };
    setHdr("User-Agent", "kirito/1.0");
    setHdr("Accept", "*/*");
    if (!contentType.empty()) setHdr("Content-Type", contentType);
    Handle authH = netOpt(vm, opts, "auth");
    if (vm.arena().deref(authH).kind() == ValueKind::List) {
        const ListVal& l = static_cast<const ListVal&>(vm.arena().deref(authH));
        if (l.elems.size() == 2 && isStr(l.elems[0]) && isStr(l.elems[1]))
            setHdr("Authorization", "Basic " + net::base64Encode(asStr(l.elems[0]) + ":" + asStr(l.elems[1])));
    }
    Handle headersH = netOpt(vm, opts, "headers");
    if (vm.arena().deref(headersH).kind() == ValueKind::Dict)
        for (const auto& [k, v] : Value(vm, headersH).pairs()) setHdr(k.str(), v.str());

    // cookie jar (seeded from cookies= option), persisted across redirects
    Dict jar(vm);
    Handle cookiesOpt = netOpt(vm, opts, "cookies");
    if (vm.arena().deref(cookiesOpt).kind() == ValueKind::Dict)
        for (const auto& [k, v] : Value(vm, cookiesOpt).pairs()) jar.set(k.str(), val(vm, v.str()));
    Handle jarH = rs.add(jar.build());
    auto& jarDict = static_cast<DictVal&>(vm.arena().deref(jarH));

    double timeout = 0.0;
    Handle toH = netOpt(vm, opts, "timeout");
    if (vm.arena().deref(toH).kind() == ValueKind::Float) timeout = static_cast<const FloatVal&>(vm.arena().deref(toH)).value();
    else if (vm.arena().deref(toH).kind() == ValueKind::Integer) timeout = static_cast<double>(static_cast<const IntVal&>(vm.arena().deref(toH)).value());

    bool verify = true;
    Handle verifyH = netOpt(vm, opts, "verify");
    if (vm.arena().deref(verifyH).kind() == ValueKind::Bool) verify = static_cast<const BoolVal&>(vm.arena().deref(verifyH)).value();

    bool allowRedirects = true;
    Handle arH = netOpt(vm, opts, "allowredirects");
    if (vm.arena().deref(arH).kind() == ValueKind::Bool) allowRedirects = static_cast<const BoolVal&>(vm.arena().deref(arH)).value();
    int maxRedirects = 10;
    Handle mrH = netOpt(vm, opts, "maxredirects");
    if (vm.arena().deref(mrH).kind() == ValueKind::Integer) maxRedirects = static_cast<int>(static_cast<const IntVal&>(vm.arena().deref(mrH)).value());

    std::string method = method0, curUrl = url, curBody = body;
    for (int redirect = 0;; ++redirect) {
        net::Url u = net::parseUrl(curUrl);
        std::string target = u.path;
        std::string req = method + " " + target + " HTTP/1.1\r\nHost: " + u.host + "\r\n";
        for (const auto& [k, v] : hdrs)
            if (net::asciiLower(k) != "host" && net::asciiLower(k) != "content-length") req += k + ": " + v + "\r\n";
        // Cookie header from the jar
        std::string cookieHdr;
        for (Handle k : jarDict.keys()) {
            const std::string& cn = static_cast<const StrVal&>(vm.arena().deref(k)).value();
            const Handle* cv = jarDict.find(vm.arena(), k);
            if (!cookieHdr.empty()) cookieHdr += "; ";
            cookieHdr += cn + "=" + (cv ? vm.stringify(*cv) : "");
        }
        if (!cookieHdr.empty()) req += "Cookie: " + cookieHdr + "\r\n";
        req += "Connection: close\r\n";
        if (!curBody.empty()) req += "Content-Length: " + std::to_string(curBody.size()) + "\r\n";
        req += "\r\n" + curBody;

        std::string raw = net::httpExchange(u, req, timeout, verify);
        net::HttpResult r = net::parseRaw(raw);
        r.finalUrl = curUrl;

        // fold any Set-Cookie headers into the jar
        for (const auto& [k, v] : r.headers) {
            if (net::asciiLower(k) != "set-cookie") continue;
            std::string nv = v.substr(0, v.find(';'));
            std::size_t eq = nv.find('=');
            if (eq != std::string::npos)
                jarDict.set(vm.arena(), rs.add(vm.makeString(nv.substr(0, eq))),
                            rs.add(vm.makeString(nv.substr(eq + 1))));
        }

        bool isRedirect = r.status == 301 || r.status == 302 || r.status == 303 ||
                          r.status == 307 || r.status == 308;
        std::string loc = r.header("location");
        if (allowRedirects && isRedirect && !loc.empty() && redirect < maxRedirects) {
            curUrl = net::resolveUrl(curUrl, loc);
            if (r.status == 303 || ((r.status == 301 || r.status == 302) && method == "POST")) {
                method = "GET";
                curBody.clear();
                for (auto& h : hdrs)
                    if (net::asciiLower(h.first) == "content-type") h.second.clear();
            }
            continue;
        }
        return net::makeResponse(vm, r, jarH);
    }
}

// A Session request: merge the session's default headers + cookie jar into `opts`, perform the
// request, then fold the response's cookies back into the jar.
inline Handle sessionDo(KiritoVM& vm, Handle self, const std::string& method, const std::string& url, Handle opts) {
    auto& s = static_cast<SessionVal&>(vm.arena().deref(self));
    RootScope rs(vm);
    auto mergeInto = [&](Dict& dst, Handle src) {
        if (vm.arena().deref(src).kind() == ValueKind::Dict)
            for (const auto& [k, v] : Value(vm, src).pairs()) dst.set(k.str(), v.handle());
    };
    Dict eff(vm);
    mergeInto(eff, opts);                              // start from caller options
    Dict hdr(vm);
    mergeInto(hdr, s.headersH);                        // session defaults...
    mergeInto(hdr, netOpt(vm, opts, "headers"));       // ...overlaid by per-call headers
    eff.set("headers", hdr.build());
    Dict ck(vm);
    mergeInto(ck, s.cookiesH);
    mergeInto(ck, netOpt(vm, opts, "cookies"));
    eff.set("cookies", ck.build());
    Handle effH = rs.add(eff.build());

    Handle resp = netRequest(vm, method, url, effH);
    // persist response cookies into the session jar
    auto& r = static_cast<ResponseVal&>(vm.arena().deref(resp));
    auto& jar = static_cast<DictVal&>(vm.arena().deref(s.cookiesH));
    const DictVal& rc = static_cast<const DictVal&>(vm.arena().deref(r.cookiesH));
    for (Handle k : rc.keys()) jar.set(vm.arena(), k, *rc.find(vm.arena(), k));
    return resp;
}

inline Handle SessionVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    if (name == "headers") return headersH;
    if (name == "cookies") return cookiesH;
    auto verb = [&](const char* nm, const char* method) {
        return makeMethod(vm, nm, {"url", "opts"},
            [self, method](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, "session");
                Handle opts = args.size() > 1 ? a[1] : vm.none();
                return sessionDo(vm, self, method, args[0].asString("url"), opts);
            }, std::vector<Handle>{self});
    };
    if (name == "get") return verb("get", "GET");
    if (name == "post") return verb("post", "POST");
    if (name == "put") return verb("put", "PUT");
    if (name == "delete") return verb("delete", "DELETE");
    if (name == "patch") return verb("patch", "PATCH");
    if (name == "head") return verb("head", "HEAD");
    if (name == "options") return verb("options", "OPTIONS");
    if (name == "request")
        return makeMethod(vm, "request", {"method", "url", "opts"},
            [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, "session.request");
                Handle opts = args.size() > 2 ? a[2] : vm.none();
                return sessionDo(vm, self, args[0].asString("method"), args[1].asString("url"), opts);
            }, std::vector<Handle>{self});
    return Object::getAttr(vm, self, name);
}

inline Handle SocketVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
        return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
    };
    auto sock = [](KiritoVM& vm, Handle self) -> SocketVal& {
        return static_cast<SocketVal&>(vm.arena().deref(self));
    };
    if (name == "connect")
        return bind("connect", {"host", "port"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = sock(vm, self);
            sockaddr_in addr{};
            if (!net::resolve(asStr(vm, a[0]), static_cast<int>(asInt(vm, a[1])), addr))
                throw KiritoError("could not resolve host");
            if (::connect(s.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                throw KiritoError("connect failed: " + netcompat::lastError());
            return vm.none();
        });
    if (name == "bind")
        return bind("bind", {"host", "port"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
        return bind("listen", {"backlog"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            int backlog = a.empty() ? 16 : static_cast<int>(asInt(vm, a[0]));
            if (::listen(sock(vm, self).fd, backlog) != 0)
                throw KiritoError("listen failed: " + netcompat::lastError());
            return vm.none();
        });
    if (name == "accept")
        return bind("accept", {}, [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            netcompat::socket_t c = ::accept(sock(vm, self).fd, nullptr, nullptr);
            if (!netcompat::isValid(c)) throw KiritoError("accept failed: " + netcompat::lastError());
            return vm.alloc(std::make_unique<SocketVal>(c));
        });
    if (name == "send")
        return bind("send", {"data"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& data = asStr(vm, a[0]);
            net::sendAll(sock(vm, self).fd, data);
            return vm.makeInt(static_cast<int64_t>(data.size()));
        });
    if (name == "recv")
        return bind("recv", {"size"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = a.empty() ? 4096 : static_cast<std::size_t>(asInt(vm, a[0]));
            std::vector<char> buf(n);
            long long got = netcompat::recvBytes(sock(vm, self).fd, buf.data(), n);
            if (got < 0) throw KiritoError("recv failed: " + netcompat::lastError());
            return vm.makeString(std::string(buf.data(), static_cast<std::size_t>(got)));
        });
    if (name == "recvall")
        return bind("recvall", {}, [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.makeString(net::recvAll(sock(vm, self).fd));
        });
    if (name == "settimeout")
        return bind("settimeout", {"seconds"}, [self, sock](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& t = vm.arena().deref(a[0]);
            double secs = t.kind() == ValueKind::Float
                              ? static_cast<const FloatVal&>(t).value()
                              : static_cast<double>(asInt(vm, a[0]));
            net::setTimeout(sock(vm, self).fd, secs);
            return vm.none();
        });
    if (name == "close" || name == "_exit_")
        return bind("close", {}, [self, sock](KiritoVM& vm, std::span<const Handle>) -> Handle {
            sock(vm, self).closeFd();
            return vm.none();
        });
    if (name == "_enter_")
        return bind("_enter_", {}, [self](KiritoVM&, std::span<const Handle>) { return self; });
    return Object::getAttr(vm, self, name);
}

inline Handle ResponseVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    if (name == "status" || name == "statuscode") return vm.makeInt(status);
    if (name == "reason") return vm.makeString(reason);
    if (name == "ok") return vm.makeBool(status >= 100 && status < 400);
    if (name == "url") return vm.makeString(url);
    if (name == "text" || name == "body" || name == "content") return vm.makeString(body);
    if (name == "headers") return headersH;
    if (name == "cookies") return cookiesH;
    auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
        return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
    };
    if (name == "json")
        return bind("json", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& r = static_cast<ResponseVal&>(vm.arena().deref(self));
            RootScope rs(vm);
            return json::Parser(vm, r.body, rs).parse();
        });
    if (name == "raiseforstatus")
        return bind("raiseforstatus", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& r = static_cast<ResponseVal&>(vm.arena().deref(self));
            if (r.status >= 400)
                throw KiritoError("HTTP " + std::to_string(r.status) +
                                  (r.reason.empty() ? "" : " " + r.reason) + " for " + r.url);
            return self;
        });
    if (name == "header")
        return bind("header", {"name"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& r = static_cast<ResponseVal&>(vm.arena().deref(self));
            std::string want = net::asciiLower(argString(vm, a[0], "header"));
            const DictVal& hd = static_cast<const DictVal&>(vm.arena().deref(r.headersH));
            for (Handle k : hd.keys())
                if (net::asciiLower(static_cast<const StrVal&>(vm.arena().deref(k)).value()) == want)
                    return *hd.find(vm.arena(), k);
            return a.size() > 1 ? a[1] : vm.none();
        });
    return Object::getAttr(vm, self, name);
}

class NetModule : public NativeModule {
public:
    std::string name() const override { return "net"; }
    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();
        m.fn("Socket", {}, "Socket", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.alloc(std::make_unique<SocketVal>());
        });
        m.fn("Session", {}, "Session", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto s = std::make_unique<SessionVal>();
            RootScope rs(vm);
            s->headersH = rs.add(Dict(vm).build());
            s->cookiesH = rs.add(Dict(vm).build());
            return vm.alloc(std::move(s));
        });

        // --- HTTP client (requests-style) ---
        // request(method, url[, options]) and the per-verb shortcuts return a Response.
        m.fn("request", {{"method", "String"}, {"url", "String"}, {"options", "", vm.none()}}, "Response",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                 Args args(vm, a, "request");
                 Handle opts = args.size() > 2 ? a[2] : vm.none();
                 return netRequest(vm, args[0].asString("method"), args[1].asString("url"), opts);
             });
        auto verb = [&m](const char* fnName, const char* method) {
            m.fn(fnName, {{"url", "String"}, {"options", "", m.vm().none()}}, "Response",
                 [method](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                     Args args(vm, a, "http");
                     Handle opts = args.size() > 1 ? a[1] : vm.none();
                     return netRequest(vm, method, args[0].asString("url"), opts);
                 });
        };
        verb("get", "GET");
        verb("post", "POST");
        verb("put", "PUT");
        verb("delete", "DELETE");
        verb("patch", "PATCH");
        verb("head", "HEAD");
        verb("options", "OPTIONS");

        // --- URL helpers (urllib.parse style) ---
        m.fn("quote", {{"s", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, net::percentEncode(Args(vm, a, "quote")[0].asString("s")));
        });
        m.fn("unquote", {{"s", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, net::percentDecode(Args(vm, a, "unquote")[0].asString("s")));
        });
        m.fn("urlencode", {{"params", "Dict"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string out;
            for (const auto& [k, v] : Args(vm, a, "urlencode")[0].pairs()) {
                if (!out.empty()) out += "&";
                out += net::percentEncode(k.str()) + "=" + net::percentEncode(v.str());
            }
            return val(vm, out);
        });
        m.fn("parseqs", {{"query", "String"}}, "Dict", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string q = Args(vm, a, "parseqs")[0].asString("parseqs");
            Dict d(vm);
            std::size_t i = 0;
            while (i < q.size()) {
                std::size_t amp = q.find('&', i);
                std::string pair = q.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
                std::size_t eq = pair.find('=');
                std::string k = net::percentDecode(eq == std::string::npos ? pair : pair.substr(0, eq));
                std::string v = eq == std::string::npos ? "" : net::percentDecode(pair.substr(eq + 1));
                if (!k.empty()) d.set(k, val(vm, v));
                if (amp == std::string::npos) break;
                i = amp + 1;
            }
            return d.build();
        });
        m.fn("urlsplit", {{"url", "String"}}, "Dict", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            net::UrlParts p = net::splitUrl(Args(vm, a, "urlsplit")[0].asString("urlsplit"));
            return Dict(vm).set("scheme", p.scheme).set("host", p.host).set("port", p.port)
                           .set("path", p.path).set("query", p.query).set("fragment", p.fragment).build();
        });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
