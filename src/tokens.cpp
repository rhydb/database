#include "tokens.hpp"
#include <tuple>

std::ostream &operator<<(std::ostream &os, const Token::Kind &kind)
{
  static const char *const names[]{
      [static_cast<int>(Token::Kind::Identifier)] = "Identifier",
      [static_cast<int>(Token::Kind::Number)] = "Number",
      [static_cast<int>(Token::Kind::OpenParen)] = "OpenParen",
      [static_cast<int>(Token::Kind::CloseParen)] = "CloseParen",
      [static_cast<int>(Token::Kind::Comma)] = "Comma",
      [static_cast<int>(Token::Kind::Semicolon)] = "Semicolon",
      [static_cast<int>(Token::Kind::SingleQuote)] = "SingleQuote",
      [static_cast<int>(Token::Kind::DoubleQuote)] = "DoubleQuote",
      [static_cast<int>(Token::Kind::Bang)] = "Bang",
      [static_cast<int>(Token::Kind::Equals)] = "Equals",
      [static_cast<int>(Token::Kind::BangEquals)] = "BangEquals",
      [static_cast<int>(Token::Kind::LessThan)] = "LessThan",
      [static_cast<int>(Token::Kind::LessThanEqual)] = "LessThanEqual",
      [static_cast<int>(Token::Kind::GreaterThan)] = "GreaterThan",
      [static_cast<int>(Token::Kind::GreaterThanEqual)] = "GreaterThanEqual",
      [static_cast<int>(Token::Kind::End)] = "End",
      [static_cast<int>(Token::Kind::Unexpected)] = "Unexpected",
  };
  return os << names[static_cast<int>(kind)];
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

  for (int i = 0; i < (sizeof(ranges) / sizeof(ranges[0])); i++)
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
      return charToken(Token::Kind::Equals);
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
      return charToken(Token::Kind::SingleQuote);
    case '"':
      return charToken(Token::Kind::DoubleQuote);
    case ';':
      return charToken(Token::Kind::Semicolon);
    default:
      return charToken(Token::Kind::Unexpected);
    }
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
    case Decimal: /* fallthrough */
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
      case CharacterKind::Alphabetical: /* fallthrough */
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
  case Identifier:
    tokenKind = Token::Kind::Identifier;
    break;
  case Number:
  case Decimal:
    tokenKind = Token::Kind::Number;
    break;
  case Unknown:
    tokenKind = Token::Kind::Unexpected;
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
