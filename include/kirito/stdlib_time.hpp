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
        char buf[32];
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
                    std::size_t n = std::strftime(buf, sizeof(buf),
                                                  static_cast<const StrVal&>(o).value().c_str(), &dt.tm);
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
        using namespace std::chrono;

        // time() -> Float seconds since the Unix epoch (wall clock).
        m.fn("time", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto now = system_clock::now().time_since_epoch();
            return vm.makeFloat(duration<double>(now).count());
        });

        // time_ns() -> Integer nanoseconds since the epoch (wall clock).
        m.fn("time_ns", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto now = system_clock::now().time_since_epoch();
            return vm.makeInt(static_cast<int64_t>(duration_cast<nanoseconds>(now).count()));
        });

        // monotonic() -> Float seconds from a steady clock (for measuring intervals).
        m.fn("monotonic", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto now = steady_clock::now().time_since_epoch();
            return vm.makeFloat(duration<double>(now).count());
        });

        // perf_counter_ns() -> Integer nanoseconds from the highest-resolution steady clock.
        m.fn("perf_counter_ns", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto now = steady_clock::now().time_since_epoch();
            return vm.makeInt(static_cast<int64_t>(duration_cast<nanoseconds>(now).count()));
        });

        // sleep(seconds) — Float or Integer seconds.
        m.fn("sleep", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            double secs = o.kind() == ValueKind::Integer
                              ? static_cast<double>(static_cast<const IntVal&>(o).value())
                          : o.kind() == ValueKind::Float ? static_cast<const FloatVal&>(o).value()
                                                         : throw KiritoError("sleep expects a number");
            if (secs > 0) std::this_thread::sleep_for(duration<double>(secs));
            return vm.none();
        });

        // datetime(timestamp) -> DateTime; with no arg, the current UTC time.
        m.fn("datetime", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            int64_t secs;
            if (a.empty()) {
                secs = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            } else {
                const Object& o = vm.arena().deref(a[0]);
                if (o.kind() == ValueKind::Integer) secs = static_cast<const IntVal&>(o).value();
                else if (o.kind() == ValueKind::Float)
                    secs = static_cast<int64_t>(static_cast<const FloatVal&>(o).value());
                else throw KiritoError("datetime expects a numeric timestamp");
            }
            return vm.alloc(std::make_unique<DateTime>(secs));
        });

        // now() -> DateTime for the current UTC time (convenience).
        m.fn("now", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            int64_t secs = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            return vm.alloc(std::make_unique<DateTime>(secs));
        });
    }
};

}  // namespace kirito

#endif
