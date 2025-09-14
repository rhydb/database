#include "ast.hpp"

#include <cassert>


Chunk Bytecode::compile(const std::unique_ptr<Expr::IExpr> &e)
{
  Expr::ReturnValue val = e->accept(*this);
  auto chunk = std::get<Chunk>(val);
  return chunk;
}

Expr::ReturnValue Bytecode::visitLiteral(const Expr::Literal &literal)
{
  if (literal.value.is(Token::Kind::String))
  {
    return Chunk{ Instruction(Instruction::Opcode::PushString, literal.value.lexeme()) };
  }

  if (literal.value.is(Token::Kind::Number))
  {
    return Chunk{ Instruction(Instruction::Opcode::PushNumber, literal.value.value.number) };
  }

  assert(false && "Unknown literal type");
  return Chunk{ Instruction(Instruction::Opcode::PushNumber, 0) };
}

Expr::ReturnValue Bytecode::visitBinary(const Expr::Binary &binary)
{
  const auto left = binary.left->accept(*this);
  const auto right = binary.right->accept(*this);

  // push (left)
  // push (right)
  // add (pop x2, push)

  auto chunk = std::get<Chunk>(left);
  const auto rightChunk = std::get<Chunk>(right);
  chunk.insert(chunk.end(), rightChunk.begin(), rightChunk.end());

  switch (binary.op.kind())
  {
    case Token::Kind::Plus:
      chunk.push_back(Instruction(Instruction::Opcode::Add));
      break;
    case Token::Kind::Minus:
      chunk.push_back(Instruction(Instruction::Opcode::Sub));
      break;
    case Token::Kind::Slash:
      chunk.push_back(Instruction(Instruction::Opcode::Div));
      break;
    case Token::Kind::Star:
      chunk.push_back(Instruction(Instruction::Opcode::Mul));
      break;
    default:
      assert(false && "Unknown operator in binary");
      break;
  }

  return chunk;
}

Expr::ReturnValue Bytecode::visitGrouping(const Expr::Grouping &grouping)
{
  return grouping.expr->accept(*this);
}

Expr::ReturnValue Bytecode::visitUnary(const Expr::Unary &unary)
{
  auto chunk = std::get<Chunk>(unary.right->accept(*this));

  switch (unary.op.kind())
  {
    case Token::Kind::Bang:
      chunk.push_back(Instruction(Instruction::Opcode::Not));
      break;
    case Token::Kind::Minus:
      // n
      // 0
      // Sub
      chunk.push_back(Instruction(Instruction::Opcode::PushNumber, 0));
      chunk.push_back(Instruction(Instruction::Opcode::Sub));
      break;
    default:
      assert(false && "Unknown operator in unary");
      break;
  }

  return chunk;
}



std::ostream &operator<<(std::ostream &os, const Instruction &i)
{
  static const char *opcodes[] = {
    [Instruction::Opcode::PushNumber] = "PushNumber",
    [Instruction::Opcode::PushString] = "PushString",
    [Instruction::Opcode::Add] = "Add",
    [Instruction::Opcode::Sub] = "Sub",
    [Instruction::Opcode::Div] = "Div",
    [Instruction::Opcode::Mul] = "Mul",
    [Instruction::Opcode::Not] = "Not",
  };
  os << opcodes[i.op] << '{';
  if (i.op == Instruction::Opcode::PushString)
  {
    return os << i.str << '}';
  }
  return os << i.p1 << '}';
}
