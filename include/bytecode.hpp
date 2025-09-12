#pragma once

struct Instruction
{
  enum class Opcode
  {
    PushNumber,
    Add,
  } op;

  Instruction(Opcode op, int p1 = 0) : op(op), p1(p1) {}

  int p1;
};
