// Networking tests use a loopback server (run on a helper thread purely to provide the other end
// of the connection; the net library itself is single-threaded).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <string>
#include <thread>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

// Serve `n` sequential connections, replying to each request with handler(request). Captures the
// last request into `lastReq` (if non-null). Each connection is closed after the reply (the client
// sends Connection: close), so a redirect simply takes the next accept().
static void serveN(int srv, int n, std::function<std::string(const std::string&)> handler,
                   std::string* lastReq = nullptr) {
    for (int i = 0; i < n; ++i) {
        int c = ::accept(srv, nullptr, nullptr);
        if (c < 0) break;
        char buf[8192];
        ssize_t r = ::recv(c, buf, sizeof(buf), 0);
        std::string req(buf, r > 0 ? static_cast<std::size_t>(r) : 0);
        if (lastReq) *lastReq = req;
        std::string resp = handler(req);
        ::send(c, resp.data(), resp.size(), 0);
        ::close(c);
    }
    ::close(srv);
}

// Create a listening socket on an ephemeral 127.0.0.1 port; returns fd and sets `port`.
static int makeListener(int& port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;  // ephemeral
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return -1;
    ::listen(fd, 1);
    socklen_t len = sizeof(addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    port = ntohs(addr.sin_port);
    return fd;
}

int main() {
    KiritoVM vm;

    // --- raw TCP: client sends, server echoes ---
    {
        int port = 0;
        int srv = makeListener(port);
        CHECK(srv >= 0);
        std::thread server([srv] {
            int c = ::accept(srv, nullptr, nullptr);
            char buf[256];
            ssize_t n = ::recv(c, buf, sizeof(buf), 0);
            std::string reply = "echo:" + std::string(buf, n > 0 ? static_cast<std::size_t>(n) : 0);
            ::send(c, reply.data(), reply.size(), 0);
            ::close(c);
            ::close(srv);
        });
        std::string src =
            "var net = import(\"net\")\n"
            "var s = net.Socket()\n"
            "s.connect(\"127.0.0.1\", " + std::to_string(port) + ")\n"
            "s.send(\"ping\")\n"
            "var reply = s.recvall()\n"
            "s.close()\n"
            "reply\n";
        CHECK(evalStr(vm, src) == "echo:ping");
        server.join();
    }

    // --- HTTP client against a canned response ---
    {
        int port = 0;
        int srv = makeListener(port);
        CHECK(srv >= 0);
        std::thread server([srv] {
            int c = ::accept(srv, nullptr, nullptr);
            char buf[2048];
            ::recv(c, buf, sizeof(buf), 0);
            std::string resp =
                "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nX-Test: yes\r\n\r\nHello HTTP";
            ::send(c, resp.data(), resp.size(), 0);
            ::close(c);
            ::close(srv);
        });
        std::string src =
            "var r = import(\"net\").httpget(\"http://127.0.0.1:" + std::to_string(port) + "/\")\n"
            "var status = r[\"status\"]\n"
            "var body = r[\"body\"]\n"
            "var ct = r[\"headers\"][\"X-Test\"]\n"
            "String(status) + \"|\" + body + \"|\" + ct\n";
        CHECK(evalStr(vm, src) == "200|Hello HTTP|yes");
        server.join();
    }

    // --- httppost: a real request carries the POST method, body, and content-type over the wire ---
    {
        int port = 0;
        int srv = makeListener(port);
        CHECK(srv >= 0);
        std::string request;  // captured by the loopback server; read after join()
        std::thread server([srv, &request] {
            int c = ::accept(srv, nullptr, nullptr);
            char buf[4096];
            ssize_t n = ::recv(c, buf, sizeof(buf), 0);
            if (n > 0) request.assign(buf, static_cast<std::size_t>(n));
            std::string resp = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nposted";
            ::send(c, resp.data(), resp.size(), 0);
            ::close(c);
            ::close(srv);
        });
        std::string src =
            "var r = import(\"net\").httppost(\"http://127.0.0.1:" + std::to_string(port) + "/\", "
            "\"name=ada\", contenttype=\"application/x-form\")\n"
            "String(r[\"status\"]) + \"|\" + r[\"body\"]\n";
        CHECK(evalStr(vm, src) == "200|posted");
        server.join();
        CHECK(request.rfind("POST ", 0) == 0);                               // method line
        CHECK(request.find("name=ada") != std::string::npos);                // body transmitted
        CHECK(request.find("Content-Type: application/x-form") != std::string::npos);  // contenttype keyword
    }

    // --- Response object: status/ok/reason/text, case-insensitive header(), json() ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::thread server(serveN, srv, 1, [](const std::string&) {
            std::string b = "{\"ok\": true, \"n\": 42}";
            return "HTTP/1.1 201 Created\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(b.size()) + "\r\n\r\n" + b;
        }, nullptr);
        std::string src =
            "var r = import(\"net\").get(\"http://127.0.0.1:" + std::to_string(port) + "/\")\n"
            "var j = r.json()\n"
            "String(r.status) + \"|\" + String(r.ok) + \"|\" + r.reason + \"|\" +"
            " r.header(\"CONTENT-TYPE\") + \"|\" + String(j[\"n\"]) + \"|\" + String(j[\"ok\"])\n";
        CHECK(evalStr(vm, src) == "201|True|Created|application/json|42|True");
        server.join();
    }

    // --- redirect following: 302 -> Location -> 200 ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::thread server(serveN, srv, 2, [](const std::string& req) -> std::string {
            if (req.rfind("GET /start", 0) == 0)
                return "HTTP/1.1 302 Found\r\nLocation: /final\r\nContent-Length: 0\r\n\r\n";
            return "HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\narrived";
        }, nullptr);
        std::string src =
            "var r = import(\"net\").get(\"http://127.0.0.1:" + std::to_string(port) + "/start\")\n"
            "String(r.status) + \"|\" + r.text + \"|\" + String(r.url.endswith(\"/final\"))\n";
        CHECK(evalStr(vm, src) == "200|arrived|True");
        server.join();
    }

    // --- HTTP verbs + params + basic auth + custom header all hit the wire ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::string req;
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
        }, &req);
        std::string src =
            "var net = import(\"net\")\n"
            "var r = net.put(\"http://127.0.0.1:" + std::to_string(port) + "/x\", "
            "{\"params\": {\"a\": \"1\"}, \"auth\": [\"user\", \"pass\"], \"headers\": {\"X-Foo\": \"bar\"}})\n"
            "String(r.status)\n";
        CHECK(evalStr(vm, src) == "200");
        server.join();
        CHECK(req.rfind("PUT /x?a=1 ", 0) == 0);                                   // verb + params
        CHECK(req.find("Authorization: Basic " + net::base64Encode("user:pass")) != std::string::npos);
        CHECK(req.find("X-Foo: bar") != std::string::npos);
    }

    // --- POST json= sets the body and Content-Type ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::string req;
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
        }, &req);
        std::string src =
            "discard import(\"net\").post(\"http://127.0.0.1:" + std::to_string(port) + "/\", "
            "{\"json\": {\"name\": \"ada\"}})\n";
        vm.runSource(src);
        server.join();
        CHECK(req.find("Content-Type: application/json") != std::string::npos);
        CHECK(req.find("\"name\": \"ada\"") != std::string::npos);
    }

    // --- cookies: Set-Cookie lands in response.cookies ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 200 OK\r\nSet-Cookie: sid=abc123; Path=/\r\nContent-Length: 0\r\n\r\n");
        }, nullptr);
        std::string src =
            "var r = import(\"net\").get(\"http://127.0.0.1:" + std::to_string(port) + "/\")\n"
            "r.cookies[\"sid\"]\n";
        CHECK(evalStr(vm, src) == "abc123");
        server.join();
    }

    // --- chunked transfer-encoding is decoded ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                               "5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n");
        }, nullptr);
        std::string src =
            "import(\"net\").get(\"http://127.0.0.1:" + std::to_string(port) + "/\").text\n";
        CHECK(evalStr(vm, src) == "Hello World");
        server.join();
    }

    // --- gzip Content-Encoding is decompressed ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::string payload = "hello gzipped world";
        std::string gz = std::string("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03", 10) +
                         deflate::compress(payload) + std::string(8, '\0');  // header + deflate + trailer
        std::thread server(serveN, srv, 1, [gz](const std::string&) {
            return "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: " +
                   std::to_string(gz.size()) + "\r\n\r\n" + gz;
        }, nullptr);
        std::string src =
            "import(\"net\").get(\"http://127.0.0.1:" + std::to_string(port) + "/\").text\n";
        CHECK(evalStr(vm, src) == payload);
        server.join();
    }

    // --- raiseforstatus throws on 4xx, ok is False ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\n\r\nno!");
        }, nullptr);
        int p = port;
        std::string okSrc =
            "import(\"net\").get(\"http://127.0.0.1:" + std::to_string(p) + "/\").ok\n";
        // run get once (server serves once); capture both ok and raiseforstatus on the same response
        std::string src =
            "var r = import(\"net\").get(\"http://127.0.0.1:" + std::to_string(p) + "/\")\n"
            "var threw = False\n"
            "try:\n"
            "    r.raiseforstatus()\n"
            "catch as e:\n"
            "    threw = True\n"
            "String(r.ok) + \"|\" + String(threw) + \"|\" + String(r.status)\n";
        CHECK(evalStr(vm, src) == "False|True|404");
        (void)okSrc;
        server.join();
    }

    // --- multipart file upload ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::string req;
        std::thread server(serveN, srv, 1, [](const std::string&) {
            return std::string("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
        }, &req);
        std::string src =
            "discard import(\"net\").post(\"http://127.0.0.1:" + std::to_string(port) + "/\", "
            "{\"files\": {\"f\": [\"data.txt\", \"file-content-here\"]}})\n";
        vm.runSource(src);
        server.join();
        CHECK(req.find("multipart/form-data; boundary=") != std::string::npos);
        CHECK(req.find("filename=\"data.txt\"") != std::string::npos);
        CHECK(req.find("file-content-here") != std::string::npos);
    }

    // --- Session: cookies set by the server persist into the next request ---
    {
        int port = 0; int srv = makeListener(port); CHECK(srv >= 0);
        std::string secondReq;
        std::thread server(serveN, srv, 2, [](const std::string& r) -> std::string {
            if (r.find("Cookie:") == std::string::npos)  // first call: hand out a cookie
                return "HTTP/1.1 200 OK\r\nSet-Cookie: sid=xyz; Path=/\r\nContent-Length: 2\r\n\r\n#1";
            return "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n#2";  // second: cookie echoed back
        }, &secondReq);
        std::string base = "http://127.0.0.1:" + std::to_string(port);
        std::string src =
            "var s = import(\"net\").Session()\n"
            "s.headers[\"X-App\"] = \"kirito\"\n"
            "discard s.get(\"" + base + "/login\")\n"     // receives Set-Cookie
            "var r = s.get(\"" + base + "/home\")\n"      // should resend it
            "r.cookies[\"sid\"]\n";
        CHECK(evalStr(vm, src) == "xyz");
        server.join();
        CHECK(secondReq.find("Cookie: sid=xyz") != std::string::npos);  // jar resent
        CHECK(secondReq.find("X-App: kirito") != std::string::npos);    // default header applied
    }

    // --- URL validation errors ---
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"https://example.com\")\n"));
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"ftp://example.com\")\n"));
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"not a url\")\n"));

    return RUN_TESTS();
}
