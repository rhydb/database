#include "tokens.hpp"
#include <tuple>

static const char* kindToString(Token::Kind kind)
{
  switch (kind)
  {
    case Token::Kind::Identifier: return "Identifier";
    case Token::Kind::Number: return "Number";
    case Token::Kind::OpenParen: return "OpenParen";
    case Token::Kind::CloseParen: return "CloseParen";
    case Token::Kind::Comma: return "Comma";
    case Token::Kind::Semicolon: return "Semicolon";
    case Token::Kind::String: return "String";
    case Token::Kind::Bang: return "Bang";
    case Token::Kind::Equals: return "Equals";
    case Token::Kind::DoubleEquals: return "DoubleEquals";
    case Token::Kind::BangEquals: return "BangEquals";
    case Token::Kind::LessThan: return "LessThan";
    case Token::Kind::LessThanEqual: return "LessThanEqual";
    case Token::Kind::GreaterThan: return "GreaterThan";
    case Token::Kind::GreaterThanEqual: return "GreaterThanEqual";
    case Token::Kind::End: return "End";
    case Token::Kind::Unexpected: return "Unexpected";
  };
  return "Unknown";
}

std::ostream &operator<<(std::ostream &os, const Token::Kind &kind)
{
  return os << kindToString(kind);
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

Token Lexer::next() noexcept
{
  while (isWhiteSpace(peek()))
  {
    get();
  }

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
    return Token(Token::Kind::End, mStart, 1);
  case CharacterKind::Puncutation:
    switch (c)
    {
    case '=':
      return matchOr(Token::Kind::Equals, '=', Token::Kind::DoubleEquals);
    case '!':
      return matchOr(Token::Kind::Bang, '=', Token::Kind::BangEquals);
    case '<':
      return matchOr(Token::Kind::LessThan, '=', Token::Kind::LessThanEqual);
    case '>':
      return matchOr(Token::Kind::GreaterThan, '=',
                     Token::Kind::GreaterThanEqual);
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
    return Token(Token::Kind::Unexpected, mStart, 1);
  }

  const char *tokenStart = mStart;
  CharacterKind kind;
  bool escapeNext = false;

  while (true)
  {
    c = peek();
    kind = classifyChar(c);

    if (isWhiteSpace(c))
    {
      break;
    }

    switch (where)
    {
    case Decimal: [[fallthrough]];
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
      case CharacterKind::Alphabetical: [[fallthrough]];
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
  Token::Kind tokenKind;
  switch (where)
  {
  case DoubleQuoteString: [[fallthrough]];
  case SingleQuoteString:
    return Token(Token::Kind::String, tokenStart, mStart++); // consume the end quote
    break;
  case Identifier:
    tokenKind = Token::Kind::Identifier;
    break;
  case Number: [[fallthrough]];
  case Decimal:
    tokenKind = Token::Kind::Number;
    break;
  case Unknown:
    tokenKind = Token::Kind::Unexpected;
    break;
  }
  return Token(tokenKind, tokenStart, mStart);
}

bool Lexer::isWhiteSpace(char c) const noexcept { return c > 0 && c <= ' '; }

Token Lexer::charToken(Token::Kind kind) noexcept
{
  return Token(kind, mStart++, 1);
}

// consumes the current character and checks the following character against
// match if equal consumes match and returns onMatch otherwise returns fallback
Token Lexer::matchOr(Token::Kind fallback, char match,
                     Token::Kind onMatch) noexcept
{
  const char *tokenStart = mStart++;
  if (peek() == match)
  {
    return Token(onMatch, tokenStart, ++mStart);
  }

  return Token(fallback, tokenStart, 1);
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
