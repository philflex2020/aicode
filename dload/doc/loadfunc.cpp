#include <iostream>
#include <string>
#include <unordered_map>
#include <dlfcn.h> // For dynamic loading in Linux

typedef void (*FunctionType)(int);

std::unordered_map<std::string, FunctionType> functionMap;

FunctionType loadFunction(const std::string& name, const std::string& library) {
    void* handle = dlopen(library.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Cannot open library: " << dlerror() << '\n';
        return nullptr;
    }

    // Reset errors
    dlerror();
    FunctionType func = (FunctionType) dlsym(handle, name.c_str());
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol '" << name << "': " << dlsym_error << '\n';
        dlclose(handle);
        return nullptr;
    }

    return func;
}

void runFunction(const std::string& name, int value) {
    if (functionMap.find(name) == functionMap.end()) {
        std::string libraryPath; // Determine the library path based on the function name
        FunctionType func = loadFunction(name, libraryPath);
        if (func) {
            functionMap[name] = func;
        } else {
            std::cerr << "Function loading failed.\n";
            return;
        }
    }

    FunctionType func = functionMap[name];
    func(value);
}

int main() {
    std::string functionName;
    int value;

    std::cout << "Enter function name: ";
    std::cin >> functionName;

    std::cout << "Enter value: ";
    std::cin >> value;

    runFunction(functionName, value);

    return 0;
}
