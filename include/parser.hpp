#pragma once
#include "tokens.hpp"
#include "expression.hpp"

#include <memory>

/*
expression     → equality ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor ( ( "-" | "+" ) factor )* ;
factor         → unary ( ( "/" | "*" ) unary )* ;
unary          → ( "!" | "-" ) unary
               | primary ;
primary        → NUMBER | STRING | "true" | "false" | "nil"
               | "(" expression ")" ;
*/

class Parser
{
public:
  Parser(Scanner &scanner) : m_scanner(scanner) {}
  std::unique_ptr<Expr::IExpr> parse();

private:
  void error(const Token &t, const char *msg);
  std::unique_ptr<Expr::IExpr> expression();
  std::unique_ptr<Expr::IExpr> equality();
  std::unique_ptr<Expr::IExpr> comparison();
  std::unique_ptr<Expr::IExpr> term();
  std::unique_ptr<Expr::IExpr> factor();
  std::unique_ptr<Expr::IExpr> unary();
  std::unique_ptr<Expr::IExpr> primary();

  Scanner m_scanner;
};
