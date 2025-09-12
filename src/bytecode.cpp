#include "ast.hpp"

#include <cassert>


std::vector<Instruction> Bytecode::compile(const std::unique_ptr<Expr::IExpr> &e)
{
  Expr::ReturnValue val = e->accept(*this);
  auto chunk = std::get<std::vector<Instruction>>(val);
  return chunk;
}

Expr::ReturnValue Bytecode::visitLiteral(const Expr::Literal &literal)
{
  return Instruction(Instruction::Opcode::PushNumber, (int)literal.value.value.number);
}
