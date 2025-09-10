#pragma once

#include <string_view>
#include <iostream>

struct Token
{
  enum class Kind
  {
    #define X(kind, str, is_kw) kind,
#include "token_list.hpp"
    #undef X
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

std::ostream &operator<<(std::ostream &os, const Token &t);
std::ostream &operator<<(std::ostream &os, const Token::Kind &kind);
