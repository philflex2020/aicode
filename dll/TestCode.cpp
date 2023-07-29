# include <stdio.h>
#include "TestHeader.hpp"

extern "C" int getNumber(Test* tObj, int number)
{
        tObj->number = number;
        printf("hi\n");
        return number+1;
}

extern "C" int showFunc(Test* tObj)
{
        //tObj->number = number;
        printf("func %p \n", (void*)getNumber);
        return 1;
}
