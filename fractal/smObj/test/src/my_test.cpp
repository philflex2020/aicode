#include <iostream>
#include "smObj.h"

extern "C" void my_test(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    // Implementation of my_test
    std::cout << "Function my_test executed; client " << client_socket << " args [" << args <<"] \n";
}
