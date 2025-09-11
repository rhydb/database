#include "token.hpp"


static const char* kindToString(const Token::Kind &k)
{
  switch (k)
  {
  #define X(kind, str, is_kw) case Token::Kind::kind: return str;
#include "token_list.hpp"
#undef X
  };
  return "Unknown";
}

const char* Token::toString() const
{
  return kindToString(m_kind);
}

std::ostream &operator<<(std::ostream &os, const Token::Kind &k)
{
  return os << kindToString(k);
}

std::ostream &operator<<(std::ostream &os, const Token &t)
{
  // e.g. LeftParen('(')@12
  return os <<  t.toString() << "('" << t.lexeme() << "')@" << t.line() << ":" << t.col(); 
}
