#include <string>

#include "common/version.hpp"

int main() {
    return cfx::version::string() == std::string_view("0.1.0") ? 0 : 1;
}