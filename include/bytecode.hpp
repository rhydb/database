#pragma once

#include <vector>
#include <iostream>
#include <string_view>

struct Instruction
{
  enum Opcode
  {
    PushNumber = 0,
    PushString,
    Add, // push(pop + pop)
    Sub, // push(pop - pop)
    Div, // push(pop / pop)
    Mul, // push(pop * pop)
    Not, // push(!pop)
  } op;

  Instruction(Opcode op, int p1 = 0) : op(op), p1(p1) {}
  Instruction(Opcode op, std::string_view str) : op(op), str(str) {}

  double p1;
  std::string_view str;
};

using Chunk = std::vector<Instruction>;

std::ostream &operator<<(std::ostream &os, const Instruction &i);
