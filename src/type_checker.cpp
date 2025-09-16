
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
  switch (literal.value.kind())
  {
    case Token::Kind::True: [[fallthrough]];
    case Token::Kind::False:
      return Expr::Type::Bool;
    case Token::Kind::String:
      return Expr::Type::String;
    case Token::Kind::Number:
      return Expr::Type::Number;
    default:
      std::cerr << literal.value.location() << " Unknown literal expression type:" <<std::endl;
      return Expr::Type::Unknown;
  }
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

  auto leftKind = std::get<Expr::Type>(leftResult);
  auto rightKind = std::get<Expr::Type>(rightResult);

  // allow casting bool to number
  if (leftKind == Expr::Type::Bool && rightKind == Expr::Type::Number)
  {
    leftKind = rightKind;
  }
  if (rightKind == Expr::Type::Bool && leftKind == Expr::Type::Number)
  {
    rightKind = leftKind;
  }

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
  // allow not on everything
  if (unary.op.is(Token::Kind::Bang))
  {
    return Expr::Type::Bool;
  }

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

  const auto type = std::get<Expr::Type>(result);

  if (type != Expr::Type::Number && type != Expr::Type::Bool)
  {
    m_hadError = true;
    std::cerr << unary.op.location() << " ";
    std::cerr << "Cannot perform " << unary.op.toString();
    std::cerr << " to " << type << std::endl;
    return std::monostate{};
  }

  return type;
}
