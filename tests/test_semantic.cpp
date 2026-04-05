#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "scanner/Scanner.h"
#include "parser/Parser.h"
#include "parser/ast/ModuleNode.h"
#include "semantic_checker/SemanticChecker.h"

struct TestCtx {
    std::ostringstream out_, err_;
    Logger logger_;
    TestCtx() : logger_(LogLevel::ERROR, out_, err_) {}
    [[nodiscard]] int errors() const { return logger_.getErrorCount(); }
};

// Run the full pipeline (scan → parse → semantic check) and return error count.
// Returns -1 if parsing itself failed (so caller can distinguish).
static int checkSource(const std::string &src) {
    TestCtx ctx;
    Scanner scanner(src, ctx.logger_);
    Parser parser(scanner, ctx.logger_);
    auto mod = parser.parse();
    if (!mod || ctx.errors() > 0) return ctx.errors();
    SemanticChecker checker(ctx.logger_);
    checker.validate_program(*mod);
    return ctx.errors();
}

// ---------------------------------------------------------------------------
// Valid programs — zero semantic errors expected
// ---------------------------------------------------------------------------

TEST(SemanticTest, EmptyModule) {
    EXPECT_EQ(checkSource("MODULE M; BEGIN END M."), 0);
}

TEST(SemanticTest, VarDeclAndAssignment) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN x := 5 END M.
)"), 0);
}

TEST(SemanticTest, ArithmeticExpression) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR x, y: INTEGER;
BEGIN
  x := 3;
  y := x + 2 * x - 1
END M.
)"), 0);
}

TEST(SemanticTest, BooleanConditionInIf) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x > 0 THEN x := x - 1 END
END M.
)"), 0);
}

TEST(SemanticTest, WhileLoop) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE i < 10 DO i := i + 1 END
END M.
)"), 0);
}

TEST(SemanticTest, RepeatLoop) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i := 0;
  REPEAT i := i + 1 UNTIL i >= 10
END M.
)"), 0);
}

TEST(SemanticTest, ConstantUsedInExpr) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
CONST N = 10;
VAR x: INTEGER;
BEGIN x := N END M.
)"), 0);
}

TEST(SemanticTest, ConstantUsedAsArrayDimension) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
CONST N = 5;
TYPE Arr = ARRAY N OF INTEGER;
VAR a: Arr;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, TypeAlias) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
TYPE MyInt = INTEGER;
VAR x: MyInt;
BEGIN x := 42 END M.
)"), 0);
}

TEST(SemanticTest, ArrayAccess) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
TYPE Arr = ARRAY 5 OF INTEGER;
VAR a: Arr; i: INTEGER;
BEGIN
  i := 0;
  a[i] := 42
END M.
)"), 0);
}

TEST(SemanticTest, RecordFieldAccess) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
TYPE Point = RECORD x, y: INTEGER END;
VAR p: Point;
BEGIN
  p.x := 1;
  p.y := 2
END M.
)"), 0);
}

TEST(SemanticTest, ProcedureWithVarParam) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN
  n := 0;
  Inc(n)
END M.
)"), 0);
}

TEST(SemanticTest, ProcedureWithValueParam) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR n: INTEGER;
PROCEDURE PrintVal(x: INTEGER);
BEGIN END PrintVal;
BEGIN
  n := 7;
  PrintVal(n)
END M.
)"), 0);
}

TEST(SemanticTest, FactorialProgram) {
    EXPECT_EQ(checkSource(R"(
MODULE Factorial;
VAR n, fact, i: INTEGER;
BEGIN
  n := 5; fact := 1; i := 1;
  WHILE i <= n DO
    fact := fact * i;
    i := i + 1
  END
END Factorial.
)"), 0);
}

TEST(SemanticTest, MultipleVarsAndProcedures) {
    EXPECT_EQ(checkSource(R"(
MODULE M;
VAR a, b, c: INTEGER;
PROCEDURE Swap(VAR x, y: INTEGER);
VAR tmp: INTEGER;
BEGIN
  tmp := x;
  x := y;
  y := tmp
END Swap;
BEGIN
  a := 1; b := 2;
  Swap(a, b)
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Semantic errors — at least one error expected
// ---------------------------------------------------------------------------

TEST(SemanticTest, UndefinedVariable) {
    EXPECT_GT(checkSource(R"(
MODULE M;
BEGIN
  x := 5
END M.
)"), 0);
}

TEST(SemanticTest, UndefinedVariableInExpression) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR y: INTEGER;
BEGIN
  y := x + 1
END M.
)"), 0);
}

TEST(SemanticTest, AssignBoolToInteger) {
    // A boolean comparison result cannot be assigned to an INTEGER variable
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  x := x > 0
END M.
)"), 0);
}

TEST(SemanticTest, WhileConditionMustBeBoolean) {
    // WHILE condition must be BOOLEAN, not INTEGER
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  WHILE i DO i := i - 1 END
END M.
)"), 0);
}

TEST(SemanticTest, IfConditionMustBeBoolean) {
    // IF condition must be BOOLEAN, not INTEGER
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x THEN x := 0 END
END M.
)"), 0);
}

TEST(SemanticTest, ArithmeticOnBoolean) {
    // Cannot add booleans
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  x := (x > 0) + 1
END M.
)"), 0);
}

TEST(SemanticTest, CompareIntegerAndBoolean) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x = (x > 0) THEN x := 1 END
END M.
)"), 0);
}

TEST(SemanticTest, DuplicateProcedureDeclaration) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE F(VAR x: INTEGER);
BEGIN x := 1 END F;
PROCEDURE F(VAR x: INTEGER);
BEGIN x := 2 END F;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, UndefinedTypeInVarDecl) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: FUN_TYPE;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, AccessFieldOnNonRecord) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i.f := 12
END M.
)"), 0);
}

TEST(SemanticTest, AccessUndefinedRecordField) {
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Pt = RECORD x, y: INTEGER END;
VAR p: Pt;
BEGIN
  p.z := 1
END M.
)"), 0);
}

TEST(SemanticTest, CallUndefinedProcedure) {
    EXPECT_GT(checkSource(R"(
MODULE M;
BEGIN
  UndefinedProc(1)
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Module / procedure name checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, ModuleNameMismatch_ReportsError) {
    EXPECT_GT(checkSource("MODULE Foo; BEGIN END Bar."), 0);
}

TEST(SemanticTest, ProcedureNameMismatch_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE Foo;
BEGIN END Bar;
BEGIN END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Constant assignment
// ---------------------------------------------------------------------------

TEST(SemanticTest, AssignToConstant_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
CONST N = 10;
BEGIN
  N := 5
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Procedure call argument checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, TooFewArguments_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE Add(VAR x: INTEGER; VAR y: INTEGER);
BEGIN x := x + y END Add;
VAR a: INTEGER;
BEGIN Add(a) END M.
)"), 0);
}

TEST(SemanticTest, TooManyArguments_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
VAR a, b: INTEGER;
BEGIN Inc(a, b) END M.
)"), 0);
}

TEST(SemanticTest, WrongArgumentType_ReportsError) {
    // Procedure expects INTEGER but condition (BOOLEAN) is passed
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE F(x: INTEGER);
BEGIN END F;
VAR a, b: INTEGER;
BEGIN F(a > b) END M.
)"), 0);
}

TEST(SemanticTest, LiteralPassedAsVarParam_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN Inc(5) END M.
)"), 0);
}

TEST(SemanticTest, ConstantPassedAsVarParam_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
CONST N = 10;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN Inc(N) END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Boolean operator type checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, AndOnIntegers_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x, y: INTEGER; b: INTEGER;
BEGIN
  b := x & y
END M.
)"), 0);
}

TEST(SemanticTest, OrOnIntegers_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x, y: INTEGER; b: INTEGER;
BEGIN
  b := x OR y
END M.
)"), 0);
}

TEST(SemanticTest, NotOnInteger_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF ~x THEN x := 0 END
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Array dimension checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, ArraySizeZero_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Z = ARRAY 0 OF INTEGER;
VAR a: Z;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, ArraySizeNegative_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Z = ARRAY -1 OF INTEGER;
VAR a: Z;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, ArrayIndexOutOfBounds_ReportsError) {
    // Constant index 10 is out of bounds for ARRAY 5 OF INTEGER
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Arr = ARRAY 5 OF INTEGER;
VAR a: Arr;
BEGIN
  a[10] := 1
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Duplicate declaration checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, DuplicateVariableDeclaration_ReportsError) {
    // Two declarations of x in a single VAR section
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER; x: INTEGER;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, DuplicateConstantDeclaration_ReportsError) {
    // Two declarations of N in a single CONST section
    EXPECT_GT(checkSource(R"(
MODULE M;
CONST N = 1; N = 2;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, DuplicateRecordField_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Pt = RECORD x, x: INTEGER END;
VAR p: Pt;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, DuplicateTypeDeclaration_ReportsError) {
    // Two declarations of T in a single TYPE section
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE T = INTEGER; T = INTEGER;
VAR x: T;
BEGIN END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Repeat condition check
// ---------------------------------------------------------------------------

TEST(SemanticTest, RepeatConditionMustBeBoolean) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  REPEAT i := i + 1 UNTIL i
END M.
)"), 0);
}

// ---------------------------------------------------------------------------
// Additional expression / type checks
// ---------------------------------------------------------------------------

TEST(SemanticTest, NegateBoolean_ReportsError) {
    // Unary minus on a BOOLEAN variable is illegal
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR b: BOOLEAN;
VAR x: INTEGER;
BEGIN
  x := -b
END M.
)"), 0);
}

TEST(SemanticTest, CallVariableAsProcedure_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  x(1)
END M.
)"), 0);
}

TEST(SemanticTest, AssignToProcedure_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
PROCEDURE F;
BEGIN END F;
BEGIN
  F := 5
END M.
)"), 0);
}

TEST(SemanticTest, ArrayDimensionIsVariable_ReportsError) {
    // Array size must be a compile-time constant
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR n: INTEGER;
TYPE Arr = ARRAY n OF INTEGER;
VAR a: Arr;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, ConstantDivisionByZero_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
CONST N = 10 DIV 0;
BEGIN END M.
)"), 0);
}

TEST(SemanticTest, IndexNonArray_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  i[0] := 1
END M.
)"), 0);
}

TEST(SemanticTest, CompareArrayTypes_ReportsError) {
    EXPECT_GT(checkSource(R"(
MODULE M;
TYPE Arr = ARRAY 3 OF INTEGER;
VAR a, b: Arr;
VAR ok: BOOLEAN;
BEGIN
  ok := a = b
END M.
)"), 0);
}
