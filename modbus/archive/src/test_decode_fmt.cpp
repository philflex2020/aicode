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

//g++ -std=c++17 -DSPDLOG_COMPILED_LIB -o t2 src/test_decode_fmt.cpp -lspdlog

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
using f64 = double;
using f32 = float;

std::string sfmt ="naked";

struct map_struct {
    std::string id;
    int offset = 0;
    int type;
    bool is_float = false;
    double scale = 0.0f;
    int shift = 0 ;
    int size = 4 ;
    int starting_bit_pos = 0 ;
    bool is_word_swapped =  false;
    bool is_signed =  false;
    bool uses_masks =  false;
    u64 invert_mask =  0;
    u64 care_mask =  0;
    bool is_enum = false;
    bool is_bool = false;
    bool is_bit_field;
    bool is_individual_bits = false;
    int bit_index = 0;  // used in individual bits

    std::vector<std::string>bit_str;
    std::vector<int>bit_str_num;
    std::vector<bool>bit_str_known;


    // suddenly it all gets a bit simpler with this
    bool use_bool = true;
    bool use_hex = false;
    
    bool is_raw =  false;       // Coil, Discrete_Input = true 
    bool is_bit =  false;       // Coil, Discrete_Input = true 
    bool can_write =  false;    // Coil, Holding = true

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
bool testAnyVal(const std::any& anyVal, const T& defaultValue) {
    return anyVal.type() == typeid(T);
}


template<typename T>
T getAnyVal(const std::any& anyVal, const T& defaultValue) {
    try {
        return std::any_cast<T>(anyVal);
    } catch (const std::bad_any_cast&) {
        return defaultValue;
    }
}

/// @brief gcom_decode_any
/// correctly decode the raw16 buffer received by the modbus into an std::any structure
/// that can then be used to process the strigstream output.
/// @param raw16 
/// @param raw8 
/// @param item 
/// @param output 
/// @param myCfg 
/// @return 
u64 gcom_decode_any(u16* raw16, u8*raw8, struct map_struct* item, std::any& output, struct cfg& myCfg)
{
//    static constexpr u64 decode(const u16* raw_registers, const Decode_Info& current_decode, Jval_buif& current_jval) noexcept
//{
    u64 raw_data = 0UL;

    u64 current_unsigned_val = 0UL;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;

    if (item->size == 1)
    {
        raw_data = *(u64 *)raw16;

        current_unsigned_val = raw_data;
        if (item->uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item->invert_mask;
            // care mask stuff:
            current_unsigned_val &= item->care_mask;
        }
        // do signed stuff (size 1 cannot be float):
        if (item->is_signed)
        {
            s16 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
    }
    else if (item->size == 2)
    {
        raw_data =  (static_cast<u64>(raw16[0]) << 16) +
                    (static_cast<u64>(raw16[1]) <<  0);

        if (!item->is_word_swapped) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swapped:
        {
            current_unsigned_val =  (static_cast<u64>(raw16[0]) <<  0) + 
                                    (static_cast<u64>(raw16[1]) << 16);
        }
        if (item->uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item->invert_mask;
            // care mask stuff:
            current_unsigned_val &= item->care_mask;
        }
        // do signed/float stuff:
        if (item->is_signed)
        {
            s32 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (item->is_float)
        {
            f32 to_reinterpret = 0.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }
    else // size of 4:
    {
        raw_data =  (static_cast<u64>(raw16[0]) << 48) +
                    (static_cast<u64>(raw16[1]) << 32) +
                    (static_cast<u64>(raw16[2]) << 16) +
                    (static_cast<u64>(raw16[3]) <<  0);

        if (!item->is_word_swapped) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swapped:
        {
            current_unsigned_val =  (static_cast<u64>(raw16[0]) <<  0) +
                                    (static_cast<u64>(raw16[1]) << 16) +
                                    (static_cast<u64>(raw16[2]) << 32) +
                                    (static_cast<u64>(raw16[3]) << 48);
        }
        if (item->uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item->invert_mask;
            // care mask stuff:
            current_unsigned_val &= item->care_mask;
        }
        // do signed/float stuff:
        if (item->is_signed) // this is only for whole numbers really (signed but not really float):
        {
            s64 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (item->is_float)
        {
            f64 to_reinterpret = 0.0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }

    // do scaling/shift stuff at the end:
    if (item->is_float)
    {
        if (item->scale) // scale != 0
        {
            current_float_val /= item->scale;
        }
        current_float_val += static_cast<f64>(item->shift); // add shift
        output = current_float_val;
    }
    else if (!item->is_signed) // unsigned whole numbers:
    {
        if (!item->scale) // scale == 0
        {
            current_unsigned_val += item->shift; // add shift
            current_unsigned_val >>= item->starting_bit_pos;
            output = current_unsigned_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_unsigned_val) / item->scale;
            scaled += static_cast<f64>(item->shift); // add shift
            output = scaled;
        }
    }
    else // signed whole numbers:
    {
        if (!item->scale) // scale == 0
        {
            current_signed_val += item->shift; // add shift
            current_signed_val >>= item->starting_bit_pos;
            output = current_signed_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_signed_val) / item->scale;
            scaled += static_cast<f64>(item->shift); // add shift
            output = scaled;
        }
    }
    //TODO float64
    return raw_data;
}

void decode_bval(bool bval, std::shared_ptr<map_struct> item, std::stringstream &ss, cfg_struct& cfg) {
    ss << addQuote(item->id) << ":";

    if (bval) {
        if (/*item->*/sfmt == "naked") 
        {
            if(item->use_bool) {
                ss << "true";
            } else {
                ss << 1;
            }
        }
        else 
        {
            if(item->use_bool) {
                ss << "{\"value\": true }";
            } else {
                ss << "{\"value\": 1 }";
            }

        }
    } else {
        if (/*item->*/sfmt == "naked") 
        {
            if(item->use_bool) {
                ss << "false";
            } else {
                ss << 0;
            }
        }
        else
        {
            if(item->use_bool) {
                ss << "{\"value\": false }";
            } else {
                ss << "{\"value\": 0 }";
            }
        }

    }
}        

void decode_bval_from_index(std::any& value, std::shared_ptr<map_struct> item, std::stringstream &ss, cfg_struct& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    auto bval = ((val >> item->bit_index) & 1UL) == 1UL;//std::any_cast<bool>(input);
    decode_bval(bval, item, ss, cfg);
}        

void decode_bval_from_value(std::any& value, std::shared_ptr<map_struct> item, std::stringstream &ss, cfg_struct& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    auto bval = (val != 0);
    decode_bval(bval, item, ss, cfg);
}        


void decode_bit_field(std::any &value, std::shared_ptr<map_struct> item, std::stringstream &bss, cfg_struct& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    bool firstOne = true;
    auto last_idx = item->bit_str.size() + 1;
    bss << "[";
    for (u8 idx = 0; idx < item->bit_str.size(); ++idx){
        //const auto& enum_str = item.bit_str[enum_str_idx];
        if (!item->bit_str_known[idx]) // "known" bits that are not ignored:
        {
            if(firstOne) { firstOne = false; } else { bss << ","; }
            if (((val >> idx) & 1UL) == 1UL) {// only format bits that are high:
                bss << "{\"value\":" << val << ", \"string\":"<< addQuote(item->bit_str[idx]) << "}";
            } 
        } else // "unknown" bits that are "inbetween" bits (and not ignored):
        {
            if(firstOne) { firstOne = false; } else { bss << ","; }
            if (((val >> idx) & 1UL) == 1UL) {// only format bits that are high:
                bss << "{\"value\":" << val << ", \"string\":"<< addQuote("unknown") << "}";
            }
        }
    }
    for (; last_idx < (item->size * 16); ++last_idx){
        if(firstOne) { firstOne = false; } else { bss << ","; }
        if (((val >> last_idx) & 1UL) == 1UL) { // only format bits that are high:
            bss << "{\"value\":" << val << ", \"string\":"<< addQuote("unknown") << "}";
        }
    }
    bss << "]";
}

void decode_enum(std::any &value, std::shared_ptr<map_struct> item, std::stringstream &bss, cfg_struct& cfg) {
    bool enum_found = false;
    u64 val = getAnyVal(value, (u64)0) ;

    for (u8 idx = 0; idx < item->bit_str.size(); ++idx){
        if (val == item->bit_str_num[idx]){
            enum_found = true;
            bss << "[{\"value\":" << val << ", \"string\":"<< addQuote(item->bit_str[idx]) << "}]";
            break;
        }
    }
    if (!enum_found) {// config did not account for this value: 
        bss << "[{\"value\":" << val << ", \"string\":"<< addQuote("unknown") << "}]";
    }
}

bool modbus_decode(u16 *raw16, std::shared_ptr<map_struct> item, std::any& value, std::stringstream &ss, cfg_struct& cfg);


/// @brief modbus decode 
////  take the std::any strucure from gcom_decode_any and produce a string from it.

/// @param raw16 
/// @param item 
/// @param value 
/// @param ss 
/// @param cfg 
/// @return 
bool modbus_decode(u16 *raw16,std::shared_ptr<map_struct> item, std::any& value, std::stringstream &ss, cfg_struct& cfg)
{
    if (item->is_bit){
        decode_bval_from_value(value, item, ss, cfg);
        
    } else if (item->is_raw){
        u64 val = getAnyVal(value, (u64)0) ;
        if(item->use_hex) {
            ss << addQuote(item->id) << ":"<< std::hex << val <<std::dec;
        } else {
            ss << addQuote(item->id) << ":"<< val;
        }

    } else if (item->is_signed){
        s64 val = getAnyVal(value, (s64)0) ;
        ss << addQuote(item->id) << ":"<< val;

//    } else if (item->is_bit_string_types){
    } else if (item->is_individual_bits){
        decode_bval_from_index(value, item, ss, cfg);

    } else if (item->is_bit_field){  
        //bool enum_found = false;
        std::stringstream bss;
        decode_bit_field(value, item, bss, cfg);
        ss << addQuote(item->id) << ":"<< bss.str();

    } else if (item->is_enum){
        std::stringstream bss;
        decode_enum(value, item, bss, cfg);
        ss << addQuote(item->id) << ":"<< bss.str();
    
    } else if (item->is_float){
        float val = getAnyVal(value, 0.0f) ;
        ss << addQuote(item->id) << ":"<< std::fixed<< std::setprecision(1)<<val <<std::defaultfloat;
    } else  {
        ss << addQuote(item->id) << ":"<< getAnyVal(value, 0);    
    }
    return true;
}


struct cfg_struct cfg;
int num_items = 16;

int main() {
    for ( int i = 0 ; i < 4 ; ++i) {
        auto item = std::make_shared<map_struct>();
        item->id = fmt::format("mybool_{}",i);
        item->is_bit = true;
        cfg.items.emplace_back(std::move(item));
    }
    for ( int i = 0 ; i <num_items ; ++i) {
        auto item = std::make_shared<map_struct>();
        item->bit_str.push_back("item_21");
        item->bit_str_num.push_back(21);
        item->bit_str.push_back("item_5");
        item->bit_str_num.push_back(5);
        if (i == 8) {
            item->is_enum = true;
        }
        item->id = fmt::format("myitem_{}",i);
        item->is_float = true;
        item->is_bit = false;
        cfg.items.emplace_back(std::move(item));
        //item->id = fmt::format("mybool_{}",i);
        //cfg.items.emplace_back(item);
    }
    float fnum=21;
    int inum = 100;
    u16 raw16[32];
    u64  bval =  1;

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
        if (item->is_enum) {
            std::any myunum = (u64)(5);
            modbus_decode(raw16, item, myunum, ss, cfg);

        }
        else if (item->is_bit) {
            std::any myunum = bval;
            bval ^= 1;
            modbus_decode(raw16, item, myunum, ss, cfg);

        }
        else if (inum%2) {
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

