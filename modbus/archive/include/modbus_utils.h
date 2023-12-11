#ifndef _MODBUS_UTILS_H_
#define _MODBUS_UTILS_H_

#include <cjson/cJSON.h>
#include <map>

enum Type_of_Register {
    Coil, Discrete_Input, Input_Register, Holding_Register, Num_Register_Types
};

const char *getRegisterType(Type_of_Register idx);


typedef struct maps_t{
    unsigned int    reg_off;
    unsigned int    num_regs;
    unsigned int    num_strings;
    int    shift;
    double scale;
    char*  reg_name;
    char*  uri;
    bool   sign;
    bool   floating_pt;
    bool   is_bool;
    bool   bit_field;
    bool   individual_bits;
    uint64_t data; // used only for individual_bits, read-modify-write construct
    uint64_t invert_mask;
    bool   enum_type;
    bool   random_enum_type;
    char ** bit_strings;
    std::map<int,char*> random_enum;

    int reg_type;
    bool byte_swap;
    bool use_byte_swap;

    maps_t()
    {
        reg_type = Num_Register_Types;
        reg_name = uri = NULL;
        bit_strings = NULL;
        reg_off = num_regs = num_strings = invert_mask = 0;
        scale = 0.0;
        shift = 0;
        sign = floating_pt = is_bool = false;
        bit_field = individual_bits = enum_type = random_enum_type = false;
        data = 0;
    }
    ~maps_t()
    {
        if(reg_name != NULL)
            free (reg_name);
        if(uri != NULL)
            free (uri);
        if(bit_strings != NULL)
        {
            for(unsigned int i = 0; i < num_strings; i++)
                 if(bit_strings[i] != NULL) free(bit_strings[i]);
            delete [] bit_strings;
        }
    }
} maps;

//Holds all the information related to registers of a single type whose value needs to be read
typedef struct {
    Type_of_Register reg_type;
    unsigned int start_offset; // the offset of the first variable that uses this register type
    unsigned int num_regs; // number of registers of this datalog's register type
    unsigned int map_size; // number of variables in the register map (a variable could possibly be made of multiple registers)
    maps *register_map;
} datalog;

void emit_event(fims* pFims, const char* source, const char* message, int severity);
cJSON* get_config_json(int argc, char* argv[]);

#define FORMAT_TO_BUF(fmt_buf, fmt_str, ...) fmt::format_to(std::back_inserter(fmt_buf), FMT_COMPILE(fmt_str), ##__VA_ARGS__)

#endif
