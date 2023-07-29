gcc -o foobar.so foobar.c -shared -fPIC
g++ -ldl -o dll_test main.cpp
