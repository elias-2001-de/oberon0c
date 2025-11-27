//
// Created by Elias on 19.04.2025.
//

#ifndef OBERON0C_CodeGeneratorIR_H
#define OBERON0C_CodeGeneratorIR_H

#include <list>
#include <vector>
#include <unordered_map>
#include <utility>
#include "TypeInfoTable.h"
#include "VariableTable.h"
#include "parser/ast/NodeVisitor.h"

enum class OutputFileType
{
    AssemblyFile,
    LLVMIRFile,
    ObjectFile
};

using namespace llvm;

class CodeGeneratorIR : public NodeVisitor
{

private:
  
    OutputFileType output_type_;
    const string filename_;
    IRTBuilder *builder_;

    std::unordered_map<string, Function *> procedures_;

    llvm::Value *value_;
    VariableTable variables_;
    TypeInfoTable type_table_;
    TypeInfoClass temp_type_;

    void init_target_machine();
    void init_builder();
    void emit();

public:
    CodeGeneratorIR(string filename, OutputFileType output_type);

    void visit(ExpressionNode &) override;
    void visit(BinaryExpressionNode &) override;
    void visit(UnaryExpressionNode &) override;
    void visit(IdentSelectorExpressionNode &) override;
    void LoadIdentSelector(IdentNode &ident, SelectorNode *selector, bool return_pointer = false);
    void LoadIdent(IdentNode &, bool return_pointer = false);

    void visit(IdentNode &) override;
    void visit(IntNode &) override;
    [[noreturn]] void visit(SelectorNode &) override;

    void visit(TypeNode &) override;
    void visit(ArrayTypeNode &) override;
    void visit(DeclarationsNode &) override;
    void create_declarations(DeclarationsNode &node, bool is_global = false);
    void visit(ProcedureDeclarationNode &) override;
    void visit(RecordTypeNode &) override;

    void visit(StatementNode &) override;
    void visit(AssignmentNode &) override;
    void visit(IfStatementNode &) override;
    void visit(ProcedureCallNode &) override;
    void visit(RepeatStatementNode &) override;
    void visit(StatementSequenceNode &) override;
    void visit(WhileStatementNode &) override;

    void visit(ModuleNode &) override;

    void generate_code(ModuleNode &);
};

#endif // OBERON0C_CodeGeneratorIR_H
