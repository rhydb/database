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
  }
}
