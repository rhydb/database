#include <gtest/gtest.h>

#include "tokens.hpp"

TEST(Lexer, ParseNumber) {
  Lexer l = "123";
  Token num = l.next();
  EXPECT_EQ(num.kind(), Token::Kind::Number);
  EXPECT_EQ(num.lexeme(), "123");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseDecimalNumber) {
  Lexer l = "123.456";
  Token num = l.next();
  EXPECT_EQ(num.kind(), Token::Kind::Number);
  EXPECT_EQ(num.lexeme(), "123.456");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
