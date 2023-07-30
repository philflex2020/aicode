// multiply_function.cpp

#include "MyFunctionInterface.h"

//g++ -shared -fPIC -o multiply_function.so multiply_function.cpp
extern "C" int multiply(int x, float y) {
    return static_cast<int>(x * y);
}

extern "C" void init(std::map<std::string, void*>& functionMap) {
    functionMap["multiply"] = reinterpret_cast<void*>(multiply);
}