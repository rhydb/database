#pragma once

#include <string_view>
#include <iostream>

struct Token
{
  struct Location
  {
    int line, col;

  };

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
  Token(Kind kind, const char *start, std::size_t len, Location location = {}) noexcept
      : m_kind(kind), m_lexeme(start, len), m_location(location)
  {
  }
  Token(Kind kind, const char *start, const char *end, Location location = {}) noexcept
      : m_kind(kind), m_lexeme(start, end - start), m_location(location)
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

  template<class S, typename ...Ks>
  void expectOrThrow(S message, Token::Kind kind, Ks... ks) const
  {
    if (!isOneOf(kind, ks...))
    {
      throw std::runtime_error(message);
    }
  }

  template<class S>
  void expectOrThrow(S message, Token::Kind kind) const
  {
    if (!is(kind))
    {
      throw std::runtime_error(message);
    }
  }

  std::string_view lexeme() const noexcept { return m_lexeme; }

  const Location& location() const noexcept { return m_location; }
  const char* toString() const;

  union
  {
    double number;
  } value;

private:
  Kind m_kind;
  std::string_view m_lexeme;
  Location m_location;
};

std::ostream &operator<<(std::ostream &os, const Token &t);
std::ostream &operator<<(std::ostream &os, const Token::Kind &kind);
std::ostream &operator<<(std::ostream &os, const Token::Location &location);

const char* kindToString(const Token::Kind &k);
