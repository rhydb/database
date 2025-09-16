#include "token.hpp"

#include <string>
#include <iostream>

#include "parser.hpp"
#include <variant>

void printPrompt()
{
  std::cout << "database: ";
}

int main()
{
  std::string input;

  while (true)
  {
    printPrompt();
    std::getline(std::cin, input);

    if (input == ".quit")
    {
      break;
    }

    Scanner lex = Scanner(input.c_str());
    Parser p = Parser(lex);
    auto instrs = p.parse();

    if (instrs.empty())
    {
      std::cout << "Failed to parse" << std::endl;
      continue;
    }

    std::vector<Instruction::Params> stack;

    for (const auto &i : instrs)
    {
      switch (i.op)
      {
      case Instruction::Opcode::PushNumber:
        {
          const auto p1 = std::get<double>(i.params);
          stack.push_back(p1);
          break;
        }
      case Instruction::Opcode::PushString:
        {
          const auto p1 = std::get<std::string_view>(i.params);
          stack.push_back(p1);
          break;
        }
      case Instruction::Opcode::Add:
      {
        const auto s1 = std::get<double>(stack.back());
        stack.pop_back();
        const auto s2 = std::get<double>(stack.back());
        stack.pop_back();
        stack.push_back(s1 + s2);
        break;
      }
      case Instruction::Opcode::Sub:
      {
        double r = std::get<double>(stack.back());
        stack.pop_back();
        r = r - std::get<double>(stack.back());
        stack.pop_back();
        stack.push_back(r);
        break;
      }
      case Instruction::Opcode::Mul:
      {
        double r = std::get<double>(stack.back());
        stack.pop_back();
        r = r * std::get<double>(stack.back());
        stack.pop_back();
        stack.push_back(r);
        break;
      }
      case Instruction::Opcode::Div:
      {
        double r = std::get<double>(stack.back());
        stack.pop_back();
        r = r / std::get<double>(stack.back());
        stack.pop_back();
        stack.push_back(r);
        break;
      }
      case Instruction::Opcode::Not:
      {
        if (double *s1 = std::get_if<double>(&stack.back()); s1 != nullptr)
        {
          double r = !(*s1);
          stack.pop_back();
          stack.push_back(r);
        }
        else if (std::string_view *s1 = std::get_if<std::string_view>(&stack.back()); s1 != nullptr)
        {
          // TODO: when this string is retrieved from database will it be string_view?
          const double r = s1->size() == 0;
          stack.pop_back();
          stack.push_back(r);
        }
        break;
      }
      case Instruction::Opcode::And:
      {
        auto s1 = std::get<double>(stack.back());
        stack.pop_back();
        const auto s2 = std::get<double>(stack.back());
        stack.pop_back();
        s1 = s1 && s2;
        stack.push_back(s1);
        break;
      }
      case Instruction::Opcode::Or:
      {
        auto s1 = std::get<double>(stack.back());
        stack.pop_back();
        const auto s2 = std::get<double>(stack.back());
        stack.pop_back();
        s1 = (int)s1 || (int)s2;
        stack.push_back(s1);
        break;
      }
      default:
        std::cerr << "Unknown opcode:" << i << std::endl;
      }
    }

    std::cout << std::get<double>(stack.back()) << std::endl;
  }
}
