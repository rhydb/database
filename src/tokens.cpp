#include "tokens.hpp"
#include <tuple>
#include <string>

static const char *kindToString(Token::Kind kind)
{
  switch (kind)
  {
  case Token::Kind::Identifier:
    return "Identifier";
  case Token::Kind::Number:
    return "Number";
  case Token::Kind::Slash:
    return "Slash";
  case Token::Kind::Star:
    return "Star";
  case Token::Kind::Plus:
    return "Plus";
  case Token::Kind::Minus:
    return "Minus";
  case Token::Kind::OpenParen:
    return "OpenParen";
  case Token::Kind::CloseParen:
    return "CloseParen";
  case Token::Kind::Comma:
    return "Comma";
  case Token::Kind::Semicolon:
    return "Semicolon";
  case Token::Kind::String:
    return "String";
  case Token::Kind::Bang:
    return "Bang";
  case Token::Kind::Equals:
    return "Equals";
  case Token::Kind::DoubleEquals:
    return "DoubleEquals";
  case Token::Kind::BangEquals:
    return "BangEquals";
  case Token::Kind::LessThan:
    return "LessThan";
  case Token::Kind::LessThanEqual:
    return "LessThanEqual";
  case Token::Kind::GreaterThan:
    return "GreaterThan";
  case Token::Kind::GreaterThanEqual:
    return "GreaterThanEqual";
  case Token::Kind::True:
    return "True";
  case Token::Kind::False:
    return "False";
  case Token::Kind::End:
    return "End";
  case Token::Kind::Unexpected:
    return "Unexpected";
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

enum class CharacterKind
{
  End,
  Puncutation,
  Alphabetical,
  Numeric,
  Unknown,
};

static CharacterKind classifyChar(char c)
{
  // store min and max ranges for classifications
  const std::tuple<CharacterKind, char, char> ranges[] = {
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

char Lexer::get() noexcept
{
  char c = *mStart++;
  mCol++;
  if (c == '\n')
  {
    mLine++;
    mCol = 0;
  }
  return c;
}

Token Lexer::next() noexcept
{
  mPrevStart = mStart;
  Token t = nextToken();
  return t;
}

void Lexer::goBack()
{
  mStart = mPrevStart;
}

Token Lexer::nextToken() noexcept
{
  while (isWhiteSpace(peek()))
  {
    get();
  }

  int startCol = mCol;

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
    return Token(Token::Kind::End, mStart, 1, mLine, mCol);
  case CharacterKind::Puncutation:
    switch (c)
    {
    case '/':
      return charToken(Token::Kind::Slash);
    case '*':
      return charToken(Token::Kind::Star);
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
      ++mStart;
      where = SingleQuoteString;
      break;
    case '"':
      // dont include the quote in the token. also starts loop inside string
      ++mStart;
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
    return Token(Token::Kind::Unexpected, mStart, 1, mLine, mCol);
  }

  const char *tokenStart = mStart;
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
        mHadError = true;
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
      mHadError = true;
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
                 mStart++ /* consume the end quote */, mLine, startCol);
    break;
  case Identifier:
    return Token(Token::Kind::Identifier, tokenStart, mStart, mLine, startCol);
    break;
  case Number:
    [[fallthrough]];
  case Decimal:
  {
    Token token = Token(Token::Kind::Number, tokenStart, mStart, mLine, startCol);
    token.value.number = std::stod(token.lexeme().data());
    return token;
  }
  default:
    return Token(Token::Kind::Unexpected, tokenStart, mStart, mLine, startCol);
    break;
  }
}

Token Lexer::identifierOrReserved(const char *start,
                                  const char *end) const noexcept
{
  struct ReservedIdentifier
  {
    const char *str;
    Token::Kind kind;
  };
  static ReservedIdentifier reserved[] = {
      {"true", Token::Kind::True},
      {"false", Token::Kind::False},
  };

  std::string_view lexeme = std::string_view(start, end - start);
  const size_t nReserved = sizeof(reserved) / sizeof(reserved[0]);
  for (size_t i = 0; i < nReserved; i++)
  {

    if (lexeme.compare(reserved[i].str) == 0)
    {
      return Token(reserved[i].kind, start, end, mLine, mCol);
    }
  }
  return Token(Token::Kind::Identifier, start, end, mLine, mCol);
}

Token Lexer::peekToken() noexcept
{
  int prevLine = mLine;
  int prevCol = mCol;
  const char *prev = mStart;
  Token t = next();
  mStart = prev;
  mLine = prevLine;
  mCol = prevCol;
  return t;
}

bool Lexer::isWhiteSpace(char c) const noexcept { return c > 0 && c <= ' '; }

Token Lexer::charToken(Token::Kind kind) noexcept
{
  return Token(kind, mStart++, 1, mLine, mCol);
}

// consumes the current character and checks the following character against
// match if equal consumes match and returns onMatch otherwise returns fallback
Token Lexer::matchOr(Token::Kind fallback, char match,
                     Token::Kind onMatch, int col) noexcept
{
  const char *tokenStart = mStart++;
  if (peek() == match)
  {
    return Token(onMatch, tokenStart, ++mStart, mLine, col);
  }

  return Token(fallback, tokenStart, 1, mLine, col);
}

bool Lexer::isNonEscaped(char c, char end, bool &escapeNext) const noexcept
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

Lexer::iterator Lexer::begin() { return iterator(this); }
Lexer::iterator Lexer::end() { return iterator(); }
