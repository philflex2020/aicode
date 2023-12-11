#pragma once

#include <limits>

#include "string_storage_v2.hpp"
#include "shared_utils.hpp"
#include "Jval_buif.hpp"

// helper macros:
#define FORMAT_TO_BUF(fmt_buf, fmt_str, ...) fmt::format_to(std::back_inserter(fmt_buf), FMT_COMPILE(fmt_str), ##__VA_ARGS__)

// regular constants:
static constexpr u8  Max_Num_Bit_Strs      = std::numeric_limits<u8>::max();
static constexpr u8  Max_Num_Enums         = std::numeric_limits<u8>::max();
// special indices for indication of all/multi:
static constexpr u8  Bit_All_Idx           = std::numeric_limits<u8>::max();
static constexpr u8  Bit_Str_All_Idx       = std::numeric_limits<u8>::max(); // might not have this?
static constexpr u16 Bit_Str_Array_All_Idx = std::numeric_limits<u16>::max();
static constexpr u8  Enum_All_Idx          = std::numeric_limits<u8>::max();

struct Decode_Flags
{
    u16 flags = 0;

    // add more indices as needed (increase size type of flags as needed as well):
    // IMPORTANT(WALKER): Make sure these bits are always next to each other:
    static constexpr u16 is_size_one_idx = 0;
    static constexpr u16 is_size_two_idx = 1;
    static constexpr u16 is_size_four_idx = 2;
    static constexpr u16 size_mask = 0b111; // size mask for get_size(): DO NOT CHANGE THIS OR THE ABOVE INDICES
    // IMPORTANT(WALKER): DO NOT change the above flags, they are necessary for the get_size() method to return a proper whole number
    static constexpr u16 is_word_swapped_idx = 3;
    static constexpr u16 is_multi_write_op_code_idx = 4;
    static constexpr u16 is_signed_idx = 5;
    static constexpr u16 is_float_idx = 6;
    // these are for "bit_strings":
    static constexpr u16 is_individual_bits_idx = 7;
    static constexpr u16 is_individual_enums_idx = 8;
    static constexpr u16 is_bit_field_idx = 9;
    static constexpr u16 is_enum_field_idx = 10;
    static constexpr u16 is_enum_idx = 11; // this doubles as both "enum" and "random_enum"
    
    static constexpr u16 use_masks_idx = 12;

    static constexpr u16 goes_into_bits_mask =  (1UL << is_individual_bits_idx) | 
                                                (1UL << is_bit_field_idx) |
                                                (1UL << is_individual_enums_idx) | 
                                                (1UL << is_enum_field_idx); 

    static constexpr u16 bit_strings_mask =     (1UL << is_individual_bits_idx) | 
                                                (1UL << is_bit_field_idx) | 
                                                (1UL << is_individual_enums_idx) | 
                                                (1UL << is_enum_field_idx) | 
                                                (1UL << is_enum_idx);

    // setters (only called once during the config phase):
    constexpr void set_size(const u64 size) noexcept
    {
        if (size == 2UL)
        {
            flags |= static_cast<u16>((1UL << is_size_two_idx));
        }
        else if (size == 4UL)
        {
            flags |= static_cast<u16>((1UL << is_size_four_idx));
        }
        else // size of 1 by default
        {
            flags |= static_cast<u16>((1UL << is_size_one_idx));
        }
    }
    constexpr void set_word_swapped(const bool is_word_swapped) noexcept
    {
        flags |= static_cast<u16>((1UL & is_word_swapped) << is_word_swapped_idx);
    }
    constexpr void set_multi_write_op_code(const bool is_multi_write_op_code) noexcept
    {
        flags |= static_cast<u16>((1UL & is_multi_write_op_code) << is_multi_write_op_code_idx);
    }
    constexpr void set_signed(const bool is_signed) noexcept
    {
        flags |= static_cast<u16>((1UL & is_signed) << is_signed_idx);
    }
    constexpr void set_float(const bool is_float) noexcept
    {
        flags |= static_cast<u16>((1UL & is_float) << is_float_idx);
    }
    // these use "bit_strings":
    constexpr void set_individual_bits(const bool is_individual_bits) noexcept
    {
        flags |= static_cast<u16>((1UL & is_individual_bits) << is_individual_bits_idx);
    }
    constexpr void set_individual_enums(const bool is_individual_enums) noexcept
    {
        flags |= static_cast<u16>((1UL & is_individual_enums) << is_individual_enums_idx);
    }
    constexpr void set_bit_field(const bool is_bit_field) noexcept
    {
        flags |= static_cast<u16>((1UL & is_bit_field) << is_bit_field_idx);
    }
    constexpr void set_enum_field(const bool is_enum_field) noexcept
    {
        flags |= static_cast<u16>((1UL & is_enum_field) << is_enum_field_idx);
    }
    constexpr void set_enum(const bool is_enum) noexcept
    {
        flags |= static_cast<u16>((1UL & is_enum) << is_enum_idx);
    }
    constexpr void set_use_masks(const bool use_masks) noexcept
    {
        flags |= static_cast<u16>((1UL & use_masks) << use_masks_idx);
    }

    // getters:
    constexpr u8 get_size() const noexcept // this returns size as a whole number (1, 2, or 4)
    {
        return (flags & size_mask); // mask out the first three bits
    }
    constexpr bool is_size_one() const noexcept
    {
        return (flags >> is_size_one_idx) & 1UL;
    }
    constexpr bool is_size_two() const noexcept
    {
        return (flags >> is_size_two_idx) & 1UL;
    }
    constexpr bool is_size_four() const noexcept
    {
        return (flags >> is_size_four_idx) & 1UL;
    }
    constexpr bool is_word_swapped() const noexcept
    {
        return (flags >> is_word_swapped_idx) & 1UL;
    }
    constexpr bool is_multi_write_op_code() const noexcept
    {
        return (flags >> is_multi_write_op_code_idx) & 1UL;
    }
    constexpr bool is_signed() const noexcept
    {
        return (flags >> is_signed_idx) & 1UL;
    }
    constexpr bool is_float() const noexcept
    {
        return (flags >> is_float_idx) & 1UL;
    }
    // all these below use "bit_strings":
    constexpr bool is_individual_bits() const noexcept
    {
        return (flags >> is_individual_bits_idx) & 1UL;
    }
    constexpr bool is_individual_enums() const noexcept
    {
        return (flags >> is_individual_enums_idx) & 1UL;
    }
    constexpr bool is_bit_field() const noexcept
    {
        return (flags >> is_bit_field_idx) & 1UL;
    }
    constexpr bool is_enum_field() const noexcept
    {
        return (flags >> is_enum_field_idx) & 1UL;
    }
    constexpr bool is_enum() const noexcept
    {
        return (flags >> is_enum_idx) & 1UL;
    }
    constexpr bool uses_masks() const noexcept
    {
        return (flags >> use_masks_idx) & 1UL;
    }
    constexpr bool is_goes_into_bits_type() const noexcept // returns whether or not this register type goes into the individual bits themselves (all the bit_strings types except for "enum" really)
    {
        return (flags & goes_into_bits_mask) > 0;
    }
    constexpr bool is_bit_string_type() const noexcept // returns whether or not this register type is one that uses "bit_strings"
    {
        return (flags & bit_strings_mask) > 0;
    }
};
// size info:
// auto x = sizeof(Decode_Flags);

struct Decode_Info
{
    u16 bit_str_array_idx = Bit_Str_Array_All_Idx; // this tells you which "bit_strings" array to reach into (using the two structs below)
    u16 offset;
    Decode_Flags flags;
    u64 invert_mask = 0;
    f64 scale = 0.0;
    s64 shift = 0;
    u64 starting_bit_pos = 0;
    u64 care_mask = std::numeric_limits<u64>::max();
};
// size info:
// auto x = sizeof(Decode_Info);

struct Bit_Strings_Info_Flags
{
    u8 flags = 0;

    static constexpr u8 is_unknown_idx = 0;
    static constexpr u8 is_ignored_idx = 1;

    // setters (only called once during the config phase):
    constexpr void set_unknown(const bool is_unknown) noexcept
    {
        flags |= static_cast<u8>((1UL & is_unknown) << is_unknown_idx);
    }
    constexpr void set_ignored(const bool is_ignored) noexcept
    {
        flags |= static_cast<u8>((1UL & is_ignored) << is_ignored_idx);
    }

    // getters:
    constexpr bool is_unknown() const noexcept
    {
        return (flags >> is_unknown_idx) & 1UL;
    }
    constexpr bool is_ignored() const noexcept
    {
        return (flags >> is_ignored_idx) & 1UL;
    }
};
// size info:
// auto x = sizeof(Decode_Flags);

struct Bit_Strings_Info
{
    Bit_Strings_Info_Flags flags;
    u8 begin_bit;
    u8 end_bit;
    // u8 last_index; // experimental (for individual_enums/enum_field only)
    String_Storage_Handle str;
    // u64 care_mask; // experimental (for individual_enums/enum_field only)
    u64 enum_val;
};
// size info:
// auto x = sizeof(Bit_Strings_Info);

struct Bit_Strings_Info_Array
{
    u8 num_bit_strs = 0;
    u64 care_mask = std::numeric_limits<u64>::max();
    u64 changed_mask = 0;
    Bit_Strings_Info* bit_strs = nullptr;
};
// size info:
// auto x = sizeof(Bit_Strings_Info_Array);

// functions for decoding and encoding (only for Holding and Input registers):

static constexpr u64 decode(const u16* raw_registers, const Decode_Info& current_decode, Jval_buif& current_jval) noexcept
{
    u64 raw_data = 0UL;

    u64 current_unsigned_val = 0UL;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;

    if (current_decode.flags.is_size_one())
    {
        raw_data = raw_registers[0];

        current_unsigned_val = raw_data;
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed stuff (size 1 cannot be float):
        if (current_decode.flags.is_signed())
        {
            s16 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
    }
    else if (current_decode.flags.is_size_two())
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 16) +
                    (static_cast<u64>(raw_registers[1]) <<  0);

        if (!current_decode.flags.is_word_swapped()) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swapped:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) + 
                                    (static_cast<u64>(raw_registers[1]) << 16);
        }
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed/float stuff:
        if (current_decode.flags.is_signed())
        {
            s32 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            f32 to_reinterpret = 0.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }
    else // size of 4:
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 48) +
                    (static_cast<u64>(raw_registers[1]) << 32) +
                    (static_cast<u64>(raw_registers[2]) << 16) +
                    (static_cast<u64>(raw_registers[3]) <<  0);

        if (!current_decode.flags.is_word_swapped()) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swapped:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) +
                                    (static_cast<u64>(raw_registers[1]) << 16) +
                                    (static_cast<u64>(raw_registers[2]) << 32) +
                                    (static_cast<u64>(raw_registers[3]) << 48);
        }
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed/float stuff:
        if (current_decode.flags.is_signed()) // this is only for whole numbers really (signed but not really float):
        {
            s64 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            f64 to_reinterpret = 0.0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }

    // do scaling/shift stuff at the end:
    if (current_decode.flags.is_float())
    {
        if (current_decode.scale) // scale != 0
        {
            current_float_val /= current_decode.scale;
        }
        current_float_val += static_cast<f64>(current_decode.shift); // add shift
        current_jval = current_float_val;
    }
    else if (!current_decode.flags.is_signed()) // unsigned whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_unsigned_val += current_decode.shift; // add shift
            current_unsigned_val >>= current_decode.starting_bit_pos;
            current_jval = current_unsigned_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_unsigned_val) / current_decode.scale;
            scaled += static_cast<f64>(current_decode.shift); // add shift
            current_jval = scaled;
        }
    }
    else // signed whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_signed_val += current_decode.shift; // add shift
            current_signed_val >>= current_decode.starting_bit_pos;
            current_jval = current_signed_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_signed_val) / current_decode.scale;
            scaled += static_cast<f64>(current_decode.shift); // add shift
            current_jval = scaled;
        }
    }
    return raw_data;
}

// TODO(WALKER): Make sure this works for individual_enums in the future:
static constexpr void encode(u16* raw_registers, const Decode_Info& current_decode, const Jval_buif& to_encode, 
    const u8 bit_id = Bit_All_Idx, const u64 bit_str_care_mask = 1UL, u64* prev_unsigned_val = nullptr) noexcept
{
    u64 current_unsigned_val = 0;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;

    // NOTE(WALKER): If "scale" is set then all numbers will be converted to a float for encoding
    // this is to maintain decimal places multiplication purposes (so multiplying by, say, 3.14 won't just multiply by 3 against 100 -> result should be 314 NOT 300)
    // therefore we can gain or lose some accuracy along the way depending on certain limits with scale (mostly in the size 4 category where most losses can occur)

    if (bit_id == Bit_All_Idx) // normal set:
    {
        // extract out set_val's value (signed check, float check, shift, scaling, etc.):
        if (to_encode.holds_uint())
        {
            current_unsigned_val = to_encode.get_uint_unsafe();
            current_unsigned_val <<= current_decode.starting_bit_pos;
            current_unsigned_val -= current_decode.shift;
            if (current_decode.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_unsigned_val);
                current_float_val *= current_decode.scale;
            }
        }
        else if (to_encode.holds_int()) // negative values only:
        {
            current_signed_val = to_encode.get_int_unsafe();
            current_signed_val <<= current_decode.starting_bit_pos;
            current_signed_val -= current_decode.shift;
            if (current_decode.scale) // scale != 0
            {
                current_float_val = static_cast<f64>(current_signed_val);
                current_float_val *= current_decode.scale;
            }
        }
        else // float:
        {
            current_float_val = to_encode.get_float_unsafe();
            // TODO: shifting floats?
            current_float_val -= static_cast<f64>(current_decode.shift);
            if (current_decode.scale) // scale != 0
            {
                current_float_val *= current_decode.scale;
            }
        }
    }
    else // set a particular bit/bits (leaving all other bits we don't care about unchanged):
    {
        current_unsigned_val = (*prev_unsigned_val & ~(bit_str_care_mask << bit_id)) | (to_encode.get_uint_unsafe() << bit_id);
        *prev_unsigned_val = current_unsigned_val; // make sure that we keep track of the bits that we just set for sets using prev_unsigned_val in the future (so we don't change them back on successive sets)
    }

    // set registers based on size:
    if (current_decode.flags.is_size_one())
    {
        u16 current_u16_val = 0;
        if (to_encode.holds_uint() && !current_decode.scale)
        {
            current_u16_val = static_cast<u16>(current_unsigned_val);
        }
        else if (to_encode.holds_int() && !current_decode.scale)
        {
            current_u16_val = static_cast<u16>(current_signed_val);
        }
        else // float (or sets with scale enabled):
        {
            if (!current_decode.flags.is_signed()) // unsigned:
            {
                current_u16_val = static_cast<u16>(current_float_val);
            }
            else // signed:
            {
                current_u16_val = static_cast<u16>(static_cast<s16>(current_float_val));
            }
        }
        current_u16_val ^= static_cast<u16>(current_decode.invert_mask); // invert before byte swapping and sending out
        raw_registers[0] = static_cast<u16>(current_u16_val);
    }
    else if (current_decode.flags.is_size_two())
    {
        u32 current_u32_val = 0U;
        if (to_encode.holds_uint() && !current_decode.scale)
        {
            if (!current_decode.flags.is_float()) // this also counts for "signed"
            {
                current_u32_val = static_cast<u32>(current_unsigned_val);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<f32>(current_unsigned_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else if (to_encode.holds_int() && !current_decode.scale)
        {
            if (!current_decode.flags.is_float())
            {
                current_u32_val = static_cast<u32>(current_signed_val);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<f32>(current_signed_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else // float (or sets with scale enabled):
        {
            if (current_decode.flags.is_float())
            {
                const auto current_float32_val = static_cast<f32>(current_float_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
            else if (!current_decode.flags.is_signed())
            {
                current_u32_val = static_cast<u32>(current_float_val);
            }
            else // signed:
            {
                current_u32_val = static_cast<u32>(static_cast<s32>(current_float_val));
            }
        }
        current_u32_val ^= static_cast<u32>(current_decode.invert_mask); // invert before word swapping and sending out
        if (!current_decode.flags.is_word_swapped())
        {
            raw_registers[0] = static_cast<u16>(current_u32_val >> 16);
            raw_registers[1] = static_cast<u16>(current_u32_val >>  0);
        }
        else
        {
            raw_registers[0] = static_cast<u16>(current_u32_val >>  0);
            raw_registers[1] = static_cast<u16>(current_u32_val >> 16);
        }
    }
    else // size 4:
    {
        u64 current_u64_val = 0UL;
        if (to_encode.holds_uint() && !current_decode.scale)
        {
            if (!current_decode.flags.is_float())
            {
                current_u64_val = current_unsigned_val;
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<f64>(current_unsigned_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_u64_val));
            }
        }
        else if (to_encode.holds_int() && !current_decode.scale)
        {
            if (!current_decode.flags.is_float())
            {
                current_u64_val = static_cast<u64>(current_signed_val);
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<f64>(current_signed_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_u64_val));
            }
        }
        else // float (or sets with scale enabled):
        {
            if (current_decode.flags.is_float())
            {
                memcpy(&current_u64_val, &current_float_val, sizeof(current_u64_val));
            }
            else if (!current_decode.flags.is_signed())
            {
                current_u64_val = static_cast<u64>(current_float_val);
            }
            else // signed:
            {
                current_u64_val = static_cast<u64>(static_cast<s64>(current_float_val));
            }
        }
        current_u64_val ^= current_decode.invert_mask;
        if (!current_decode.flags.is_word_swapped())
        {
            raw_registers[0] = static_cast<u16>(current_u64_val >> 48);
            raw_registers[1] = static_cast<u16>(current_u64_val >> 32);
            raw_registers[2] = static_cast<u16>(current_u64_val >> 16);
            raw_registers[3] = static_cast<u16>(current_u64_val >>  0);
        }
        else // word swap:
        {
            raw_registers[0] = static_cast<u16>(current_u64_val >>  0);
            raw_registers[1] = static_cast<u16>(current_u64_val >> 16);
            raw_registers[2] = static_cast<u16>(current_u64_val >> 32);
            raw_registers[3] = static_cast<u16>(current_u64_val >> 48);
        }
    }
}

// to string method for decoded_vals (for publishing/get purposes):
template<Register_Types Reg_Type>
static void decode_to_string(const Decode_Info& current_decode, const Jval_buif& to_stringify, fmt::memory_buffer& buf, u8 bit_idx = Bit_All_Idx, Bit_Strings_Info_Array* bit_str_array = nullptr, String_Storage* str_storage = nullptr) noexcept
{
    if constexpr (Reg_Type == Register_Types::Coil || Reg_Type == Register_Types::Discrete_Input)
    {
        FORMAT_TO_BUF(buf, "{}", to_stringify.get_bool_unsafe());
    }
    else // Holding and Input:
    {
        if (!current_decode.flags.is_bit_string_type()) // normal formatting:
        {
            FORMAT_TO_BUF(buf, "{}", to_stringify);
        }
        else // format using "bit_strings":
        {
            const auto curr_unsigned_val = to_stringify.get_uint_unsafe();
            if (current_decode.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
            {
                // if (bit_idx == Bit_All_Idx) // they want the whole thing
                // {
                //     // POSSIBLE TODO(WALKER): The raw id pub might or might not need to include the binary string as a part of its pub (maybe wrap it in an object?)
                //     FORMAT_TO_BUF(buf, "{}", curr_unsigned_val); // this is the whole individual_bits as a raw number (after basic decoding)
                // }
                // else // they want the individual true/false bit (that belongs to this bit_idx)
                // {
                    FORMAT_TO_BUF(buf, "{}", ((curr_unsigned_val >> bit_idx) & 1UL) == 1UL); // this is the individual bit itself (true/false)
                // }
            }
            else if (current_decode.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
            {
                buf.push_back('[');
                u8 last_idx = 0;
                for (u8 bit_str_idx = 0; bit_str_idx < bit_str_array->num_bit_strs; ++bit_str_idx)
                {
                    const auto& bit_str = bit_str_array->bit_strs[bit_str_idx];
                    last_idx = bit_str.end_bit + 1;
                    if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
                    {
                        if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                        {
                            FORMAT_TO_BUF(buf, R"({{"value":{},"string":"{}"}},)",
                                                bit_str.begin_bit, 
                                                str_storage->get_str(bit_str.str));
                        }
                    }
                    else // "unknown" bits that are "inbetween" bits (and not ignored):
                    {
                        for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
                        {
                            if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
                            {
                                FORMAT_TO_BUF(buf, R"({{"value":{},"string":"Unknown"}},)",
                                                    unknown_idx, 
                                                    str_storage->get_str(bit_str.str));
                            }
                        }
                    }
                }
                // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                for (; last_idx < (current_decode.flags.get_size() * 16); ++last_idx)
                {
                    if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
                    {
                        FORMAT_TO_BUF(buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
                    }
                }
                buf.resize(buf.size() - 1); // this gets rid of the last excessive ',' character
                buf.push_back(']'); // finish the array
            }
            else if (current_decode.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
            {
                bool enum_found = false;
                for (u8 enum_str_idx = 0; enum_str_idx < bit_str_array->num_bit_strs; ++enum_str_idx)
                {
                    const auto& enum_str = bit_str_array->bit_strs[enum_str_idx];
                    if (curr_unsigned_val == enum_str.enum_val)
                    {
                        enum_found = true;
                        FORMAT_TO_BUF(buf, R"([{{"value":{},"string":"{}"}}])", curr_unsigned_val, str_storage->get_str(enum_str.str));
                        break;
                    }
                }
                if (!enum_found) // config did not account for this value:
                {
                    FORMAT_TO_BUF(buf, R"([{{"value":{},"string":"Unknown"}}])", curr_unsigned_val);
                }
            }
            // TODO(WALKER): in the future, once everything is done, I can get back to redoing these

            // else if (current_decode.flags.is_individual_enums()) // resolves to multiple uris:
            // {
            //     if (current_msg.bit_id == bit_all_index) // they want the whole thing
            //     {
            //         fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), current_unsigned_val); // send out the whole raw register (all the enums in one)
            //     }
            //     else // they want a particular enum inside the bits themselves:
            //     {
            //         // only iterate on that particular sub_array (from the beginning using enum_id):
            //         for (uint8_t j = current_msg.enum_id + 1; j < current_enum_bit_string_array[current_msg.enum_id].last_index + 1; ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
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
        }
    }
}
