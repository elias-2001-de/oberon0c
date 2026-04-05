#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "scanner/Scanner.h"
#include "parser/Parser.h"
#include "parser/ast/ModuleNode.h"

struct TestCtx {
    std::ostringstream out_, err_;
    Logger logger_;
    TestCtx() : logger_(LogLevel::ERROR, out_, err_) {}
    [[nodiscard]] int errors() const { return logger_.getErrorCount(); }
};

// Parse src string; returns {module_ptr, error_count}.
static std::pair<std::unique_ptr<ModuleNode>, int> parseSource(const std::string &src) {
    TestCtx ctx;
    Scanner scanner(src, ctx.logger_);
    Parser parser(scanner, ctx.logger_);
    auto mod = parser.parse();
    return {std::move(mod), ctx.errors()};
}

// ---------------------------------------------------------------------------
// Valid programs — zero errors expected
// ---------------------------------------------------------------------------

TEST(ParserTest, EmptyModule) {
    auto [mod, errs] = parseSource("MODULE Empty; BEGIN END Empty.");
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithoutBeginBlock) {
    // BEGIN is optional when there are no statements
    auto [mod, errs] = parseSource("MODULE NoBegin; END NoBegin.");
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithVarDecl) {
    const std::string src = R"(
MODULE VarTest;
VAR x, y: INTEGER;
BEGIN
  x := 1
END VarTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithConstDecl) {
    const std::string src = R"(
MODULE ConstTest;
CONST N = 10;
BEGIN END ConstTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithTypeAlias) {
    const std::string src = R"(
MODULE TypeTest;
TYPE MyInt = INTEGER;
VAR x: MyInt;
BEGIN END TypeTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithArrayType) {
    const std::string src = R"(
MODULE ArrayTest;
TYPE Vec = ARRAY 10 OF INTEGER;
VAR v: Vec;
BEGIN END ArrayTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithRecordType) {
    const std::string src = R"(
MODULE RecordTest;
TYPE Point = RECORD x, y: INTEGER END;
VAR p: Point;
BEGIN END RecordTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithProcedureNoParams) {
    const std::string src = R"(
MODULE ProcTest;
VAR x: INTEGER;
PROCEDURE Reset;
BEGIN x := 0 END Reset;
BEGIN END ProcTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithProcedureValueParam) {
    const std::string src = R"(
MODULE ProcTest;
PROCEDURE Double(x: INTEGER);
BEGIN END Double;
BEGIN END ProcTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ModuleWithProcedureVarParam) {
    const std::string src = R"(
MODULE ProcTest;
PROCEDURE Inc(VAR x: INTEGER);
BEGIN x := x + 1 END Inc;
BEGIN END ProcTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, IfStatement) {
    const std::string src = R"(
MODULE IfTest;
VAR x: INTEGER;
BEGIN
  IF x = 0 THEN x := 1 END
END IfTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, IfElseStatement) {
    const std::string src = R"(
MODULE IfTest;
VAR x: INTEGER;
BEGIN
  IF x > 0 THEN x := x - 1
  ELSE x := 0
  END
END IfTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, IfElsifElseStatement) {
    const std::string src = R"(
MODULE IfTest;
VAR x: INTEGER;
BEGIN
  IF x = 0 THEN x := 1
  ELSIF x = 1 THEN x := 2
  ELSE x := 3
  END
END IfTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, WhileLoop) {
    const std::string src = R"(
MODULE WhileTest;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE i < 10 DO i := i + 1 END
END WhileTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, RepeatLoop) {
    const std::string src = R"(
MODULE RepeatTest;
VAR i: INTEGER;
BEGIN
  i := 0;
  REPEAT i := i + 1 UNTIL i >= 10
END RepeatTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ProcedureCall) {
    const std::string src = R"(
MODULE CallTest;
VAR n: INTEGER;
PROCEDURE F(VAR x: INTEGER);
BEGIN x := 0 END F;
BEGIN F(n) END CallTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ArrayAccess) {
    const std::string src = R"(
MODULE ArrTest;
TYPE Arr = ARRAY 5 OF INTEGER;
VAR a: Arr; i: INTEGER;
BEGIN
  i := 2;
  a[i] := 7
END ArrTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, RecordFieldAccess) {
    const std::string src = R"(
MODULE RecTest;
TYPE Pt = RECORD x, y: INTEGER END;
VAR p: Pt;
BEGIN
  p.x := 1;
  p.y := 2
END RecTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, ArithmeticExpression) {
    const std::string src = R"(
MODULE ExprTest;
VAR x: INTEGER;
BEGIN
  x := 2 + 3 * 4 - 1
END ExprTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, UnaryPlus) {
    const std::string src = R"(
MODULE PlusTest;
VAR x: INTEGER;
BEGIN
  x := +3
END PlusTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, UnaryNegation) {
    const std::string src = R"(
MODULE NegTest;
VAR x: INTEGER;
BEGIN
  x := -5
END NegTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, FactorialProgram) {
    const std::string src = R"(
MODULE Factorial;
VAR n, fact, i: INTEGER;
BEGIN
  n := 5; fact := 1; i := 1;
  WHILE i <= n DO
    fact := fact * i;
    i := i + 1
  END
END Factorial.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, EmptyActualParameters) {
    // F() is a valid call for a no-parameter procedure
    const std::string src = R"(
MODULE CallTest;
PROCEDURE F;
BEGIN END F;
BEGIN F() END CallTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, MultipleElsif) {
    const std::string src = R"(
MODULE ElsifTest;
VAR x: INTEGER;
BEGIN
  IF x = 1 THEN x := 10
  ELSIF x = 2 THEN x := 20
  ELSIF x = 3 THEN x := 30
  ELSE x := 0
  END
END ElsifTest.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

TEST(ParserTest, NestedProcedures) {
    const std::string src = R"(
MODULE Nested;
PROCEDURE Outer(VAR x: INTEGER);
PROCEDURE Inner(VAR y: INTEGER);
BEGIN y := y + 1 END Inner;
BEGIN Inner(x) END Outer;
BEGIN END Nested.
)";
    auto [mod, errs] = parseSource(src);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(errs, 0);
}

// ---------------------------------------------------------------------------
// Error cases — at least one error expected
// ---------------------------------------------------------------------------

TEST(ParserTest, MissingEnd_ReportsError) {
    // Truncated at BEGIN, no END or module name
    auto [mod, errs] = parseSource("MODULE M; BEGIN");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingPeriod_ReportsError) {
    auto [mod, errs] = parseSource("MODULE M; BEGIN END M");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingSemicolonAfterModuleIdent_ReportsError) {
    auto [mod, errs] = parseSource("MODULE M BEGIN END M.");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, ExtraTokensAfterPeriod_ReportsError) {
    auto [mod, errs] = parseSource("MODULE M; BEGIN END M. EXTRA");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingThenInIf_ReportsError) {
    auto [mod, errs] = parseSource(R"(
MODULE M;
VAR x: INTEGER;
BEGIN
  IF x = 0 x := 1 END
END M.
)");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingDoInWhile_ReportsError) {
    auto [mod, errs] = parseSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  WHILE i < 10 i := i + 1 END
END M.
)");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingUntilInRepeat_ReportsError) {
    auto [mod, errs] = parseSource(R"(
MODULE M;
VAR i: INTEGER;
BEGIN
  REPEAT i := i + 1 i >= 10
END M.
)");
    EXPECT_GT(errs, 0);
}

TEST(ParserTest, MissingSemicolonBetweenProcedures_ReportsError) {
    auto [mod, errs] = parseSource(R"(
MODULE M;
PROCEDURE F; BEGIN END F
PROCEDURE G; BEGIN END G;
BEGIN END M.
)");
    EXPECT_GT(errs, 0);
}
