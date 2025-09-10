#include "tokens.hpp"

#include <string>
#include <iostream>

#include "parser.hpp"
#include "visitor.hpp"

void printPrompt()
{
  std::cout << "database: ";
}

int main() {
  std::string input;

  while (true) {
    printPrompt();
    std::getline(std::cin, input);

    if (input == ".quit")
    {
      break;
    }

    Lexer lex = Lexer(input.c_str());
    Parser p = Parser(lex);
    auto expr = p.parse();

    if (!expr)
    {
      std::cout << "Failed to parse" << std::endl;
      continue;
    }

    AstPrinter ap = AstPrinter();
    std::cout << ap.print(expr) << std::endl;
  }
}
