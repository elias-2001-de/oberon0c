#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "scanner/Scanner.h"
#include "parser/Parser.h"
#include "semantic_checker/SemanticChecker.h"
#include "ir/IRBuilder.h"

struct TestCtx {
    std::ostringstream out_, err_;
    Logger logger_;
    TestCtx() : logger_(LogLevel::ERROR, out_, err_) {}
};

// Run the full pipeline and return the serialized LLVM IR text.
static std::string build_ir(const std::string &src) {
    TestCtx ctx;
    Scanner scanner(src, ctx.logger_);
    Parser  parser(scanner, ctx.logger_);
    auto mod = parser.parse();
    if (!mod || ctx.logger_.getErrorCount() > 0) return "";
    SemanticChecker checker(ctx.logger_);
    checker.validate_program(*mod);
    if (ctx.logger_.getErrorCount() > 0) return "";
    IRBuilder builder("test");
    builder.generate_code(*mod);
    return builder.module().str();
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------

TEST(CodegenTest, EmptyModule) {
    auto ir = build_ir("MODULE M; BEGIN END M.");
    EXPECT_NE(ir.find("define i32 @main()"), std::string::npos);
    EXPECT_NE(ir.find("ret i32 0"),          std::string::npos);
}

TEST(CodegenTest, ModuleNameInHeader) {
    auto ir = build_ir("MODULE Hello; BEGIN END Hello.");
    EXPECT_NE(ir.find("ModuleID = 'Hello'"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Global variables
// ---------------------------------------------------------------------------

TEST(CodegenTest, GlobalIntVar) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN END M.");
    EXPECT_NE(ir.find("@x = internal global i64 0"), std::string::npos);
}

TEST(CodegenTest, GlobalBoolVar) {
    auto ir = build_ir("MODULE M; VAR b: BOOLEAN; BEGIN END M.");
    EXPECT_NE(ir.find("@b = internal global i1 0"), std::string::npos);
}

TEST(CodegenTest, GlobalConstant) {
    auto ir = build_ir("MODULE M; CONST N = 42; BEGIN END M.");
    EXPECT_NE(ir.find("@N = internal global i64 42"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Assignments
// ---------------------------------------------------------------------------

TEST(CodegenTest, AssignLiteral) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := 7 END M.");
    EXPECT_NE(ir.find("store i64 7"), std::string::npos);
}

TEST(CodegenTest, AssignArithmetic) {
    auto ir = build_ir("MODULE M; VAR x, y: INTEGER; BEGIN x := 3; y := x + 2 END M.");
    EXPECT_NE(ir.find("add i64"), std::string::npos);
    EXPECT_NE(ir.find("store i64"), std::string::npos);
}

TEST(CodegenTest, AssignSubtraction) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := 10 - 3 END M.");
    EXPECT_NE(ir.find("sub i64"), std::string::npos);
}

TEST(CodegenTest, AssignMultiplication) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := 4 * 5 END M.");
    EXPECT_NE(ir.find("mul i64"), std::string::npos);
}

TEST(CodegenTest, AssignDivision) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := 10 DIV 2 END M.");
    EXPECT_NE(ir.find("sdiv i64"), std::string::npos);
}

TEST(CodegenTest, AssignMod) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := 10 MOD 3 END M.");
    EXPECT_NE(ir.find("srem i64"), std::string::npos);
}

TEST(CodegenTest, UnaryNegation) {
    auto ir = build_ir("MODULE M; VAR x: INTEGER; BEGIN x := -5 END M.");
    EXPECT_NE(ir.find("sub i64 0,"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Boolean / comparison
// ---------------------------------------------------------------------------

TEST(CodegenTest, Comparison) {
    auto ir = build_ir("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x > 0 END M.");
    EXPECT_NE(ir.find("icmp sgt i64"), std::string::npos);
    EXPECT_NE(ir.find("store i1"), std::string::npos);
}

TEST(CodegenTest, BooleanAnd) {
    auto ir = build_ir(R"(
MODULE M;
VAR a, b: BOOLEAN;
BEGIN
  a := a & b
END M.
)");
    EXPECT_NE(ir.find("and i1"), std::string::npos);
}

TEST(CodegenTest, BooleanNot) {
    auto ir = build_ir(R"(
MODULE M;
VAR b: BOOLEAN;
BEGIN
  b := ~b
END M.
)");
    EXPECT_NE(ir.find("xor i1"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Control flow
// ---------------------------------------------------------------------------

TEST(CodegenTest, WhileLoop) {
    auto ir = build_ir(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE i < 10 DO i := i + 1 END
END M.
)");
    EXPECT_NE(ir.find("while_check"), std::string::npos);
    EXPECT_NE(ir.find("while_loop"),  std::string::npos);
    EXPECT_NE(ir.find("while_tail"),  std::string::npos);
    EXPECT_NE(ir.find("icmp slt i64"), std::string::npos);
}

TEST(CodegenTest, IfStatement) {
    auto ir = build_ir(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x = 0 THEN x := 1 END
END M.
)");
    EXPECT_NE(ir.find("then"), std::string::npos);
    EXPECT_NE(ir.find("post"), std::string::npos);
    EXPECT_NE(ir.find("icmp eq i64"), std::string::npos);
}

TEST(CodegenTest, RepeatLoop) {
    auto ir = build_ir(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  REPEAT i := i + 1 UNTIL i >= 10
END M.
)");
    EXPECT_NE(ir.find("repeat_loop"), std::string::npos);
    EXPECT_NE(ir.find("repeat_tail"), std::string::npos);
    EXPECT_NE(ir.find("icmp sge i64"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Procedures
// ---------------------------------------------------------------------------

TEST(CodegenTest, ProcedureDefinition) {
    auto ir = build_ir(R"(
MODULE M;
PROCEDURE F;
BEGIN END F;
BEGIN F() END M.
)");
    EXPECT_NE(ir.find("define void @F()"), std::string::npos);
    EXPECT_NE(ir.find("call void @F()"),   std::string::npos);
}

TEST(CodegenTest, ProcedureWithValueParam) {
    auto ir = build_ir(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE Double(x: INTEGER);
BEGIN END Double;
BEGIN Double(n) END M.
)");
    EXPECT_NE(ir.find("define void @Double(i64 %x.arg)"), std::string::npos);
    EXPECT_NE(ir.find("call void @Double(i64"),            std::string::npos);
}

TEST(CodegenTest, ProcedureWithVarParam) {
    auto ir = build_ir(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN Inc(n) END M.
)");
    EXPECT_NE(ir.find("define void @Inc(ptr %x.arg)"), std::string::npos);
    EXPECT_NE(ir.find("call void @Inc(ptr"),            std::string::npos);
}

// ---------------------------------------------------------------------------
// Array and record types
// ---------------------------------------------------------------------------

TEST(CodegenTest, ArrayTypeGlobal) {
    auto ir = build_ir(R"(
MODULE M;
TYPE Arr = ARRAY 5 OF INTEGER;
VAR a: Arr;
BEGIN END M.
)");
    EXPECT_NE(ir.find("@a = internal global [5 x i64]"), std::string::npos);
}

TEST(CodegenTest, RecordTypeDef) {
    auto ir = build_ir(R"(
MODULE M;
TYPE Point = RECORD x, y: INTEGER END;
VAR p: Point;
BEGIN END M.
)");
    // A named struct type should be emitted
    EXPECT_NE(ir.find("= type {i64, i64}"), std::string::npos);
}
