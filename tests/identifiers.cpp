#include <gtest/gtest.h>

#include "tokens.hpp"

TEST(Lexer, ParseIdentifier) {
  Lexer l = "hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseIdentifierUnderscores) {
  Lexer l = "hello_world";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello_world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseIdentifierUnderscoreStart) {
  Lexer l = "_hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Unexpected);
  EXPECT_EQ(token.lexeme(), "_");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}


TEST(Lexer, ParseMultipleIdentifiers) {
  Lexer l = "hello world";
  Token hello = l.next();
  EXPECT_EQ(hello.kind(), Token::Kind::Identifier);
  EXPECT_EQ(hello.lexeme(), "hello");

  Token world = l.next();
  EXPECT_EQ(world.kind(), Token::Kind::Identifier);
  EXPECT_EQ(world.lexeme(), "world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(Lexer, ParseIdentifierNumber) {
  Lexer l = "hello123";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::Identifier);
  EXPECT_EQ(identifier.lexeme(), "hello123");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
