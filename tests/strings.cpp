#include <gtest/gtest.h>

#include "tokens.hpp"

TEST(LexerStrings, DoubleQuoteString) {
  Lexer l = "\"hello\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerStrings, SingleQuoteString) {
  Lexer l = "'hello'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerStrings, EscapedDoubleString) {
  // escape the slash to escape the quote
  Lexer l = "\"hel\\\"lo\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hel\\\"lo");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerStrings, EscapedSingleString) {
  // escape the slash to escape the quote
  Lexer l = "'hel\\'lo'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hel\\'lo");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerStrings, DoubleInSingleString) {
  Lexer l = "'\"hello\"'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "\"hello\"");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerStrings, SingleInDoubleString) {
  Lexer l = "\"'hello'\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "'hello'");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
