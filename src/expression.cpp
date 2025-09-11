#include "expression.hpp"
#include "visitor.hpp"

Expr::ReturnValue Expr::Binary::accept(ExprVisitor &visitor)
{
  return visitor.visitBinary(*this);
}
Expr::ReturnValue Expr::Literal::accept(ExprVisitor &visitor)
{
  return visitor.visitLiteral(*this);
}
Expr::ReturnValue Expr::Grouping::accept(ExprVisitor &visitor)
{
  return visitor.visitGrouping(*this);
}
Expr::ReturnValue Expr::Unary::accept(ExprVisitor &visitor)
{
  return visitor.visitUnary(*this);
}

Expr::ReturnValue Expr::Create::accept(ExprVisitor &visitor)
{
  return visitor.visitCreate(*this);
}
