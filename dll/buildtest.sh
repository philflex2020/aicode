g++ -fPIC -c -Wall TestHeader.hpp
g++ -fPIC -c -Wall TestCode.cpp 
g++ -shared TestCode.o -o TestCode.so
g++ -fPIC -c -Wall TestMain.cpp
g++ TestMain.cpp -o testmain TestCode.o -ldl

