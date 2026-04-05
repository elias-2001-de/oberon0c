#pragma once
#include "CodeGenerator.h"
#include "parser/ast/NodeVisitor.h"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Generates WebAssembly Text Format (WAT) from an Oberon-0 AST.
// No LLVM dependency — the resulting WAT text can be assembled to
// WASM binary with wabt.js (or wat2wasm) and run directly in a browser.
//
// Current limitations (linear-memory features are left for a future pass):
//   - ARRAY and RECORD types are not supported
//   - VAR parameters are not supported
class WATCodeGenerator : public NodeVisitor, public CodeGenerator {
public:
    WATCodeGenerator();

    void generate_code(ModuleNode &mod) override;
    const std::string &get_wat() const { return wat_; }

    // ── NodeVisitor ────────────────────────────────────────────────────
    void visit(ModuleNode &) override;
    void visit(DeclarationsNode &) override;
    void visit(ProcedureDeclarationNode &) override;

    void visit(TypeNode &) override;
    void visit(ArrayTypeNode &) override;
    void visit(RecordTypeNode &) override;

    void visit(StatementNode &) override;
    void visit(StatementSequenceNode &) override;
    void visit(AssignmentNode &) override;
    void visit(ProcedureCallNode &) override;
    void visit(IfStatementNode &) override;
    void visit(WhileStatementNode &) override;
    void visit(RepeatStatementNode &) override;

    void visit(ExpressionNode &) override;
    void visit(BinaryExpressionNode &) override;
    void visit(UnaryExpressionNode &) override;
    void visit(IdentSelectorExpressionNode &) override;
    void visit(IdentNode &) override;
    void visit(IntNode &) override;
    [[noreturn]] void visit(SelectorNode &) override;

private:
    std::string wat_;

    // Accumulated sections (assembled into wat_ at the end of visit(ModuleNode))
    std::ostringstream globals_ss_;
    std::ostringstream exports_ss_;
    std::ostringstream funcs_ss_;

    // State for the function currently being built
    struct FnState {
        std::string name;
        std::ostringstream params_ss;
        std::vector<std::string> locals;   // "(local $x i64)" lines
        std::ostringstream body_ss;
        int indent = 4;
    };
    FnState *fn_ = nullptr;   // non-owning pointer to current function (on stack)

    int block_counter_ = 0;
    int scope_depth_   = 0;   // 0 = module level

    // ── Tables ─────────────────────────────────────────────────────────

    struct VarEntry {
        bool        is_global;
        std::string wat_type;   // "i64" or "i32"
    };
    std::vector<std::unordered_map<std::string, VarEntry>> var_scopes_;
    std::vector<std::unordered_map<std::string, std::string>> type_scopes_;

    void push_scope();
    void pop_scope();
    VarEntry   *lookup_var(const std::string &name);
    std::string lookup_type(const std::string &name);

    // ── Helpers ────────────────────────────────────────────────────────

    void        emit(const std::string &line);
    void        indent_more();
    void        indent_less();
    void        emit_load(const std::string &name);
    void        emit_store(const std::string &name);
    std::string fresh_label(const std::string &hint);
    void        finalize_fn(bool is_main = false);
};
