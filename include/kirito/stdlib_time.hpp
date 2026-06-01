#ifndef KIRITO_STDLIB_TIME_HPP
#define KIRITO_STDLIB_TIME_HPP

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <span>
#include <string>
#include <thread>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// Portable UTC broken-down-time -> epoch seconds (POSIX timegm / Windows _mkgmtime aren't standard).
// Pure civil-date arithmetic (Howard Hinnant's days-from-civil algorithm) so it needs no globals or
// timezone state and matches gmtime_r round-trips.
inline int64_t timegmCompat(const std::tm& tm) {
    int64_t y = tm.tm_year + 1900;
    int64_t m = tm.tm_mon + 1;
    int64_t d = tm.tm_mday;
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = era * 146097 + doe - 719468;  // days since 1970-01-01
    return days * 86400 + tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
}

// Minimal strptime covering the common UTC fields (%Y %m %d %H %M %S and literal separators). Avoids
// the platform strptime (absent on Windows). Returns false if the text doesn't match the format.
inline bool strptimeCompat(const char* s, const char* fmt, std::tm& tm) {
    auto num = [&](int width, int& out) -> bool {
        int v = 0, n = 0;
        while (*s >= '0' && *s <= '9' && n < width) { v = v * 10 + (*s - '0'); ++s; ++n; }
        if (n == 0) return false;
        out = v;
        return true;
    };
    tm.tm_mday = 1;  // sensible defaults so a partial format still yields a valid date
    while (*fmt) {
        if (*fmt == '%') {
            ++fmt;
            int v = 0;
            switch (*fmt) {
                case 'Y': if (!num(4, v)) return false; tm.tm_year = v - 1900; break;
                case 'm': if (!num(2, v)) return false; tm.tm_mon = v - 1; break;
                case 'd': if (!num(2, v)) return false; tm.tm_mday = v; break;
                case 'H': if (!num(2, v)) return false; tm.tm_hour = v; break;
                case 'M': if (!num(2, v)) return false; tm.tm_min = v; break;
                case 'S': if (!num(2, v)) return false; tm.tm_sec = v; break;
                case '%': if (*s != '%') return false; ++s; break;
                default: return false;  // unsupported directive
            }
            ++fmt;
        } else {
            if (*s != *fmt) return false;  // literal must match
            ++s; ++fmt;
        }
    }
    return true;
}

// A point in calendar time, broken into fields (like Python's datetime). Constructed from a Unix
// timestamp (seconds since the epoch, UTC). Exposes year/month/day/hour/minute/second plus
// formatting; immutable once built.
class DateTime : public NativeClass<DateTime> {
public:
    static constexpr const char* kTypeName = "DateTime";
    int64_t epoch = 0;  // whole seconds since 1970-01-01 UTC
    std::tm tm{};       // broken-down UTC fields

    explicit DateTime(int64_t secs) : epoch(secs) {
        std::time_t t = static_cast<std::time_t>(secs);
#if defined(_WIN32)
        ::gmtime_s(&tm, &t);
#else
        ::gmtime_r(&t, &tm);
#endif
    }

    std::string iso() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        return buf;
    }
    std::string str(StringifyCtx&) const override { return "DateTime(" + iso() + ")"; }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        if (name == "year") return vm.makeInt(tm.tm_year + 1900);
        if (name == "month") return vm.makeInt(tm.tm_mon + 1);
        if (name == "day") return vm.makeInt(tm.tm_mday);
        if (name == "hour") return vm.makeInt(tm.tm_hour);
        if (name == "minute") return vm.makeInt(tm.tm_min);
        if (name == "second") return vm.makeInt(tm.tm_sec);
        if (name == "weekday") return vm.makeInt(tm.tm_wday);   // 0 = Sunday
        if (name == "yearday") return vm.makeInt(tm.tm_yday + 1);
        if (name == "timestamp")
            return vm.alloc(std::make_unique<NativeFunction>(
                "timestamp",
                [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeInt(static_cast<DateTime&>(vm.arena().deref(self)).epoch);
                }, std::vector<Handle>{self}));
        // Arithmetic: add/sub a number of seconds -> a new DateTime; diff -> seconds between two.
        if (name == "add" || name == "sub")
            return vm.alloc(std::make_unique<NativeFunction>(
                std::string(name),
                [self, sub = (name == "sub")](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    if (a.size() != 1) throw KiritoError("add/sub expects 1 argument (seconds)");
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() != ValueKind::Integer) throw KiritoError("add/sub expects an Integer (seconds)");
                    int64_t delta = static_cast<const IntVal&>(o).value();
                    int64_t base = static_cast<DateTime&>(vm.arena().deref(self)).epoch;
                    return vm.alloc(std::make_unique<DateTime>(base + (sub ? -delta : delta)));
                }, std::vector<Handle>{self}));
        if (name == "diff")
            return vm.alloc(std::make_unique<NativeFunction>(
                "diff",
                [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    if (a.size() != 1) throw KiritoError("diff expects 1 argument (a DateTime)");
                    const auto* other = dynamic_cast<const DateTime*>(&vm.arena().deref(a[0]));
                    if (!other) throw KiritoError("diff expects a DateTime");
                    return vm.makeInt(static_cast<DateTime&>(vm.arena().deref(self)).epoch - other->epoch);
                }, std::vector<Handle>{self}));
        if (name == "iso" || name == "isoformat")
            return vm.alloc(std::make_unique<NativeFunction>(
                "iso",
                [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeString(static_cast<DateTime&>(vm.arena().deref(self)).iso());
                }, std::vector<Handle>{self}));
        if (name == "format")
            return vm.alloc(std::make_unique<NativeFunction>(
                "format",
                [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    auto& dt = static_cast<DateTime&>(vm.arena().deref(self));
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() != ValueKind::String) throw KiritoError("format expects a String");
                    char buf[256];
                    // The format string is intentionally user-supplied (that's the feature), so the
                    // non-literal-format warning is expected here; silence it locally.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
                    std::size_t n = std::strftime(buf, sizeof(buf),
                                                  static_cast<const StrVal&>(o).value().c_str(), &dt.tm);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
                    return vm.makeString(std::string(buf, n));
                }, std::vector<Handle>{self}));
        return Object::getAttr(vm, self, name);
    }
};

// The `time` module: high-precision clocks and a small datetime facility.
class TimeModule : public NativeModule {
public:
    std::string name() const override { return "time"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();
        using namespace std::chrono;

        // time() -> Float seconds since the Unix epoch (wall clock).
        m.fn("time", {}, "Float", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return val(vm, duration<double>(system_clock::now().time_since_epoch()).count());
        });

        // timens() -> Integer nanoseconds since the epoch (wall clock).
        m.fn("timens", {}, "Integer", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return val(vm, static_cast<int64_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count()));
        });

        // monotonic() -> Float seconds from a steady clock (for measuring intervals).
        m.fn("monotonic", {}, "Float", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return val(vm, duration<double>(steady_clock::now().time_since_epoch()).count());
        });

        // perfcounterns() -> Integer nanoseconds from the highest-resolution steady clock.
        m.fn("perfcounterns", {}, "Integer", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return val(vm, static_cast<int64_t>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()));
        });

        // sleep(seconds) — Float or Integer seconds.
        m.fn("sleep", {{"seconds", "Number"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            double secs = Args(vm, a, "sleep")[0].asFloat("sleep");
            if (secs > 0) std::this_thread::sleep_for(duration<double>(secs));
            return none(vm);
        });

        // datetime(timestamp) -> DateTime; with no arg (or None), the current UTC time.
        m.fn("datetime", {{"timestamp", "", vm.none()}}, "DateTime", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "datetime");
            int64_t secs;
            if (args.empty() || args[0].isNone())
                secs = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            else
                secs = static_cast<int64_t>(args[0].asFloat("datetime timestamp"));
            return vm.alloc(std::make_unique<DateTime>(secs));
        });

        // now() -> DateTime for the current UTC time (convenience).
        m.fn("now", {}, "DateTime", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            int64_t secs = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            return vm.alloc(std::make_unique<DateTime>(secs));
        });

        // make(year, month, day[, hour, minute, second]) -> DateTime built from UTC components.
        m.fn("make", {{"year", "Integer"}, {"month", "Integer"}, {"day", "Integer"}, {"hour", "Integer", vm.makeInt(0)}, {"minute", "Integer", vm.makeInt(0)}, {"second", "Integer", vm.makeInt(0)}}, "DateTime", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "make");
            auto iv = [&](std::size_t i, int dflt) {
                return i >= args.size() ? dflt : static_cast<int>(args[i].asInt("make component"));
            };
            std::tm tm{};
            tm.tm_year = iv(0, 1970) - 1900;
            tm.tm_mon = iv(1, 1) - 1;
            tm.tm_mday = iv(2, 1);
            tm.tm_hour = iv(3, 0);
            tm.tm_min = iv(4, 0);
            tm.tm_sec = iv(5, 0);
            return vm.alloc(std::make_unique<DateTime>(timegmCompat(tm)));
        });

        // strptime(text, format) -> DateTime, parsing UTC fields with the given strftime format.
        m.fn("strptime", {{"text", "String"}, {"format", "String"}}, "DateTime", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "strptime");
            std::tm tm{};
            if (!strptimeCompat(args[0].asString("strptime text").c_str(),
                                args[1].asString("strptime format").c_str(), tm))
                throw KiritoError("strptime: text does not match format");
            return vm.alloc(std::make_unique<DateTime>(timegmCompat(tm)));
        });
    }
};

}  // namespace kirito

#endif
