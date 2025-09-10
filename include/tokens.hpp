#pragma once

#include <string_view>
#include <iostream>

struct Token
{
  enum class Kind
  {
    Identifier,
    Number,
    Plus,
    Minus,
    Slash,
    Star,
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
    True,
    False,
    End,
    Unexpected,
  };

  Token() noexcept : mKind(Token::Kind::End), mLexeme() {}

  bool operator==(const Token& b) const { return kind() == b.kind() && lexeme() == b.lexeme(); }

  Token(double number, const char *str, std::size_t len) noexcept
      : mKind(Token::Kind::Number), mLexeme(str, len)
  {
    value.number = number;
  }
  Token(Kind kind, const char *start, std::size_t len, int line = 0, int col = 0) noexcept
      : mKind(kind), mLexeme(start, len), mLine(line), mCol(col)
  {
  }
  Token(Kind kind, const char *start, const char *end, int line = 0, int col = 0) noexcept
      : mKind(kind), mLexeme(start, end - start), mLine(line), mCol(col)
  {
  }

  Kind kind() const noexcept { return mKind; }
  void setKind(Kind kind) noexcept { mKind = kind; }

  bool is(Kind kind) const noexcept { return mKind == kind; }
  bool isOneOf(Kind k1, Kind k2) const noexcept
  {
    return mKind == k1 || mKind == k2;
  }

  template <typename... Ts>
  bool isOneOf(Kind k1, Kind k2, Ts... ks) const noexcept
  {
    return is(k1) || isOneOf(k2, ks...);
  }

  std::string_view lexeme() const noexcept { return mLexeme; }
  int line() const noexcept { return mLine; }
  int col() const noexcept { return mCol; }

  union
  {
    double number;
  } value;

private:
  Kind mKind;
  std::string_view mLexeme;
  int mLine, mCol;
};

class Lexer
{
public:
  Lexer(const char *start) noexcept : mStart(start) {}

  Token next() noexcept;
  Token peekToken() noexcept;
  Token expect(Token::Kind kind, const char *message);
  void goBack();
  bool hadError() const noexcept { return mHadError; }

  struct iterator;
  iterator begin();
  iterator end();

private:
  Token nextToken() noexcept;
  char peek() const noexcept { return *mStart; }
  char get() noexcept;
  Token charToken(Token::Kind kind) noexcept;
  Token identifierOrReserved(const char* start, const char* end) const noexcept;
  Token matchOr(Token::Kind fallback, char match, Token::Kind onMatch, int col) noexcept;

  bool isNonEscaped(char c, char end, bool &escapeNext) const noexcept;
  bool isWhiteSpace(char c) const noexcept;
  const char *mStart;
  const char *mPrevStart;

  Token endToken = Token();
  bool mHadError = false;
  int mLine = 1;
  int mCol = 0;
};

struct Lexer::iterator
{
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = Token;
  using pointer = const Token *;
  using reference = const Token &;

  explicit iterator(Lexer *lexer) : mLexer(lexer), isEnd(false), cached() {
    if (!lexer || lexer->peek() == '\0')
    {
      isEnd = true;
      return;
    }

    cached = lexer->next();
    if (cached.is(Token::Kind::End))
    {
      isEnd = true;
    }
  }
  iterator() : mLexer(nullptr), isEnd(true), cached() {}

  reference operator*() const { return cached; }
  pointer operator->() const
  {
    return &cached;
  }
  iterator &operator++()
  {
    std::cout << "incrementing" << std::endl;
    if (isEnd || !mLexer)
    {
      return *this;
    }

    cached = mLexer->next();
    if (cached.is(Token::Kind::End))
    {
      isEnd = true;
    }

    return *this;
  }
  iterator operator++(int)
  {
    iterator tmp = *this;
    ++(*this);
    return tmp;
  }
  friend bool operator==(const iterator& a, const iterator& b)
  {
    if (a.isEnd && b.isEnd)
    {
      return true;
    }

    return a.mLexer == b.mLexer && a.cached == b.cached && a.isEnd == b.isEnd;
  }
  friend bool operator!=(const iterator& a, const iterator& b)
  {
    return !(a == b);
  }

private:
  Lexer *mLexer;
  bool isEnd = false;
  Token cached;
};

std::ostream &operator<<(std::ostream &os, const Token &t);
std::ostream &operator<<(std::ostream &os, const Token::Kind &kind);
