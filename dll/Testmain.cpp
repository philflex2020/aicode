#include "TestHeader.hpp"
#include <iostream>
#include <dlfcn.h>
#include <stdio.h>


int main() {
        using std::cout;
        using std::cerr;
        Test tMainObj;    
        typedef int (*TestNumber)(Test*, int);
        typedef int (*TestShow)(Test*);

        void* thandle = dlopen("./TestCode.so", RTLD_LAZY);
        if (!thandle) {
                cerr << "Cannot load TestCode: " << dlerror() << '\n';
                return 1;
        }

        // reset errors
        dlerror();

        // load the symbols
        TestNumber getAge = (TestNumber) dlsym(thandle, "getNumber");
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
                cerr << "Cannot load symbol getNumber: " << dlsym_error << '\n';
                return 1;
        }
        TestShow ts = (TestShow) dlsym(thandle, "showFunc");
        dlsym_error = dlerror();
        if (dlsym_error) {
                cerr << "Cannot load symbol showFunc: " << dlsym_error << '\n';
                return 1;
        }
        printf("Getting my age\n");
        int myAge = 25; 

        getAge(&tMainObj,myAge);
        printf("My age from the so is: %d\n",tMainObj.number);

        ts(&tMainObj);
        dlclose(thandle);
}
