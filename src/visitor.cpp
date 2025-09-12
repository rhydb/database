#include "visitor.hpp"
#include "expression.hpp"

std::string AstPrinter::print(const std::unique_ptr<Expr::IExpr> &e)
{
  e->accept(*this);
  return output.str();
}

Expr::ReturnValue AstPrinter::visitBinary(const Expr::Binary &binary)
{
  std::string lexeme = std::string(binary.op.lexeme());
  parenthesise(lexeme.c_str(), binary.left, binary.right);
  return std::monostate{};
}

Expr::ReturnValue AstPrinter::visitGrouping(const Expr::Grouping &grouping)
{
  parenthesise("group", grouping.expr);
  return std::monostate{};
}

Expr::ReturnValue AstPrinter::visitLiteral(const Expr::Literal &literal)
{
  output << literal.value.lexeme();
  return std::monostate{};
}

Expr::ReturnValue AstPrinter::visitUnary(const Expr::Unary &unary)
{

  std::string lexeme = std::string(unary.op.lexeme());
  parenthesise(lexeme.c_str(), unary.right);
  return std::monostate{};
}

Expr::ReturnValue AstPrinter::visitCreate(const Expr::Create &create)
{

  output << "create " << create.table_name;
  output << "(";
  for (const Expr::ColumnDef &col : create.columns)
  {
    output << " " << col.name.lexeme() << ":" << col.type.token.lexeme() << ", ";
  }
  output << ")";
  return std::monostate{};
}

template <typename... Es>
void AstPrinter::parenthesise(const char *name,
                              const std::unique_ptr<Expr::IExpr> &e1, const Es &...es)
{
  output << "(";
  output << name;
  ((e1->accept(*this), output << ' '), ..., es->accept(*this));
  output << ")";
}
