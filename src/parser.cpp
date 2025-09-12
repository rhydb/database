#include <optional>

#include "parser.hpp"
#include "ast.hpp"

void Parser::error(const Token &t, const char *msg)
{
  std::cerr << t.location() << " " << msg << std::endl;
  throw std::runtime_error(msg);
}

std::unique_ptr<Expr::IExpr> Parser::parse()
{
  try
  {
    auto expr = expression();
    TypeChecker tc;
    if (tc.check(expr))
    {
      std::cerr << "Type check failed" << std::endl;
      return nullptr;
    }
    return expr;
  }
  catch (std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    return nullptr;
  }
}

std::unique_ptr<Expr::IExpr> Parser::statement()
{
  return ddl();
}

std::unique_ptr<Expr::IExpr> Parser::ddl()
{
  Token ddl_tok = m_scanner.next();
  ddl_tok.expectOrThrow(std::string("Expected a DDL token, instead saw ") + ddl_tok.toString(), Token::Kind::Create);

  if (ddl_tok.is(Token::Kind::Create))
  {
    Token t = m_scanner.next();
    t.expectOrThrow(std::string("Expected 'table' after 'create', instead saw ") + t.toString(), Token::Kind::Table);
    Token table = m_scanner.next();
    table.expectOrThrow(std::string("Expected identifier for table name, instead saw ") + table.toString(), Token::Kind::Identifier);

    std::vector<Expr::ColumnDef> columns = column_def_list();

    return std::make_unique<Expr::Create>(table.lexeme(), std::move(columns));
  }

  error(ddl_tok, "Unknown DLL token");
  return nullptr;
}

std::optional<Expr::ColumnDef::Type> resolveColumnType(const std::string_view &type_str)
{
  // we can check these now wihtout having to travel the tree
  static const std::pair<const char*, Expr::ColumnDef::Type> types[] = {
    {"integer", Expr::ColumnDef::Type::Integer},
    {"string", Expr::ColumnDef::Type::String},
  };

  for (const auto t : types)
  {
    if (type_str.compare(t.first) == 0)
    {
      return t.second;
    }
  }

  return std::nullopt;
}

Expr::ColumnDef Parser::column_def()
{
  const Token name = m_scanner.next();
  name.expectOrThrow((std::stringstream()
      << name.location() << " Expected column name, instead saw " << name.toString()
      ).str(), Token::Kind::Identifier);
  const Token type = m_scanner.next();
  type.expectOrThrow((std::stringstream()
      << type.location() << " Expected column type, instead saw " << type.toString()
      ).str(), Token::Kind::Identifier); 

  auto colType = resolveColumnType(type.lexeme());
  if (!colType)
  {
    throw std::runtime_error((std::stringstream() << type.location() << " Unknown column type: " << type.lexeme()).str());
  }
  return Expr::ColumnDef{name, {type, colType.value()}};
}

std::vector<Expr::ColumnDef> Parser::column_def_list()
{
  m_scanner.next().expectOrThrow("Expected '(' before column definitions", Token::Kind::OpenParen);
  std::vector<Expr::ColumnDef> columns = {column_def()};

  while (true)
  {
    Token t = m_scanner.peekToken();
    if (!t.is(Token::Kind::Comma))
    {
      break;
    }
    m_scanner.next(); // consume the comma

    t = m_scanner.peekToken();
    if (t.is(Token::Kind::CloseParen))
    {
      // allow a trailing comma
      break;
    }
    columns.push_back(column_def());
  }

  m_scanner.next().expectOrThrow("Expected ')' after column definitions", Token::Kind::CloseParen);
  return columns;
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
  if (m_scanner.hadError())
  {
    error(op, "Error in tokeniser");
    return nullptr;
  }
  if (op.isOneOf(Token::Kind::Bang, Token::Kind::Minus))
  {
    m_scanner.next();
    if (m_scanner.hadError())
    {
      error(op, "Error in tokeniser");
      return nullptr;
    }
    auto right = unary();
    return std::make_unique<Expr::Unary>(op, std::move(right));
  }

  return primary();
}

std::unique_ptr<Expr::IExpr> Parser::primary()
{
  Token t = m_scanner.peekToken();
  if (m_scanner.hadError())
  {
    error(t, "Error in tokeniser");
    return nullptr;
  }

  if (t.isOneOf(Token::Kind::True, Token::Kind::False, Token::Kind::Number,
                Token::Kind::String))
  {
    m_scanner.next();
    if (m_scanner.hadError())
    {
      error(t, "Error in tokeniser");
      return nullptr;
    }

    return std::make_unique<Expr::Literal>(t);
  }

  if (t.is(Token::Kind::OpenParen))
  {
    m_scanner.next();
    if (m_scanner.hadError())
    {
      error(t, "Error in tokeniser");
      return nullptr;
    }

    auto expr = expression();
    Token next = m_scanner.next();
    if (m_scanner.hadError())
    {
      error(t, "Error in tokeniser");
      return nullptr;
    }

    if (next.kind() != Token::Kind::CloseParen)
    {
      error(next, "Expected ')' after expression");
      return nullptr;
    }

    return std::make_unique<Expr::Grouping>(std::move(expr));
  }

  error(t, "Expected expression");
  return nullptr;
}
