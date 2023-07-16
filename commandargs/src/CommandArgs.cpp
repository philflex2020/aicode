// CommandArgs
// p wilshire
// 07_16_2023

#include "CommandArgs.h"

int main(int argc, char* argv[]) {
    CommandLineArgs cmdArgs;
    cmdArgs.parse(argc, argv);

    std::string name = cmdArgs.getValue("name","n");
    std::string age = cmdArgs.getValue("age","a");
    std::string city = cmdArgs.getValue("city","c");
    std::string country = cmdArgs.getValue("country","t");
    std::string somedef = cmdArgs.getValue("dummy","d","Default");
    std::string extra = cmdArgs.getExtra();

    std::cout << "Name: " << (name.empty() ? "[Not Specified]" : name) << std::endl;
    std::cout << "Age: " << (age.empty() ? "[Not Specified]" : age) << std::endl;
    std::cout << "City: " << (city.empty() ? "[Not Specified]" : city) << std::endl;
    std::cout << "Country: " << (country.empty() ? "[Not Specified]" : country) << std::endl;
    std::cout << "Dummy: " << (somedef.empty() ? "[Not Specified]" : somedef) << std::endl;
    std::cout << "Extra: " << (extra.empty() ? "[Not Specified]" : extra) << std::endl;


    return 0;
}