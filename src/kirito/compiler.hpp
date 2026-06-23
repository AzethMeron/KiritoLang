#ifndef KIRITO_COMPILER_HPP
#define KIRITO_COMPILER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ast.hpp"
#include "bytecode.hpp"
#include "vm.hpp"

namespace kirito {

// Compiles a Block (a function body or the top-level program) into a Proto — the AST's second
// visitor, alongside the tree-walking Evaluator. It emits stack-machine instructions that reuse the
// runtime's value/operator/call semantics verbatim. Any node it does not yet handle raises
// BytecodeUnsupported, which the body boundary catches to fall back to the evaluator — so the
// bytecode path grows node-by-node while every program keeps running and stays correct.
class Compiler : public ast::ExprVisitor, public ast::StmtVisitor {
public:
    Compiler(KiritoVM& vm, Proto& proto) : vm_(vm), proto_(proto) {}

    // Compile a whole body. isFunction picks the implicit tail: a function falls off the end
    // returning None; the top-level program returns its last expression value (the REPL echo).
    void compile(const ast::Block& body, bool isFunction) {
        compileBlock(body);
        if (isFunction) { emit(Op::LoadNone); emit(Op::Return); }
        else { emit(Op::LoadResult); emit(Op::Return); }
    }

private:
    // --- emit / operand tables ---
    std::size_t emit(Op op, uint32_t a = 0, SourceSpan span = {}) {
        proto_.code.push_back(Instr{op, a, span});
        return proto_.code.size() - 1;
    }
    uint32_t here() const { return static_cast<uint32_t>(proto_.code.size()); }
    void patch(std::size_t at, uint32_t target) { proto_.code[at].a = target; }

    uint32_t addConst(Handle h) {
        proto_.consts.push_back(h);
        vm_.pushTemp(h);  // rooted while compiling; the VM pins it permanently once the Proto is kept
        return static_cast<uint32_t>(proto_.consts.size() - 1);
    }
    uint32_t addName(const std::string& n) {
        for (std::size_t i = 0; i < proto_.names.size(); ++i)
            if (proto_.names[i] == n) return static_cast<uint32_t>(i);
        proto_.names.push_back(n);
        return static_cast<uint32_t>(proto_.names.size() - 1);
    }

    // --- recursion with a depth guard mirroring the evaluator's, so a pathologically deep AST falls
    // back to the tree-walker (whose own guard raises a clean error) instead of overflowing here. ---
    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& dd) : d(dd) { if (++d > kMaxDepth) { --d; throw BytecodeUnsupported{}; } }
        ~DepthGuard() { --d; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;
        static constexpr int kMaxDepth = 3000;  // mirrors Evaluator::kMaxExprDepth: beyond it, fall back
    };
    void compileExpr(const ast::Expr& e) { DepthGuard g(depth_); e.accept(*this); }
    void compileStmt(const ast::Stmt& s) { DepthGuard g(depth_); s.accept(*this); }
    void compileBlock(const ast::Block& b) { for (const auto& s : b) compileStmt(*s); }

    // --- statements ---
    void visit(const ast::ExprStmt& s) override { compileExpr(*s.expr); emit(Op::SetResult); }
    void visit(const ast::DiscardStmt& s) override { compileExpr(*s.expr); emit(Op::Pop); emit(Op::ClearResult); }

    void visit(const ast::VarDeclStmt& s) override {
        if (s.names.size() != 1 || s.starIndex != -1) throw BytecodeUnsupported{};  // unpacking: slice 2
        compileExpr(*s.init);
        emit(Op::StoreName, addName(s.names[0]), s.span);
        emit(Op::ClearResult);
    }

    void visit(const ast::AssignStmt& s) override {
        if (s.target->exprKind() == ast::ExprKind::Tuple) throw BytecodeUnsupported{};  // unpacking: slice 2
        compileExpr(*s.value);
        compileAssignTarget(*s.target, s.span);
        emit(Op::ClearResult);
    }

    // Store the value already on the stack into a single target (name, index, or member).
    void compileAssignTarget(const ast::Expr& target, SourceSpan span) {
        switch (target.exprKind()) {
            case ast::ExprKind::Name:
                emit(Op::AssignName, addName(static_cast<const ast::NameExpr&>(target).name), span);
                break;
            case ast::ExprKind::Index: {
                const auto& idx = static_cast<const ast::IndexExpr&>(target);
                compileExpr(*idx.object);
                for (const auto& ix : idx.indices) compileExpr(*ix);
                emit(Op::SetItem, static_cast<uint32_t>(idx.indices.size()), span);
                break;
            }
            case ast::ExprKind::Member: {
                const auto& mem = static_cast<const ast::MemberExpr&>(target);
                compileExpr(*mem.object);
                emit(Op::SetAttr, addName(mem.name), span);
                break;
            }
            default: throw BytecodeUnsupported{};
        }
    }

    void visit(const ast::IfStmt& s) override {
        std::vector<std::size_t> endJumps;
        for (const auto& [cond, body] : s.branches) {
            compileExpr(*cond);
            std::size_t next = emit(Op::PopJumpIfFalse, 0, s.span);
            compileBlock(body);
            endJumps.push_back(emit(Op::Jump));
            patch(next, here());
        }
        if (s.orelse) compileBlock(*s.orelse);
        for (std::size_t j : endJumps) patch(j, here());
        emit(Op::ClearResult);
    }

    void visit(const ast::WhileStmt& s) override {
        uint32_t start = here();
        compileExpr(*s.cond);
        std::size_t exit = emit(Op::PopJumpIfFalse, 0, s.span);
        loops_.push_back(LoopCtx{0, {}, {}});
        compileBlock(s.body);
        emit(Op::Jump, start);
        uint32_t end = here();
        patch(exit, end);
        for (std::size_t j : loops_.back().breaks) patch(j, end);
        for (std::size_t j : loops_.back().continues) patch(j, start);
        loops_.pop_back();
        emit(Op::ClearResult);
    }

    void visit(const ast::ForStmt& s) override {
        if (s.vars.size() != 1 || s.starIndex != -1) throw BytecodeUnsupported{};  // unpacking: slice 2
        compileExpr(*s.iterable);
        emit(Op::GetIter, 0, s.span);
        uint32_t top = here();
        std::size_t exit = emit(Op::ForIter, 0, s.span);  // exhausted -> pops the cursor, jumps to end
        emit(Op::StoreName, addName(s.vars[0]), s.span);
        loops_.push_back(LoopCtx{1, {}, {}});  // breaking out must pop the live cursor
        compileBlock(s.body);
        emit(Op::Jump, top);
        uint32_t end = here();
        patch(exit, end);
        for (std::size_t j : loops_.back().breaks) patch(j, end);
        for (std::size_t j : loops_.back().continues) patch(j, top);
        loops_.pop_back();
        emit(Op::ClearResult);
    }

    void visit(const ast::BreakStmt&) override {
        if (loops_.empty()) throw BytecodeUnsupported{};  // parser rejects this anyway
        for (int i = 0; i < loops_.back().unwind; ++i) emit(Op::Pop);
        loops_.back().breaks.push_back(emit(Op::Jump));
    }
    void visit(const ast::ContinueStmt&) override {
        if (loops_.empty()) throw BytecodeUnsupported{};
        loops_.back().continues.push_back(emit(Op::Jump));
    }
    void visit(const ast::PassStmt&) override { emit(Op::ClearResult); }
    void visit(const ast::TodoStmt&) override { emit(Op::ClearResult); }

    void visit(const ast::AssertStmt& s) override {
        compileExpr(*s.cond);
        std::size_t ok = emit(Op::PopJumpIfTrue, 0, s.span);
        if (s.message) compileExpr(*s.message);
        else emit(Op::LoadConst, addConst(vm_.makeString("assertion failed")));
        emit(Op::Throw, 0, s.span);
        patch(ok, here());
        emit(Op::ClearResult);
    }

    void visit(const ast::ReturnStmt& s) override {
        if (s.value) compileExpr(*s.value);
        else emit(Op::LoadNone);
        emit(Op::Return, 0, s.span);
    }

    void visit(const ast::ThrowStmt& s) override {
        compileExpr(*s.value);
        emit(Op::Throw, 0, s.span);
    }

    // Not yet compiled — fall back to the tree-walker for any body that uses these (slice 2+).
    void visit(const ast::TryStmt&) override { throw BytecodeUnsupported{}; }
    void visit(const ast::WithStmt&) override { throw BytecodeUnsupported{}; }
    void visit(const ast::ClassStmt&) override { throw BytecodeUnsupported{}; }
    void visit(const ast::SwitchStmt&) override { throw BytecodeUnsupported{}; }

    // --- expressions ---
    void visit(const ast::LiteralExpr& e) override {
        if (std::holds_alternative<int64_t>(e.value))
            emit(Op::LoadConst, addConst(vm_.makeInt(std::get<int64_t>(e.value))));
        else if (std::holds_alternative<double>(e.value))
            emit(Op::LoadConst, addConst(vm_.makeFloat(std::get<double>(e.value))));
        else if (std::holds_alternative<bool>(e.value))
            emit(Op::LoadConst, addConst(vm_.makeBool(std::get<bool>(e.value))));
        else if (std::holds_alternative<std::string>(e.value))
            emit(Op::LoadConst, addConst(vm_.makeString(std::get<std::string>(e.value))));
        else
            emit(Op::LoadNone);
    }

    void visit(const ast::NameExpr& e) override { emit(Op::LoadName, addName(e.name), e.span); }

    void visit(const ast::UnaryExpr& e) override {
        compileExpr(*e.operand);
        emit(Op::UnaryOp, static_cast<uint32_t>(e.op), e.span);
    }

    void visit(const ast::BinaryExpr& e) override {
        compileExpr(*e.lhs);
        compileExpr(*e.rhs);
        emit(Op::BinaryOp, static_cast<uint32_t>(e.op), e.span);
    }

    void visit(const ast::LogicalExpr& e) override {
        compileExpr(*e.lhs);
        std::size_t shortcut = emit(e.isAnd ? Op::JumpIfFalseOrPop : Op::JumpIfTrueOrPop, 0, e.span);
        compileExpr(*e.rhs);
        patch(shortcut, here());
    }

    void visit(const ast::ConditionalExpr& e) override {
        compileExpr(*e.cond);
        std::size_t toElse = emit(Op::PopJumpIfFalse, 0, e.span);
        compileExpr(*e.then);
        std::size_t toEnd = emit(Op::Jump);
        patch(toElse, here());
        compileExpr(*e.orelse);
        patch(toEnd, here());
    }

    void visit(const ast::FunctionExpr& e) override {
        proto_.funcs.push_back(&e);
        emit(Op::MakeFunction, static_cast<uint32_t>(proto_.funcs.size() - 1), e.span);
    }

    void visit(const ast::CallExpr& e) override {
        compileExpr(*e.callee);
        CallSpec spec;
        bool sawNamed = false;
        for (const auto& arg : e.args) {
            if (arg.name.empty()) {
                if (sawNamed) throw BytecodeUnsupported{};  // positional-after-keyword: let the evaluator raise it
                ++spec.positional;
            } else {
                sawNamed = true;
                spec.names.push_back(arg.name);
            }
        }
        for (const auto& arg : e.args)            // positional values, in source order
            if (arg.name.empty()) compileExpr(*arg.value);
        for (const auto& arg : e.args)            // then keyword values, in CallSpec.names order
            if (!arg.name.empty()) compileExpr(*arg.value);
        proto_.calls.push_back(std::move(spec));
        emit(Op::Call, static_cast<uint32_t>(proto_.calls.size() - 1), e.span);
    }

    void visit(const ast::MemberExpr& e) override {
        compileExpr(*e.object);
        emit(Op::GetAttr, addName(e.name), e.span);
    }

    void visit(const ast::IndexExpr& e) override {
        compileExpr(*e.object);
        for (const auto& ix : e.indices) compileExpr(*ix);
        emit(Op::GetItem, static_cast<uint32_t>(e.indices.size()), e.span);
    }

    void visit(const ast::SliceExpr& e) override {
        compileExpr(*e.object);
        if (e.start) compileExpr(*e.start); else emit(Op::LoadNone);
        if (e.stop) compileExpr(*e.stop); else emit(Op::LoadNone);
        if (e.step) compileExpr(*e.step); else emit(Op::LoadNone);
        emit(Op::GetSlice, 0, e.span);
    }

    void visit(const ast::ListLiteral& e) override {
        for (const auto& el : e.elems) compileExpr(*el);
        emit(Op::BuildList, static_cast<uint32_t>(e.elems.size()), e.span);
    }

    void visit(const ast::SetLiteral& e) override {
        for (const auto& el : e.elems) compileExpr(*el);
        emit(Op::BuildSet, static_cast<uint32_t>(e.elems.size()), e.span);
    }

    void visit(const ast::DictLiteral& e) override {
        for (const auto& [k, v] : e.entries) { compileExpr(*k); compileExpr(*v); }
        emit(Op::BuildDict, static_cast<uint32_t>(e.entries.size()), e.span);
    }

    void visit(const ast::TupleExpr& e) override {
        for (const auto& el : e.elems)
            if (el->exprKind() == ast::ExprKind::Star) throw BytecodeUnsupported{};  // evaluator raises it
        for (const auto& el : e.elems) compileExpr(*el);
        emit(Op::BuildPack, static_cast<uint32_t>(e.elems.size()), e.span);
    }

    void visit(const ast::StarExpr&) override { throw BytecodeUnsupported{}; }  // evaluator raises it

    void visit(const ast::FStringExpr& e) override {
        for (const auto& part : e.parts) {
            if (!part.isExpr) {
                emit(Op::LoadConst, addConst(vm_.makeString(part.literal)));
            } else {
                compileExpr(*part.expr);
                emit(Op::FormatValue, addName(part.spec), e.span);
            }
        }
        emit(Op::BuildString, static_cast<uint32_t>(e.parts.size()), e.span);
    }

    struct LoopCtx {
        int unwind;  // operand-stack slots a break must pop (the for-loop's live cursor; 0 for while)
        std::vector<std::size_t> breaks;
        std::vector<std::size_t> continues;
    };

    KiritoVM& vm_;
    Proto& proto_;
    std::vector<LoopCtx> loops_;
    int depth_ = 0;
};

// Compile a body once and cache the result on the VM (keyed by the body's address). Returns the
// compiled Proto, or nullptr if the body uses a not-yet-supported node (the caller then tree-walks
// it). A failed compile is cached as nullptr so it is attempted only once.
inline const Proto* protoForBody(KiritoVM& vm, const ast::Block& body, bool isFunction) {
    const void* key = &body;
    if (vm.protoTried(key)) return vm.protoGet(key);
    std::unique_ptr<Proto> proto;
    try {
        RootScope rs(vm);  // roots the constants the compiler materialises until they are pinned
        auto p = std::make_unique<Proto>();
        Compiler c(vm, *p);
        c.compile(body, isFunction);
        for (Handle h : p->consts) vm.pinConst(h);  // survive past rs; live for the VM's lifetime
        proto = std::move(p);
    } catch (const BytecodeUnsupported&) {
        proto = nullptr;
    }
    const Proto* result = proto.get();
    vm.protoPut(key, std::move(proto));
    return result;
}

}  // namespace kirito

#endif
