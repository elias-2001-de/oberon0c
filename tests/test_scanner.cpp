#include <gtest/gtest.h>
#include <sstream>
#include <vector>

#include "scanner/Scanner.h"
#include "scanner/IdentToken.h"
#include "scanner/LiteralToken.h"

// Redirect logger output to string streams so tests stay quiet
struct TestCtx {
    std::ostringstream out_, err_;
    Logger logger_;
    TestCtx() : logger_(LogLevel::ERROR, out_, err_) {}
    [[nodiscard]] int errors() const { return logger_.getErrorCount(); }
};

// Scan all tokens from src and return their types (including final eof)
static std::vector<TokenType> scanTypes(const std::string &src) {
    TestCtx ctx;
    Scanner scanner(src, ctx.logger_);
    std::vector<TokenType> types;
    while (true) {
        auto tok = scanner.next();
        auto t = tok->type();
        types.push_back(t);
        if (t == TokenType::eof) break;
    }
    return types;
}

// ---------------------------------------------------------------------------
// EOF / empty input
// ---------------------------------------------------------------------------

TEST(ScannerTest, EmptyInputYieldsOnlyEof) {
    auto types = scanTypes("");
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], TokenType::eof);
}

TEST(ScannerTest, WhitespaceOnlyYieldsOnlyEof) {
    auto types = scanTypes("   \t\n  ");
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], TokenType::eof);
}

// ---------------------------------------------------------------------------
// Keywords
// ---------------------------------------------------------------------------

TEST(ScannerTest, Keywords) {
    EXPECT_EQ(scanTypes("MODULE")[0],    TokenType::kw_module);
    EXPECT_EQ(scanTypes("BEGIN")[0],     TokenType::kw_begin);
    EXPECT_EQ(scanTypes("END")[0],       TokenType::kw_end);
    EXPECT_EQ(scanTypes("PROCEDURE")[0], TokenType::kw_procedure);
    EXPECT_EQ(scanTypes("IF")[0],        TokenType::kw_if);
    EXPECT_EQ(scanTypes("THEN")[0],      TokenType::kw_then);
    EXPECT_EQ(scanTypes("ELSE")[0],      TokenType::kw_else);
    EXPECT_EQ(scanTypes("ELSIF")[0],     TokenType::kw_elsif);
    EXPECT_EQ(scanTypes("WHILE")[0],     TokenType::kw_while);
    EXPECT_EQ(scanTypes("DO")[0],        TokenType::kw_do);
    EXPECT_EQ(scanTypes("REPEAT")[0],    TokenType::kw_repeat);
    EXPECT_EQ(scanTypes("UNTIL")[0],     TokenType::kw_until);
    EXPECT_EQ(scanTypes("VAR")[0],       TokenType::kw_var);
    EXPECT_EQ(scanTypes("CONST")[0],     TokenType::kw_const);
    EXPECT_EQ(scanTypes("TYPE")[0],      TokenType::kw_type);
    EXPECT_EQ(scanTypes("ARRAY")[0],     TokenType::kw_array);
    EXPECT_EQ(scanTypes("RECORD")[0],    TokenType::kw_record);
    EXPECT_EQ(scanTypes("OF")[0],        TokenType::kw_of);
    EXPECT_EQ(scanTypes("DIV")[0],       TokenType::op_div);
    EXPECT_EQ(scanTypes("MOD")[0],       TokenType::op_mod);
    EXPECT_EQ(scanTypes("OR")[0],        TokenType::op_or);
}

TEST(ScannerTest, RemainingKeywords) {
    EXPECT_EQ(scanTypes("IN")[0],      TokenType::op_in);
    EXPECT_EQ(scanTypes("IS")[0],      TokenType::op_is);
    EXPECT_EQ(scanTypes("IMPORT")[0],  TokenType::kw_import);
    EXPECT_EQ(scanTypes("EXTERN")[0],  TokenType::kw_extern);
    EXPECT_EQ(scanTypes("RETURN")[0],  TokenType::kw_return);
    EXPECT_EQ(scanTypes("LOOP")[0],    TokenType::kw_loop);
    EXPECT_EQ(scanTypes("EXIT")[0],    TokenType::kw_exit);
    EXPECT_EQ(scanTypes("FOR")[0],     TokenType::kw_for);
    EXPECT_EQ(scanTypes("TO")[0],      TokenType::kw_to);
    EXPECT_EQ(scanTypes("BY")[0],      TokenType::kw_by);
    EXPECT_EQ(scanTypes("CASE")[0],    TokenType::kw_case);
    EXPECT_EQ(scanTypes("POINTER")[0], TokenType::kw_pointer);
    EXPECT_EQ(scanTypes("NIL")[0],     TokenType::kw_nil);
}

TEST(ScannerTest, LowercaseKeywordIsIdentifier) {
    // Oberon-0 keywords are uppercase; lowercase is a regular identifier
    TestCtx ctx;
    Scanner scanner("module", ctx.logger_);
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::const_ident);
}

// ---------------------------------------------------------------------------
// Identifiers
// ---------------------------------------------------------------------------

TEST(ScannerTest, SimpleIdentifier) {
    TestCtx ctx;
    Scanner scanner("foo", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::const_ident);
    auto *ident = dynamic_cast<const IdentToken *>(tok.get());
    ASSERT_NE(ident, nullptr);
    EXPECT_EQ(ident->value(), "foo");
}

TEST(ScannerTest, IdentifierWithLeadingUnderscore) {
    TestCtx ctx;
    Scanner scanner("_foo", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::const_ident);
    auto *ident = dynamic_cast<const IdentToken *>(tok.get());
    ASSERT_NE(ident, nullptr);
    EXPECT_EQ(ident->value(), "_foo");
}

TEST(ScannerTest, IdentifierWithDigits) {
    TestCtx ctx;
    Scanner scanner("x1y2", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::const_ident);
    auto *ident = dynamic_cast<const IdentToken *>(tok.get());
    ASSERT_NE(ident, nullptr);
    EXPECT_EQ(ident->value(), "x1y2");
}

// ---------------------------------------------------------------------------
// Integer literals
// ---------------------------------------------------------------------------

TEST(ScannerTest, IntegerLiteral) {
    // 40000 > INT16_MAX (32767) and <= INT32_MAX → int_literal
    TestCtx ctx;
    Scanner scanner("40000", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::int_literal);
    auto *lit = dynamic_cast<const IntLiteralToken *>(tok.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value(), 40000);
}

TEST(ScannerTest, ShortLiteral) {
    // Values ≤ 65535 (0xFFFF) come back as short_literal
    TestCtx ctx;
    Scanner scanner("100", ctx.logger_);
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::short_literal);
    EXPECT_EQ(dynamic_cast<const ShortLiteralToken *>(tok.get())->value(), 100);
}

TEST(ScannerTest, LongLiteral) {
    // Values > INT32_MAX come back as long_literal
    TestCtx ctx;
    Scanner scanner("3000000000", ctx.logger_);
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::long_literal);
    EXPECT_EQ(dynamic_cast<const LongLiteralToken *>(tok.get())->value(), 3000000000LL);
}

TEST(ScannerTest, HexIntegerLiteral) {
    TestCtx ctx;
    Scanner scanner("0FFH", ctx.logger_);
    auto tok = scanner.next();
    // 0xFF = 255, fits in short
    EXPECT_EQ(tok->type(), TokenType::short_literal);
    EXPECT_EQ(dynamic_cast<const ShortLiteralToken *>(tok.get())->value(), 0xFF);
}

TEST(ScannerTest, CharLiteralFromXSuffix) {
    TestCtx ctx;
    Scanner scanner("41X", ctx.logger_);   // 0x41 = 'A'
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::char_literal);
    EXPECT_EQ(dynamic_cast<const CharLiteralToken *>(tok.get())->value(), 0x41u);
}

TEST(ScannerTest, ZeroLiteral) {
    // 0 <= INT16_MAX → short_literal
    TestCtx ctx;
    Scanner scanner("0", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::short_literal);
    auto *lit = dynamic_cast<const ShortLiteralToken *>(tok.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value(), 0);
}

// ---------------------------------------------------------------------------
// String literals
// ---------------------------------------------------------------------------

TEST(ScannerTest, StringLiteral) {
    TestCtx ctx;
    Scanner scanner("\"hello\"", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::string_literal);
    auto *lit = dynamic_cast<const StringLiteralToken *>(tok.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value(), "hello");
}

TEST(ScannerTest, SingleCharStringBecomesCharLiteral) {
    // A one-character string "x" is emitted as char_literal, not string_literal
    TestCtx ctx;
    Scanner scanner("\"A\"", ctx.logger_);
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::char_literal);
    EXPECT_EQ(dynamic_cast<const CharLiteralToken *>(tok.get())->value(), static_cast<uint8_t>('A'));
}

TEST(ScannerTest, EmptyStringLiteral) {
    // "" has length 0 <= 1, so scanner emits char_literal with value '\0'
    TestCtx ctx;
    Scanner scanner("\"\"", ctx.logger_);
    auto tok = scanner.next();
    ASSERT_EQ(tok->type(), TokenType::char_literal);
    auto *lit = dynamic_cast<const CharLiteralToken *>(tok.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value(), static_cast<uint8_t>('\0'));
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

TEST(ScannerTest, ArithmeticOperators) {
    EXPECT_EQ(scanTypes("+")[0], TokenType::op_plus);
    EXPECT_EQ(scanTypes("-")[0], TokenType::op_minus);
    EXPECT_EQ(scanTypes("*")[0], TokenType::op_times);
    EXPECT_EQ(scanTypes("/")[0], TokenType::op_divide);
}

TEST(ScannerTest, ComparisonOperators) {
    EXPECT_EQ(scanTypes("=")[0],  TokenType::op_eq);
    EXPECT_EQ(scanTypes("#")[0],  TokenType::op_neq);
    EXPECT_EQ(scanTypes("<")[0],  TokenType::op_lt);
    EXPECT_EQ(scanTypes(">")[0],  TokenType::op_gt);
    EXPECT_EQ(scanTypes("<=")[0], TokenType::op_leq);
    EXPECT_EQ(scanTypes(">=")[0], TokenType::op_geq);
}

TEST(ScannerTest, AssignmentOperator) {
    EXPECT_EQ(scanTypes(":=")[0], TokenType::op_becomes);
}

TEST(ScannerTest, BooleanOperators) {
    EXPECT_EQ(scanTypes("&")[0], TokenType::op_and);
    EXPECT_EQ(scanTypes("~")[0], TokenType::op_not);
}

// ---------------------------------------------------------------------------
// Punctuation
// ---------------------------------------------------------------------------

TEST(ScannerTest, Punctuation) {
    EXPECT_EQ(scanTypes(".")[0],   TokenType::period);
    EXPECT_EQ(scanTypes(",")[0],   TokenType::comma);
    EXPECT_EQ(scanTypes(":")[0],   TokenType::colon);
    EXPECT_EQ(scanTypes(";")[0],   TokenType::semicolon);
    EXPECT_EQ(scanTypes("(")[0],   TokenType::lparen);
    EXPECT_EQ(scanTypes(")")[0],   TokenType::rparen);
    EXPECT_EQ(scanTypes("[")[0],   TokenType::lbrack);
    EXPECT_EQ(scanTypes("]")[0],   TokenType::rbrack);
    EXPECT_EQ(scanTypes("{")[0],   TokenType::lbrace);
    EXPECT_EQ(scanTypes("}")[0],   TokenType::rbrace);
    EXPECT_EQ(scanTypes("^")[0],   TokenType::caret);
    EXPECT_EQ(scanTypes("|")[0],   TokenType::pipe);
    EXPECT_EQ(scanTypes("..")[0],  TokenType::range);
    EXPECT_EQ(scanTypes("...")[0], TokenType::varargs);
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

TEST(ScannerTest, CommentIsSkipped) {
    auto types = scanTypes("(* this is a comment *)");
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], TokenType::eof);
}

TEST(ScannerTest, CommentBetweenTokens) {
    auto types = scanTypes("a (* comment *) b");
    ASSERT_EQ(types.size(), 3u);
    EXPECT_EQ(types[0], TokenType::const_ident);
    EXPECT_EQ(types[1], TokenType::const_ident);
    EXPECT_EQ(types[2], TokenType::eof);
}

TEST(ScannerTest, NestedComment) {
    // Oberon-0 supports nested comments (* (* inner *) outer *)
    auto types = scanTypes("(* outer (* inner *) still outer *)");
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], TokenType::eof);
}

// ---------------------------------------------------------------------------
// Multi-token sequences
// ---------------------------------------------------------------------------

TEST(ScannerTest, AssignmentStatement) {
    auto types = scanTypes("x := 5");
    ASSERT_EQ(types.size(), 4u);
    EXPECT_EQ(types[0], TokenType::const_ident);
    EXPECT_EQ(types[1], TokenType::op_becomes);
    EXPECT_EQ(types[2], TokenType::short_literal);
    EXPECT_EQ(types[3], TokenType::eof);
}

TEST(ScannerTest, SimpleModuleTokens) {
    auto types = scanTypes("MODULE M; BEGIN END M.");
    std::vector<TokenType> expected = {
        TokenType::kw_module,
        TokenType::const_ident,   // M
        TokenType::semicolon,
        TokenType::kw_begin,
        TokenType::kw_end,
        TokenType::const_ident,   // M
        TokenType::period,
        TokenType::eof
    };
    EXPECT_EQ(types, expected);
}

// ---------------------------------------------------------------------------
// Boolean literal keywords
// ---------------------------------------------------------------------------

TEST(ScannerTest, TrueAndFalseAreBooleanLiterals) {
    EXPECT_EQ(scanTypes("TRUE")[0],  TokenType::boolean_literal);
    EXPECT_EQ(scanTypes("FALSE")[0], TokenType::boolean_literal);
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST(ScannerTest, UnterminatedComment_ReportsError) {
    TestCtx ctx;
    Scanner scanner("(* never closed", ctx.logger_);
    // Consume tokens until eof
    while (scanner.next()->type() != TokenType::eof) {}
    EXPECT_GT(ctx.errors(), 0);
}

TEST(ScannerTest, BadCharacter_ReportsError) {
    TestCtx ctx;
    Scanner scanner("@", ctx.logger_);
    // Bad char is skipped; scanner tries again and returns eof
    auto tok = scanner.next();
    EXPECT_EQ(tok->type(), TokenType::eof);
    EXPECT_GT(ctx.errors(), 0);
}

// ---------------------------------------------------------------------------
// escape / unescape round-trip
// ---------------------------------------------------------------------------

TEST(ScannerTest, EscapeUnescapeRoundTrip) {
    const std::string original = "hello\nworld\t!";
    EXPECT_EQ(Scanner::unescape(Scanner::escape(original)), original);
}

TEST(ScannerTest, EscapeEmptyString) {
    EXPECT_EQ(Scanner::unescape(Scanner::escape("")), "");
}
