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


std::ostream &operator<<(std::ostream &os, const Expr::Type &type)
{
  switch (type)
  {
    case Expr::Type::Number:
      return os << "Number";
    case Expr::Type::Bool:
      return os << "Bool";
    case Expr::Type::String:
      return os << "String";
    case Expr::Type::Unknown:
  default:
      return os << "Unkown";
  }
}
