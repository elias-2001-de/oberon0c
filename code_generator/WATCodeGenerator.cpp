#include "WATCodeGenerator.h"
#include "parser/ast/base_blocks/IdentNode.h"
#include "parser/ast/base_blocks/IntNode.h"
#include "parser/ast/base_blocks/SelectorNode.h"
#include "parser/ast/declarations/ArrayTypeNode.h"
#include "parser/ast/declarations/DeclarationsNode.h"
#include "parser/ast/declarations/ProcedureDeclarationNode.h"
#include "parser/ast/declarations/RecordTypeNode.h"
#include "parser/ast/statements/AssignmentNode.h"
#include "parser/ast/statements/IfStatementNode.h"
#include "parser/ast/statements/ProcedureCallNode.h"
#include "parser/ast/statements/RepeatStatementNode.h"
#include "parser/ast/statements/StatementSequenceNode.h"
#include "parser/ast/statements/WhileStatementNode.h"
#include "parser/ast/ModuleNode.h"
#include "util/panic.h"
#include <cassert>

// ── Constructor ───────────────────────────────────────────────────────────

WATCodeGenerator::WATCodeGenerator() = default;

void WATCodeGenerator::generate_code(ModuleNode &mod) { visit(mod); }

// ── Scope helpers ─────────────────────────────────────────────────────────

void WATCodeGenerator::push_scope()
{
    var_scopes_.emplace_back();
    type_scopes_.emplace_back();
}

void WATCodeGenerator::pop_scope()
{
    var_scopes_.pop_back();
    type_scopes_.pop_back();
}

WATCodeGenerator::VarEntry *WATCodeGenerator::lookup_var(const std::string &name)
{
    for (int i = static_cast<int>(var_scopes_.size()) - 1; i >= 0; --i) {
        auto it = var_scopes_[static_cast<size_t>(i)].find(name);
        if (it != var_scopes_[static_cast<size_t>(i)].end()) return &it->second;
    }
    return nullptr;
}

std::string WATCodeGenerator::lookup_type(const std::string &name)
{
    for (int i = static_cast<int>(type_scopes_.size()) - 1; i >= 0; --i) {
        auto it = type_scopes_[static_cast<size_t>(i)].find(name);
        if (it != type_scopes_[static_cast<size_t>(i)].end()) return it->second;
    }
    panic("WATCodeGenerator: unknown type: " + name);
}

// ── Emit helpers ──────────────────────────────────────────────────────────

void WATCodeGenerator::emit(const std::string &line)
{
    assert(fn_ && "emit called outside function context");
    fn_->body_ss << std::string(static_cast<size_t>(fn_->indent), ' ') << line << "\n";
}

void WATCodeGenerator::indent_more() { if (fn_) fn_->indent += 2; }
void WATCodeGenerator::indent_less() { if (fn_) fn_->indent -= 2; }

void WATCodeGenerator::emit_load(const std::string &name)
{
    auto *v = lookup_var(name);
    if (!v) panic("WATCodeGenerator: variable not found: " + name);
    if (v->is_global)
        emit("global.get $" + name);
    else
        emit("local.get $" + name);
}

void WATCodeGenerator::emit_store(const std::string &name)
{
    auto *v = lookup_var(name);
    if (!v) panic("WATCodeGenerator: variable not found: " + name);
    if (v->is_global)
        emit("global.set $" + name);
    else
        emit("local.set $" + name);
}

std::string WATCodeGenerator::fresh_label(const std::string &hint)
{
    return "$" + hint + std::to_string(block_counter_++);
}

// Flush the current FnState into funcs_ss_
void WATCodeGenerator::finalize_fn(bool is_main)
{
    assert(fn_);
    funcs_ss_ << "  (func $" << fn_->name;
    funcs_ss_ << fn_->params_ss.str();
    if (is_main) funcs_ss_ << " (result i32)";
    funcs_ss_ << "\n";
    for (auto &loc : fn_->locals)
        funcs_ss_ << "    " << loc << "\n";
    funcs_ss_ << fn_->body_ss.str();
    funcs_ss_ << "  )\n";
}

// ── Module ────────────────────────────────────────────────────────────────

void WATCodeGenerator::visit(ModuleNode &node)
{
    push_scope();
    type_scopes_.back()["INTEGER"] = "i64";
    type_scopes_.back()["BOOLEAN"] = "i32";

    // Process declarations (globals + procedure functions)
    scope_depth_ = 0;
    if (node.get_declarations()) node.get_declarations()->accept(*this);

    // Build main function
    FnState main_state;
    main_state.name   = "main";
    main_state.indent = 4;
    fn_ = &main_state;

    if (node.get_statements()) node.get_statements()->accept(*this);
    emit("i32.const 0");   // return 0

    finalize_fn(/*is_main=*/true);
    fn_ = nullptr;

    exports_ss_ << "  (export \"main\" (func $main))\n";

    // Assemble final WAT
    std::ostringstream ss;
    ss << "(module\n";
    ss << globals_ss_.str();
    if (!globals_ss_.str().empty()) ss << "\n";
    ss << funcs_ss_.str();
    ss << "\n";
    ss << exports_ss_.str();
    ss << ")\n";
    wat_ = ss.str();

    pop_scope();
}

// ── Declarations ─────────────────────────────────────────────────────────

void WATCodeGenerator::visit(DeclarationsNode &node)
{
    // Type declarations
    for (auto [id, typenode] : node.get_typenames()) {
        typenode->accept(*this);   // sets current_type_str_ (unused — types are always primitive here)
        // Register alias in type table
        std::string alias = id->get_value();
        std::string base  = lookup_type(id->get_actual_type().name);
        type_scopes_.back()[alias] = base;
    }

    // Constant declarations
    for (auto [id, expr] : node.get_constants()) {
        assert(expr->get_value().has_value());
        long val          = expr->get_value().value();
        std::string wtype = lookup_type(id->get_actual_type().name);

        if (scope_depth_ == 0) {
            globals_ss_ << "  (global $" << id->get_value()
                        << " (mut " << wtype << ") ("
                        << wtype << ".const " << val << "))\n";
            exports_ss_ << "  (export \"" << id->get_value()
                        << "\" (global $" << id->get_value() << "))\n";
            var_scopes_.back()[id->get_value()] = {true, wtype};
        } else {
            assert(fn_);
            fn_->locals.push_back("(local $" + id->get_value() + " " + wtype + ")");
            emit(wtype + ".const " + std::to_string(val));
            emit("local.set $" + id->get_value());
            var_scopes_.back()[id->get_value()] = {false, wtype};
        }
    }

    // Variable declarations
    for (auto [idents, _typenode] : node.get_variables()) {
        for (auto *id : idents) {
            std::string wtype = lookup_type(id->get_actual_type().name);
            if (scope_depth_ == 0) {
                globals_ss_ << "  (global $" << id->get_value()
                            << " (mut " << wtype << ") ("
                            << wtype << ".const 0))\n";
                exports_ss_ << "  (export \"" << id->get_value()
                            << "\" (global $" << id->get_value() << "))\n";
                var_scopes_.back()[id->get_value()] = {true, wtype};
            } else {
                assert(fn_);
                fn_->locals.push_back("(local $" + id->get_value() + " " + wtype + ")");
                var_scopes_.back()[id->get_value()] = {false, wtype};
            }
        }
    }

    // Procedure declarations
    for (auto *proc : node.get_procedures()) {
        proc->accept(*this);
    }
}

// ── Procedure Declaration ─────────────────────────────────────────────────

void WATCodeGenerator::visit(ProcedureDeclarationNode &node)
{
    push_scope();
    ++scope_depth_;

    FnState fn_state;
    fn_state.name   = node.get_names().first->get_value();
    fn_state.indent = 4;
    fn_ = &fn_state;

    auto *params = node.get_parameters();
    if (params) {
        for (auto &section : *params) {
            bool  is_var  = std::get<0>(*section);
            auto *idents  = std::get<1>(*section).get();
            auto *tnode   = std::get<2>(*section).get();

            if (is_var)
                panic("WATCodeGenerator: VAR parameters require linear memory "
                      "(not yet supported)");

            std::string type_name =
                dynamic_cast<IdentNode *>(tnode)->get_value();
            std::string wtype = lookup_type(type_name);

            for (auto &id_ptr : *idents) {
                fn_state.params_ss << " (param $" << id_ptr->get_value()
                                   << " " << wtype << ")";
                var_scopes_.back()[id_ptr->get_value()] = {false, wtype};
            }
        }
    }

    if (node.get_declarations()) node.get_declarations()->accept(*this);
    if (node.get_statements())   node.get_statements()->accept(*this);

    finalize_fn(/*is_main=*/false);
    fn_ = nullptr;

    // Export the procedure so it can be called from JS
    exports_ss_ << "  (export \"" << fn_state.name
                << "\" (func $" << fn_state.name << "))\n";

    --scope_depth_;
    pop_scope();
}

// ── Type visitors ─────────────────────────────────────────────────────────

void WATCodeGenerator::visit(TypeNode &node)
{
    switch (node.getNodeType()) {
    case NodeType::ident:      visit(dynamic_cast<IdentNode &>(node));      break;
    case NodeType::array_type: visit(dynamic_cast<ArrayTypeNode &>(node));  break;
    case NodeType::record_type:visit(dynamic_cast<RecordTypeNode &>(node)); break;
    default: panic("WATCodeGenerator: unexpected TypeNode");
    }
}

void WATCodeGenerator::visit(ArrayTypeNode &)
{
    panic("WATCodeGenerator: ARRAY types require linear memory (not yet supported)");
}

void WATCodeGenerator::visit(RecordTypeNode &)
{
    panic("WATCodeGenerator: RECORD types require linear memory (not yet supported)");
}

// ── Statement visitors ────────────────────────────────────────────────────

void WATCodeGenerator::visit(StatementNode &node)
{
    switch (node.getNodeType()) {
    case NodeType::assignment:     visit(dynamic_cast<AssignmentNode &>(node));     break;
    case NodeType::if_statement:   visit(dynamic_cast<IfStatementNode &>(node));    break;
    case NodeType::procedure_call: visit(dynamic_cast<ProcedureCallNode &>(node));  break;
    case NodeType::repeat_statement:visit(dynamic_cast<RepeatStatementNode &>(node));break;
    case NodeType::while_statement: visit(dynamic_cast<WhileStatementNode &>(node)); break;
    default: break;
    }
}

void WATCodeGenerator::visit(StatementSequenceNode &node)
{
    for (auto &stmt : *node.get_statements())
        if (stmt) stmt->accept(*this);
}

void WATCodeGenerator::visit(AssignmentNode &node)
{
    // Evaluate RHS — leaves value on stack
    node.get_expr()->accept(*this);
    // Store into LHS (simple ident only — no selector in WAT basic version)
    if (node.get_selector() && !node.get_selector()->get_selector()->empty())
        panic("WATCodeGenerator: array/record assignment requires linear memory");
    emit_store(node.get_variable()->get_value());
}

void WATCodeGenerator::visit(ProcedureCallNode &node)
{
    std::string fn_name = node.get_name();
    auto *decl          = node.get_declaration();
    if (!decl) panic("WATCodeGenerator: missing declaration for call to " + fn_name);

    auto *formal_params = decl->get_parameters();
    auto *actual_params = node.get_parameters();

    if (formal_params && actual_params) {
        auto act_it = actual_params->begin();
        for (auto &section : *formal_params) {
            bool  is_var = std::get<0>(*section);
            auto *idents = std::get<1>(*section).get();
            if (is_var)
                panic("WATCodeGenerator: VAR parameters require linear memory");
            for (size_t k = 0; k < idents->size(); ++k) {
                (*act_it)->accept(*this);
                ++act_it;
            }
        }
    }

    emit("call $" + fn_name);
}

void WATCodeGenerator::visit(IfStatementNode &node)
{
    auto *else_ifs = node.get_else_ifs();
    auto *else_seq = node.get_else();

    // Evaluate condition → i32 on stack
    node.get_condition()->accept(*this);
    emit("if");
    indent_more();
    node.get_then()->accept(*this);

    // ELSIF chains — nested else blocks
    if (else_ifs) {
        for (auto &[elif_cond, elif_body] : *else_ifs) {
            indent_less();
            emit("else");
            indent_more();
            elif_cond->accept(*this);
            emit("if");
            indent_more();
            elif_body->accept(*this);
        }
    }

    if (else_seq) {
        indent_less();
        emit("else");
        indent_more();
        else_seq->accept(*this);
    }

    // Close all open if/else blocks
    size_t close_count = 1 + (else_ifs ? else_ifs->size() : 0);
    for (size_t i = 0; i < close_count; ++i) {
        indent_less();
        emit("end");
    }
}

void WATCodeGenerator::visit(WhileStatementNode &node)
{
    auto exit_lbl = fresh_label("while_exit");
    auto loop_lbl = fresh_label("while_loop");

    emit("block " + exit_lbl);
    indent_more();
    emit("loop " + loop_lbl);
    indent_more();

    // Exit if condition is false
    node.get_expr()->accept(*this);
    emit("i32.eqz");
    emit("br_if " + exit_lbl);

    node.get_statements()->accept(*this);
    emit("br " + loop_lbl);

    indent_less();
    emit("end");
    indent_less();
    emit("end");
}

void WATCodeGenerator::visit(RepeatStatementNode &node)
{
    auto exit_lbl = fresh_label("repeat_exit");
    auto loop_lbl = fresh_label("repeat_loop");

    emit("block " + exit_lbl);
    indent_more();
    emit("loop " + loop_lbl);
    indent_more();

    node.get_statements()->accept(*this);

    // Exit when condition is true
    node.get_expr()->accept(*this);
    emit("br_if " + exit_lbl);
    emit("br " + loop_lbl);

    indent_less();
    emit("end");
    indent_less();
    emit("end");
}

// ── Expression visitors ───────────────────────────────────────────────────

void WATCodeGenerator::visit(ExpressionNode &node)
{
    switch (node.getNodeType()) {
    case NodeType::unary_expression:
        visit(dynamic_cast<UnaryExpressionNode &>(node)); break;
    case NodeType::binary_expression:
        visit(dynamic_cast<BinaryExpressionNode &>(node)); break;
    case NodeType::ident_selector_expression:
        visit(dynamic_cast<IdentSelectorExpressionNode &>(node)); break;
    case NodeType::integer:
        visit(dynamic_cast<IntNode &>(node)); break;
    default:
        panic("WATCodeGenerator: unexpected ExpressionNode");
    }
}

void WATCodeGenerator::visit(BinaryExpressionNode &expr)
{
    auto op = expr.get_op();

    // Determine operand WAT type from LHS actual type
    auto lhs_general = expr.get_lhs()->get_actual_type().general;
    std::string wtype = (lhs_general == GeneralType::BOOLEAN) ? "i32" : "i64";

    expr.get_lhs()->accept(*this);
    expr.get_rhs()->accept(*this);

    switch (op) {
    case SourceOperator::PLUS:  emit(wtype + ".add");   break;
    case SourceOperator::MINUS: emit(wtype + ".sub");   break;
    case SourceOperator::MULT:  emit(wtype + ".mul");   break;
    case SourceOperator::DIV:   emit(wtype + ".div_s"); break;
    case SourceOperator::MOD:   emit(wtype + ".rem_s"); break;
    case SourceOperator::AND:   emit("i32.and");        break;
    case SourceOperator::OR:    emit("i32.or");         break;
    case SourceOperator::EQ:    emit(wtype + ".eq");    break;
    case SourceOperator::NEQ:   emit(wtype + ".ne");    break;
    case SourceOperator::LT:    emit(wtype + ".lt_s");  break;
    case SourceOperator::LEQ:   emit(wtype + ".le_s");  break;
    case SourceOperator::GT:    emit(wtype + ".gt_s");  break;
    case SourceOperator::GEQ:   emit(wtype + ".ge_s");  break;
    default:
        panic("WATCodeGenerator: unsupported binary operator");
    }
}

void WATCodeGenerator::visit(UnaryExpressionNode &expr)
{
    expr.get_expr()->accept(*this);
    switch (expr.get_op()) {
    case SourceOperator::NEG:
        // 0 - x  (need to push 0 first, then swap operand order)
        // WAT has no neg instruction; use (i64.const 0; expr; i64.sub)
        // But we already evaluated expr... we need to save it.
        // Use a local temp: emit expr first, then compute 0 - result.
        // Trick: use i64.const 0 before the expression via a restructuring.
        // Since we can't un-emit, we emit the expression result and then
        // apply negation via the two-complement trick:
        emit("i64.const -1");
        emit("i64.mul");   // x * -1  == -x
        break;
    case SourceOperator::NOT:
        emit("i32.eqz");   // 0 → 1, non-zero → 0
        break;
    case SourceOperator::NO_OPERATOR:
    case SourceOperator::PAREN:
        break;
    default:
        panic("WATCodeGenerator: unsupported unary operator");
    }
}

void WATCodeGenerator::visit(IdentSelectorExpressionNode &node)
{
    if (node.get_selector() && !node.get_selector()->get_selector()->empty())
        panic("WATCodeGenerator: array/record access requires linear memory");
    emit_load(node.get_identifier()->get_value());
}

void WATCodeGenerator::visit(IdentNode &node)
{
    // Used when IdentNode is visited as a TypeNode
    (void)node;
}

void WATCodeGenerator::visit(IntNode &node)
{
    emit("i64.const " + std::to_string(node.get_value()));
}

void WATCodeGenerator::visit(SelectorNode &)
{
    panic("WATCodeGenerator: SelectorNode must not be visited directly");
}
