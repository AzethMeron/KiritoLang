#ifndef KIRITO_RUNTIME_HPP
#define KIRITO_RUNTIME_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "ast.hpp"
#include "builtins.hpp"
#include "evaluator.hpp"
#include "function.hpp"
#include "lexer.hpp"
#include "object.hpp"
#include "parser.hpp"
#include "vm.hpp"

// Definitions that need a complete KiritoVM (and the front end): they live here, included last,
// so the per-component headers stay free of upward dependencies.

namespace kirito {

// --- numeric helpers (Python semantics) -----------------------------------------------------

inline bool isNumeric(const Object& o) {
    return o.kind() == ValueKind::Integer || o.kind() == ValueKind::Float;
}
inline double asDouble(const Object& o) {
    return o.kind() == ValueKind::Integer
               ? static_cast<double>(static_cast<const IntVal&>(o).value())
               : static_cast<const FloatVal&>(o).value();
}
inline bool floatEqual(double l, double r) {
    constexpr double absEps = 1e-9, relEps = 1e-9;
    double diff = std::fabs(l - r);
    if (diff <= absEps) return true;
    return diff <= relEps * std::max(std::fabs(l), std::fabs(r));
}
inline int64_t ifloordiv(int64_t a, int64_t b) {
    int64_t q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}
inline int64_t imod(int64_t a, int64_t b) {
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
inline int64_t ipow(int64_t base, int64_t exp) {
    int64_t result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        exp >>= 1;
        if (exp) base *= base;
    }
    return result;
}

// Shared arithmetic dispatch for Integer/Float. Integer⊕Integer stays Integer (except `/`),
// any Float promotes to Float, and `/` always yields Float.
inline Handle numericBinary(KiritoVM& vm, BinOp op, Handle aH, Handle bH) {
    const Object& a = vm.arena().deref(aH);
    const Object& b = vm.arena().deref(bH);
    if (!isNumeric(b))
        throw KiritoError("unsupported operand type '" + b.typeName() +
                          "' for arithmetic with '" + a.typeName() + "'");

    if (op == BinOp::Div) {
        double db = asDouble(b);
        if (db == 0.0) throw KiritoError("division by zero");
        return vm.makeFloat(asDouble(a) / db);
    }

    switch (op) {
        case BinOp::Lt: return vm.makeBool(asDouble(a) < asDouble(b));
        case BinOp::Le: return vm.makeBool(asDouble(a) <= asDouble(b));
        case BinOp::Gt: return vm.makeBool(asDouble(a) > asDouble(b));
        case BinOp::Ge: return vm.makeBool(asDouble(a) >= asDouble(b));
        default: break;
    }

    if (a.kind() == ValueKind::Integer && b.kind() == ValueKind::Integer) {
        int64_t x = static_cast<const IntVal&>(a).value();
        int64_t y = static_cast<const IntVal&>(b).value();
        switch (op) {
            case BinOp::Add: return vm.makeInt(x + y);
            case BinOp::Sub: return vm.makeInt(x - y);
            case BinOp::Mul: return vm.makeInt(x * y);
            case BinOp::FloorDiv:
                if (y == 0) throw KiritoError("integer division by zero");
                return vm.makeInt(ifloordiv(x, y));
            case BinOp::Mod:
                if (y == 0) throw KiritoError("integer modulo by zero");
                return vm.makeInt(imod(x, y));
            case BinOp::Pow:
                if (y < 0) return vm.makeFloat(std::pow(static_cast<double>(x), static_cast<double>(y)));
                return vm.makeInt(ipow(x, y));
            default: break;
        }
    } else {
        double x = asDouble(a), y = asDouble(b);
        switch (op) {
            case BinOp::Add: return vm.makeFloat(x + y);
            case BinOp::Sub: return vm.makeFloat(x - y);
            case BinOp::Mul: return vm.makeFloat(x * y);
            case BinOp::FloorDiv:
                if (y == 0.0) throw KiritoError("float division by zero");
                return vm.makeFloat(std::floor(x / y));
            case BinOp::Mod:
                if (y == 0.0) throw KiritoError("float modulo by zero");
                return vm.makeFloat(x - std::floor(x / y) * y);
            case BinOp::Pow: return vm.makeFloat(std::pow(x, y));
            default: break;
        }
    }
    throw KiritoError("unsupported numeric operator");
}

// --- IntVal / FloatVal out-of-line members ---------------------------------------------------

inline bool IntVal::equals(const ObjectArena&, const Object& other) const {
    if (other.kind() == ValueKind::Integer) return value_ == static_cast<const IntVal&>(other).value();
    if (other.kind() == ValueKind::Float)
        return floatEqual(static_cast<double>(value_), static_cast<const FloatVal&>(other).value());
    return false;
}
inline Handle IntVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    return numericBinary(vm, op, self, rhs);
}
inline Handle IntVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op == UnOp::Neg) return vm.makeInt(-value_);
    throw KiritoError("Integer does not support this unary operator");
}

inline bool FloatVal::equals(const ObjectArena&, const Object& other) const {
    if (other.kind() == ValueKind::Float) return floatEqual(value_, static_cast<const FloatVal&>(other).value());
    if (other.kind() == ValueKind::Integer)
        return floatEqual(value_, static_cast<double>(static_cast<const IntVal&>(other).value()));
    return false;
}
inline std::size_t FloatVal::hash() const {
    double rounded = std::round(value_);
    if (rounded == value_ && std::fabs(value_) < 9.2e18)
        return std::hash<int64_t>{}(static_cast<int64_t>(rounded));
    return std::hash<double>{}(value_);
}
inline Handle FloatVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    return numericBinary(vm, op, self, rhs);
}
inline Handle FloatVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op == UnOp::Neg) return vm.makeFloat(-value_);
    throw KiritoError("Float does not support this unary operator");
}

// --- StrVal out-of-line members --------------------------------------------------------------

inline Handle StrVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    const Object& b = vm.arena().deref(rhs);
    if (op == BinOp::Add) {
        if (b.kind() != ValueKind::String)
            throw KiritoError("can only concatenate String to String, not '" + b.typeName() + "'");
        return vm.makeString(value_ + static_cast<const StrVal&>(b).value());
    }
    if (b.kind() == ValueKind::String) {
        const std::string& r = static_cast<const StrVal&>(b).value();
        switch (op) {
            case BinOp::Lt: return vm.makeBool(value_ < r);
            case BinOp::Le: return vm.makeBool(value_ <= r);
            case BinOp::Gt: return vm.makeBool(value_ > r);
            case BinOp::Ge: return vm.makeBool(value_ >= r);
            default: break;
        }
    }
    throw KiritoError("type 'String' does not support this operator");
}

// --- KiFunction call -------------------------------------------------------------------------

inline Handle KiFunction::call(KiritoVM& vm, std::span<const Handle> args) {
    const auto& params = def_->params;
    if (args.size() != params.size())
        throw KiritoError("function expected " + std::to_string(params.size()) +
                          " argument(s), got " + std::to_string(args.size()));
    Handle scope = vm.newScope(closure_);
    auto& env = static_cast<EnvValue&>(vm.arena().deref(scope));
    for (std::size_t i = 0; i < params.size(); ++i) env.define(params[i], args[i]);
    Evaluator ev(vm, scope);
    return ev.callBody(def_->body);
}

// --- VM entry point & lifetime ---------------------------------------------------------------

inline void KiritoVM::retainChunk(std::unique_ptr<ast::Program> chunk) {
    chunks_.push_back(std::move(chunk));
}

inline KiritoVM::~KiritoVM() = default;

inline Handle KiritoVM::runSource(std::string_view source, std::string_view) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    auto prog = std::make_unique<ast::Program>(parser.parseProgram());
    const ast::Program& program = *prog;
    retainChunk(std::move(prog));  // keep the AST alive for the VM's lifetime (closures)
    Handle moduleScope = newModuleScope();
    Evaluator ev(*this, moduleScope);
    return ev.run(program);
}

}  // namespace kirito

#endif
