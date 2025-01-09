#include <iostream>
#include "smObj.h"

extern "C" void _my_test(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    // Implementation of _my_test
    std::cout << "Function _my_test executed.\n";
}
