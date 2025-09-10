#include "expression.hpp"
#include "visitor.hpp"

void Expr::Binary::accept(ExprVisitor &visitor)
{
  visitor.visitBinary(*this);
}
void Expr::Literal::accept(ExprVisitor &visitor)
{
  visitor.visitLiteral(*this);
}
void Expr::Grouping::accept(ExprVisitor &visitor)
{
  visitor.visitGrouping(*this);
}
void Expr::Unary::accept(ExprVisitor &visitor)
{
  visitor.visitUnary(*this);
}
