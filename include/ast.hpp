#pragma once

#include "visitor.hpp"
#include "bytecode.hpp"
#include "common.hpp"

#include <vector>

struct TypeChecker : public ExprVisitor
{
  bool check(const std::unique_ptr<Expr::IExpr> &e);
  Expr::ReturnValue visitBinary(const Expr::Binary &binary);
  Expr::ReturnValue visitGrouping(const Expr::Grouping &grouping);
  Expr::ReturnValue visitLiteral(const Expr::Literal &literal);
  Expr::ReturnValue visitUnary(const Expr::Unary &unary);
  Expr::ReturnValue visitCreate(const Expr::Create &t) { UNUSED(t); return std::monostate{}; }

private:
  bool m_hadError = false;
};


struct Bytecode : public ExprVisitor
{
  std::vector<Instruction> compile(const std::unique_ptr<Expr::IExpr> &e);
  Expr::ReturnValue visitBinary(const Expr::Binary &binary);
  Expr::ReturnValue visitGrouping(const Expr::Grouping &grouping);
  Expr::ReturnValue visitLiteral(const Expr::Literal &literal);
  Expr::ReturnValue visitUnary(const Expr::Unary &unary);
  Expr::ReturnValue visitCreate(const Expr::Create &t) { UNUSED(t); return std::monostate{}; }
};
