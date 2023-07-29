#include <dlfcn.h>

typedef void (*foo_handle)(); //same signature as in the lib
typedef void (*bar_handle)(); //same signature as in the lib

foo_handle foo;
bar_handle bar;
void *foobar_lib;

int main()
{
    foobar_lib = dlopen("./foobar.so", RTLD_LAZY | RTLD_DEEPBIND);

    foo = reinterpret_cast<foo_handle>(dlsym(foobar_lib, "foo"));
    bar = reinterpret_cast<bar_handle>(dlsym(foobar_lib, "bar"));

    foo();
    bar();

    dlclose(foobar_lib);
    
    return 0;
}
