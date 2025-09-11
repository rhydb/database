#pragma once

#include "token.hpp"

class Scanner
{
public:
  Scanner(const char *start) noexcept : m_start(start) {}

  Token next() noexcept;
  Token peekToken() noexcept;
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
  Token identifierOrReserved(const char *start, const char *end, int line, int col) const noexcept;
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

  explicit iterator(Scanner *scanner);
  iterator() : m_scanner(nullptr), m_isEnd(true), m_cached() {}

  reference operator*() const
  {
    return m_cached;
  }
  pointer operator->() const
  {
    return &m_cached;
  }
  iterator &operator++();
  iterator operator++(int);
  friend bool operator==(const iterator &a, const iterator &b)
  {
    if (a.m_isEnd && b.m_isEnd)
    {
      return true;
    }

    return a.m_scanner == b.m_scanner && a.m_cached == b.m_cached && a.m_isEnd == b.m_isEnd;
  }
  friend bool operator!=(const iterator &a, const iterator &b)
  {
    return !(a == b);
  }

private:
  Scanner *m_scanner;
  bool m_isEnd = false;
  Token m_cached;
};
