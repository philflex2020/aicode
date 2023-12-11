#include <iostream>
#include <iomanip>
#include <sstream>
#include <any>

#include "gcom_config.h"
#include "gcom_iothread.h"

#include "shared_utils.hpp"

//std::string cfg::typeToStr(cfg::Register_Types rtype);

void gcom_modbus_decode(std::shared_ptr<IO_Work>io_work, std::stringstream &ss, struct cfg& myCfg);
void decode_bval(bool bval, std::shared_ptr<cfg::map_struct> item, std::stringstream &ss, struct cfg& cfg);
u64 gcom_decode_any(u16* raw16, u8*raw8, std::shared_ptr<cfg::map_struct>item, std::any& output, struct cfg& myCfg);
bool modbus_decode(u16 *raw16, std::shared_ptr<cfg::map_struct> item, std::any& value, std::stringstream &ss, struct cfg& cfg);



void gcom_modbus_decode(std::shared_ptr<IO_Work>io_work, std::stringstream &ss, struct cfg& myCfg)
{
    std::cout << " decode starting ... # items " << io_work->items.size()
            << " start_register :"               << io_work->start_register
            << " offset :"                       << io_work->offset
            << " num_registers :"                << io_work->num_registers
            << " type :"                            <<  myCfg.typeToStr(io_work->reg_type) 
            << std::endl;
    std::cout << "                    buf16_data  :"<< " 0x"; 
    for (int i = 0 ; i < io_work->num_registers ; ++i)
    {
        std::cout <<  std::setfill('0') << std::setw(8) << std::hex <<  io_work->buf16[i] <<" ";
        std::cout << " "<< std::dec ;
    }
    // std::cout << "  buf8_data  :"<< " 0x"; 
    // for (int i = 0 ; i < io_work->num_registers ; ++ i)
    // {
    //     std::cout <<  std::setfill('0') << std::setw(4) << std::hex <<  io_work->buf8[i] <<" ";
    //     std::cout <<std::dec<<  "  ";
    // }
    if ((io_work->reg_type == cfg::Register_Types::Discrete_Input) || (io_work->reg_type == cfg::Register_Types::Coil))
    {
        std::cout << "  buf8_data  dec :"; 
        for (int i = 0 ; i < io_work->num_registers ; ++ i)
        {
            std::cout <<   (int)io_work->buf8[i] <<" ";
            std::cout << " " << std::dec ;  
        }
    }
    else 
    {
        std::cout << "  buf16_data  dec :"; 
        for (int i = 0 ; i < io_work->num_registers ; ++ i)
        {
            std::cout <<   (int)io_work->buf16[i] <<" ";
            std::cout << " " << std::dec ;
        }
        std::cout << " " <<  std::endl; 
    }
    if ((io_work->reg_type == cfg::Register_Types::Discrete_Input) || (io_work->reg_type == cfg::Register_Types::Coil))
    {
        bool firstItem = true;
        for (auto item : io_work->items)
        { 
            if (!firstItem) 
            {
                ss << ",";
            }
            else
            {
                firstItem = false;
            }

            auto index = item->offset - io_work->offset;
            int buf_idx = index / 8;
            int buf_item = index % 8;
            std::cout << " bit offset : "          << item->offset 
                    << " index  : "        << index 
                    << " buf_idx  : "      << buf_idx 
                    << " buf_item  : "    << buf_item 
                    << " size : "          << item->size 
                    << std::endl; 

            bool bval = (io_work->buf8[index]>0);
            //bool bval = ((io_work->buf8[buf_idx] & (1<<buf_item)) > 0);
            decode_bval(bval, item, ss, myCfg);
        }
    }
    else if ((io_work->reg_type == cfg::Register_Types::Input) || (io_work->reg_type == cfg::Register_Types::Holding)) {
        bool firstItem = true;
        for (auto item : io_work->items)
        { 
            if (!firstItem) 
            {
                ss << ",";
            }
            else
            {
                firstItem = false;
            }

            auto index = item->offset - io_work->offset;
            //int buf_idx = index / 8;
            //int buf_item = index % 8;
            std::cout << " reg  offset : "          << item->offset 
                      << " index  : "           << index 
                      //<< " buf_idx  : "      << buf_idx 
                      //<< " buf_iitem  : "    << buf_item 
                      << " size : "             << item->size 
                      << std::endl; 

            std::any output;
            //u64 
            gcom_decode_any(&io_work->buf16[index], nullptr, item, output, myCfg);
            modbus_decode(&io_work->buf16[index], item, output, ss, myCfg);

        }

    }
    std::cout << " decode output : [" << ss.str() << "]" << std::endl;

} 


std::string anyToString(const std::any& value);
bool anyTypeString(const std::any& value);


std::string sfmt("naked");

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
u64 gcom_decode_any(u16* raw16, u8*raw8, std::shared_ptr<cfg::map_struct>item, std::any& output, struct cfg& myCfg)
{
//    static constexpr u64 decode(const u16* raw_registers, const Decode_Info& current_decode, Jval_buif& current_jval) noexcept
//{
    u64 raw_data = 0UL;

    u64 current_unsigned_val = 0UL;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;
    //bool use_signed  = false;
    
    if (item->size == 1)
    {
        raw_data = *(u16 *)raw16;
        //std::cout << " item raw_data " << raw_data << std::endl;

        current_unsigned_val = raw_data;
        //std::cout << " item raw_data " << raw_data <<" current_unsigned_val "<<  current_unsigned_val<< std::endl;
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
            output = current_signed_val;
            std::cout << " output  " <<" current_signed_val "<<  current_signed_val<< std::endl;
            //use_signed = true;
        }
        else
        {
            output = current_unsigned_val;
            std::cout << " output  " <<" current_unsigned_val "<<  current_unsigned_val<< std::endl;
        }
    }
    else if (item->size == 2)
    {
        raw_data =  (static_cast<u64>(raw16[0]) << 16) +
                    (static_cast<u64>(raw16[1]) <<  0);

        if (!item->is_byte_swap) // normal:
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
            output = current_signed_val;
            std::cout << " output  " <<" current_signed_val "<<  current_signed_val<< std::endl;

            //use_signed = true;
        }
        else if (item->is_float)
        {
            f32 to_reinterpret = 0.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
            output = current_float_val;
            std::cout << " output  " <<" current_float_val "<<  current_float_val<< std::endl;
       }
    }
    else // size of 4:
    {
        raw_data =  (static_cast<u64>(raw16[0]) << 48) +
                    (static_cast<u64>(raw16[1]) << 32) +
                    (static_cast<u64>(raw16[2]) << 16) +
                    (static_cast<u64>(raw16[3]) <<  0);

        if (!item->is_byte_swap) // normal:
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
            std::cout << " output  " <<" current_signed_val "<<  current_signed_val<< std::endl;
            output = current_signed_val;
            //use_signed = true;
        }
        else if (item->is_float)
        {
            f64 to_reinterpret = 0.0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
            std::cout << " output  " <<" current_float_val "<<  current_float_val<< std::endl;
            output = current_float_val;
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
        std::cout << " output  " <<" current_float_val #2 "<<  current_float_val<< std::endl;
        output = current_float_val;
    }
    else if (item->is_signed) // unsigned whole numbers:
    {
        if (item->scale) // scale == 0
        {
            auto scaled = static_cast<f64>(current_signed_val) / item->scale;
            scaled += static_cast<f64>(item->shift); // add shift
            std::cout << " output  " <<" scaled "<<  scaled<< std::endl;
            output = scaled;
        }
        else
        {
            current_signed_val += item->shift; // add shift
            current_signed_val >>= item->starting_bit_pos;
            output = current_signed_val;
            std::cout << " output  " <<" current_signed_val #2 "<<  current_signed_val<< std::endl;
        }
    }
    else // unsigned whole numbers:
    {
        if (item->scale) // scale == 0
        {
            auto scaled = static_cast<f64>(current_unsigned_val) / item->scale;
            scaled += static_cast<f64>(item->shift); // add shift
            std::cout << " output  " <<" scaled #2"<<  scaled<< std::endl;
            output = scaled;
        }
        else
        {
            current_unsigned_val += item->shift; // add shift
            current_unsigned_val >>= item->starting_bit_pos;
            output = current_unsigned_val;
            std::cout << " output  " <<" current_unsigned_val #3 "<<  current_unsigned_val<< std::endl;
        }

    }
    //TODO float64
    return raw_data;
}

void decode_bval(bool bval, std::shared_ptr<cfg::map_struct> item, std::stringstream &ss, struct cfg& cfg) {
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


//decode_individual_bits

/// @brief 
/// @param value 
/// @param item 
/// @param ss 
/// @param cfg 
void decode_individual_bits(std::any& value, std::shared_ptr<cfg::map_struct> item, std::stringstream &ss, struct cfg& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    bool firstOne = true;
    ss << "[";
        for (u8 idx = 0; idx < item->bit_str.size(); ++idx){
        if (!(item->bits_unknown & (1<<idx)))
        {
            if(!firstOne)
            {
                ss << ",";
            }
            else
            {
                firstOne = false;
            }
            bool bval = (bool)((val >> idx) & 1UL) == 1UL;//std::any_cast<bool>(input);
            std::string bstr("false");
            if (bval)
                bstr = "true";
            ss << addQuote(item->bit_str[idx]) << ":" << bstr;
        }
    }  
    ss << "]";
}


void decode_bval_from_value(std::any& value, std::shared_ptr<cfg::map_struct> item, std::stringstream &ss, struct cfg& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    auto bval = (val != 0);
    decode_bval(bval, item, ss, cfg);
}        

// TODO add ignored bits
void decode_bit_field(std::any &value, std::shared_ptr<cfg::map_struct> item, std::stringstream &bss, struct cfg& cfg) {
    u64 val = getAnyVal(value, (u64)0) ;
    bool firstOne = true;
    int last_idx = (int)(item->bit_str.size() + 1);
    bss << "[";
    for (int idx = 0; idx < (int)item->bit_str.size(); ++idx){
        //const auto& enum_str = item.bit_str[enum_str_idx];
        bool bval = (bool)((val >> idx) & 1UL) == 1UL;//std::any_cast<bool>(input);
        if (item->bit_str[idx] == "IGNORE") bval = false;
        if (!(item->bits_unknown & (1<<idx)))
        //if (!item->bit_str_known[idx]) // "known" bits that are not ignored:
        {
            if(bval) {
            //if (((val >> idx) & 1UL) == 1UL) {// only format bits that are high:
                if(firstOne) { firstOne = false; } else { bss << ","; }
                bss << "{\"value\":" << idx << ",\"string\":"<< addQuote(item->bit_str[idx]) << "}";
            } 
        } else // "unknown" bits that are "inbetween" bits (and not ignored):
        {
            if (bval) {//((val >> idx) & 1UL) == 1UL) {// only format bits that are high:
                if(firstOne) { firstOne = false; } else { bss << ","; }
                bss << "{\"value\":  " << idx << ",\"string\":"<< addQuote("unknown") << "}";
            }
        }
    }
    for (; last_idx < (int)(item->size * 16); ++last_idx){
        bool bval = (bool)((val >> last_idx) & 1UL) == 1UL;//std::any_cast<bool>(input);
        if (bval) { // only format bits that are high:
            if(firstOne) { firstOne = false; } else { bss << ","; }
            bss << "{\"value\":" << last_idx << ",  \"string\":"<< addQuote("unknown") << "}";
        }
    }
    bss << "]";
}

/// @brief 
/// @param value 
/// @param item 
/// @param bss 
/// @param cfg 
void decode_enum(std::any &value, std::shared_ptr<cfg::map_struct> item, std::stringstream &bss, struct cfg& cfg) {
    bool enum_found = false;
    u64 val = getAnyVal(value, (u64)0) ;
    std::cout << __func__ << " val "  << val << " bitstr size "<< item->bit_str.size() << " bitstr num size "<< item->bit_str_num.size()<< std::endl;
    if (item->bit_str.size() == item->bit_str_num.size())
    {
        for (int idx = 0; idx < (int)(item->bit_str.size()); ++idx){
            std::cout << __func__ << " idx " << (int)idx <<" bit_str_num "  << item->bit_str_num[idx] << std::endl;
            if (val == (u64)item->bit_str_num[idx]){
                enum_found = true;
                bss << "[{\"value\":" << val << ", \"string\":"<< addQuote(item->bit_str[idx]) << "}]";
                break;
            }
        }
    }
    if (!enum_found) {// config did not account for this value: 
        bss << "[{\"value\":" << val << ", \"string\":"<< addQuote("unknown") << "}]";
    }
    std::cout << __func__ << " bss " << bss.str() << std::endl;

}

bool modbus_decode(u16 *raw16, std::shared_ptr<cfg::map_struct> item, std::any& value, std::stringstream &ss, struct cfg& cfg);

void decode_packed(u16 *raw16, std::any &value, std::shared_ptr<cfg::map_struct> item, std::stringstream &bss, struct cfg& cfg) {
    //bool enum_found = false;
    u64 val = getAnyVal(value, (u64)0) ;
    std::cout << __func__ << " val "  << val << " bitstr size "<< item->bit_ranges.size() << std::endl;
    bool firstOne =  true;
    for (auto bitem : item->bit_ranges)
    {
        if(firstOne) { firstOne = false; } else { bss << ","; }
        std::any bvalue = (val>>bitem->starting_bit_pos & ((bitem->number_of_bits*bitem->number_of_bits) -1));

        //std::cout << __func__ << " id "  << bitem->id << std::endl;
        modbus_decode(raw16, bitem, bvalue, bss, cfg);
        //std::cout << __func__ << " string "  << css.str() << std::endl;

        //bss << css.str();
    }
}



/// @brief modbus decode 
////  take the std::any strucure from gcom_decode_any and produce a string from it.

/// @param raw16 
/// @param item 
/// @param value 
/// @param ss 
/// @param cfg 
/// @return 
bool modbus_decode(u16 *raw16, std::shared_ptr<cfg::map_struct> item, std::any& value, std::stringstream &ss, struct cfg& cfg)
{
    if (item->is_bit){
        decode_bval_from_value(value, item, ss, cfg);
        
    } else if (item->use_raw){
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
        std::stringstream bss;
        decode_individual_bits(value, item, bss, cfg);
        ss << addQuote(item->id) << ":"<< bss.str();

    } else if (item->is_bit_field){  
        std::stringstream bss;
        decode_bit_field(value, item, bss, cfg);
        ss << addQuote(item->id) << ":"<< bss.str();

    } else if (item->packed_register){
        std::stringstream bss;
        //bss << addQuote(" this is packed");
        decode_packed(raw16, value, item, bss, cfg);
        ss << bss.str();
    
    } else if (item->is_enum){
        std::stringstream bss;
        decode_enum(value, item, bss, cfg);
        ss << addQuote(item->id) << ":"<< bss.str();
    
    } else if (item->is_float){
        double dval = -12.3;
        float fval = -12.3;
        double val;
        if (testAnyVal(value, dval)) {
            val = getAnyVal(value, dval) ;
        }
        else if (testAnyVal(value, fval)) {
            val = getAnyVal(value, fval) ;
        }
        ss << addQuote(item->id) << ":"<< std::fixed<< std::setprecision(4.7)<< val <<std::defaultfloat;
    } else  {
        ss << addQuote(item->id) << ":"<< getAnyVal(value, (u64)4);    
    }
    return true;
}




// older stuff possible  deprecated
// functions for decoding and encoding (only for Holding and Input registers):
u64 decode_raw(const u16* raw_registers, cfg::map_struct& item, std::any &decode_output)
{
    u64 raw_data = 0UL;

    u64 current_unsigned_val = 0UL;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;

    if (item.size == 1)
    {
        raw_data = raw_registers[0];

        current_unsigned_val = raw_data;
        if (item.uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item.invert_mask;
            // care mask stuff:
            current_unsigned_val &= item.care_mask;
        }
        // do signed stuff (size 1 cannot be float):
        if (item.is_signed)
        {
            s16 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
    }
    else if (item.size == 2 )
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 16) +
                    (static_cast<u64>(raw_registers[1]) <<  0);

        if (!item.is_word_swap) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) + 
                                    (static_cast<u64>(raw_registers[1]) << 16);
        }
        if (item.uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item.invert_mask;
            // care mask stuff:
            current_unsigned_val &= item.care_mask;
        }
        // do signed/float stuff:
        if (item.is_signed)
        {
            s32 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (item.is_float)
        {
            std::cout << " float size 2 Curr unsigned val "<< current_unsigned_val << std::endl;
            f32 to_reinterpret = 0.0f;
            //f32 test_val = 1230.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            //memcpy(&raw_data, &test_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }
    else // size of 4:
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 48) +
                    (static_cast<u64>(raw_registers[1]) << 32) +
                    (static_cast<u64>(raw_registers[2]) << 16) +
                    (static_cast<u64>(raw_registers[3]) <<  0);

        if (!item.is_word_swap) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) +
                                    (static_cast<u64>(raw_registers[1]) << 16) +
                                    (static_cast<u64>(raw_registers[2]) << 32) +
                                    (static_cast<u64>(raw_registers[3]) << 48);
        }
        if (item.uses_masks)
        {
            // invert mask stuff:
            current_unsigned_val ^= item.invert_mask;
            // care mask stuff:
            current_unsigned_val &= item.care_mask;
        }
        // do signed/float stuff:
        if (item.is_signed) // this is only for whole numbers really (signed but not really float):
        {
            s64 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (item.is_float)
        {
            f64 to_reinterpret = 0.0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }

    // do scaling/shift stuff at the end:
    if (item.is_float)
    {
        if (item.scale) // scale != 0
        {
            current_float_val /= item.scale;
        }
        current_float_val += static_cast<f64>(item.shift); // add shift
        decode_output = current_float_val;
    }
    else if (!item.is_signed) // unsigned whole numbers:
    {
        if (!item.scale) // scale == 0
        {
            current_unsigned_val += item.shift; // add shift
            current_unsigned_val >>= item.starting_bit_pos;
            decode_output = current_unsigned_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_unsigned_val) / item.scale;
            scaled += static_cast<f64>(item.shift); // add shift
            decode_output = scaled;
        }
    }
    else // signed whole numbers:
    {
        if (!item.scale) // scale == 0
        {
            current_signed_val += item.shift; // add shift
            current_signed_val >>= item.starting_bit_pos;
            decode_output = current_signed_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_signed_val) / item.scale;
            scaled += static_cast<f64>(item.shift); // add shift
            decode_output = scaled;
        }
    }
    return raw_data;
}

//
// this little chunk of code will decode the registers we get in from the server read into a raw value 
// and then into std::any for pub output
//  we may have to do something different for the bits.  Not sure yet.
// TODO pick register 
u64 gcom_decode_any(u16* raw16, u8*raw8, struct cfg::map_struct* item, std::any& output, struct cfg& myCfg)
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

        if (!item->is_word_swap) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
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

        if (!item->is_word_swap) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
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



// deprecated we use string stream now
//u64 gcom_decode_any(u16* raw16, u8*raw8, struct cfg::map_struct* item, std::any& output, struct cfg& myCfg)
//void decode_to_string(u16* regs16, u8* regs8, struct cfg::map_struct& item, std::any input, fmt::memory_buffer& buf, struct cfg& myCfg)
void decode_to_string(u16* regs16, u8* regs8, struct cfg::map_struct& item, fmt::memory_buffer& buf, struct cfg& myCfg)
                    //  const Jval_buif& to_stringify, 
                    //  fmt::memory_buffer& buf, u8 bit_idx = Bit_All_Idx, 
                    //  Bit_Strings_Info_Array* bit_str_array = nullptr, 
                    //  String_Storage* str_storage = nullptr)
{
//    int key = 1234;
//    int bit_str_idx = 21;
//    std::string bit_str = "hello kitty";
//    fmt::format_to(std::back_inserter(buf), "{}: ", key);
//    fmt::format_to(std::back_inserter(buf),
 //    R"({{"value":{},"string":"{}"}},)",
 //                                                bit_str_idx, 
  //                                               bit_str);

     // questions how do the bits arrive 
    // if (item->reg->u8_buf 
    //poll_work->errors = modbus_read_bits(thread->ctx, poll_work->offset, poll_work->num_registers, poll_work->u8_buff);

     // first we have to add the flags and work out how to decode the bit_strings and the enum strings
    if (item.reg_type == cfg::Register_Types::Coil || item.reg_type == cfg::Register_Types::Discrete_Input)
    {
        bool val = (regs8[0] != 0);
        // what about item inverted
        if (val) {

            fmt::format_to(std::back_inserter(buf), "{}: ", "true");
            //FORMAT_TO_BUF(buf, "{}", val);
        }
        else
        {
            fmt::format_to(std::back_inserter(buf), "{}: ", "false");
        }
    }
    else // Holding and Input:
    {
        if (!item.is_bit_string_type) // normal formatting:
        {
            // we may get these values from the register
            //auto val = std::any_cast<bool>(input);
            bool val = (regs16[0] != 0);
            if (val) {

                fmt::format_to(std::back_inserter(buf), "{}: ", "true");
            }
            else
            {
                fmt::format_to(std::back_inserter(buf), "{}: ", "false");
            }

        }
        else // format using "bit_strings":
        {
            u8 last_idx = 0;
            // to do allow this to span multiple registers as per size
            auto curr_unsigned_val = regs16[0];
            if (item.is_individual_bits) // resolve to multiple uris (true/false per bit):
            {
                // if (bit_idx == Bit_All_Idx) // they want the whole thing
                // {
                //     // POSSIBLE TODO(WALKER): The raw id pub might or might not need to include the binary string as a part of its pub (maybe wrap it in an object?)
                //     FORMAT_TO_BUF(buf, "{}", curr_unsigned_val); // this is the whole individual_bits as a raw number (after basic decoding)
                // }
                // else // they want the individual true/false bit (that belongs to this bit_idx)
                // {
                auto val = ((curr_unsigned_val >> item.bit_index) & 1UL) == 1UL;//std::any_cast<bool>(input);
                fmt::format_to(std::back_inserter(buf), "{}: ", val);
                //    FORMAT_TO_BUF(buf, "{}", ); // this is the individual bit itself (true/false)
                // }
            }
            else if (item.is_bit_field) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
            {
                 buf.push_back('[');
                 for (u8 bit_str_idx = 0; bit_str_idx < item.bit_str.size(); ++bit_str_idx)
                 {
                    const auto& bit_str = item.bit_str[bit_str_idx];
                    last_idx = item.bit_str.size() + 1;
                    // I think we'll preload the unknown bit strings
                    if (!item.bit_str_known[bit_str_idx]) // "known" bits that are not ignored:
                    {
                        if (((curr_unsigned_val >> bit_str_idx) & 1UL) == 1UL) // only format bits that are high:
                        {
                            //FORMAT_TO_BUF(buf, 
                            fmt::format_to(std::back_inserter(buf),
                                    R"({{"value":{},"string":"{}"}},)",
                                                bit_str_idx, 
                                                bit_str);
                        }
                    }
                    else // "unknown" bits that are "inbetween" bits (and not ignored):
                    {
                        if (((curr_unsigned_val >> bit_str_idx) & 1UL) == 1UL) // only format bits that are high:
                        {
                                //FORMAT_TO_BUF(buf,
                            fmt::format_to(std::back_inserter(buf),
                                         R"({{"value":{},"string":"Unknown"}},)",
                                                    bit_str_idx);
                        }
                    }
                 }
            //     // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                 for (; last_idx < (item.size * 16); ++last_idx)
                 {
                    if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
                    {
                        //FORMAT_TO_BUF(buf,
                        fmt::format_to(std::back_inserter(buf),
                                R"({{"value":{},"string":"Unknown"}},)", last_idx);
                    }
                }
                buf.resize(buf.size() - 1); // this gets rid of the last excessive ',' character
                buf.push_back(']'); // finish the array
                
            }
            else if (item.is_enum) // resolve to single string based on current input value (unsigned whole numbers):
            {
                bool enum_found = false;
                for (u8 enum_str_idx = 0; enum_str_idx < item.bit_str.size(); ++enum_str_idx)
                {
                    //const auto& enum_str = item.bit_str[enum_str_idx];
                    if (curr_unsigned_val == item.bit_str_num[enum_str_idx])
                    {
                        enum_found = true;
                        //FORMAT_TO_BUF(buf, 
                        fmt::format_to(std::back_inserter(buf),
                            R"([{{"value":{},"string":"{}"}}])", curr_unsigned_val, item.bit_str[enum_str_idx]);
                        break;
                    }
                }
                if (!enum_found) // config did not account for this value:
                {
                    //FORMAT_TO_BUF(buf, 
                    fmt::format_to(std::back_inserter(buf),
                            R"([{{"value":{},"string":"Unknown"}}])", curr_unsigned_val);
                }
            }
        }

    }
                   // TODO(WALKER): in the future, once everything is done, I can get back to redoing these

                    // else if (current_decode.flags.is_individual_enums()) // resolves to multiple uris:
                    // {
                    //     if (current_msg.bit_id == bit_all_index) // they want the whole thing
                    //     {
                    //         fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), current_unsigned_val); 
                    // send out the whole raw register (all the enums in one)
                    //     }
                    //     else // they want a particular enum inside the bits themselves:
                    //     {
                    //         // only iterate on that particular sub_array (from the beginning using enum_id):
                    //         for (uint8_t j = current_msg.enum_id + 1; j < current_enum_bit_string_array[current_msg.enum_id].last_index + 1; ++j) 
                    // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                    //         {
                    //             auto& enum_str = current_enum_bit_string_array[j];
                    //             const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                    //             if (current_enum_val == enum_str.enum_bit_val) // we found the enum value:
                    //             {
                    //                 fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"{}"}}])"), current_enum_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                    //             }
                    //             else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub_array and it is NOT == our current_enum_val)
                    //             {
                    //                 fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"Unknown"}}])"), current_enum_val);
                    //                 break;
                    //             }
                    //         }
                    //     }
                    // }
                    // else if (current_decode.flags.is_enum_field())
                    // {
                    //     send_buf.push_back('[');
                    //     for (uint8_t j = 0; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                    //     {
                    //         auto& enum_str = current_enum_bit_string_array[j];
                    //         const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                    //         if (current_enum_val == enum_str.enum_bit_val) // we have found the enum
                    //         {
                    //             j = enum_str.last_index; // we have finished iterating over this sub array, go to the next one
                    //             fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"{}"}},)"), 
                    //                                                                         enum_str.begin_bit,
                    //                                                                         enum_str.end_bit,
                    //                                                                         enum_str.care_mask,
                    //                                                                         current_enum_val,
                    //                                                                         main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                    //         }
                    //         else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub array and it is NOT == our current_enum_val and also hasn't been ignored)
                    //         {
                    //             fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"Unknown"}},)"), 
                    //                                                                         enum_str.begin_bit,
                    //                                                                         enum_str.end_bit,
                    //                                                                         enum_str.care_mask,
                    //                                                                         current_enum_val);
                    //         }
                    //     }
                    //     send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                    //     send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                    // }
   // }
}





// given a range of item options and values test the decode operation 
bool test_decode_raw()
{
    cfg::map_struct item;
    u16 raw_registers[4];
    std::any decode_output;
// first the number 1234 no scale no shift etc
    raw_registers[0] = (u16)1234;raw_registers[1] = (u16)0;raw_registers[2] = (u16)0; raw_registers[3] = (u16)0;
    item.scale = 0;
    item.shift = 0;
    item.starting_bit_pos = 0;
    item.is_float = false;
    item.is_signed = false;
    item.is_byte_swap = false;
    item.is_word_swap = false;
    item.invert_mask = 0;
    item.care_mask = 0;
    item.uses_masks = false;
    item.size = 1;
    

    auto res = decode_raw(raw_registers, item, decode_output); anyTypeString(decode_output);
    std::cout << "test decode " << std::hex<< res << std::dec << " decode_output " << anyToString(decode_output) << std::endl;
 
    raw_registers[0] = (u16)0x4499;raw_registers[1] = (u16)0xc000;raw_registers[2] = (u16)3; raw_registers[3] = (u16)4;
    item.is_float = true;
    item.size = 2;

    res = decode_raw(raw_registers, item, decode_output); anyTypeString(decode_output);
    std::cout << "test decode " << std::hex<< res << std::dec << " decode_output " << anyToString(decode_output) << std::endl;

    return true;

}
// To convert a `uint64_t` to `int64_t` in C++, you can simply assign the unsigned value to a signed variable. However, you should be cautious about this operation since you might face issues when the `uint64_t` value is too large to fit into an `int64_t` without overflowing. 

// Here’s a simple example:

// ```cpp
// #include <iostream>
// #include <cstdint> // for std::uint64_t, std::int64_t

// int main() {
//     std::uint64_t unsignedValue = 12345678901234567890u;
//     std::int64_t signedValue;

//     if (unsignedValue <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
//         signedValue = static_cast<std::int64_t>(unsignedValue);
//         std::cout << "Converted value: " << signedValue << std::endl;
//     } else {
//         std::cerr << "Warning: Conversion would overflow. Cannot convert." << std::endl;
//     }

//     return 0;
// }
// ```

// Explanation:

// 1. The `if` condition checks if `unsignedValue` is within the valid range for `int64_t`. `std::numeric_limits<std::int64_t>::max()` provides the maximum value that `int64_t` can hold.

// 2. If `unsignedValue` is within the range, it gets cast to `int64_t` and stored in `signedValue`.

// 3. If the unsigned value is too large, a warning message is printed, and no conversion is performed to avoid overflow. You need to decide how to handle such cases in your actual application.

// Remember to include the necessary headers, and if your environment supports C++11 or later, use the `<cstdint>` header instead of `<stdint.h>`.
// this is walkers stuff I think we can make this work
//fmt::memory_buffer send_buf
// if (work_qs.pub_q.try_pop(pub_work)) { // IO_threads produce onto this for main to consume
//             // TODO(WALKER): Do pub work here (jsonify, etc.)
//             auto& comp_workspace = *client_workspace.msg_staging_areas[pub_work.comp_idx].comp_workspace;
//             auto& decoded_cache = comp_workspace.decoded_caches[pub_work.thread_idx];
//             bool is_heartbeat_mapping = comp_workspace.heartbeat_enabled && comp_workspace.heartbeat_read_thread_idx == pub_work.thread_idx;

//             // TODO(WALKER): Make sure that heartbeat is taken into account in the case of an errno for this?
//             // probably just want to move the heartbeat code into this really?
//             auto print_poll_if_finished = [&]() {
//                 if(pub_work.errno_code != 0) 
//                     NEW_FPS_ERROR_PRINT("Component error while polling [] -> {} \n", pub_work.errno_code, modbus_strerror(pub_work.errno_code));
//                 if (--comp_workspace.num_polls_in_q_left == 0) {
//                     // reset our error checking variables real quick:
//                     if (!comp_workspace.has_error_checked_already) comp_workspace.num_poll_errors = 0; // only reset if we had no errors this cycle
//                     comp_workspace.has_error_checked_already = false;
//                     // time to publish:
//                     bool has_one_change = false;
//                     send_buf.clear();
//                     if( pub_work.errno_code == 0)
//                     {
//                     send_buf.push_back('{'); // begin object
//                     for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
//                     {
//                         auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];

//                         // if (curr_decoded_cache.changed_mask.none()) continue; // skip stringifying caches that have no changes
//                         has_one_change = true;

//                         if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 // if (!curr_decoded_cache.changed_mask[decoded_idx]) continue; // skip values that have no changes

//                                 auto& curr_decoded_info = curr_decoded_cache.decoded_vals[decoded_idx];
//                                 if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
//                                 {
//                                     FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), 
//                                                                                     curr_decoded_info.decoded_val);
//                                 }
//                                 else // format using "bit_strings":
//                                 {
//                                     const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
//                                     const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

//                                     if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
//                                     {
//                                         for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
//                                         {
//                                             const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
//                                             // if (((curr_bit_str_info.changed_mask >> bit_str.begin_bit) & 1UL) != 1UL) continue; // skip bits that have not changed since last time
//                                             FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(bit_str.str), 
//                                                                      ((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
//                                         }
//                                     }
//                                     else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
//                                     {
//                                         FORMAT_TO_BUF(send_buf, R"("{}":[)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]));
//                                         bool has_one_bit_high = false;
//                                         u8 last_idx = 0;
//                                         for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
//                                         {
//                                             const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
//                                             last_idx = bit_str.end_bit + 1;
//                                             if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
//                                             {
//                                                 if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
//                                                 {
//                                                     has_one_bit_high = true;
//                                                     FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
//                                                                         bit_str.begin_bit, 
//                                                                         str_storage.get_str(bit_str.str));
//                                                 }
//                                             }
//                                             else // "unknown" bits that are "inbetween" bits (and not ignored):
//                                             {
//                                                 for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
//                                                 {
//                                                     if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
//                                                     {
//                                                         has_one_bit_high = true;
//                                                         FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
//                                                     }
//                                                 }
//                                             }
//                                         }
//                                         // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
//                                         const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
//                                         for (; last_idx < num_bits; ++last_idx)
//                                         {
//                                             if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
//                                             {
//                                                 has_one_bit_high = true;
//                                                 FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
//                                             }
//                                         }
//                                         send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
//                                         send_buf.append("],"sv); // finish the array
//                                     }
//                                     else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
//                                     {
//                                         bool enum_found = false;
//                                         for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
//                                         {
//                                             const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
//                                             if (curr_unsigned_val == enum_str.enum_val)
//                                             {
//                                                 enum_found = true;
//                                                 FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"{}"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val, str_storage.get_str(enum_str.str));
//                                                 break;
//                                             }
//                                         }
//                                         if (!enum_found) // config did not account for this value:
//                                         {
//                                             FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"Unknown"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val);
//                                         }
//                                     }
//                                     // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
//                                 }
//                             }
//                         }
//                         else // Coil and Discrete Input:
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 // if (!curr_decoded_cache.changed_mask[decoded_idx]) continue; // skip values that have no changes
//                                 FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), 
//                                                            curr_decoded_cache.bool_vals[decoded_idx]);
//                             }
//                         }
//                     }
//                     // do heartbeat stuff here if we have it:
//                     // TODO(WALKER): Finish this up when you do heartbeat stuff
//                     if (comp_workspace.heartbeat_enabled)
//                     {
//                         has_one_change = true;
//                         const auto curr_decoded_val = comp_workspace.decoded_caches[comp_workspace.heartbeat_read_thread_idx].decoded_vals[comp_workspace.heartbeat_read_decode_idx].decoded_val;
//                         FORMAT_TO_BUF(send_buf, R"("modbus_heartbeat":{},"component_connected":{},)", curr_decoded_val, comp_workspace.component_connected);
//                     }

//                     // pub out the strings if we have changes:
//                     if (has_one_change)
//                     {
//                         // "Timestamp" stuff:
//                         const auto timestamp       = std::chrono::system_clock::now();
//                         const auto timestamp_micro = time_fraction<std::chrono::microseconds>(timestamp);
//                         FORMAT_TO_BUF_NO_COMPILE(send_buf, R"("Timestamp":"{:%m-%d-%Y %T}.{}"}})", timestamp, timestamp_micro.count());

//                         // send_buf.resize(send_buf.size() - 1); // This gets rid of the last excessive ',' character
//                         // send_buf.push_back('}');
//                         if (!send_pub(fims_gateway, str_storage.get_str(comp_workspace.comp_uri), std::string_view{send_buf.data(), send_buf.size()}))
//                         {
//                             NEW_FPS_ERROR_PRINT_NO_ARGS("Main, cannot publish onto fims, exiting\n");
//                             return false;
//                         }
//                     }
//                     }
//                     // reset variables for polling:
//                     comp_workspace.is_polling = false;
//                     comp_workspace.poll_time_left = decoded_cache.thread_workspace->frequency;
//                     comp_workspace.last_poll_time = mono_clock.rdns();
//                 }
//                 return true;
//             };

//             // record response times:
//             if (comp_workspace.min_response_time > pub_work.response_time) { comp_workspace.min_response_time = pub_work.response_time; }
//             if (comp_workspace.max_response_time < pub_work.response_time) { comp_workspace.max_response_time = pub_work.response_time; }
//             const auto prev_avg = comp_workspace.avg_response_time * comp_workspace.num_timings_recorded;
//             ++comp_workspace.num_timings_recorded;
//             comp_workspace.avg_response_time = (prev_avg + pub_work.response_time) / comp_workspace.num_timings_recorded;
//             // process pub:
//             if (pub_work.errno_code) {
//                 //104 -> "Connection reset by peer"
//                 //32 -> "Broken pipe"
//                 //110 -> "Connection Timed Out"
//                 //115 -> "Operation now in pogress"
//                 NEW_FPS_ERROR_PRINT("Component \"{}\", map array #{} had an error while polling, it is {} -> \"{}\"\n",
//                       str_storage.get_str(comp_workspace.comp_uri), pub_work.thread_idx, pub_work.errno_code, modbus_strerror(pub_work.errno_code));
//                 if ((pub_work.errno_code == 104) || (pub_work.errno_code == 32))
//                 {
//                     // no point in trying again
//                     return false;
//                 }

//                 if ((pub_work.errno_code != 115))
//                 {
//                     if (!comp_workspace.has_error_checked_already) {
//                         comp_workspace.has_error_checked_already = true;
//                         ++comp_workspace.num_poll_errors;
//                         if (comp_workspace.num_poll_errors >= 5) {
//                             NEW_FPS_ERROR_PRINT("Component \"{}\" had 5 or more poll error cycles in a row. Exiting\n", str_storage.get_str(comp_workspace.comp_uri));
//                             return false;
//                         }
//                     }
//                 }
//                 if (!print_poll_if_finished()) return false;
//                 continue;
//             }
//             switch(decoded_cache.reg_type) {
//                 case Register_Types::Holding: // fallthrough
//                 case Register_Types::Input: {
//                     for (std::size_t i = 0; i < pub_work.pub_vals.size(); ++i) {
//                         auto& prev_val = decoded_cache.decoded_vals[i];
//                         auto& new_val = pub_work.pub_vals[i];
//                         prev_val.raw_data = new_val.raw_data;
//                         if (prev_val.bit_str_array_idx == Bit_Str_All_Idx) { // regular if changed check:
//                             decoded_cache.changed_mask[i] = prev_val.decoded_val.u != new_val.decoded_val.u;
//                         } else { // masking required:
//                         // PHIL TO CHECK this with Jack S
//                         //auto curr_unsigned_val = val.decoded_val.get_uint_unsafe(); val.decoded_val = curr_unsigned_val;
//                             auto& bit_str_info = decoded_cache.bit_strings_arrays[prev_val.bit_str_array_idx];
//                             const auto temp_val = new_val.decoded_val.u & bit_str_info.care_mask;
//                             decoded_cache.changed_mask[i] = (prev_val.decoded_val.u & bit_str_info.care_mask) != temp_val;
//                         }
//                         // heartbeat stuff:
//                         if (is_heartbeat_mapping && comp_workspace.heartbeat_read_decode_idx == i) {
//                             if (decoded_cache.changed_mask[i]) {
//                                 if (comp_workspace.component_connected == false) {
//                                     if (!emit_event(send_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Alarm,
//                                             R"(Heartbeat for component "{}" reconnected.)", str_storage.get_str(comp_workspace.comp_uri)))
//                                     {
//                                         NEW_FPS_ERROR_PRINT_NO_ARGS("Cannot send an event out on fims. Exiting\n");
//                                         return false;
//                                     }
//                                 }
//                                 comp_workspace.component_connected = true;
//                                 comp_workspace.heartbeat_last_change_time = mono_clock.rdns();
//                             }
//                             // now send out the heartbeat_val + 1 to be set (if we have a write idx):
//                             if (comp_workspace.heartbeat_write_thread_idx != Thread_All_Idx) {
//                                 Set_Work heartbeat_set;
//                                 auto heartbeat_val = new_val.decoded_val;
//                                 ++heartbeat_val.u;
//                                 heartbeat_set.set_vals.emplace_back(Set_Info{heartbeat_val, heartbeat_val.u, pub_work.comp_idx, comp_workspace.heartbeat_write_thread_idx, comp_workspace.heartbeat_write_decode_idx}); // NOTE(WALKER): heartbeat registers can't have bit_strings stuff anyway, so bit_idx and enum_idx don't matter
//                                 if (!io_work_qs.set_q.try_emplace(std::move(heartbeat_set))) {
//                                     // TODO(WALKER): Error print here
//                                     NEW_FPS_ERROR_PRINT_NO_ARGS("sets are backed up and this should NEVER happen. Exiting\n");
//                                     return false;
//                                 }
//                                 io_work_qs.signaller.signal(); // tell IO threads there is work to do
//                             }
//                         }
//                         if (prev_val.bit_str_array_idx != Bit_Str_All_Idx) { // to tell which bits changed since last time:
//                             auto& bit_str_info = decoded_cache.bit_strings_arrays[prev_val.bit_str_array_idx];
//                             bit_str_info.changed_mask = (prev_val.decoded_val.u ^ new_val.decoded_val.u) & bit_str_info.care_mask;
//                         }
//                         prev_val.decoded_val = new_val.decoded_val;
//                     }
//                     if (is_heartbeat_mapping) {
//                         if (comp_workspace.heartbeat_timeout > std::chrono::nanoseconds{mono_clock.rdns() - comp_workspace.heartbeat_last_change_time}) {
//                             // TODO(WALKER): get the "rising edge" print in here on heartbeat disconnect
//                             if (comp_workspace.component_connected == true) {
//                                 if (!emit_event(send_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Alarm,
//                                         R"(Heartbeat for component "{}" disconnected.)", str_storage.get_str(comp_workspace.comp_uri)))
//                                 {
//                                     NEW_FPS_ERROR_PRINT_NO_ARGS("Cannot send an event out on fims. Exiting\n");
//                                     return false;
//                                 }
//                             }
//                             comp_workspace.component_connected = false;
//                         }
//                     }
//                     break;
//                 }
//                 case Register_Types::Coil: // fallthrough
//                 default: { // Discrete_Input
//                     for (std::size_t i = 0; i < pub_work.pub_vals.size(); ++i) {
//                         const auto prev_val = decoded_cache.bool_vals[i];
//                         const auto& new_val = pub_work.pub_vals[i];
//                         decoded_cache.changed_mask[i] = prev_val != new_val.decoded_val.u;
//                     }
//                 }
//             }
//             // then if we are on the last pub to come back publish it then set the current time as the last_poll_time:
//             if (!print_poll_if_finished()) return false;
//             continue;
//         }  // end of pubs
//         // then gets:
//         if (try_pop(work_qs.get_q, get_work)) {
//             auto& comp_workspace = *client_workspace.msg_staging_areas[get_work.comp_idx].comp_workspace;
//             send_buf.clear();
//             if (get_work.thread_idx == Thread_All_Idx) // They want everything:
//             {
//                 send_buf.push_back('{');
//                 if (!get_work.flags.flags) // normal get (no flags set):
//                 {
//                     for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
//                     {
//                         auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];
//                         if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 auto& curr_decoded_info = curr_decoded_cache.decoded_vals[decoded_idx];
//                                 if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
//                                 {
//                                     FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_info.decoded_val);
//                                 }
//                                 else // format using "bit_strings":
//                                 {
//                                     const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
//                                     const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

//                                     if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
//                                     {
//                                         for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
//                                         {
//                                             const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
//                                             FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(bit_str.str), ((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
//                                         }
//                                     }
//                                     else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
//                                     {
//                                         FORMAT_TO_BUF(send_buf, R"("{}":[)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]));
//                                         bool has_one_bit_high = false;
//                                         u8 last_idx = 0;
//                                         for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
//                                         {
//                                             const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
//                                             last_idx = bit_str.end_bit + 1;
//                                             if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
//                                             {
//                                                 if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
//                                                 {
//                                                     has_one_bit_high = true;
//                                                     FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
//                                                                         bit_str.begin_bit, 
//                                                                         str_storage.get_str(bit_str.str));
//                                                 }
//                                             }
//                                             else // "unknown" bits that are "inbetween" bits (and not ignored):
//                                             {
//                                                 for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
//                                                 {
//                                                     if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
//                                                     {
//                                                         has_one_bit_high = true;
//                                                         FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
//                                                     }
//                                                 }
//                                             }
//                                         }
//                                         // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
//                                         const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
//                                         for (; last_idx < num_bits; ++last_idx)
//                                         {
//                                             if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
//                                             {
//                                                 has_one_bit_high = true;
//                                                 FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
//                                             }
//                                         }
//                                         send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
//                                         send_buf.append("],"sv); // finish the array
//                                     }
//                                     else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
//                                     {
//                                         bool enum_found = false;
//                                         for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
//                                         {
//                                             const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
//                                             if (curr_unsigned_val == enum_str.enum_val)
//                                             {
//                                                 enum_found = true;
//                                                 FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"{}"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val, str_storage.get_str(enum_str.str));
//                                                 break;
//                                             }
//                                         }
//                                         if (!enum_found) // config did not account for this value:
//                                         {
//                                             FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"Unknown"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val);
//                                         }
//                                     }
//                                     // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
//                                 }
//                             }
//                         }
//                         else // Coil and Discrete Input:
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_cache.bool_vals[decoded_idx]);
//                             }
//                         }
//                     }
//                 }
//                 else if (get_work.flags.is_raw_request()) // raw get: 
//                 {
//                     for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
//                     {
//                         const auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];
//                         if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 const auto& curr_decode_info = curr_decoded_cache.decoded_vals[decoded_idx];
//                                 FORMAT_TO_BUF(send_buf, R"("{0}":{{"value":{1},"binary":"{1:0>{2}b}","hex":"{1:0>{3}X}"}},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decode_info.raw_data, curr_decode_info.flags.get_size() * 16, curr_decode_info.flags.get_size() * 4);
//                             }
//                         }
//                         else // Coil and Discrete Input:
//                         {
//                             for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
//                             {
//                                 FORMAT_TO_BUF(send_buf, R"("{0}":{{"value":{1},"binary":"{1:b}","hex":"{1:X}"}},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_cache.bool_vals[decoded_idx]);
//                             }
//                         }
//                     }
//                 }
//                 else // timings requests (could also be a reset):
//                 {
//                     if (get_work.flags.is_reset_timings_request())
//                     {
//                         comp_workspace.min_response_time = timings_duration_type{std::numeric_limits<f64>::max()};
//                         comp_workspace.max_response_time = timings_duration_type{0.0};
//                         comp_workspace.avg_response_time = timings_duration_type{0.0};
//                         comp_workspace.num_timings_recorded = 0.0;
//                         continue; // don't send anything out
//                     }
//                     FORMAT_TO_BUF(send_buf, R"("min_response_time":"{}","max_response_time":"{}","avg_response_time":"{}","num_timings_recorded":{},)", 
//                         comp_workspace.min_response_time, comp_workspace.max_response_time, comp_workspace.avg_response_time, comp_workspace.num_timings_recorded);
//                 }
//                 send_buf.resize(send_buf.size() - 1); // gets rid of the last excessive ',' character
//                 send_buf.push_back('}');
//             }
//             else // They want a single variable (no timings requests, no key -> POSSIBLE TODO(WALKER): do they want it "clothed", I assume unclothed for now, maybe have a config option for this?):
//             {
//                 // NOTE(WALKER): the check for single get timings request is done in listener thread.
//                 auto& curr_decoded_cache = comp_workspace.decoded_caches[get_work.thread_idx];
//                 if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
//                 {
//                     auto& curr_decoded_info = curr_decoded_cache.decoded_vals[get_work.decode_idx];
//                     if (!get_work.flags.flags) // normal get (no flags):
//                     {
//                         if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
//                         {
//                             FORMAT_TO_BUF(send_buf, R"({})", curr_decoded_info.decoded_val);
//                         }
//                         else // format using "bit_strings":
//                         {
//                             const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
//                             const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

//                             if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
//                             {
//                                 FORMAT_TO_BUF(send_buf, R"({})", ((curr_unsigned_val >> get_work.bit_idx) & 1UL) == 1UL);
//                             }
//                             else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
//                             {
//                                 send_buf.push_back('[');
//                                 bool has_one_bit_high = false;
//                                 u8 last_idx = 0;
//                                 for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
//                                 {
//                                     const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
//                                     last_idx = bit_str.end_bit + 1;
//                                     if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
//                                     {
//                                         if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
//                                         {
//                                             has_one_bit_high = true;
//                                             FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
//                                                                 bit_str.begin_bit, 
//                                                                 str_storage.get_str(bit_str.str));
//                                         }
//                                     }
//                                     else // "unknown" bits that are "inbetween" bits (and not ignored):
//                                     {
//                                         for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
//                                         {
//                                             if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
//                                             {
//                                                 has_one_bit_high = true;
//                                                 FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
//                                             }
//                                         }
//                                     }
//                                 }
//                                 // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
//                                 const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
//                                 for (; last_idx < num_bits; ++last_idx)
//                                 {
//                                     if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
//                                     {
//                                         has_one_bit_high = true;
//                                         FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
//                                     }
//                                 }
//                                 send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
//                                 send_buf.push_back(']'); // finish the array
//                             }
//                             else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
//                             {
//                                 bool enum_found = false;
//                                 for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
//                                 {
//                                     const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
//                                     if (curr_unsigned_val == enum_str.enum_val)
//                                     {
//                                         enum_found = true;
//                                         FORMAT_TO_BUF(send_buf, R"([{{"value":{},"string":"{}"}}])", curr_unsigned_val, str_storage.get_str(enum_str.str));
//                                         break;
//                                     }
//                                 }
//                                 if (!enum_found) // config did not account for this value:
//                                 {
//                                     FORMAT_TO_BUF(send_buf, R"([{{"value":{},"string":"Unknown"}}])", curr_unsigned_val);
//                                 }
//                             }
//                             // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
//                         }
//                     }
//                     else if (get_work.flags.is_raw_request()) // they want raw data (format it):
//                     {
//                         FORMAT_TO_BUF(send_buf, R"({{"value":{0},"binary":"{0:0>{1}b}","hex":"{0:0>{2}X}"}})", curr_decoded_info.raw_data, curr_decoded_info.flags.get_size() * 16, curr_decoded_info.flags.get_size() * 4);
//                     }
//                 }
//                 else // Coil and Discrete Input:
//                 {
//                     const auto curr_bool_val = curr_decoded_cache.bool_vals[get_work.decode_idx];
//                     if (!get_work.flags.flags) // normal get (no flags):
//                     {
//                         FORMAT_TO_BUF(send_buf, R"({})", curr_bool_val);
//                     }
//                     else if (get_work.flags.is_raw_request()) // they want raw data (format it):
//                     {
//                         FORMAT_TO_BUF(send_buf, R"({{"value":{0},"binary":"{0:b}","hex":"{0:X}"}})", curr_bool_val);
//                     }
//                 }
//             }
//             // Send out using replyto here:
//             if (!send_set(fims_gateway, get_work.replyto, std::string_view{send_buf.data(), send_buf.size()}))
//             {
//                 NEW_FPS_ERROR_PRINT("could not send reply to uri \"{}\" over fims, exiting\n", get_work.replyto);
//                 return false;
//             }
//             continue;
//         }