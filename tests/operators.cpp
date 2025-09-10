#include <gtest/gtest.h>

#include "scanner.hpp"

TEST(ScannerOperators, ParsePunctuation) {
  Scanner l = ";()";
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

TEST(ScannerOperators, ParseLessThan) {
  Scanner l = "5<4";
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

TEST(ScannerOperators, ParseLessThanEqual) {
  Scanner l = "5<=4";
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

TEST(ScannerOperators, ParseEquals) {
  Scanner l = "5=4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Equals);
  EXPECT_EQ(token.lexeme(), "=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerOperators, DoubleEquals) {
  Scanner l = "5==4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::DoubleEquals);
  EXPECT_EQ(token.lexeme(), "==");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerOperators, TwoEqualsWithSpace) {
  Scanner l = "5 = = 4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Equals);
  EXPECT_EQ(token.lexeme(), "=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Equals);
  EXPECT_EQ(token.lexeme(), "=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerOperators, ParseBang) {
  Scanner l = "5!4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Bang);
  EXPECT_EQ(token.lexeme(), "!");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerOperators, ParseBangEquals) {
  Scanner l = "5!=4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::BangEquals);
  EXPECT_EQ(token.lexeme(), "!=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerOperators, ParseEqualsBang) {
  Scanner l = "5=!4";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "5");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Equals);
  EXPECT_EQ(token.lexeme(), "=");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Bang);
  EXPECT_EQ(token.lexeme(), "!");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Number);
  EXPECT_EQ(token.lexeme(), "4");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
