#include <gtest/gtest.h>
#include <string>

#include "scanner/Scanner.h"
#include "parser/Parser.h"
#include "semantic_checker/SemanticChecker.h"
#include "code_generator/WATCodeGenerator.h"

struct WATCtx {
    std::ostringstream out_, err_;
    Logger logger_;
    WATCtx() : logger_(LogLevel::ERROR, out_, err_) {}
};

static std::string build_wat(const std::string &src) {
    WATCtx ctx;
    Scanner scanner(src, ctx.logger_);
    Parser  parser(scanner, ctx.logger_);
    auto mod = parser.parse();
    if (!mod || ctx.logger_.getErrorCount() > 0) return "";
    SemanticChecker checker(ctx.logger_);
    checker.validate_program(*mod);
    if (ctx.logger_.getErrorCount() > 0) return "";
    WATCodeGenerator gen;
    gen.generate_code(*mod);
    return gen.get_wat();
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, EmptyModule) {
    auto wat = build_wat("MODULE M; BEGIN END M.");
    EXPECT_NE(wat.find("(module"),         std::string::npos);
    EXPECT_NE(wat.find("(func $main"),     std::string::npos);
    EXPECT_NE(wat.find("(result i32)"),    std::string::npos);
    EXPECT_NE(wat.find("i32.const 0"),     std::string::npos);
    EXPECT_NE(wat.find("(export \"main\""), std::string::npos);
}

TEST(WATCodegenTest, ModuleClosingParen) {
    auto wat = build_wat("MODULE M; BEGIN END M.");
    // Last non-whitespace character should close the module
    auto pos = wat.rfind(')');
    EXPECT_NE(pos, std::string::npos);
}

// ---------------------------------------------------------------------------
// Global variables and constants
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, GlobalIntVar) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN END M.");
    EXPECT_NE(wat.find("(global $x (mut i64) (i64.const 0))"), std::string::npos);
    EXPECT_NE(wat.find("(export \"x\" (global $x))"),          std::string::npos);
}

TEST(WATCodegenTest, GlobalBoolVar) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; BEGIN END M.");
    EXPECT_NE(wat.find("(global $b (mut i32) (i32.const 0))"), std::string::npos);
    EXPECT_NE(wat.find("(export \"b\" (global $b))"),          std::string::npos);
}

TEST(WATCodegenTest, GlobalConstant) {
    auto wat = build_wat("MODULE M; CONST N = 42; BEGIN END M.");
    EXPECT_NE(wat.find("(global $N (mut i64) (i64.const 42))"), std::string::npos);
    EXPECT_NE(wat.find("(export \"N\" (global $N))"),           std::string::npos);
}

TEST(WATCodegenTest, MultipleGlobalVars) {
    auto wat = build_wat("MODULE M; VAR x, y: INTEGER; BEGIN END M.");
    EXPECT_NE(wat.find("$x"), std::string::npos);
    EXPECT_NE(wat.find("$y"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Assignments and arithmetic
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, AssignLiteral) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := 7 END M.");
    EXPECT_NE(wat.find("i64.const 7"),  std::string::npos);
    EXPECT_NE(wat.find("global.set $x"), std::string::npos);
}

TEST(WATCodegenTest, LoadGlobal) {
    auto wat = build_wat("MODULE M; VAR x, y: INTEGER; BEGIN y := x END M.");
    EXPECT_NE(wat.find("global.get $x"), std::string::npos);
    EXPECT_NE(wat.find("global.set $y"), std::string::npos);
}

TEST(WATCodegenTest, Addition) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := x + 1 END M.");
    EXPECT_NE(wat.find("i64.add"), std::string::npos);
}

TEST(WATCodegenTest, Subtraction) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := x - 1 END M.");
    EXPECT_NE(wat.find("i64.sub"), std::string::npos);
}

TEST(WATCodegenTest, Multiplication) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := x * 2 END M.");
    EXPECT_NE(wat.find("i64.mul"), std::string::npos);
}

TEST(WATCodegenTest, Division) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := x DIV 2 END M.");
    EXPECT_NE(wat.find("i64.div_s"), std::string::npos);
}

TEST(WATCodegenTest, Modulo) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := x MOD 3 END M.");
    EXPECT_NE(wat.find("i64.rem_s"), std::string::npos);
}

TEST(WATCodegenTest, UnaryNegation) {
    auto wat = build_wat("MODULE M; VAR x: INTEGER; BEGIN x := -x END M.");
    EXPECT_NE(wat.find("i64.const -1"), std::string::npos);
    EXPECT_NE(wat.find("i64.mul"),      std::string::npos);
}

// ---------------------------------------------------------------------------
// Comparisons
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, CompareEQ) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x = 0 END M.");
    EXPECT_NE(wat.find("i64.eq"), std::string::npos);
}

TEST(WATCodegenTest, CompareNEQ) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x # 0 END M.");
    EXPECT_NE(wat.find("i64.ne"), std::string::npos);
}

TEST(WATCodegenTest, CompareLT) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x < 5 END M.");
    EXPECT_NE(wat.find("i64.lt_s"), std::string::npos);
}

TEST(WATCodegenTest, CompareLEQ) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x <= 5 END M.");
    EXPECT_NE(wat.find("i64.le_s"), std::string::npos);
}

TEST(WATCodegenTest, CompareGT) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x > 0 END M.");
    EXPECT_NE(wat.find("i64.gt_s"), std::string::npos);
}

TEST(WATCodegenTest, CompareGEQ) {
    auto wat = build_wat("MODULE M; VAR b: BOOLEAN; x: INTEGER; BEGIN b := x >= 0 END M.");
    EXPECT_NE(wat.find("i64.ge_s"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Boolean operators
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, BooleanAnd) {
    auto wat = build_wat(R"(
MODULE M;
VAR a, b: BOOLEAN;
BEGIN a := a & b END M.
)");
    EXPECT_NE(wat.find("i32.and"), std::string::npos);
}

TEST(WATCodegenTest, BooleanOr) {
    auto wat = build_wat(R"(
MODULE M;
VAR a, b: BOOLEAN;
BEGIN a := a OR b END M.
)");
    EXPECT_NE(wat.find("i32.or"), std::string::npos);
}

TEST(WATCodegenTest, BooleanNot) {
    auto wat = build_wat(R"(
MODULE M;
VAR b: BOOLEAN;
BEGIN b := ~b END M.
)");
    EXPECT_NE(wat.find("i32.eqz"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Control flow — WHILE
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, WhileLoop) {
    auto wat = build_wat(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE i < 10 DO i := i + 1 END
END M.
)");
    EXPECT_NE(wat.find("block $while_exit"), std::string::npos);
    EXPECT_NE(wat.find("loop $while_loop"),  std::string::npos);
    EXPECT_NE(wat.find("i32.eqz"),           std::string::npos);
    EXPECT_NE(wat.find("br_if $while_exit"), std::string::npos);
    EXPECT_NE(wat.find("br $while_loop"),    std::string::npos);
}

TEST(WATCodegenTest, WhileConditionUsesComparison) {
    auto wat = build_wat(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  WHILE i < 5 DO i := i + 1 END
END M.
)");
    EXPECT_NE(wat.find("i64.lt_s"), std::string::npos);
}

TEST(WATCodegenTest, NestedWhile) {
    auto wat = build_wat(R"(
MODULE M;
VAR i, j: INTEGER;
BEGIN
  WHILE i < 3 DO
    WHILE j < 3 DO j := j + 1 END;
    i := i + 1
  END
END M.
)");
    // Two sets of block/loop labels (different counters)
    EXPECT_NE(wat.find("block $while_exit0"), std::string::npos);
    EXPECT_NE(wat.find("block $while_exit2"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Control flow — REPEAT
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, RepeatLoop) {
    auto wat = build_wat(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  REPEAT i := i + 1 UNTIL i >= 10
END M.
)");
    EXPECT_NE(wat.find("block $repeat_exit"), std::string::npos);
    EXPECT_NE(wat.find("loop $repeat_loop"),  std::string::npos);
    EXPECT_NE(wat.find("br_if $repeat_exit"), std::string::npos);
    EXPECT_NE(wat.find("br $repeat_loop"),    std::string::npos);
}

TEST(WATCodegenTest, RepeatConditionAfterBody) {
    // The condition check must come after the body instructions
    auto wat = build_wat(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  REPEAT i := i + 1 UNTIL i >= 5
END M.
)");
    auto body_pos = wat.find("i64.add");
    auto cond_pos = wat.find("i64.ge_s");
    EXPECT_NE(body_pos, std::string::npos);
    EXPECT_NE(cond_pos, std::string::npos);
    EXPECT_LT(body_pos, cond_pos);   // body before condition
}

// ---------------------------------------------------------------------------
// Control flow — IF
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, IfStatement) {
    auto wat = build_wat(R"(
MODULE M;
VAR x: INTEGER;
BEGIN IF x = 0 THEN x := 1 END
END M.
)");
    EXPECT_NE(wat.find("if"),  std::string::npos);
    EXPECT_NE(wat.find("end"), std::string::npos);
    EXPECT_NE(wat.find("i64.eq"), std::string::npos);
}

TEST(WATCodegenTest, IfElse) {
    auto wat = build_wat(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x > 0 THEN x := 1
  ELSE x := 0
  END
END M.
)");
    EXPECT_NE(wat.find("if"),   std::string::npos);
    EXPECT_NE(wat.find("else"), std::string::npos);
    EXPECT_NE(wat.find("end"),  std::string::npos);
}

TEST(WATCodegenTest, IfElsif) {
    auto wat = build_wat(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x < 0 THEN x := -1
  ELSIF x = 0 THEN x := 0
  ELSE x := 1
  END
END M.
)");
    // The elsif introduces a nested else/if
    auto first_else = wat.find("else");
    EXPECT_NE(first_else, std::string::npos);
    auto second_if = wat.find("if", first_else + 1);
    EXPECT_NE(second_if, std::string::npos);
}

// ---------------------------------------------------------------------------
// Procedures
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, ProcedureDefinition) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE F;
BEGIN END F;
BEGIN F() END M.
)");
    EXPECT_NE(wat.find("(func $F)"),          std::string::npos);
    EXPECT_NE(wat.find("(export \"F\""),       std::string::npos);
    EXPECT_NE(wat.find("call $F"),             std::string::npos);
}

TEST(WATCodegenTest, ProcedureWithValueParam) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE Add(x, y: INTEGER);
BEGIN END Add;
BEGIN END M.
)");
    EXPECT_NE(wat.find("(param $x i64)"), std::string::npos);
    EXPECT_NE(wat.find("(param $y i64)"), std::string::npos);
}

TEST(WATCodegenTest, ProcedureCallPassesArgs) {
    auto wat = build_wat(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE Inc(x: INTEGER);
BEGIN END Inc;
BEGIN Inc(n) END M.
)");
    // Argument must be loaded before call
    auto load_pos = wat.find("global.get $n");
    auto call_pos = wat.find("call $Inc");
    EXPECT_NE(load_pos, std::string::npos);
    EXPECT_NE(call_pos, std::string::npos);
    EXPECT_LT(load_pos, call_pos);
}

TEST(WATCodegenTest, ProcedureExported) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE Foo;
BEGIN END Foo;
BEGIN END M.
)");
    EXPECT_NE(wat.find("(export \"Foo\" (func $Foo))"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Local variables
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, LocalVar) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE P;
VAR k: INTEGER;
BEGIN k := 5 END P;
BEGIN END M.
)");
    EXPECT_NE(wat.find("(local $k i64)"), std::string::npos);
    EXPECT_NE(wat.find("i64.const 5"),    std::string::npos);
    EXPECT_NE(wat.find("local.set $k"),   std::string::npos);
}

TEST(WATCodegenTest, LocalVarLoad) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE P;
VAR i, j: INTEGER;
BEGIN j := i END P;
BEGIN END M.
)");
    EXPECT_NE(wat.find("local.get $i"), std::string::npos);
    EXPECT_NE(wat.find("local.set $j"), std::string::npos);
}

TEST(WATCodegenTest, LocalConst) {
    auto wat = build_wat(R"(
MODULE M;
PROCEDURE P;
CONST C = 99;
BEGIN END P;
BEGIN END M.
)");
    EXPECT_NE(wat.find("(local $C i64)"), std::string::npos);
    EXPECT_NE(wat.find("i64.const 99"),   std::string::npos);
    EXPECT_NE(wat.find("local.set $C"),   std::string::npos);
}

// ---------------------------------------------------------------------------
// Unsupported features panic gracefully
// ---------------------------------------------------------------------------

TEST(WATCodegenTest, ArrayTypePanics) {
    EXPECT_DEATH({
        build_wat(R"(
MODULE M;
TYPE Arr = ARRAY 5 OF INTEGER;
VAR a: Arr;
BEGIN END M.
)");
    }, "");
}

TEST(WATCodegenTest, RecordTypePanics) {
    EXPECT_DEATH({
        build_wat(R"(
MODULE M;
TYPE P = RECORD x, y: INTEGER END;
VAR p: P;
BEGIN END M.
)");
    }, "");
}

TEST(WATCodegenTest, VarParamPanics) {
    EXPECT_DEATH({
        build_wat(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN Inc(n) END M.
)");
    }, "");
}
