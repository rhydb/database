#pragma once

#include <vector>
#include <iostream>
#include <string_view>
#include <variant>

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
    Or,
    And,
  } op;

  Instruction(Opcode op, double p1 = 0) : op(op), params(p1) {}
  Instruction(Opcode op, std::string_view str) : op(op), params(str) {}

  using Params = std::variant<double, std::string_view>;
  Params params;
};

using Chunk = std::vector<Instruction>;

std::ostream &operator<<(std::ostream &os, const Instruction &i);
