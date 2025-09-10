#include <gtest/gtest.h>

#include "scanner.hpp"

TEST(ScannerIdentifiers, ParseIdentifier) {
  Scanner l = "hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerIdentifiers, ParseIdentifierUnderscores) {
  Scanner l = "hello_world";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello_world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerIdentifiers, ParseIdentifierUnderscoreStart) {
  Scanner l = "_hello";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Unexpected);
  EXPECT_EQ(token.lexeme(), "_");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "hello");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}


TEST(ScannerIdentifiers, ParseMultipleIdentifiers) {
  Scanner l = "hello world";
  Token hello = l.next();
  EXPECT_EQ(hello.kind(), Token::Kind::Identifier);
  EXPECT_EQ(hello.lexeme(), "hello");

  Token world = l.next();
  EXPECT_EQ(world.kind(), Token::Kind::Identifier);
  EXPECT_EQ(world.lexeme(), "world");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerIdentifiers, ParseIdentifierNumber) {
  Scanner l = "hello123";
  Token identifier = l.next();
  EXPECT_EQ(identifier.kind(), Token::Kind::Identifier);
  EXPECT_EQ(identifier.lexeme(), "hello123");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}

TEST(ScannerIdentifiers, LineCol) {
  Scanner l = "one\ntwo three\nfour";
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

TEST(ScannerIdentifiers, ReservedWords) {
  Scanner l = "true or andy oR";
  Token token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::True);
  EXPECT_EQ(token.lexeme(), "true");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Or);
  EXPECT_EQ(token.lexeme(), "or");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "andy");

  token = l.next();
  EXPECT_EQ(token.kind(), Token::Kind::Identifier);
  EXPECT_EQ(token.lexeme(), "oR");

  EXPECT_EQ(l.next().kind(), Token::Kind::End);
}
