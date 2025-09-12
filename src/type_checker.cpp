
#include "ast.hpp"
#include <cassert>

bool TypeChecker::check(const std::unique_ptr<Expr::IExpr> &e)
{
  m_hadError = false;
  e->accept(*this);
  return m_hadError;
}

Expr::ReturnValue TypeChecker::visitLiteral(const Expr::Literal &literal)
{
  return literal.value.kind();
}

Expr::ReturnValue TypeChecker::visitBinary(const Expr::Binary &binary)
{
  // other errors don't matter we can stil check here
  // just make sure we dont have an error also
  bool prevHadError = m_hadError;
  m_hadError = false;
  const auto leftResult = binary.left->accept(*this);
  const auto rightResult = binary.right->accept(*this);
  if (m_hadError)
  {
    m_hadError = true;
    return std::monostate{};
  }
  m_hadError = prevHadError;

  const auto leftKind = std::get<Token::Kind>(leftResult);
  const auto rightKind = std::get<Token::Kind>(rightResult);

  if (leftKind != rightKind)
  {
    m_hadError = true;
    std::cerr << binary.op.location() << " ";
    std::cerr << "Type mismatch: attempting to perform " << binary.op.lexeme() << " with ";
    std::cerr << leftKind;
    std::cerr << " and ";
    std::cerr << rightKind;
    std::cerr << std::endl;

    return std::monostate{};
  }

  return leftKind;
}

Expr::ReturnValue TypeChecker::visitGrouping(const Expr::Grouping &grouping)
{
  return grouping.expr->accept(*this);
}

Expr::ReturnValue TypeChecker::visitUnary(const Expr::Unary &unary)
{
  // other errors don't matter we can stil check here
  // just make sure we dont have an error also
  bool prevHadError = m_hadError;
  m_hadError = false;
  const auto result = unary.right->accept(*this);
  if (m_hadError)
  {
    m_hadError = true;
    return std::monostate{};
  }
  m_hadError = prevHadError;

  const auto kind = std::get<Token::Kind>(result);
  if (kind != Token::Kind::Number)
  {
    m_hadError = true;
    std::cerr << unary.op.location() << " ";
    std::cerr << "Cannot perform " << unary.op.toString();
    std::cerr << " to " << kind << std::endl;
    return std::monostate{};
  }

  return kind;
}
