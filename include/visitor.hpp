#pragma once
#include <string>
#include <memory>
#include <sstream>

namespace Expr
{
struct IExpr;
struct Binary;
struct Literal;
struct Grouping;
struct Unary;
struct Create;
} // namespace Expr

struct ExprVisitor
{
  virtual void visitBinary(const Expr::Binary &binary) = 0;
  virtual void visitGrouping(const Expr::Grouping &grouping) = 0;
  virtual void visitLiteral(const Expr::Literal &literal) = 0;
  virtual void visitUnary(const Expr::Unary &unary) = 0;
  virtual void visitCreate(const Expr::Create &unary) = 0;
};

struct AstPrinter : public ExprVisitor
{
  std::string print(const std::unique_ptr<Expr::IExpr> &e);

  void visitBinary(const Expr::Binary &binary);
  void visitGrouping(const Expr::Grouping &grouping);
  void visitLiteral(const Expr::Literal &literal);
  void visitUnary(const Expr::Unary &unary);
  void visitCreate(const Expr::Create &unary);

private:
  template <typename T>
  void write(T str);
  std::stringstream output;
  template <typename... Es>
  void parenthesise(const char *name, const std::unique_ptr<Expr::IExpr> &e1, const Es &...es);
};
