#include <iostream>
#include <sstream>
#include <chrono>
#include <any>
#include <vector>

#include <spdlog/spdlog.h>
#include "spdlog/details/fmt_helper.h" // for microseconds formattting
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/bundled/ranges.h"
#include "spdlog/fmt/chrono.h"
//I have an any value and a map_struct
// I want to decode that 

#include <cstdint>
#include <iomanip>

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

// For signed integers
using s64 = int64_t;
using s32 = int32_t;
using s16 = int16_t;
using s8 = int8_t;

struct map_struct {
    std::string id;
    int offset = 0;
    int type;
    bool is_float = false;
    double scale = 0.0f;
    int shift = 0 ;
};

struct cfg_struct {
    std::string comp;
    int offset;
    int type;
    std::vector<std::shared_ptr<map_struct>> items;
};

std::string addQuote(const std::string& si) {
    return "\"" + si + "\"";
}

template<typename T>
T getAnyVal(const std::any& anyVal, const T& defaultValue) {
    try {
        return std::any_cast<T>(anyVal);
    } catch (const std::bad_any_cast&) {
        return defaultValue;
    }
}


bool modbus_decode(u16 *raw16, std::shared_ptr<map_struct> item, std::any value, std::stringstream &ss, cfg_struct& cfg);

bool modbus_decode(u16 *raw16, std::shared_ptr<map_struct> item, std::any value, std::stringstream &ss, cfg_struct& cfg)
{
    //if (item->is_float){float val = getAnyVal(value, 0.0f) ; ss << addQuote(item->id) << ":"<< std::fixed<< std::setprecision(1)<<val << ","<<std::defaultfloat;}
    //else               {                                     ss << addQuote(item->id) << ":"<< getAnyVal(value, 0) << ",";    }
    if (item->is_float){float val = getAnyVal(value, 0.0f) ;
                        if(item->scale)
                            val = val/item->scale;
                        if (item->shift)
                            val =  val - item->shift;

                        ss << addQuote(item->id) << ":"<< std::fixed<< std::setprecision(1)<<val <<std::defaultfloat;}
    else               {                                     ss << addQuote(item->id) << ":"<< getAnyVal(value, 0);    }
    return true;
}


struct cfg_struct cfg;
int num_items = 16;

int main() {
    for ( int i = 0 ; i <num_items ; ++i) {
        auto item = std::make_shared<map_struct>();
        item->id = fmt::format("myitem_{}",i);
        item->is_float = true;
        cfg.items.emplace_back(item);
    }
    float fnum=21;
    int inum = 100;
    u16 raw16[32];
    std::stringstream ss;
    bool firstOne = true;
    for(auto item :cfg.items){
        if(firstOne) {
            firstOne = false;
        }
        else {
            ss << ",";
        }
        std::any myfnum = ++fnum;        
        std::any myinum = ++inum;
        if (inum%2) {
            item->is_float = true;
            item->id = fmt::format("myfloat_{}",inum);
            modbus_decode(raw16, item, myfnum, ss, cfg);
        } else {
            item->is_float = false;
            item->id = fmt::format("myint_{}",inum);
            modbus_decode(raw16, item, myinum, ss, cfg);
        }
    }

    std::cout << " result [" <<ss.str()<<"]"<<std::endl;
    return 0;
}



// `std::stringstream` in C++ doesn't inherently provide the same level of format control as `fmt` or `printf` style formatting. `fmt` is designed to offer Python-like format string syntax, making it more expressive and easier to use than `stringstream`.

// However, you can achieve better format control in `stringstream` by using manipulators and other IO functions provided by the C++ standard library. Here’s an example of how you can achieve some format control:

// ### Example of Format Control with `stringstream`:

// ```cpp
// #include <iostream>
// #include <iomanip>
// #include <sstream>

// int main() {
//     std::stringstream ss;

//     // Setting precision
//     ss << std::fixed << std::setprecision(2);
//     ss << "This is a float: " << 3.14159 << std::endl;

//     // Setting width and fill character
//     ss << "This is an integer with width 5 and fill character '0': " 
//        << std::setw(5) << std::setfill('0') << 42 << std::endl;

//     // Formatting as hexadecimal
//     ss << "This is a hexadecimal: " << std::hex << 255 << std::endl;

//     // Resetting to decimal format
//     ss << std::dec;

//     // Using boolalpha to print booleans as true/false
//     ss << "This is a boolean: " << std::boolalpha << true << std::endl;

//     std::cout << ss.str();

//     return 0;
// }
// ```

// ### Explanation:

// - `std::fixed` and `std::setprecision(n)` are used to format floating-point numbers with `n` digits after the decimal point.
// - `std::setw(n)` sets the width of the next input/output field.
// - `std::setfill(c)` sets the fill character for padding.
// - `std::hex` and `std::dec` are used to switch between hexadecimal and decimal format.
// - `std::boolalpha` is used to print booleans as `true` or `false`.

