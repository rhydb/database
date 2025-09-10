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

  for (const Token &t : m_scanner)
  {
    if (!t.isOneOf(Token::Kind::BangEquals, Token::Kind::DoubleEquals))
    {
      m_scanner.goBack();
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

  for (const Token &t : m_scanner)
  {
    if (!t.isOneOf(Token::Kind::GreaterThan, Token::Kind::GreaterThanEqual,
                   Token::Kind::LessThan, Token::Kind::LessThanEqual))
    {
      m_scanner.goBack();
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
  for (const Token &t : m_scanner)
  {
    if (!t.isOneOf(Token::Kind::Minus, Token::Kind::Plus))
    {
      m_scanner.goBack();
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
  for (const Token &t : m_scanner)
  {
    if (!t.isOneOf(Token::Kind::Slash, Token::Kind::Star))
    {
      m_scanner.goBack();
      break;
    }

    auto right = unary();
    expr = std::make_unique<Expr::Binary>(std::move(expr), t, std::move(right));
  }

  return expr;
}

std::unique_ptr<Expr::IExpr> Parser::unary()
{
  Token op = m_scanner.peekToken();
  if (op.isOneOf(Token::Kind::Bang, Token::Kind::Minus))
  {
    m_scanner.next();
    auto right = unary();
    return std::make_unique<Expr::Unary>(op, std::move(right));
  }

  return primary();
}

std::unique_ptr<Expr::IExpr> Parser::primary()
{
  Token t = m_scanner.peekToken();
  if (t.isOneOf(Token::Kind::True, Token::Kind::False, Token::Kind::Number,
                Token::Kind::String))
  {
    m_scanner.next();
    return std::make_unique<Expr::Literal>(t);
  }

  if (t.is(Token::Kind::OpenParen))
  {
    m_scanner.next();
    auto expr = expression();
    Token next = m_scanner.next();
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
