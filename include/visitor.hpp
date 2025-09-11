#pragma once
#include <string>
#include <memory>
#include <sstream>

#include "expression.hpp"

struct ExprVisitor
{
  virtual Expr::ReturnValue visitBinary(const Expr::Binary &binary) = 0;
  virtual Expr::ReturnValue visitGrouping(const Expr::Grouping &grouping) = 0;
  virtual Expr::ReturnValue visitLiteral(const Expr::Literal &literal) = 0;
  virtual Expr::ReturnValue visitUnary(const Expr::Unary &unary) = 0;
  virtual Expr::ReturnValue visitCreate(const Expr::Create &unary) = 0;
};

struct AstPrinter : public ExprVisitor
{
  std::string print(const std::unique_ptr<Expr::IExpr> &e);

  Expr::ReturnValue visitBinary(const Expr::Binary &binary);
  Expr::ReturnValue visitGrouping(const Expr::Grouping &grouping);
  Expr::ReturnValue visitLiteral(const Expr::Literal &literal);
  Expr::ReturnValue visitUnary(const Expr::Unary &unary);
  Expr::ReturnValue visitCreate(const Expr::Create &unary);

private:
  template <typename T>
  void write(T str);
  std::stringstream output;
  template <typename... Es>
  void parenthesise(const char *name, const std::unique_ptr<Expr::IExpr> &e1, const Es &...es);
};
