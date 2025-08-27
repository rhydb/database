#include "tokens.hpp"

#include <string>
#include <iostream>

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

    Token token = Token();
    while (!(token = lex.next()).isOneOf(Token::Kind::End, Token::Kind::Unexpected))
    {
      std::cout << token.kind() << ":" << token.lexeme() << std::endl;
    }
  }
}
