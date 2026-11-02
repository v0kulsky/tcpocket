#include "tcpocket/tcpocket.hpp"
#include <iostream>

int main() {
    std::cout << "tcpocket v" << tcpocket::version() << " - TCP connection library\n";
    return 0;
}
