#include <iostream>
#include <functional>
//#include <unordered_map>
#include <string>
#include <map>
//g++ -std=c++11 -o fptr fptr2.cpp

struct myVar {
    // Define your struct members here
};

// Base class for all function types
struct FunctionBase {
    virtual ~FunctionBase() {}
};

int SomeFunc1(std::map<std::string, std::map<std::string, struct myVar*>> v, std::map<std::string, struct myVar*> m, std::string& s, struct myVar* aV) 
{
    std::cout << __func__ << " called  arg2 is :" << s << std::endl;
    return 0;
}

int SomeFunc2(std::map<std::string, struct myVar*> m, std::string& s, struct myVar* aV) 
{
    std::cout << __func__ << " called  arg2 is :" << s << std::endl;
    return 0;
}

// Function type 1
struct Function1 : public FunctionBase {
    virtual int operator()(std::map<std::string, std::map<std::string, struct myVar*>> v, std::map<std::string, struct myVar*> m, std::string& s, struct myVar* aV) {
        // Implement funct1 logic here        
        std::cout << " Function1 called  arg2 is :" << s << std::endl;
        return SomeFunc1(v,m,s,aV);
        //return 0;
    }
};

// Function type 2
struct Function2 : public FunctionBase {
    virtual int operator()(std::map<std::string, struct myVar*> m, std::string& s, struct myVar* v) {
        // Implement funct2 logic here
        std::cout << " Function2 called  arg2 is :" << s << std::endl;
        return SomeFunc2(m,s,v);
        //return 0;
    }
};

// Function type 3
struct Function3 : public FunctionBase {
    virtual bool operator()(struct myVar* v, double f) {
        std::cout << " Function3 called  arg2 is : " << f << std::endl;
        return false;
    }
};

int main() {
    // Create a map to store function objects
    std::map<std::string, FunctionBase*> functions;

    // Store function objects with different signatures
    functions["funct1"] = new Function1();
    functions["funct2"] = new Function2();
    functions["func3"] = new Function3();

    // Call functions by name with different arguments
    std::string functionName = "funct1";
    int result1 = dynamic_cast<Function1*>(functions[functionName])->operator()({}, {}, functionName, nullptr);
    std::cout << "Result of " << functionName << ": " << result1 << std::endl;

    functionName = "funct2";
    int result2 = dynamic_cast<Function2*>(functions[functionName])->operator()({}, functionName, nullptr);
    std::cout << "Result of " << functionName << ": " << result2 << std::endl;

    functionName = "func3";
    bool result3 = dynamic_cast<Function3*>(functions[functionName])->operator()(nullptr, 2.0);
    std::cout << "Result of " << functionName << ": " << result3 << std::endl;

    std::string fname("func3");

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << " Looking for function called " << fname << std::endl;
    FunctionBase* func = functions[fname];
    if (func) 
    {
        std::cout << " found func   " << fname << std::endl;
        std::cout << " trying to map as Function2 type  " << fname << std::endl;
        Function2* func2 = dynamic_cast<Function2*>(func);
        if (func2) {
            // func2 is a Function2
            std::string foo("thisisfunc2");
            int result = (*func2)({}, foo, nullptr);
            std::cout << "Result of func2 : " << result << std::endl;
        } else {
            std::cout << "Function [" << fname << "] is not of type Function2" << std::endl;
        }   
        std::cout << std::endl;
        std::cout << std::endl;
    

        std::cout << " trying to map as Function3 type  " << fname << std::endl;
        Function3* func3 = dynamic_cast<Function3*>(func);
        if (func3) {
            // func3 is a Function3
            bool result = (*func3)(nullptr, 22.0);
            std::cout << "Result of func3 : " << result << std::endl;
        } else {
            std::cout << "Function [" << fname << "] is not of type Function3" << std::endl;
        }   
        std::cout << std::endl;
        std::cout << std::endl;
    }

    // Clean up memory
    for (auto& pair : functions) {
        delete pair.second;
    }

    return 0;
}
