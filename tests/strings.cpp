#include <gtest/gtest.h>

#include "scanner.hpp"

TEST(ScannerStrings, DoubleQuoteString) {
  Scanner l = "\"hello\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, SingleQuoteString) {
  Scanner l = "'hello'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, EscapedDoubleString) {
  // escape the slash to escape the quote
  Scanner l = "\"hel\\\"lo\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hel\\\"lo");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, EscapedSingleString) {
  // escape the slash to escape the quote
  Scanner l = "'hel\\'lo'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "hel\\'lo");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, DoubleInSingleString) {
  Scanner l = "'\"hello\"'";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "\"hello\"");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, SingleInDoubleString) {
  Scanner l = "\"'hello'\"";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::String);
  EXPECT_EQ(identifier.lexeme(), "'hello'");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, MultiWordString) {
  Scanner l = "'hello world'";
  Token s = l.next();
  EXPECT_EQ(s.kind(), Token::Kind::String);
  EXPECT_EQ(s.lexeme(), "hello world");
  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerStrings, OpenString) {
  Scanner l = "'hello";
  Token s = l.next();
  EXPECT_EQ(s.kind(), Token::Kind::String);
  EXPECT_EQ(s.lexeme(), "hello");
  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
