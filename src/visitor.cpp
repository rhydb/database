#include "visitor.hpp"
#include "expression.hpp"

std::string AstPrinter::print(const std::unique_ptr<Expr::IExpr> &e)
{
  e->accept(*this);
  return output.str();
}

void AstPrinter::visitBinary(const Expr::Binary &binary)
{
  std::string lexeme = std::string(binary.op.lexeme());
  parenthesise(lexeme.c_str(), binary.left, binary.right);
}

void AstPrinter::visitGrouping(const Expr::Grouping &grouping)
{
  parenthesise("group", grouping.expr);
}

void AstPrinter::visitLiteral(const Expr::Literal &literal)
{
  output << literal.value.lexeme();
}

void AstPrinter::visitUnary(const Expr::Unary &unary)
{

  std::string lexeme = std::string(unary.op.lexeme());
  parenthesise(lexeme.c_str(), unary.right);
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
