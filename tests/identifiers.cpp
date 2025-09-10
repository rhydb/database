#include <gtest/gtest.h>

#include "tokens.hpp"

TEST(LexerIdentifiers, ParseIdentifier) {
  Lexer l = "hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerIdentifiers, ParseIdentifierUnderscores) {
  Lexer l = "hello_world";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello_world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerIdentifiers, ParseIdentifierUnderscoreStart) {
  Lexer l = "_hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Unexpected);
  EXPECT_EQ(token.lexeme(), "_");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}


TEST(LexerIdentifiers, ParseMultipleIdentifiers) {
  Lexer l = "hello world";
  Token hello = l.next();
  EXPECT_EQ(hello.kind(), Token::Kind::Identifier);
  EXPECT_EQ(hello.lexeme(), "hello");

  Token world = l.next();
  EXPECT_EQ(world.kind(), Token::Kind::Identifier);
  EXPECT_EQ(world.lexeme(), "world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerIdentifiers, ParseIdentifierNumber) {
  Lexer l = "hello123";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::Identifier);
  EXPECT_EQ(identifier.lexeme(), "hello123");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(LexerIdentifiers, LineCol) {
  Lexer l = "one\ntwo three\nfour";
  Token t = l.next();
  EXPECT_EQ(t.kind(), Token::Kind::Identifier);
  EXPECT_EQ(t.lexeme(), "one");
  EXPECT_EQ(t.line(), 1);
  EXPECT_EQ(t.col(), 0);

  t = l.next();
  EXPECT_EQ(t.kind(), Token::Kind::Identifier);
  EXPECT_EQ(t.lexeme(), "two");
  EXPECT_EQ(t.line(), 2);
  EXPECT_EQ(t.col(), 0);

  t = l.next();
  EXPECT_EQ(t.kind(), Token::Kind::Identifier);
  EXPECT_EQ(t.lexeme(), "three");
  EXPECT_EQ(t.line(), 2);
  EXPECT_EQ(t.col(), 4);

  t = l.next();
  EXPECT_EQ(t.kind(), Token::Kind::Identifier);
  EXPECT_EQ(t.lexeme(), "four");
  EXPECT_EQ(t.line(), 3);
  EXPECT_EQ(t.col(), 0);
}
