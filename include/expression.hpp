#pragma once
#include "token.hpp"

#include <memory>
#include <vector>

struct ExprVisitor;

namespace Expr {

struct IExpr
{
  virtual ~IExpr() {};
  virtual void accept(ExprVisitor &visitor) = 0;
};

struct Binary : public IExpr
{
  Binary(std::unique_ptr<IExpr> left, Token op, std::unique_ptr<IExpr> right) : left(std::move(left)), op(op), right(std::move(right)) {}
  void accept(ExprVisitor &visitor) override;

  std::unique_ptr<IExpr> left;
  Token op;
  std::unique_ptr<IExpr> right;
};

struct Literal : public IExpr
{
  Literal(Token value) : value(value) {}
  void accept(ExprVisitor &visitor) override;
  Token value;
};

struct Grouping : public IExpr
{
  Grouping(std::unique_ptr<IExpr> expr) : expr(std::move(expr)) {}
  void accept(ExprVisitor &visitor) override;
  std::unique_ptr<IExpr> expr;
};

struct Unary : public IExpr
{
  Unary(Token op, std::unique_ptr<IExpr> right) : op(op), right(std::move(right)) {}
  void accept(ExprVisitor &visitor) override;
  Token op;
  std::unique_ptr<IExpr> right;
};

struct ColumnDef
{
  Token name;
  Token type;
};

struct Create : public IExpr
{
  Create(std::string_view table_name, std::vector<ColumnDef> &&columns) : table_name(table_name), columns(std::move(columns)) {}
  void accept(ExprVisitor &visitor) override;
  std::string_view table_name;
  std::vector<ColumnDef> columns;
};

}
