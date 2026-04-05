#include "IRBuilder.h"
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
#include <stdexcept>

// ── Constructor ───────────────────────────────────────────────────────────

IRBuilder::IRBuilder(std::string filename) : filename_(std::move(filename)) {}

// ── CodeGenerator interface ───────────────────────────────────────────────

void IRBuilder::generate_code(ModuleNode &mod) { visit(mod); }

// ── Internal helpers ──────────────────────────────────────────────────────

std::string IRBuilder::tmp() { return std::to_string(tmp_counter_++); }

std::string IRBuilder::fresh_label(const std::string &hint)
{
    return hint + std::to_string(block_counter_++);
}

void IRBuilder::emit(IRInstr instr)
{
    assert(current_fn_idx_ != NONE && current_bb_idx_ != NONE);
    current_bb().instrs.push_back(std::move(instr));
}

IRBasicBlock &IRBuilder::current_bb()
{
    return module_.functions[current_fn_idx_].blocks[current_bb_idx_];
}

IRFunction &IRBuilder::current_fn()
{
    return module_.functions[current_fn_idx_];
}

void IRBuilder::new_block(const std::string &hint)
{
    assert(current_fn_idx_ != NONE);
    module_.functions[current_fn_idx_].blocks.push_back({fresh_label(hint), {}});
    current_bb_idx_ = module_.functions[current_fn_idx_].blocks.size() - 1;
}

void IRBuilder::push_scope()
{
    type_scopes_.emplace_back();
    var_scopes_.emplace_back();
}

void IRBuilder::pop_scope()
{
    assert(!type_scopes_.empty());
    type_scopes_.pop_back();
    var_scopes_.pop_back();
}

IRType IRBuilder::lookup_type(const std::string &name)
{
    return lookup_type_entry(name).ir_type;
}

IRBuilder::TypeEntry &IRBuilder::lookup_type_entry(const std::string &name)
{
    for (int i = static_cast<int>(type_scopes_.size()) - 1; i >= 0; --i) {
        auto it = type_scopes_[static_cast<size_t>(i)].find(name);
        if (it != type_scopes_[static_cast<size_t>(i)].end()) return it->second;
    }
    panic("IRBuilder: type not found: " + name);
}

IRBuilder::VarEntry *IRBuilder::lookup_var(const std::string &name)
{
    for (int i = static_cast<int>(var_scopes_.size()) - 1; i >= 0; --i) {
        auto it = var_scopes_[static_cast<size_t>(i)].find(name);
        if (it != var_scopes_[static_cast<size_t>(i)].end()) return &it->second;
    }
    return nullptr;
}

// ── Load an ident+selector (rvalue) ──────────────────────────────────────

void IRBuilder::load_ident_selector(IdentNode &ident, SelectorNode *sel)
{
    ptr_ident_selector(ident, sel);
    // current_val_ now holds the pointer, current_type_ the base type.
    auto ptr  = current_val_;
    auto type = current_type_;
    auto dst  = tmp();
    emit(IRLoad{dst, type, ptr});
    current_val_  = "%" + dst;
    current_type_ = type;
}

// ── Get pointer to an ident+selector lvalue ───────────────────────────────

void IRBuilder::ptr_ident_selector(IdentNode &ident, SelectorNode *sel)
{
    std::string name = ident.get_value();
    auto *v = lookup_var(name);
    if (!v) panic("IRBuilder: variable not found: " + name);

    std::string cur_ptr  = v->ptr;
    IRType      cur_type = v->type;

    // VAR parameter: the slot holds a pointer → load it first
    if (v->is_var_param) {
        auto dst = tmp();
        emit(IRLoad{dst, IRType::ptr(), cur_ptr});
        cur_ptr = "%" + dst;
        // cur_type stays as the base type
    }

    // Apply selectors
    if (sel && !sel->get_selector()->empty()) {
        for (auto &[is_array, id_ptr, expr_ptr] : *sel->get_selector()) {
            if (expr_ptr) {
                // Array index
                assert(cur_type.kind == IRType::Kind::Array);
                // Evaluate index
                want_ptr_ = false;
                expr_ptr->accept(*this);
                std::string idx = current_val_;

                auto elem_type = *cur_type.elem_type;
                auto gep_dst   = tmp();
                emit(IRGEP{gep_dst, cur_type, cur_ptr, {"0", idx}});
                cur_ptr  = "%" + gep_dst;
                cur_type = elem_type;
            } else {
                // Record field
                assert(cur_type.kind == IRType::Kind::Record);
                assert(id_ptr);
                std::string field_name = id_ptr->get_value();

                auto &entry  = lookup_type_entry(cur_type.record_name);
                int   field_idx = -1;
                IRType field_type;
                for (int i = 0; i < static_cast<int>(entry.fields.size()); ++i) {
                    if (entry.fields[static_cast<size_t>(i)].first == field_name) {
                        field_idx  = i;
                        field_type = entry.fields[static_cast<size_t>(i)].second;
                        break;
                    }
                }
                assert(field_idx >= 0 && "Record field not found");

                auto gep_dst = tmp();
                emit(IRGEP{gep_dst, cur_type, cur_ptr,
                           {"0", std::to_string(field_idx)}});
                cur_ptr  = "%" + gep_dst;
                cur_type = field_type;
            }
        }
    }

    current_val_  = cur_ptr;
    current_type_ = cur_type;
}

// ── Module ────────────────────────────────────────────────────────────────

void IRBuilder::visit(ModuleNode &node)
{
    push_scope();
    type_scopes_.back()["INTEGER"] = {IRType::int64(), {}};
    type_scopes_.back()["BOOLEAN"] = {IRType::int1(),  {}};

    module_.name = node.get_name().first->get_value();

    // Process declarations at module level (globals + nested procedure functions)
    scope_depth_ = 0;
    if (node.get_declarations()) node.get_declarations()->accept(*this);

    // Build main function
    IRFunction main_fn;
    main_fn.name        = "main";
    main_fn.return_type = IRType::int32();
    module_.functions.push_back(std::move(main_fn));
    current_fn_idx_ = module_.functions.size() - 1;

    module_.functions[current_fn_idx_].blocks.push_back({"entry", {}});
    current_bb_idx_ = 0;

    if (node.get_statements()) node.get_statements()->accept(*this);
    emit(IRRetI32{0});

    current_fn_idx_ = NONE;
    current_bb_idx_ = NONE;
    pop_scope();
}

// ── Declarations ─────────────────────────────────────────────────────────

void IRBuilder::visit(DeclarationsNode &node)
{
    // ── Type declarations ──────────────────────────────────────────────
    for (auto [id, typenode] : node.get_typenames()) {
        typenode->accept(*this);
        TypeEntry entry;
        entry.ir_type = current_type_;
        // For records, temp_fields_ is populated by visit(RecordTypeNode)
        // We rebuild it here from the record_name in current_type_
        if (current_type_.kind == IRType::Kind::Record) {
            entry = lookup_type_entry(current_type_.record_name);
            // Re-register under the alias name too
            type_scopes_.back()[id->get_value()] = entry;
        } else {
            type_scopes_.back()[id->get_value()] = entry;
        }
    }

    // ── Constant declarations ──────────────────────────────────────────
    for (auto [id, expr] : node.get_constants()) {
        auto type = lookup_type(id->get_actual_type().name);
        // Constants always have a statically-known value (set by semantic checker)
        assert(expr->get_value().has_value() &&
               "Constant expression must have a compile-time value");
        std::string val = std::to_string(expr->get_value().value());

        if (scope_depth_ == 0) {
            module_.globals.push_back({id->get_value(), type, val});
            var_scopes_.back()[id->get_value()] = {"@" + id->get_value(), type, false, true};
        } else {
            std::string slot = id->get_value() + ".addr";
            emit(IRAlloca{slot, type});
            emit(IRStore{type, val, "%" + slot});
            var_scopes_.back()[id->get_value()] = {"%" + slot, type, false, false};
        }
    }

    // ── Variable declarations ──────────────────────────────────────────
    for (auto [idents, _typenode] : node.get_variables()) {
        for (auto *id : idents) {
            auto type = lookup_type(id->get_actual_type().name);
            if (scope_depth_ == 0) {
                module_.globals.push_back({id->get_value(), type, type.zero_init()});
                var_scopes_.back()[id->get_value()] = {"@" + id->get_value(), type, false, true};
            } else {
                std::string slot = id->get_value() + ".addr";
                emit(IRAlloca{slot, type});
                var_scopes_.back()[id->get_value()] = {"%" + slot, type, false, false};
            }
        }
    }

    // ── Procedure declarations ─────────────────────────────────────────
    for (auto *proc : node.get_procedures()) {
        proc->accept(*this);
    }
}

// ── Procedure Declaration ─────────────────────────────────────────────────

void IRBuilder::visit(ProcedureDeclarationNode &node)
{
    auto prev_fn_idx = current_fn_idx_;
    auto prev_bb_idx = current_bb_idx_;

    push_scope();
    ++scope_depth_;

    std::string fn_name = node.get_names().first->get_value();

    IRFunction fn;
    fn.name        = fn_name;
    fn.return_type = IRType::void_();

    // Build parameter list
    auto *params = node.get_parameters();
    if (params) {
        for (auto &section : *params) {
            bool        is_var  = std::get<0>(*section);
            auto       *idents  = std::get<1>(*section).get();
            auto       *typenode = std::get<2>(*section).get();

            std::string type_name;
            if (typenode->getNodeType() == NodeType::ident) {
                type_name = dynamic_cast<IdentNode *>(typenode)->get_value();
            } else {
                panic("IRBuilder: inline type in procedure parameter not supported");
            }
            IRType base_type = lookup_type(type_name);
            IRType param_type = is_var ? IRType::ptr() : base_type;

            for (auto &id_ptr : *idents) {
                std::string pname = id_ptr->get_value() + ".arg";
                fn.params.emplace_back(param_type, pname);
            }
        }
    }

    module_.functions.push_back(std::move(fn));
    current_fn_idx_ = module_.functions.size() - 1;

    module_.functions[current_fn_idx_].blocks.push_back({"entry", {}});
    current_bb_idx_ = 0;

    // Allocate slots for parameters and register in var table
    if (params) {
        size_t arg_idx = 0;
        for (auto &section : *params) {
            bool  is_var  = std::get<0>(*section);
            auto *idents  = std::get<1>(*section).get();
            auto *typenode = std::get<2>(*section).get();

            std::string type_name =
                dynamic_cast<IdentNode *>(typenode)->get_value();
            IRType base_type = lookup_type(type_name);
            IRType param_type = is_var ? IRType::ptr() : base_type;

            for (auto &id_ptr : *idents) {
                std::string vname    = id_ptr->get_value();
                std::string arg_name = vname + ".arg";
                std::string slot     = vname + ".addr";

                emit(IRAlloca{slot, param_type});
                emit(IRStore{param_type, "%" + arg_name, "%" + slot});

                var_scopes_.back()[vname] = {"%" + slot, base_type, is_var, false};
                ++arg_idx;
            }
        }
    }

    // Process procedure body declarations and statements
    if (node.get_declarations()) node.get_declarations()->accept(*this);
    if (node.get_statements())   node.get_statements()->accept(*this);

    emit(IRRetVoid{});

    --scope_depth_;
    pop_scope();

    current_fn_idx_ = prev_fn_idx;
    current_bb_idx_ = prev_bb_idx;
}

// ── Type visitors ─────────────────────────────────────────────────────────

void IRBuilder::visit(TypeNode &node)
{
    switch (node.getNodeType()) {
    case NodeType::ident:
        visit(dynamic_cast<IdentNode &>(node));
        break;
    case NodeType::array_type:
        visit(dynamic_cast<ArrayTypeNode &>(node));
        break;
    case NodeType::record_type:
        visit(dynamic_cast<RecordTypeNode &>(node));
        break;
    default:
        panic("IRBuilder: unexpected TypeNode kind");
    }
}

void IRBuilder::visit(ArrayTypeNode &node)
{
    auto dim = node.get_dim();
    assert(dim.has_value() && dim.value() > 0);

    std::string elem_type_name = node.get_base_type_info().name;
    IRType elem_type = lookup_type(elem_type_name);
    current_type_ = IRType::array_(static_cast<int>(dim.value()), elem_type);
    current_val_  = "";
}

void IRBuilder::visit(RecordTypeNode &node)
{
    auto raw_fields  = node.get_fields();
    auto field_types = *node.get_field_types();

    // Build an IRTypeDef with fields in declaration order
    IRTypeDef td;
    td.name = "rec" + std::to_string(block_counter_++);

    TypeEntry entry;
    entry.ir_type = IRType::record_(td.name);

    for (auto &[names, _typenode] : raw_fields) {
        for (auto &fname : names) {
            auto it = field_types.find(fname);
            assert(it != field_types.end());
            IRType ftype = lookup_type(it->second.name);
            td.field_types.push_back(ftype);
            entry.fields.emplace_back(fname, ftype);
        }
    }

    module_.type_defs.push_back(td);
    type_scopes_.back()[td.name] = entry;

    current_type_ = entry.ir_type;
    current_val_  = "";
}

// ── Statement visitors ────────────────────────────────────────────────────

void IRBuilder::visit(StatementNode &node)
{
    switch (node.getNodeType()) {
    case NodeType::assignment:
        visit(dynamic_cast<AssignmentNode &>(node)); break;
    case NodeType::if_statement:
        visit(dynamic_cast<IfStatementNode &>(node)); break;
    case NodeType::procedure_call:
        visit(dynamic_cast<ProcedureCallNode &>(node)); break;
    case NodeType::repeat_statement:
        visit(dynamic_cast<RepeatStatementNode &>(node)); break;
    case NodeType::while_statement:
        visit(dynamic_cast<WhileStatementNode &>(node)); break;
    default:
        break;
    }
}

void IRBuilder::visit(StatementSequenceNode &node)
{
    for (auto &stmt : *node.get_statements()) {
        if (stmt) stmt->accept(*this);
    }
}

void IRBuilder::visit(AssignmentNode &node)
{
    // Evaluate RHS
    want_ptr_ = false;
    node.get_expr()->accept(*this);
    std::string rval  = current_val_;
    IRType      rtype = current_type_;

    // Get LHS pointer
    ptr_ident_selector(*node.get_variable(), node.get_selector());
    emit(IRStore{rtype, rval, current_val_});
}

void IRBuilder::visit(ProcedureCallNode &node)
{
    std::string fn_name = node.get_name();
    auto *decl          = node.get_declaration();
    if (!decl) panic("IRBuilder: missing declaration for call to " + fn_name);

    auto *formal_params  = decl->get_parameters();
    auto *actual_params  = node.get_parameters();

    std::vector<std::pair<IRType, std::string>> args;

    if (formal_params && actual_params) {
        auto act_it = actual_params->begin();
        for (auto &section : *formal_params) {
            bool  is_var = std::get<0>(*section);
            auto *idents = std::get<1>(*section).get();
            auto *tnode  = std::get<2>(*section).get();

            std::string type_name =
                dynamic_cast<IdentNode *>(tnode)->get_value();
            IRType base_type = lookup_type(type_name);

            for (size_t k = 0; k < idents->size(); ++k) {
                assert(act_it != actual_params->end());

                if (is_var) {
                    // Pass pointer
                    auto &act_expr = **act_it;
                    assert(act_expr.getNodeType() == NodeType::ident_selector_expression);
                    auto &ise = dynamic_cast<IdentSelectorExpressionNode &>(act_expr);
                    ptr_ident_selector(*ise.get_identifier(), ise.get_selector());
                    args.emplace_back(IRType::ptr(), current_val_);
                } else {
                    // Pass by value
                    want_ptr_ = false;
                    (*act_it)->accept(*this);
                    args.emplace_back(base_type, current_val_);
                }
                ++act_it;
            }
        }
    }

    emit(IRCall{std::nullopt, IRType::void_(), fn_name, args});
}

void IRBuilder::visit(IfStatementNode &node)
{
    auto then_lbl = fresh_label("then");
    auto post_lbl = fresh_label("post");

    auto *else_ifs = node.get_else_ifs();
    auto *else_seq = node.get_else();

    // Precompute labels for elsif chains
    std::vector<std::string> cond_lbls;
    std::vector<std::string> then_lbls;
    if (else_ifs) {
        for (size_t i = 0; i < else_ifs->size(); ++i) {
            cond_lbls.push_back(fresh_label("elsif_cond"));
            then_lbls.push_back(fresh_label("elsif_then"));
        }
    }
    std::string else_lbl = else_seq ? fresh_label("else") : post_lbl;

    // Evaluate initial condition
    want_ptr_ = false;
    node.get_condition()->accept(*this);
    std::string first_false = cond_lbls.empty() ? else_lbl : cond_lbls[0];
    emit(IRCondBr{current_val_, then_lbl, first_false});

    // then block
    new_block(then_lbl); current_bb().label = then_lbl;
    node.get_then()->accept(*this);
    emit(IRBr{post_lbl});

    // elsif blocks
    if (else_ifs) {
        for (size_t i = 0; i < else_ifs->size(); ++i) {
            // cond block
            new_block(cond_lbls[i]); current_bb().label = cond_lbls[i];
            want_ptr_ = false;
            (*else_ifs)[i].first->accept(*this);
            std::string next_false = (i + 1 < else_ifs->size())
                                         ? cond_lbls[i + 1]
                                         : else_lbl;
            emit(IRCondBr{current_val_, then_lbls[i], next_false});

            // then block
            new_block(then_lbls[i]); current_bb().label = then_lbls[i];
            (*else_ifs)[i].second->accept(*this);
            emit(IRBr{post_lbl});
        }
    }

    // else block
    if (else_seq) {
        new_block(else_lbl); current_bb().label = else_lbl;
        else_seq->accept(*this);
        emit(IRBr{post_lbl});
    }

    new_block(post_lbl); current_bb().label = post_lbl;
}

void IRBuilder::visit(WhileStatementNode &node)
{
    auto check_lbl = fresh_label("while_check");
    auto loop_lbl  = fresh_label("while_loop");
    auto tail_lbl  = fresh_label("while_tail");

    emit(IRBr{check_lbl});

    new_block(check_lbl); current_bb().label = check_lbl;
    want_ptr_ = false;
    node.get_expr()->accept(*this);
    emit(IRCondBr{current_val_, loop_lbl, tail_lbl});

    new_block(loop_lbl); current_bb().label = loop_lbl;
    node.get_statements()->accept(*this);
    emit(IRBr{check_lbl});

    new_block(tail_lbl); current_bb().label = tail_lbl;
}

void IRBuilder::visit(RepeatStatementNode &node)
{
    auto loop_lbl = fresh_label("repeat_loop");
    auto tail_lbl = fresh_label("repeat_tail");

    emit(IRBr{loop_lbl});

    new_block(loop_lbl); current_bb().label = loop_lbl;
    node.get_statements()->accept(*this);

    want_ptr_ = false;
    node.get_expr()->accept(*this);
    emit(IRCondBr{current_val_, tail_lbl, loop_lbl});

    new_block(tail_lbl); current_bb().label = tail_lbl;
}

// ── Expression visitors ───────────────────────────────────────────────────

void IRBuilder::visit(ExpressionNode &node)
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
        panic("IRBuilder: unexpected ExpressionNode kind");
    }
}

void IRBuilder::visit(BinaryExpressionNode &expr)
{
    auto op = expr.get_op();

    want_ptr_ = false;
    expr.get_lhs()->accept(*this);
    std::string lhs_val  = current_val_;
    IRType      lhs_type = current_type_;

    want_ptr_ = false;
    expr.get_rhs()->accept(*this);
    std::string rhs_val  = current_val_;
    IRType      rhs_type = current_type_;

    auto dst = tmp();

    switch (op) {
    case SourceOperator::PLUS:
        emit(IRBinOp{dst, "add", lhs_type, lhs_val, rhs_val});
        current_type_ = lhs_type;
        break;
    case SourceOperator::MINUS:
        emit(IRBinOp{dst, "sub", lhs_type, lhs_val, rhs_val});
        current_type_ = lhs_type;
        break;
    case SourceOperator::MULT:
        emit(IRBinOp{dst, "mul", lhs_type, lhs_val, rhs_val});
        current_type_ = lhs_type;
        break;
    case SourceOperator::DIV:
        emit(IRBinOp{dst, "sdiv", lhs_type, lhs_val, rhs_val});
        current_type_ = lhs_type;
        break;
    case SourceOperator::MOD:
        emit(IRBinOp{dst, "srem", lhs_type, lhs_val, rhs_val});
        current_type_ = lhs_type;
        break;
    case SourceOperator::AND:
        emit(IRBinOp{dst, "and", IRType::int1(), lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::OR:
        emit(IRBinOp{dst, "or", IRType::int1(), lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::EQ:
        emit(IRICmp{dst, "eq",  lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::NEQ:
        emit(IRICmp{dst, "ne",  lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::LT:
        emit(IRICmp{dst, "slt", lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::LEQ:
        emit(IRICmp{dst, "sle", lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::GT:
        emit(IRICmp{dst, "sgt", lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    case SourceOperator::GEQ:
        emit(IRICmp{dst, "sge", lhs_type, lhs_val, rhs_val});
        current_type_ = IRType::int1();
        break;
    default:
        panic("IRBuilder: unsupported binary operator");
    }

    current_val_ = "%" + dst;
}

void IRBuilder::visit(UnaryExpressionNode &expr)
{
    want_ptr_ = false;
    expr.get_expr()->accept(*this);

    switch (expr.get_op()) {
    case SourceOperator::NEG: {
        auto dst = tmp();
        emit(IRBinOp{dst, "sub", IRType::int64(), "0", current_val_});
        current_val_  = "%" + dst;
        current_type_ = IRType::int64();
        break;
    }
    case SourceOperator::NOT: {
        auto dst = tmp();
        emit(IRBinOp{dst, "xor", IRType::int1(), current_val_, "true"});
        current_val_  = "%" + dst;
        current_type_ = IRType::int1();
        break;
    }
    case SourceOperator::NO_OPERATOR:
    case SourceOperator::PAREN:
        break;
    default:
        panic("IRBuilder: unsupported unary operator");
    }
}

void IRBuilder::visit(IdentSelectorExpressionNode &node)
{
    if (want_ptr_) {
        ptr_ident_selector(*node.get_identifier(), node.get_selector());
    } else {
        load_ident_selector(*node.get_identifier(), node.get_selector());
    }
}

void IRBuilder::visit(IdentNode &node)
{
    // Called when an IdentNode is used as a TypeNode (type reference).
    // Look up the type and set current_type_.
    if (node.get_type_embedding().has_value()) {
        auto emb = node.get_type_embedding().value();
        current_type_ = (emb.general == GeneralType::BOOLEAN)
                            ? IRType::int1()
                            : IRType::int64();
        current_val_  = "";
        return;
    }
    // Plain type reference
    current_type_ = lookup_type(node.get_value());
    current_val_  = "";
}

void IRBuilder::visit(IntNode &node)
{
    current_val_  = std::to_string(node.get_value());
    current_type_ = IRType::int64();
}

void IRBuilder::visit(SelectorNode &)
{
    panic("IRBuilder: SelectorNode should never be visited directly");
}
