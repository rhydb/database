
#include "type_checker.hpp"
#include <cassert>

bool TypeChecker::check(const std::unique_ptr<Expr::IExpr> &e)
{
  m_hadError = false;
  e->accept(*this);
  return m_hadError;
}

Expr::ReturnValue TypeChecker::visitLiteral(const Expr::Literal &literal)
{
  return {Expr::ReturnValue::Type::ExprType, {literal.value.kind()}};
}

Expr::ReturnValue TypeChecker::visitBinary(const Expr::Binary &binary)
{
  const Expr::ReturnValue leftValue = binary.left->accept(*this);
  const Expr::ReturnValue rightValue = binary.right->accept(*this);


  if (leftValue.type != Expr::ReturnValue::Type::ExprType)
  {
    assert(m_hadError == true && "No error but return value is not ExprType");
    return EXPR_VOID;
  }

  if (rightValue.type != Expr::ReturnValue::Type::ExprType)
  {
    assert(m_hadError == true && "No error but return value is not ExprType");
    return EXPR_VOID;
  }

  const Token::Kind leftKind = leftValue.value.exprType;
  const Token::Kind rightKind = rightValue.value.exprType;

  if (leftKind != rightKind)
  {
    m_hadError = true;
    std::cerr << binary.op.location() << " ";
    std::cerr << "Type mismatch: attempting to perform " << binary.op.lexeme() << " with ";
    std::cerr << leftKind;
    std::cerr << " and ";
    std::cerr << rightKind;
    std::cerr << std::endl;

    return EXPR_VOID;
  }

  return {Expr::ReturnValue::Type::ExprType, {leftKind}};
}

Expr::ReturnValue TypeChecker::visitGrouping(const Expr::Grouping &grouping)
{
  return grouping.expr->accept(*this);
}

Expr::ReturnValue TypeChecker::visitUnary(const Expr::Unary &unary)
{
  const Expr::ReturnValue exprValue = unary.right->accept(*this);
  if (exprValue.type != Expr::ReturnValue::Type::ExprType)
  {
    assert(m_hadError == true && "No error but return value is not ExprType");
    return EXPR_VOID;
  }

  const Token::Kind kind = exprValue.value.exprType;

  if (kind != Token::Kind::Number)
  {
    m_hadError = true;
    std::cerr << unary.op.location() << " ";
    std::cerr << "Cannot perform " << unary.op.toString();
    std::cerr << " to " << kind << std::endl;
    return EXPR_VOID;
  }

  return {Expr::ReturnValue::Type::ExprType, {kind}};
}
