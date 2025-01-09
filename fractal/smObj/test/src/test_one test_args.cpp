#include <iostream>
#include "smObj.h"

extern "C" void test_one test_args(std::shared_ptr<smObj> obj) {
    std::cout << "Function test_one test_args executed.\n";
}
