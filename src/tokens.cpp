#include "token.hpp"
#include <tuple>

inline constexpr const char *kindToString(Token::Kind kind)
{
  switch (kind)
  {
  #define X(kind, str, is_kw) case Token::Kind::kind: return str;
#include "token_list.hpp"
#undef X
  };
  return "Unknown";
}

std::ostream &operator<<(std::ostream &os, const Token::Kind &kind)
{
  return os << kindToString(kind);
}

std::ostream &operator<<(std::ostream &os, const Token &t)
{
  // e.g. LeftParen('(')@12
  return os <<  kindToString(t.kind()) << "('" << t.lexeme() << "')@" << t.line() << ":" << t.col(); 
}
