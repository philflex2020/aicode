
#include "smObj.h"


 // build  g++ -o test -I ../inc test1.cpp

int main() {
    auto root = std::make_shared<smObj>("root");
    root->attributes["temperature"] = 25;

    auto child = std::make_shared<smObj>("child");
    child->attributes["pressure"] = 100;

    root->children.push_back(child);./test

    root->debug_print();
    return 0;
}