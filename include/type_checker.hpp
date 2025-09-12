#pragma once

#include "visitor.hpp"

struct TypeChecker : public ExprVisitor
{
  bool check(const std::unique_ptr<Expr::IExpr> &e);
  Expr::ReturnValue visitBinary(const Expr::Binary &binary);
  Expr::ReturnValue visitGrouping(const Expr::Grouping &grouping);
  Expr::ReturnValue visitLiteral(const Expr::Literal &literal);
  Expr::ReturnValue visitUnary(const Expr::Unary &unary);
  Expr::ReturnValue visitCreate(const Expr::Create &t) { (void)t; return EXPR_VOID; }

private:
  bool m_hadError = false;
};
