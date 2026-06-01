// Networking tests use a loopback server (run on a helper thread purely to provide the other end
// of the connection; the net library itself is single-threaded).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <thread>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
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

    // --- URL validation errors ---
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"https://example.com\")\n"));
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"ftp://example.com\")\n"));
    CHECK_THROWS(vm.runSource("import(\"net\").httpget(\"not a url\")\n"));

    return RUN_TESTS();
}
