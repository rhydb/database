#pragma once

#include "token.hpp"
#include "bytecode.hpp"

#include <memory>
#include <variant>
#include <vector>

struct ExprVisitor;

namespace Expr
{
// can't template virtual functions, so use this
// void
// expression type
// instruction
// instruction chunk
using ReturnValue = std::variant<std::monostate, Token::Kind, Instruction, std::vector<Instruction>>;

struct IExpr
{
  virtual ~IExpr() {};
  virtual ReturnValue accept(ExprVisitor &visitor) = 0;
};

struct Binary : public IExpr
{
  Binary(std::unique_ptr<IExpr> left, Token op, std::unique_ptr<IExpr> right) : left(std::move(left)), op(op), right(std::move(right)) {}
  ReturnValue accept(ExprVisitor &visitor) override;

  std::unique_ptr<IExpr> left;
  Token op;
  std::unique_ptr<IExpr> right;
};

struct Literal : public IExpr
{
  Literal(Token value) : value(value) {}
  ReturnValue accept(ExprVisitor &visitor) override;
  Token value;
};

struct Grouping : public IExpr
{
  Grouping(std::unique_ptr<IExpr> expr) : expr(std::move(expr)) {}
  ReturnValue accept(ExprVisitor &visitor) override;
  std::unique_ptr<IExpr> expr;
};

struct Unary : public IExpr
{
  Unary(Token op, std::unique_ptr<IExpr> right) : op(op), right(std::move(right)) {}
  ReturnValue accept(ExprVisitor &visitor) override;
  Token op;
  std::unique_ptr<IExpr> right;
};

struct ColumnDef
{
  enum class Type
  {
    Integer,
    String,
  };

  Token name;
  struct
  {
    Token token;
    Type type;
  } type;
};

struct Create : public IExpr
{
  Create(std::string_view table_name, std::vector<ColumnDef> &&columns) : table_name(table_name), columns(std::move(columns)) {}
  ReturnValue accept(ExprVisitor &visitor) override;
  std::string_view table_name;
  std::vector<ColumnDef> columns;
};

}
