#include <iostream>
#include <map>
#include <string>
#include <dlfcn.h>

//g++ -o main_program main_program.cpp -ldl
extern "C" typedef int (*MyFunction)(int, float);
extern "C" typedef void (*InitFunction)(std::map<std::string, void*>&);


std::map<std::string, void*> functionMap;



#define FUN_LIB_DIR "/usr/local/lib/myfun/"
#define FUN_TERM "_function.so"

//typedef int (*MyFunction)(int, float);
// void* loadMySharedObject(const std::string& funname)
// {
//     std::string = FUN_LIB_DIR + funname + FUN_TERM
// }

void* loadSharedObject(const std::string& filename) {
    void* handle = dlopen(filename.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Error loading shared object: " << dlerror() << std::endl;
        return nullptr;
    }
    return handle;
}
void* loadMySharedObject(const std::string& funname)
{
    std::string fname = FUN_LIB_DIR + funname + FUN_TERM;
    return loadSharedObject(fname);

}

//Call the initialization function in the shared object:
//After loading the shared object, you can call the init function from the loaded shared object to register the function in the main program's map:

void callInitFunction(void* handle, std::map<std::string, void*>& functionMap) {
    InitFunction initFunc = reinterpret_cast<InitFunction>(dlsym(handle, "init"));
    if (!initFunc) {
        std::cerr << "Error getting the init function: " << dlerror() << std::endl;
        dlclose(handle);
        return;
    }
    initFunc(functionMap);
}
// Call the functions from the main program:
// To call a function by its string name from the main program, you can use the function map and function pointer casting:
int callFunctionByName(const std::string& functionName, int arg1, float arg2) {
    //std::map<std::string, void*>::iterator
    //std::cout << "Function seeking: [" << functionName << "]"<<std::endl;
    auto it = functionMap.find(functionName);
    if (it != functionMap.end()) {
        //std::cerr << "Function Found: " << functionName << std::endl;
        MyFunction funcPtr = reinterpret_cast<MyFunction>(it->second);
        return funcPtr(arg1, arg2);
    }
    std::cerr << "Function not found: " << functionName << std::endl;
    return -1; // Or handle the error as needed
}


int main() {


    //std::map<std::string, void*> functionMap;

   // void* handle1 = loadSharedObject("/usr/local/lib/myfun/multiply_function.so");
    void* handle1 = loadMySharedObject("multiply");
    if (handle1) {
        callInitFunction(handle1, functionMap);
    }

    for (auto myf : functionMap)
    {
        std::cout << " name ["<<myf.first << "]" << std::endl; 
    }
    
    // void* handle2 = loadSharedObject("shared_object_2.so");
    // if (handle2) {
    //     callInitFunction(handle2, functionMap);
    // }

    int result = callFunctionByName("multiply", 42, 3.14);
    std::cout << "Result: " << result << std::endl;

    // Don't forget to clean up: close the handles and free the resources
    dlclose(handle1);
   // dlclose(handle2);

    return 0;
}