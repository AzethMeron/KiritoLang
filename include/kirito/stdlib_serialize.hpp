#ifndef KIRITO_STDLIB_SERIALIZE_HPP
#define KIRITO_STDLIB_SERIALIZE_HPP

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// Object-graph serialization that preserves sharing and cycles. Every reachable object is given an
// id by identity, so a value that appears twice (e.g. A in [A, A, B]) is written once and both
// slots reference the same id — and on load they are the same object again. Self-referential and
// mutually-referential structures round-trip too. Supported types: None/Bool/Integer/Float/String/
// List/Dict/Set.
namespace serial {

class Writer {
public:
    explicit Writer(KiritoVM& vm) : vm_(vm) {}

    std::string dump(Handle root) {
        int rootId = assign(root);
        std::ostringstream out;
        out << "KSER1 " << order_.size() << " ";
        for (Handle h : order_) emit(out, h);
        out << rootId;
        return out.str();
    }

private:
    int assign(Handle h) {
        const Object* o = &vm_.arena().deref(h);
        auto it = ids_.find(o);
        if (it != ids_.end()) return it->second;
        int id = static_cast<int>(order_.size());
        ids_[o] = id;
        order_.push_back(h);  // id assigned before recursing -> cycles terminate
        if (++depth_ > 10000) throw KiritoError("structure too deeply nested to serialize");
        std::vector<Handle> kids;
        o->children(kids);
        for (Handle k : kids) assign(k);
        --depth_;
        return id;
    }
    int idOf(Handle h) { return ids_.at(&vm_.arena().deref(h)); }

    void emit(std::ostringstream& out, Handle h) {
        const Object& o = vm_.arena().deref(h);
        switch (o.kind()) {
            case ValueKind::None: out << "N "; return;
            case ValueKind::Bool: out << "B " << (static_cast<const BoolVal&>(o).value() ? 1 : 0) << " "; return;
            case ValueKind::Integer: out << "I " << static_cast<const IntVal&>(o).value() << " "; return;
            case ValueKind::Float: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", static_cast<const FloatVal&>(o).value());
                out << "F " << buf << " ";
                return;
            }
            case ValueKind::String: {
                const std::string& s = static_cast<const StrVal&>(o).value();
                out << "S " << s.size() << " " << s << " ";
                return;
            }
            case ValueKind::List: {
                const auto& l = static_cast<const ListVal&>(o);
                out << "L " << l.elems.size() << " ";
                for (Handle e : l.elems) out << idOf(e) << " ";
                return;
            }
            case ValueKind::Dict: {
                const auto& d = static_cast<const DictVal&>(o);
                out << "D " << d.count << " ";
                for (Handle k : d.keys()) out << idOf(k) << " " << idOf(*d.find(vm_.arena(), k)) << " ";
                return;
            }
            case ValueKind::Set: {
                const auto& s = static_cast<const SetVal&>(o);
                out << "T " << s.count << " ";
                for (Handle e : s.items()) out << idOf(e) << " ";
                return;
            }
            default:
                throw KiritoError("cannot serialize type '" + o.typeName() + "'");
        }
    }

    KiritoVM& vm_;
    std::unordered_map<const Object*, int> ids_;
    std::vector<Handle> order_;
    int depth_ = 0;
};

class Reader {
public:
    Reader(KiritoVM& vm, const std::string& s) : vm_(vm), s_(s) {}

    Handle load() {
        try {
            return loadImpl();
        } catch (const std::exception& e) {
            // stoi/stod on a malformed token, etc. -> a clean Kirito error, never an escape.
            throw KiritoError(std::string("corrupt serialized data: ") + e.what());
        }
    }

private:
    Handle loadImpl() {
        if (token() != "KSER1") throw KiritoError("bad serialization header");
        long n = std::stol(token());
        if (n < 0 || static_cast<std::size_t>(n) > s_.size())
            throw KiritoError("corrupt serialized data: bad object count");
        RootScope roots(vm_);
        // Pass 1: create every object (scalars valued, containers empty) so refs/cycles resolve.
        std::vector<Handle> objs(static_cast<std::size_t>(n));
        std::vector<std::vector<int>> links(static_cast<std::size_t>(n));
        std::vector<char> kind(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            std::string t = token();
            kind[i] = t[0];
            if (t == "N") objs[i] = vm_.none();
            else if (t == "B") objs[i] = vm_.makeBool(std::stoi(token()) != 0);
            else if (t == "I") objs[i] = roots.add(vm_.makeInt(std::stoll(token())));
            else if (t == "F") objs[i] = roots.add(vm_.makeFloat(std::stod(token())));
            else if (t == "S") { int len = std::stoi(token()); objs[i] = roots.add(vm_.makeString(rawBytes(len))); }
            else if (t == "L") { objs[i] = roots.add(vm_.alloc(std::make_unique<ListVal>())); readIds(links[i]); }
            else if (t == "D") { objs[i] = roots.add(vm_.alloc(std::make_unique<DictVal>())); int c = std::stoi(token()); for (int k = 0; k < c * 2; ++k) links[i].push_back(std::stoi(token())); }
            else if (t == "T") { objs[i] = roots.add(vm_.alloc(std::make_unique<SetVal>())); readIds(links[i]); }
            else throw KiritoError("bad serialization tag '" + t + "'");
        }
        int rootId = std::stoi(token());
        // Pass 2: wire up the container links now that every object exists.
        for (int i = 0; i < n; ++i) {
            if (kind[i] == 'L') {
                auto& l = static_cast<ListVal&>(vm_.arena().deref(objs[i]));
                for (int id : links[i]) l.elems.push_back(objs[static_cast<std::size_t>(id)]);
            } else if (kind[i] == 'T') {
                auto& s = static_cast<SetVal&>(vm_.arena().deref(objs[i]));
                for (int id : links[i]) s.add(vm_.arena(), objs[static_cast<std::size_t>(id)]);
            } else if (kind[i] == 'D') {
                auto& d = static_cast<DictVal&>(vm_.arena().deref(objs[i]));
                for (std::size_t k = 0; k + 1 < links[i].size(); k += 2)
                    d.set(vm_.arena(), objs[static_cast<std::size_t>(links[i][k])], objs[static_cast<std::size_t>(links[i][k + 1])]);
            }
        }
        return objs[static_cast<std::size_t>(rootId)];
    }

private:
    void readIds(std::vector<int>& out) {
        int c = std::stoi(token());
        for (int k = 0; k < c; ++k) out.push_back(std::stoi(token()));
    }
    std::string token() {
        while (pos_ < s_.size() && s_[pos_] == ' ') ++pos_;
        std::size_t start = pos_;
        while (pos_ < s_.size() && s_[pos_] != ' ') ++pos_;
        if (start == pos_) throw KiritoError("unexpected end of serialized data");
        return s_.substr(start, pos_ - start);
    }
    std::string rawBytes(int len) {
        if (pos_ < s_.size() && s_[pos_] == ' ') ++pos_;  // single separator
        if (pos_ + static_cast<std::size_t>(len) > s_.size()) throw KiritoError("truncated string");
        std::string out = s_.substr(pos_, static_cast<std::size_t>(len));
        pos_ += static_cast<std::size_t>(len);
        return out;
    }

    KiritoVM& vm_;
    const std::string& s_;
    std::size_t pos_ = 0;
};

}  // namespace serial

class SerializeModule : public NativeModule {
public:
    std::string name() const override { return "serialize"; }
    void setup(ModuleBuilder& m) override {
        m.fn("dumps", {{"value"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            serial::Writer w(vm);
            return val(vm, w.dump(Args(vm, a, "dumps")[0]));
        });
        m.fn("loads", {{"text", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            serial::Reader r(vm, Args(vm, a, "loads")[0].asString("loads"));
            return r.load();
        });
        m.fn("save", {{"value"}, {"path", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "save");
            serial::Writer w(vm);
            std::ofstream f(args[1].asString("save path"), std::ios::binary);
            if (!f) throw KiritoError("could not open file for saving");
            f << w.dump(args[0]);
            return none(vm);
        });
        m.fn("load", {{"path", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::ifstream f(Args(vm, a, "load")[0].asString("load path"), std::ios::binary);
            if (!f) throw KiritoError("could not open file for loading");
            std::stringstream ss;
            ss << f.rdbuf();
            std::string data = ss.str();
            serial::Reader r(vm, data);
            return r.load();
        });
    }
};

}  // namespace kirito

#endif
