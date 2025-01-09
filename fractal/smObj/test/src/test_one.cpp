#include <iostream>
#include "smObj.h"

extern "C" void test_one(std::shared_ptr<smObj> obj) {
    std::cout << "Function test_one executed.\n";
}
