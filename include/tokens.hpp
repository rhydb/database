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

  Token() noexcept : m_kind(Token::Kind::End), m_lexeme() {}

  bool operator==(const Token& b) const { return kind() == b.kind() && lexeme() == b.lexeme(); }

  Token(double number, const char *str, std::size_t len) noexcept
      : m_kind(Token::Kind::Number), m_lexeme(str, len)
  {
    value.number = number;
  }
  Token(Kind kind, const char *start, std::size_t len, int line = 0, int col = 0) noexcept
      : m_kind(kind), m_lexeme(start, len), m_line(line), m_col(col)
  {
  }
  Token(Kind kind, const char *start, const char *end, int line = 0, int col = 0) noexcept
      : m_kind(kind), m_lexeme(start, end - start), m_line(line), m_col(col)
  {
  }

  Kind kind() const noexcept { return m_kind; }
  void setKind(Kind kind) noexcept { m_kind = kind; }

  bool is(Kind kind) const noexcept { return m_kind == kind; }
  bool isOneOf(Kind k1, Kind k2) const noexcept
  {
    return m_kind == k1 || m_kind == k2;
  }

  template <typename... Ts>
  bool isOneOf(Kind k1, Kind k2, Ts... ks) const noexcept
  {
    return is(k1) || isOneOf(k2, ks...);
  }

  std::string_view lexeme() const noexcept { return m_lexeme; }
  int line() const noexcept { return m_line; }
  int col() const noexcept { return m_col; }

  union
  {
    double number;
  } value;

private:
  Kind m_kind;
  std::string_view m_lexeme;
  int m_line, m_col;
};

class Scanner
{
public:
  Scanner(const char *start) noexcept : m_start(start) {}

  Token next() noexcept;
  Token peekToken() noexcept;
  Token expect(Token::Kind kind, const char *message);
  void goBack();
  bool hadError() const noexcept { return m_hadError; }

  struct iterator;
  iterator begin();
  iterator end();

private:
  Token nextToken() noexcept;
  char peek() const noexcept { return *m_start; }
  char get() noexcept;
  Token charToken(Token::Kind kind) noexcept;
  Token identifierOrReserved(const char* start, const char* end) const noexcept;
  Token matchOr(Token::Kind fallback, char match, Token::Kind onMatch, int col) noexcept;

  bool isNonEscaped(char c, char end, bool &escapeNext) const noexcept;
  bool isWhiteSpace(char c) const noexcept;
  const char *m_start;
  const char *m_prevStart;

  Token endToken = Token();
  bool m_hadError = false;
  int m_line = 1;
  int m_col = 0;
};

struct Scanner::iterator
{
  using iterator_category = std::input_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = Token;
  using pointer = const Token *;
  using reference = const Token &;

  explicit iterator(Scanner *scanner) : m_scanner(scanner), isEnd(false), cached() {
    if (!scanner || scanner->peek() == '\0')
    {
      isEnd = true;
      return;
    }

    cached = scanner->next();
    if (cached.is(Token::Kind::End))
    {
      isEnd = true;
    }
  }
  iterator() : m_scanner(nullptr), isEnd(true), cached() {}

  reference operator*() const { return cached; }
  pointer operator->() const
  {
    return &cached;
  }
  iterator &operator++()
  {
    std::cout << "incrementing" << std::endl;
    if (isEnd || !m_scanner)
    {
      return *this;
    }

    cached = m_scanner->next();
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

    return a.m_scanner == b.m_scanner && a.cached == b.cached && a.isEnd == b.isEnd;
  }
  friend bool operator!=(const iterator& a, const iterator& b)
  {
    return !(a == b);
  }

private:
  Scanner *m_scanner;
  bool isEnd = false;
  Token cached;
};

std::ostream &operator<<(std::ostream &os, const Token &t);
std::ostream &operator<<(std::ostream &os, const Token::Kind &kind);
