#include <iostream>

#include "common/version.hpp"

int main() {
    std::cout << "CrossFlow-X Agent " << cfx::version::string() << "\n";
    std::cout << "Driverless User-Mode Architecture\n";
    return 0;
}