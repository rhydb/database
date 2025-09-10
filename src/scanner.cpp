#include "scanner.hpp"
#include <string>

char Scanner::get() noexcept
{
  char c = *m_start++;
  m_col++;
  if (c == '\n')
  {
    m_line++;
    m_col = 0;
  }
  return c;
}

Token Scanner::next() noexcept
{
  m_prevStart = m_start;
  Token t = nextToken();
  return t;
}

void Scanner::goBack()
{
  m_start = m_prevStart;
}

enum class CharacterKind
{
  End,
  Puncutation,
  Alphabetical,
  Numeric,
  Unknown,
};

constexpr static CharacterKind classifyChar(char c)
{
  // store min and max ranges for classifications
  constexpr const std::tuple<CharacterKind, char, char> ranges[] = {
      {CharacterKind::End, '\0', '\0'},
      {CharacterKind::Puncutation, '!', '/'},
      {CharacterKind::Numeric, '0', '9'},
      {CharacterKind::Puncutation, ':', '@'},
      {CharacterKind::Alphabetical, 'A', 'Z'},
      {CharacterKind::Puncutation, '[', '`'},
      {CharacterKind::Alphabetical, 'a', 'z'},
      {CharacterKind::Puncutation, '{', '}'},
  };

  for (std::size_t i = 0; i < (sizeof(ranges) / sizeof(ranges[0])); i++)
  {
    if (c >= std::get<1>(ranges[i]) && c <= std::get<2>(ranges[i]))
    {
      return std::get<0>(ranges[i]);
    }
  }

  return CharacterKind::Unknown;
}

Token Scanner::nextToken() noexcept
{
  while (isWhiteSpace(peek()))
  {
    get();
  }

  int startCol = m_col;

  // more in depth state for reading mutli-character tokens
  enum Where
  {
    Number,
    Decimal,
    Identifier,
    SingleQuoteString,
    DoubleQuoteString,
    Unknown,
  } where = Unknown;

  char c = peek();
  switch (classifyChar(c))
  {
  case CharacterKind::End:
    return Token(Token::Kind::End, m_start, 1, m_line, m_col);
  case CharacterKind::Puncutation:
    switch (c)
    {
    case '/':
      return charToken(Token::Kind::Slash);
    case '*':
      return charToken(Token::Kind::Star);
    case '&':
      return charToken(Token::Kind::BitAnd);
    case '|':
      return charToken(Token::Kind::BitOr);
    case '^':
      return charToken(Token::Kind::BitXor);
    case '+':
      return charToken(Token::Kind::Plus);
    case '-':
      return charToken(Token::Kind::Minus);
    case '=':
      return matchOr(Token::Kind::Equals, '=', Token::Kind::DoubleEquals, startCol);
    case '!':
      return matchOr(Token::Kind::Bang, '=', Token::Kind::BangEquals, startCol);
    case '<':
      return matchOr(Token::Kind::LessThan, '=', Token::Kind::LessThanEqual, startCol);
    case '>':
      return matchOr(Token::Kind::GreaterThan, '=',
                     Token::Kind::GreaterThanEqual, startCol);
    case '(':
      return charToken(Token::Kind::OpenParen);
    case ')':
      return charToken(Token::Kind::CloseParen);
    case ',':
      return charToken(Token::Kind::Comma);
    case '\'':
      // dont include the quote in the token. also starts loop inside string
      ++m_start;
      where = SingleQuoteString;
      break;
    case '"':
      // dont include the quote in the token. also starts loop inside string
      ++m_start;
      where = DoubleQuoteString;
      break;
    case ';':
      return charToken(Token::Kind::Semicolon);
    default:
      return charToken(Token::Kind::Unexpected);
    }
    break;
  case CharacterKind::Alphabetical:
    where = Identifier;
    break;
  case CharacterKind::Numeric:
    where = Number;
    break;
  default:
    return Token(Token::Kind::Unexpected, m_start, 1, m_line, m_col);
  }

  const char *tokenStart = m_start;
  CharacterKind kind;
  bool escapeNext = false;

  while (true)
  {
    c = peek();
    kind = classifyChar(c);

    if (kind == CharacterKind::End)
    {
      break;
    }

    const bool allowWhiteSpace = (where == SingleQuoteString) || (where == DoubleQuoteString);
    if (isWhiteSpace(c) && !allowWhiteSpace)
    {
      break;
    }

    switch (where)
    {
    case Decimal:
      [[fallthrough]];
    case Number:
      // only allow going from a number to a decimal
      switch (kind)
      {
      case CharacterKind::Numeric:
        break;
      case CharacterKind::Puncutation:
        if (c == '.' && where == Number)
        {
          where = Decimal;
        }
        else
        {
          goto done;
        }
        break;
      default:
        // don't allow going from number to identifier - it would be messy with
        // underscores
        m_hadError = true;
        goto done;
      }
      break;
    case Identifier:
      switch (kind)
      {
      case CharacterKind::Alphabetical:
        [[fallthrough]];
      case CharacterKind::Numeric:
        break;
      case CharacterKind::Puncutation:
        if (c == '_')
        {
          // allow underscores in identifiers
          break;
        }
        goto done;
      default:
        goto done;
      }
      break;
    case SingleQuoteString:
      // TODO: remove the backslash
      if (isNonEscaped(c, '\'', escapeNext))
      {
        goto done;
      }
      break;
    case DoubleQuoteString:
      if (isNonEscaped(c, '"', escapeNext))
      {
        goto done;
      }
      break;
    case Unknown:
      m_hadError = true;
      goto done;
      break;
    }

    get();
  }

done:

  // map the state to a token
  // if there was an error above the error character is not consumed
  switch (where)
  {
  case DoubleQuoteString:
    [[fallthrough]];
  case SingleQuoteString:
    return Token(Token::Kind::String, tokenStart,
                 m_start++ /* consume the end quote */, m_line, startCol);
    break;
  case Identifier:
    return identifierOrReserved(tokenStart, m_start, m_line, startCol);
    break;
  case Number:
    [[fallthrough]];
  case Decimal:
  {
    Token token = Token(Token::Kind::Number, tokenStart, m_start, m_line, startCol);
    token.value.number = std::stod(token.lexeme().data());
    return token;
  }
  default:
    return Token(Token::Kind::Unexpected, tokenStart, m_start, m_line, startCol);
    break;
  }
}

struct ReservedIdentifier
{
  const char *str;
  Token::Kind kind;
};
inline constexpr ReservedIdentifier reserved[] = {
#define RESERVED_true(str, kind) {str, Token::Kind::kind},
#define RESERVED_false(str, kind) // empty
#define X(kind, str, is_kw) RESERVED_##is_kw(str, kind)
#include "token_list.hpp"
#undef X
#undef RESERVED_true
#undef RESERVED_false
};

Token Scanner::identifierOrReserved(const char *start,
                                    const char *end, int line, int col) const noexcept
{
  std::string_view lexeme = std::string_view(start, end - start);
  const size_t nReserved = sizeof(reserved) / sizeof(reserved[0]);
  for (size_t i = 0; i < nReserved; i++)
  {

    if (lexeme.compare(reserved[i].str) == 0)
    {
      return Token(reserved[i].kind, start, end, line, col);
    }
  }
  return Token(Token::Kind::Identifier, start, end, line, col);
}

Token Scanner::peekToken() noexcept
{
  int prevLine = m_line;
  int prevCol = m_col;
  const char *prev = m_start;
  Token t = next();
  m_start = prev;
  m_line = prevLine;
  m_col = prevCol;
  return t;
}

bool Scanner::isWhiteSpace(char c) const noexcept { return c > 0 && c <= ' '; }

Token Scanner::charToken(Token::Kind kind) noexcept
{
  return Token(kind, m_start++, 1, m_line, m_col);
}

// consumes the current character and checks the following character against
// match if equal consumes match and returns onMatch otherwise returns fallback
Token Scanner::matchOr(Token::Kind fallback, char match,
                       Token::Kind onMatch, int col) noexcept
{
  const char *tokenStart = m_start++;
  if (peek() == match)
  {
    return Token(onMatch, tokenStart, ++m_start, m_line, col);
  }

  return Token(fallback, tokenStart, 1, m_line, col);
}

bool Scanner::isNonEscaped(char c, char end, bool &escapeNext) const noexcept
{
  if (escapeNext)
  {
    escapeNext = false;
    return false;
  }

  if (c == '\\')
  {
    escapeNext = true;
    return false;
  }

  if (c == end)
  {
    return true;
  }

  return false;
}

Scanner::iterator Scanner::begin() { return iterator(this); }
Scanner::iterator Scanner::end() { return iterator(); }

Scanner::iterator::iterator(Scanner *scanner) : m_scanner(scanner), m_isEnd(false), m_cached()
{
  if (!scanner || scanner->peek() == '\0')
  {
    m_isEnd = true;
    return;
  }

  m_cached = scanner->next();
  if (m_cached.is(Token::Kind::End))
  {
    m_isEnd = true;
  }
}

Scanner::iterator& Scanner::iterator::operator++()
{
    if (m_isEnd || !m_scanner)
    {
      return *this;
    }

    m_cached = m_scanner->next();
    if (m_cached.is(Token::Kind::End))
    {
      m_isEnd = true;
    }

    return *this;
}

Scanner::iterator Scanner::iterator::operator++(int)
{
  iterator tmp = *this;
  ++(*this);
  return tmp;
}
