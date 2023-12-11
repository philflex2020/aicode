#include <iostream>
#include <any>

#include "gcom_config.h"
#include "shared_utils.hpp"



//
// this little chunk of code will decode the std::any thing we got in from fims into the raw_registers.
//  we may have to do something different for the bits.  Not sure yey.
// I'll follow this with a raw decode dn then a fims decode 
//
bool gcom_encode_any(u16* raw16, u8 * raw8, struct cfg::map_struct& item, std::any input, struct cfg& myCfg)
{
    // the item already has the input from the fims message we'll provide the item_map anyway
    // lets review the options
    // simple  encode an int int 1,2 or 4 registers.
    // Helper functions to get values from std::any
    bool err = false;

    u64 current_unsigned_val64 = 0;
    s64 current_signed_val64 = 0;

    u32 current_unsigned_val32 = 0;
    s32 current_signed_val32 = 0;
    f64 current_float_val = 0.0;

    if(item.normal_set)
    {
        if(input.type() == typeid(u64)) {
            current_unsigned_val64 = std::any_cast<u64>(input);
            current_unsigned_val64 <<= item.starting_bit_pos;
            current_unsigned_val64 -= item.shift;
            if (item.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_unsigned_val64);
                current_float_val *= item.scale;
            }
        }
        else if(input.type() == typeid(s64)) {
            current_signed_val64 = std::any_cast<s64>(input);
            current_signed_val64 <<= item.starting_bit_pos;
            current_signed_val64 -= item.shift;
            if (item.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_signed_val64);
                current_float_val *= item.scale;            
            }
        }
        else if(input.type() == typeid(u32)) {
            current_unsigned_val32 = std::any_cast<u32>(input);
            current_unsigned_val32 <<= item.starting_bit_pos;
            current_unsigned_val32 -= item.shift;
            if (item.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_unsigned_val32);
                current_float_val *= item.scale;
            }
        }
        else if(input.type() == typeid(s32)) {
            current_signed_val32 = std::any_cast<s32>(input);
            current_signed_val32 <<= item.starting_bit_pos;
            current_signed_val32 -= item.shift;
            if (item.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_signed_val32);
                current_float_val *= item.scale;            
            }
        }
        else if(input.type() == typeid(f64)) {
            current_float_val = std::any_cast<f64>(input);
                // TODO: shifting floats?
            //current_float_val -= static_cast<f64>(item.shift);
            if (item.scale) // scale != 0
            {
                current_float_val *= item.scale;
            }
        }
        else if(input.type() == typeid(f32)) {
            current_float_val = static_cast<f64>(std::any_cast<f32>(input));
                // TODO: shifting floats?
            //current_float_val -= static_cast<f64>(item.shift);
            if (item.scale) // scale != 0
            {
                current_float_val *= item.scale;
            }
        }
        else if(input.type() == typeid(bool)) {
            // like dnp3 use scal to invert
            int def_tval = 1;
            int def_fval = 0;
            if (item.scale < 0) // scale != 0
            {
                def_tval = 0;
                def_fval = 1;
            }

            if (std::any_cast<bool>(input))
                current_signed_val32 = def_tval;
            else
                current_signed_val32 = def_fval;
        }
        else // whateva
        {
            // TODO show name and type
            std::cout << " unknown encode type " << std::endl; 
        }
    } 
    else
    {
        //current_unsigned_val = (*prev_unsigned_val & ~(bit_str_care_mask << bit_id)) | (to_encode.get_uint() << bit_id);
        //*prev_unsigned_val = current_unsigned_val; // make sure that we keep track of the bits that we just set for sets using 
                                                            // prev_unsigned_val in the future (so we don't change them back on successive sets)
        // TODO show name ant type
        std::cout << " only normal set allowed so far " << std::endl; 

    }
        // set registers based on size:
    if (item.size == 1)
    {
        u16 current_u16_val = 0;
        if (input.type() == typeid(u32) && !item.scale)
        {
            current_u16_val = static_cast<u16>(current_unsigned_val32);
        }
        else if (input.type() == typeid(s32) && !item.scale)
        {
            current_u16_val = static_cast<u16>(current_signed_val32);
        }
        else if (input.type() == typeid(bool))
        {
            current_u16_val = static_cast<u16>(current_signed_val32);
        }
        else // float (or sets with scale enabled):
        {
            if (item.is_signed) // signed:
            {
                current_u16_val = static_cast<u16>(static_cast<s16>(current_float_val));
            }
            else // unsigned signed:
            {
                current_u16_val = static_cast<u16>(current_float_val);
            }
            
        }
        current_u16_val ^= static_cast<u16>(item.invert_mask); // invert before byte swapping and sending out
        raw16[0] = static_cast<u16>(current_u16_val);
    }
    else if (item.size == 2)
    {
        u32 current_u32_val = 0U;
        if (input.type() == typeid(u32) && !item.scale)
        {
            if (!item.is_float) // this also counts for "signed"
            {
                current_u32_val = static_cast<u32>(current_unsigned_val32);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<f32>(current_unsigned_val32);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else if (input.type() == typeid(s32) && !item.scale)
        {
            if (!item.is_float)
            {
                current_u32_val = static_cast<u32>(current_signed_val32);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<f32>(current_signed_val32);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else if (input.type() == typeid(bool))
        {
            current_u32_val = static_cast<u16>(current_signed_val32);
        }
        else // float (or sets with scale enabled):
        {
            if (item.is_float)
            {
                const auto current_float32_val = static_cast<f32>(current_float_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
            else
            {
                if (item.is_signed)
                {
                    current_u32_val = static_cast<u32>(static_cast<s32>(current_float_val));
                }
                else // unsigned:
                {
                    current_u32_val = static_cast<u32>(current_float_val);
                }
            }
        }
        current_u32_val ^= static_cast<u32>(item.invert_mask); // invert before word swapping and sending out
        if (item.is_word_swap)
        {
            raw16[0] = static_cast<u16>(current_u32_val >>  0);
            raw16[1] = static_cast<u16>(current_u32_val >> 16);
        }
        else
        {
            raw16[0] = static_cast<u16>(current_u32_val >> 16);
            raw16[1] = static_cast<u16>(current_u32_val >>  0);
        }
    }
    else // size 4:
    {
        u64 current_u64_val = 0UL;
        if (input.type() == typeid(u32) && !item.scale)
        {
            if (!item.is_float)
            {
                current_u64_val = current_unsigned_val32;
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<f64>(current_unsigned_val32);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_u64_val));
            }
        }
        else if (input.type() == typeid(s32) && !item.scale)
        {
            if (!item.is_float)
            {
                current_u64_val = current_signed_val32;
                //const auto current_float64_val = static_cast<f64>(current_signed_val32);
                //printf(" playing with int value %d  float %f \n", current_signed_val32 ,current_float64_val);
                //memcpy(&current_u64_val, &current_float64_val, sizeof(current_float64_val));
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<f64>(current_signed_val32);
                //printf(" playing with int value %d  float %f \n", current_signed_val32 ,current_float64_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_float64_val));
                // char * c;
                // c  = (char *) &current_u64_val;
                // for ( int i = 0 ; i < (int)sizeof(current_u64_val); i++)
                // {
                //     printf( " [%d] [%02x]  ", i, c[i]);
                // }
                //printf( " \n");
            }
        }
        else if (input.type() == typeid(bool))
        {
            if (!item.is_float64)
            {
                current_u64_val = current_signed_val32;
            }
            else
            {
                const auto current_float64_val = static_cast<f64>(current_signed_val32);
                //printf(" playing with int value %d  float %f \n", current_signed_val32 ,current_float64_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_float64_val));
                //current_u64_val = static_cast<u16>(current_signed_val32);
            }
        }
        else if (input.type() == typeid(f64) && !item.scale)
        {
            if (item.is_float64)
            {
                memcpy(&current_u64_val, &current_float_val, sizeof(current_float_val));
            }
            else 
            {
                if (!item.is_signed)
                {
                    current_u64_val = static_cast<u64>(current_float_val);
                }
                else // signed:
                {
                    current_u64_val = static_cast<u64>(static_cast<s64>(current_float_val));
                }
            }
        }
        else if (input.type() == typeid(f32) && !item.scale)
        {
            if (item.is_float64)
            {
                memcpy(&current_u64_val, &current_float_val, sizeof(current_float_val));
            }
            else 
            {
                if (!item.is_signed)
                {
                    current_u64_val = static_cast<u64>(current_float_val);
                }
                else // signed:
                {
                    current_u64_val = static_cast<u64>(static_cast<s64>(current_float_val));
                }
            }   
        }
        else
        {

        }
        current_u64_val ^= item.invert_mask;
        if (!item.is_word_swap)
        {
            raw16[0] = static_cast<u16>(current_u64_val >> 48);
            raw16[1] = static_cast<u16>(current_u64_val >> 32);
            raw16[2] = static_cast<u16>(current_u64_val >> 16);
            raw16[3] = static_cast<u16>(current_u64_val >>  0);
        }
        else // word swap:
        {
            raw16[0] = static_cast<u16>(current_u64_val >>  0);
            raw16[1] = static_cast<u16>(current_u64_val >> 16);
            raw16[2] = static_cast<u16>(current_u64_val >> 32);
            raw16[3] = static_cast<u16>(current_u64_val >> 48);
        }
        if (item.is_float64)
        {
            raw16[0] = static_cast<u16>(current_u64_val >>  0);
            raw16[1] = static_cast<u16>(current_u64_val >> 16);
            raw16[2] = static_cast<u16>(current_u64_val >> 32);
            raw16[3] = static_cast<u16>(current_u64_val >> 48);

        }

    }


    return err;

}


void gcom_test_encode_base() {
    u16 regs16[4];
    u8 regs8[4];
    cfg::map_struct item;
    struct cfg myCfg;

    u32 inputValue1 = 0xDEADBEEF;

    //u64 inputValue1_64 = 0xDEADBEEF;
    u32 inputValue2  = 1234 ;
    s32 sinputValue1 = 4321;
    s32 sinputValue2 = 4321;
    f32 fin32_1 = 23.45;
    f32 fin32_2 = -23.45;
    bool bin1 = true;
    bool bin2 = false;
    // Test case 1: encode an unsigned 32-bit integer
    item.starting_bit_pos=0; item.shift = 0; item.scale = 0.0; 
    item.normal_set = true;

    item.is_signed = false;  
    item.invert_mask = 0; 
    item.is_float = false; 
    item.is_word_swap = false; 
    item.is_float64 = false;

    item.size = 1;
    printf("==========size 1=========\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8,  item, std::any(inputValue2), myCfg);
    printf ("  u32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8,  item, std::any(sinputValue1), myCfg);
    printf ("  s32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->1 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->1 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->1  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->1  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    item.is_float = true;
    printf("==========size 1======is_float=\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->1  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->1 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->1 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->1  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->1  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );


    item.size = 2;
    item.is_float = false;
    printf("==========size 2=========\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    item.size = 2;
    item.scale = 2.5;
    printf("==========size 2  scale 2.5 =========\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );


    item.size = 2;
    item.scale = -2.5;
    printf("==========size 2  scale -2.5 =========\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );


    item.size = 2;
    item.is_float = true;
    item.scale = 0.0;
    printf("==========size 2=== is_float======\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->2  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->2 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->2  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );


    item.size = 4;
    item.scale = 0.0;
    item.is_float = false;
    printf("==========size 4=========\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    item.size = 4;
    item.scale = 0.0;
    item.invert_mask = 0xff; 
    printf("==========size 4====inv 0xff=====\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );


    item.size = 4;
    item.scale = 0.0;
    item.invert_mask = 0; 
    item.is_word_swap = true; 
    printf("==========size 4====word_swap=====\n");
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    printf ("  u32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue1, inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    printf ("  u32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    printf ("  s32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3]);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    printf ("  s32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3] );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  s32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  s32->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    printf ("  f32->4 :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    printf ("  f32->4 :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3] );

    item.is_word_swap = false; 

    item.size = 4;
    item.scale = 0.0;
    item.is_float = true;
    item.is_float64 = true;
    f64 f64_val=56.67;
    printf("==========size 4=====is_float64====\n");

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue1), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", (s32)inputValue1, (s32)inputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(inputValue2), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));

    printf ("  u32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", inputValue2, inputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue1), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", sinputValue1, sinputValue1, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(sinputValue2), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", sinputValue2, sinputValue2, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );
    
    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  s32->4  :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin1), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", bin1, bin1, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(bin2), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  bool->4 :  input 0x%08x %14d\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", bin2, bin2, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_1), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", (unsigned int)fin32_1, fin32_1, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val);

    regs16[0] = 0; regs16[1] = 0; regs16[2] = 0; regs16[3] = 0;
    gcom_encode_any(regs16, regs8, item, std::any(fin32_2), myCfg);
    memcpy(&f64_val, &regs16[0], sizeof(f64_val));
    printf ("  f32->4  :  input 0x%08x %14f\treg [0] 0x%04x [1] 0x%04x [2] 0x%04x [3] 0x%04x float %f\n", (unsigned int)fin32_2, fin32_2, regs16[0] , regs16[1], regs16[2] , regs16[3], f64_val );

    //assert(!result);  // Expecting no error
    //assert(registers[0] != 0xDEAD && registers[1] != 0xBEEF);
          //if (registers[0] != 0xDEAD && registers[1] != 0xBEED) {
    //}



    std::cout << "All tests passed! ( well I'm not really checking yet!!)" << std::endl;
}

