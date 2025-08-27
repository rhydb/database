#pragma once

#include <string_view>
#include <iostream>

struct Token {
  enum class Kind {
    Identifier,
    Number,
    OpenParen,
    CloseParen,
    Comma,
    Semicolon,
    Bang,
    Equals,
    DoubleEquals,
    BangEquals,
    LessThan,
    LessThanEqual,
    GreaterThan,
    GreaterThanEqual,
    String,
    End,
    Unexpected,
  };

  Token() noexcept : mKind(Token::Kind::End), mLexeme() {}
  Token(Kind kind, const char *start, std::size_t len) noexcept : mKind(kind), mLexeme(start, len) {}
  Token(Kind kind, const char *start, const char *end) noexcept : mKind(kind), mLexeme(start, end - start) {}

  Kind kind() const noexcept { return mKind; }
  void setKind(Kind kind) noexcept { mKind = kind; }

  bool is(Kind kind) const noexcept { return mKind == kind; }
  bool isOneOf(Kind k1, Kind k2) const noexcept { return mKind == k1 || mKind == k2; }

  template <typename... Ts>
  bool isOneOf(Kind k1, Kind k2, Ts... ks) const noexcept {
    return is(k1) || isOneOf(k2, ks...);
  }

  std::string_view lexeme() const noexcept { return mLexeme; }

private:
  Kind mKind;
  std::string_view mLexeme;
};


class Lexer {
public:
  Lexer(const char *start) noexcept : mStart(start) {}
  
  Token next() noexcept;
  bool hadError() const noexcept { return mHadError; }

private:
  char peek() const noexcept { return *mStart; }
  char get() noexcept { return *mStart++; }
  Token charToken(Token::Kind kind) noexcept;

  Token matchOr(Token::Kind fallback, char match, Token::Kind onMatch) noexcept;

  bool isNonEscaped(char c, char end, bool& escapeNext) const noexcept;
  bool isWhiteSpace(char c) const noexcept;
  const char *mStart; 

  bool mHadError = false;
};


std::ostream& operator<<(std::ostream& os, const Token::Kind& kind);
