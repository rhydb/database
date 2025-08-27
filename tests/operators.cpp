#include <gtest/gtest.h>

#include "tokens.hpp"

TEST(Lexer, ParsePunctuation) {
  Lexer l = ";()";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Semicolon);
  EXPECT_EQ(token.lexeme(), ";");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::OpenParen);
  EXPECT_EQ(token.lexeme(), "(");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::CloseParen);
  EXPECT_EQ(token.lexeme(), ")");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseLessThan) {
  Lexer l = "5<4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::LessThan);
  EXPECT_EQ(token.lexeme(), "<");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseLessThanEqual) {
  Lexer l = "5<=4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::LessThanEqual);
  EXPECT_EQ(token.lexeme(), "<=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
