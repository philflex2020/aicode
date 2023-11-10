
#include <iostream>
#include <functional>
#include <unordered_map>
#include <string>
#include <map>

struct myVar {
    // Define your struct members here
};

// Define a common interface using std::function
template<typename... Args>
using CommonFunction = std::function<int(Args...)>;

int funct1(std::map<std::string, std::map<std::string, struct myVar*>> arg1, std::map<std::string, struct myVar*> arg2, std::string& name, struct myVar* arg3) {
    // Implement funct1 logic here
    return 0;
}

int funct2(std::map<std::string, struct myVar*>arg1, std::string&arg2, struct myVar*arg3) {
    // Implement funct2 logic here
    return 0;
}


bool func3(struct myVar* arg1, double tNow) {
    // Implement func3 logic here
    return false;
}

int main() {
    // Create a map to store functions with a common interface
    std::unordered_map<std::string, CommonFunction<
        std::map<std::string, std::map<std::string, struct myVar*>>,
        std::map<std::string, struct myVar*>,
        std::string&, struct myVar*
    >> functions;

    // Store functions with different signatures
    functions["funct1"] = funct1;
    functions["funct2"] = funct2;
    functions["func3"] = func3;

    // Call functions by name with different arguments
    std::string functionName = "funct1";
    int result1 = functions[functionName]({}, {}, functionName, nullptr);
    std::cout << "Result of " << functionName << ": " << result1 << std::endl;

    functionName = "funct2";
    int result2 = functions[functionName]({}, functionName, nullptr);
    std::cout << "Result of " << functionName << ": " << result2 << std::endl;

    functionName = "func3";
    bool result3 = functions[functionName](nullptr, 2.0);
    std::cout << "Result of " << functionName << ": " << result3 << std::endl;

    auto f3 = functions["func3"];
    bool result = f3(nullptr, 2.0);
    return 0;
}

// <!-- In this example:

// - We define a common interface `CommonFunction` that accepts the provided function signatures as template arguments.
// - We update the map to use `CommonFunction` as the value type, specifying the expected argument types for each function stored in the map.
// - When calling the functions, we provide the appropriate arguments for each function based on their signatures.

// This code allows you to store and call functions with different signatures and argument types in the same map while maintaining type safety.


// Yes, you can use the code you provided to retrieve a function from the `std::unordered_map` and call it with the appropriate arguments. Here's how you can do it:

// ```cpp -->
