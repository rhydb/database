#include "parser.hpp"

void Parser::error(const Token &t, const char *msg)
{
  std::cerr << t.line() << ":" << t.col() << " " << msg << std::endl;
  throw std::exception();
}

std::unique_ptr<Expr::IExpr> Parser::parse()
{
  try
  {
    return expression();
  }
  catch (std::exception &e)
  {
    return nullptr;
  }
}

std::unique_ptr<Expr::IExpr> Parser::expression()
{
  return equality();
}

std::unique_ptr<Expr::IExpr> Parser::equality()
{
  auto expr = comparison();

  for (const Token &t : mLexer)
  {
    if (!t.isOneOf(Token::Kind::BangEquals, Token::Kind::DoubleEquals))
    {
      mLexer.goBack();
      break;
    }

    auto right = comparison();
    expr = std::make_unique<Expr::Binary>(std::move(expr), t, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr::IExpr> Parser::comparison()
{
  auto expr = term();

  for (const Token &t : mLexer)
  {
    if (!t.isOneOf(Token::Kind::GreaterThan, Token::Kind::GreaterThanEqual,
                   Token::Kind::LessThan, Token::Kind::LessThanEqual))
    {
      mLexer.goBack();
      break;
    }

    auto right = term();
    expr = std::make_unique<Expr::Binary>(std::move(expr), t, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr::IExpr> Parser::term()
{
  auto expr = factor();
  for (const Token &t : mLexer)
  {
    if (!t.isOneOf(Token::Kind::Minus, Token::Kind::Plus))
    {
      mLexer.goBack();
      break;
    }

    auto right = factor();
    expr = std::make_unique<Expr::Binary>(std::move(expr), t, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr::IExpr> Parser::factor()
{
  auto expr = unary();
  for (const Token &t : mLexer)
  {
    if (!t.isOneOf(Token::Kind::Slash, Token::Kind::Star))
    {
      mLexer.goBack();
      break;
    }

    auto right = unary();
    expr = std::make_unique<Expr::Binary>(std::move(expr), t, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr::IExpr> Parser::unary()
{
  Token op = mLexer.peekToken();
  if (op.isOneOf(Token::Kind::Bang, Token::Kind::Minus))
  {
    mLexer.next();
    auto right = unary();
    return std::make_unique<Expr::Unary>(op, std::move(right));
  }

  return primary();
}

std::unique_ptr<Expr::IExpr> Parser::primary()
{
  Token t = mLexer.peekToken();
  if (t.isOneOf(Token::Kind::True, Token::Kind::False, Token::Kind::Number,
                Token::Kind::String))
  {
    mLexer.next();
    return std::make_unique<Expr::Literal>(t);
  }

  if (t.is(Token::Kind::OpenParen))
  {
    mLexer.next();
    auto expr = expression();
    Token next = mLexer.next();
    if (next.kind() != Token::Kind::CloseParen)
    {
      error(next, "Expected ')' after expression");
      return nullptr;
    }

    return std::make_unique<Expr::Grouping>(std::move(expr));
  }

  error(t, "Expected expression");
  return nullptr;
  ;
}
