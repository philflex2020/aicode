#include "smObj.h"

int example_func(std::shared_ptr<smObj> obj, const std::string& args) {
    std::cout << "Function called on object: " << obj->name << " with args: " << args << "\n";
    return 0;
}

int main() {
    auto obj = std::make_shared<smObj>("example");
    obj->get_ext()->register_func("example_func", example_func);

    obj->get_ext()->call_func("example_func", obj, "test_arg");
    return 0;
}
