#pragma once
#include "IR.h"
#include "code_generator/CodeGenerator.h"
#include "parser/ast/NodeVisitor.h"
#include <string>
#include <unordered_map>
#include <vector>

// Builds a custom IRModule from an Oberon-0 AST.
// No LLVM dependency — the resulting IRModule serializes to valid LLVM IR text.
class IRBuilder : public NodeVisitor, public CodeGenerator {
public:
    explicit IRBuilder(std::string filename);

    // CodeGenerator interface
    void generate_code(ModuleNode &mod) override;

    const IRModule &module() const { return module_; }

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
    // ── Type table entry ───────────────────────────────────────────────

    struct TypeEntry {
        IRType ir_type;
        // For records: field name → (field index, IRType), in declaration order
        std::vector<std::pair<std::string, IRType>> fields;
    };

    // ── Variable table entry ───────────────────────────────────────────

    struct VarEntry {
        std::string ptr;           // "%name.addr" for locals, "@name" for globals
        IRType      type;          // the base type (not the pointer)
        bool        is_var_param;  // VAR parameter → double indirection
        bool        is_global;     // stored as IR global
    };

    // ── State ──────────────────────────────────────────────────────────

    IRModule    module_;
    std::string filename_;

    // Use indices instead of pointers to survive vector reallocations.
    static constexpr size_t NONE = ~size_t{0};
    size_t current_fn_idx_ = NONE;
    size_t current_bb_idx_ = NONE;

    int  tmp_counter_   = 0;
    int  block_counter_ = 0;
    int  scope_depth_   = 0;   // 0 = module level, 1+ = inside procedure

    std::vector<std::unordered_map<std::string, TypeEntry>> type_scopes_;
    std::vector<std::unordered_map<std::string, VarEntry>>  var_scopes_;

    // Result of the last expression visit
    std::string current_val_;
    IRType      current_type_;

    // When true, visiting an IdentSelectorExpression leaves a pointer in
    // current_val_ instead of a loaded value.
    bool want_ptr_ = false;

    // ── Helpers ────────────────────────────────────────────────────────

    std::string   tmp();
    std::string   fresh_label(const std::string &hint);
    void          emit(IRInstr instr);
    IRBasicBlock &current_bb();
    IRFunction   &current_fn();
    void          new_block(const std::string &hint);

    void push_scope();
    void pop_scope();

    IRType      lookup_type(const std::string &name);
    TypeEntry  &lookup_type_entry(const std::string &name);
    VarEntry   *lookup_var(const std::string &name);

    // Load an ident+selector as an rvalue (sets current_val_ / current_type_).
    void load_ident_selector(IdentNode &ident, SelectorNode *sel);

    // Get pointer to an ident+selector lvalue (sets current_val_ / current_type_).
    void ptr_ident_selector(IdentNode &ident, SelectorNode *sel);
};
