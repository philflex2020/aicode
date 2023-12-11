#pragma once

#include <string>
#include <vector>
#include <charconv>

#include "spdlog/fmt/bundled/format.h"

#include "simdjson_noexcept.hpp"
#include "config_loaders/error_location.hpp"
#include "shared_utils.hpp"

namespace config_loader
{

// returns a 64 bit mask from begin_bit to end_bit (produces arbitrary 64 bit masks)
// NOTE(WALKER): Begin bit starts at 0 (0 -> 63) NOT 1 (1 -> 64)
// CREDIT: https://www.geeksforgeeks.org/set-bits-given-range-number/
static constexpr u64 gen_mask(const u64 begin_bit_idx, const u64 end_bit_idx) noexcept
{
    const auto lower_mask = (1UL << (begin_bit_idx)) - 1UL;
    const auto upper_mask = end_bit_idx < 63UL ? ((1UL << (end_bit_idx + 1UL)) - 1UL) : std::numeric_limits<u64>::max();
    return lower_mask ^ upper_mask;
    // return (((1UL << (begin_bit_idx)) - 1UL) ^ ((1UL << (end_bit_idx + 1UL)) - 1UL));
}

static bool check_mask(const std::string& mask_str, const u64 bounds_mask, u64& to_get, Error_Location& err_loc)
{
    if (mask_str.size() < 3)
    {
        err_loc.err_msg = fmt::format("mask (currently: {}) must be more than two characters and begin with 0x or 0b", mask_str);
        return false;
    }

    const auto mask_type = mask_str.substr(0, 2);
    const auto to_convert = mask_str.substr(2);
    bool is_hex = false;

    if (mask_type == "0x") { is_hex = true; }
    else if (mask_type != "0b")
    {
        err_loc.err_msg = fmt::format("mask (currently: {}) must begin with 0x or 0b", mask_str);
        return false;
    }

    const auto [ptr, ec] = is_hex ? std::from_chars(to_convert.data(), to_convert.data() + to_convert.size(), to_get, 16) : std::from_chars(to_convert.data(), to_convert.data() + to_convert.size(), to_get, 2);

    if (ec == std::errc())
    {
        if (*ptr != '\0') // non-null-terminated end (found some invalid characters)
        {
            err_loc.err_msg =  is_hex ? fmt::format("mask (currently: {}) contains the illegal hex character '{}'", mask_str, *ptr) :
                                        fmt::format("mask (currently: {}) contains the illegal binary character '{}'", mask_str, *ptr);
            return false;
        }
        if ((to_get | bounds_mask) > bounds_mask) // out of bounds check
        {
            err_loc.err_msg = fmt::format("mask (currently: {}) goes outside of bounds", mask_str);
            return false;
        }
    }
    else if (ec == std::errc::invalid_argument)
    {
        err_loc.err_msg =  is_hex ? fmt::format("mask (currently: {}) does not begin with a valid hex character after 0x", mask_str) :
                                    fmt::format("mask (currently: {}) does not begin with a valid binary character after 0b", mask_str);
        return false;
    }
    else if (ec == std::errc::result_out_of_range)
    {
        err_loc.err_msg = fmt::format("mask (currently: {}) would exceed the absolute maximum of 64 bits", mask_str);
        return false;
    }

    return true;
}

// NOTE(WALKER): This is currently unused but will be in the future for "enum_field"/"individual_enums"
struct Enum_String_Pair
{
    u64 enum_val = 0;
    std::string enum_string;

    // for sorting purposes (enum_strings array):
    constexpr bool operator <(const Enum_String_Pair& rhs) noexcept
    {
        return enum_val < rhs.enum_val;
    }
};

struct Bit_String
{
    static constexpr std::string_view Ignore_Keyword = "IGNORE";

    bool is_unknown = false; // This means it is "Unknown" if this is true (really for "null" being the value, only used in bit_field really) -> probably just going to have an "unknown counter" for bit_strings during runtime
    std::string id;
    Enum_String_Pair enum_pair;
    // experimental (individual_enums and enum_field stuff):
    u64 begin_bit; // also acts as bit_idx for bit_field/individual_bits
    u64 end_bit;   // also acts as bit_idx for bit_field/individual_bits
    std::string care_mask_str;
    std::string ignore_mask_str; // opposite of this makes the care_mask (don't forget to mask out the out of bounds bits)
    std::vector<Enum_String_Pair> enum_strings;

    // derived info:
    u64 num_ignored = 0;
    u64 care_mask; // experimental (not used for now)

    // for sorting purposes (Enum type only):
    constexpr bool operator <(const Bit_String& rhs) noexcept
    {
        return enum_pair.enum_val < rhs.enum_pair.enum_val;
    }

    bool load_individual_bit(simdjson::simdjson_result<simdjson::ondemand::value> val, Error_Location& err_loc, u64& curr_bit_idx, const u64 max_num_bits, u64& curr_care_mask)
    {
        err_loc.key = "";
        err_loc.type = Required_Types::None;
        // null check (only kind that can ignore stuff -> for now):
        if (val.error()) { err_loc.err_msg = simdjson::error_message(val.error()); return false; }
        if (val.is_null())
        {
            ++num_ignored;
            // curr_care_mask &= ~(gen_mask(0, num_ignored - 1UL) << curr_bit_idx); // clear out the ignored bits in the care_mask
            begin_bit = curr_bit_idx;
            end_bit = curr_bit_idx;
            ++curr_bit_idx;
            if (curr_bit_idx > max_num_bits)
            {
                err_loc.err_msg = fmt::format("the number of bits in this bit_strings array (currently: {}) would exceed the maximum of {}", curr_bit_idx, max_num_bits);
                return false;
            }
            return true;
        } // only 1 ignored for now, in future support the "IGNORE" keyword and object

        // regular string check:
        err_loc.type = Required_Types::String;
        if (!get_mandatory_val(val, id, err_loc)) return false;
        if (!check_str_for_error(id, err_loc)) return false;
        err_loc.type = Required_Types::None;
        begin_bit = curr_bit_idx;
        end_bit = curr_bit_idx;
        curr_care_mask |= (1UL << curr_bit_idx); // set this bit to one we care about
        ++curr_bit_idx;
        if (curr_bit_idx > max_num_bits)
        {
            err_loc.err_msg = fmt::format("the number of bits in this bit_strings array (currently: {}) would exceed the maximum of {}", curr_bit_idx, max_num_bits);
            return false;
        }
        return true;
    }

    // NOTE(walker): In the future make sure that this changes "care_mask" for "ignore" bits appropriately
    // TODO(WALKER): In the future, I don't think that we need to "compress" ignored bits into the array
    // because those bits are going to be masked out anyway, we only really need "known/unknown" for flags
    bool load_bit_field(simdjson::simdjson_result<simdjson::ondemand::value> val, Error_Location& err_loc, u64& curr_bit_idx, const u64 max_num_bits)
    {
        err_loc.key = "";
        err_loc.type = Required_Types::None;
        // null check ("Unknown" check):
        if (val.error()) { err_loc.err_msg = simdjson::error_message(val.error()); return false; }
        if (val.is_null())
        {
            is_unknown = true;
            begin_bit = curr_bit_idx;
            end_bit = curr_bit_idx;
            ++curr_bit_idx;
            if (curr_bit_idx > max_num_bits)
            {
                err_loc.err_msg = fmt::format("the number of bits in this bit_strings array (currently: {}) would exceed the maximum of {}", curr_bit_idx, max_num_bits);
                return false;
            }
            return true;
        }
        // TODO(WALKER): In the future allow for "IGNORE" stuff (after discussion with John)
        // make sure to change care_mask as well

        // regular string check:
        err_loc.type = Required_Types::String;
        if (!get_mandatory_val(val, id, err_loc)) return false;
        if (!check_str_for_error(id, err_loc, R"(\")")) return false;
        err_loc.type = Required_Types::None;
        begin_bit = curr_bit_idx;
        end_bit = curr_bit_idx;
        ++curr_bit_idx;
        if (curr_bit_idx > max_num_bits)
        {
            err_loc.err_msg = fmt::format("the number of bits in this bit_strings array (currently: {}) would exceed the maximum of {}", curr_bit_idx, max_num_bits);
            return false;
        }
        return true;
    }

    bool load_enum(simdjson::simdjson_result<simdjson::ondemand::value> val, Error_Location& err_loc, u64& curr_enum_val, const u64 max_enum_val)
    {
        err_loc.key = "";
        err_loc.type = Required_Types::Object;
        // object check (enum_string pair):
        simdjson::ondemand::object enum_obj;
        if (const auto err = val.get(enum_obj); simdj_optional_type_error(err))
        {
            err_loc.err_msg = simdjson::error_message(err);
            return false;
        }
        else if (!err) // we can get it as an object:
        {
            if (!get_val(enum_obj, "value", enum_pair.enum_val, err_loc, Get_Type::Mandatory)) return false;
            if (!check_enum_value(err_loc, enum_pair.enum_val, max_enum_val)) return false;
            curr_enum_val = enum_pair.enum_val; // set the new enum_val with this
            if (!get_val(enum_obj, "string", enum_pair.enum_string, err_loc, Get_Type::Mandatory)) return false;
            if (!check_str_for_error(enum_pair.enum_string, err_loc, R"(\")")) return false;
            return true;
        }

        // regular string check:
        err_loc.type = Required_Types::String;
        if (!get_mandatory_val(val, enum_pair.enum_string, err_loc)) return false;
        if (!check_str_for_error(enum_pair.enum_string, err_loc, R"(\")")) return false;
        enum_pair.enum_val = curr_enum_val;
        if (!check_enum_value(err_loc, enum_pair.enum_val, max_enum_val)) return false;
        return true;
    }

    // NOTE(WALKER): experimental
    // bool load_individual_enum(simdjson::simdjson_result<simdjson::ondemand::value> val, Error_Location& err_loc)
    // {
    //     return true;
    // }

    // bool load_enum_field(simdjson::simdjson_result<simdjson::ondemand::value> val, Error_Location& err_loc)
    // {
    //     return true;
    // }

    bool check_enum_value(Error_Location& err_loc, const u64 curr_enum_val, const u64 max_enum_val)
    {
        if (curr_enum_val > max_enum_val)
        {
            err_loc.err_msg = fmt::format("enum value (currently: {}) exceeds the current maximum of {}", curr_enum_val, max_enum_val);
            return false;
        }
        return true;
    }
};

// helper struct when compressing bit strings into the compressed arrays (more helpful when converting into the runtime structs)
struct Compressed_Bit_String
{
    bool is_unknown = false;
    bool is_ignored = false;
    bool is_id = false; // only for "individual_enums" in the future
    u64 begin_bit = 0;
    u64 end_bit = 0;
    u64 care_mask = 0; // only for "individual_enums"/"enum_field" in the future
    u64 enum_val = 0;
    u64 last_index = 0; // only for "individual_enums"/"enum_field" in the future
    std::string str;
};

struct Decode
{
    std::string id;
    std::string uri; // server only
    u64 offset;

    // optional (for Holding and Input registers):
    u64 size = 1;
    f64 scale = 0.0;
    s64 shift = 0;
    u64 starting_bit_pos = 0;
    u64 number_of_bits = 16; // How many bits this register tracks. Used by packed_registers to track a specific portion of the register
    std::string invert_mask_str = "0x0";
    bool multi_write_op_code = false; // inheritance stuff (from component, which inherits from connection)
    bool Signed = false;
    bool Float = false;
    bool bit_field = false;
    bool individual_bits = false;
    bool Enum = false;
    bool random_enum = false; // same as "enum"
    bool packed_register = false; // Whether this register is packed alongside other registers into the same offset
    // experimental (left out for now):
    // bool enum_field = false;
    // bool individual_enums = false;
    std::vector<Bit_String> bit_strings;

    // might move this somewhere else or remove this entirely
    static constexpr auto Max_Num_Enums = std::numeric_limits<u8>::max();

    // derived info:
    u64 invert_mask = 0;
    bool has_bit_strings = false;
    u64 care_mask = std::numeric_limits<u64>::max(); // dervied from bit_strings array
    bool uses_masks = false; // Whether this register uses an invert or care mask
    std::vector<Compressed_Bit_String> compressed_bit_strings; // to help remove "unknown" stuff and to make it more in line with what the runtime structs/array will look like (derive once here during config time)

    // for sorting purposes:
    constexpr bool operator <(const Decode& other) noexcept
    {
        return offset < other.offset;
    }

    bool load(simdjson::ondemand::object& decode, Error_Location& err_loc, bool is_coil_or_discrete_input = false, bool off_by_one = false
           , bool is_server_config = false, Decode* parent_config = NULL)
    {
        if (parent_config)
        {
            // Copy any parent configuration to this register
            // may be overridden if values are provided in the parsing below for this child register
            *this = *parent_config;
        }
        if (!get_val(decode, "id", id, err_loc, Get_Type::Mandatory)) return false;
        if (!check_str_for_error(id, err_loc)) return false;
        err_loc.decode_id = id; // set error location here once we parse the id (and error check it)

        // only for server (everything plus uri):
        if (is_server_config)
        {
            if (!get_val(decode, "uri", uri, err_loc, Get_Type::Mandatory)) return false;
            if (!check_str_for_error(uri, err_loc, R"({}\ "%)")) return false; // uri can have '/' in it
        }
        // If parent config was provided for a packed register, the offset will have already been provided
        if (!get_val(decode, "offset", offset, err_loc, packed_register ? Get_Type::Optional : Get_Type::Mandatory)) return false;
        offset -= (1UL * off_by_one);
        if (!check_offset(off_by_one, err_loc)) return false;

        if (!get_val(decode, "size", size, err_loc, Get_Type::Optional)) return false;
        if (!check_size(is_coil_or_discrete_input, err_loc)) return false;

        // only do the rest of it if we are Holding or Input registers:
        if (!is_coil_or_discrete_input)
        {
            if (!get_val(decode, "scale", scale, err_loc, Get_Type::Optional)) return false;
            if (scale == 1.0) scale = 0.0; // scale of 1 might as well be 0

            if (!get_val(decode, "shift", shift, err_loc, Get_Type::Optional)) return false;

            if (!get_val(decode, "invert_mask", invert_mask_str, err_loc, Get_Type::Optional)) return false;
            if (!check_mask(invert_mask_str, gen_mask(0, size * 16UL - 1UL), invert_mask, err_loc)) return false;

            if (!get_val(decode, "multi_write_op_code", multi_write_op_code, err_loc, Get_Type::Optional)) return false;

            if (!get_val(decode, "signed", Signed, err_loc, Get_Type::Optional)) return false;
            if (!get_val(decode, "float", Float, err_loc, Get_Type::Optional)) return false;
            if (!check_signed_float(err_loc)) return false;

            if (!get_val(decode, "bit_field", bit_field, err_loc, Get_Type::Optional)) return false;
            if (!get_val(decode, "individual_bits", individual_bits, err_loc, Get_Type::Optional)) return false;
            if (!get_val(decode, "enum", Enum, err_loc, Get_Type::Optional)) return false;
            if (!get_val(decode, "random_enum", random_enum, err_loc, Get_Type::Optional)) return false;
            Enum = Enum || random_enum; // set enum = to true for both enum and random_enum (we combine them in the new modbus_client)
            // For packed registers, configuration can be provided at the parent level, and will be distributed to each of the children here.
            if (packed_register)
            {
                // Starting bit pos is a shift operation that defines where this register will start decoding
                // for the given offset
                if (!get_val(decode, "starting_bit_pos", starting_bit_pos, err_loc, Get_Type::Optional)) return false;
                if (!get_val(decode, "number_of_bits", number_of_bits, err_loc, Get_Type::Mandatory)) return false;
                if (!check_bitwise_float(err_loc)) return false;

                care_mask = 0UL;
                for (u64 i = 0UL; i < number_of_bits; i++)
                    care_mask |= 1 << i;
                care_mask = care_mask << starting_bit_pos;
            }
            // Use masks if either the invert or care mask has been configured with a non default value
            if (invert_mask != 0 || care_mask != std::numeric_limits<u64>::max()) uses_masks = true;
            // experimental (will do these later):
            // if (!get_val(decode, "enum_field", enum_field, err_loc, Get_Type::Optional)) return false;
            // if (!get_val(decode, "individual_enums", individual_enums, err_loc, Get_Type::Optional)) return false;
            if (!check_bit_str_flags(err_loc)) return false;

            if (has_bit_strings)
            {
                u64 curr_bit_idx = 0;
                const auto max_num_bits = size * 16UL;
                u64 curr_enum_val = 0;
                const auto max_enum_val = gen_mask(0, size * 16UL - 1UL);

                simdjson::ondemand::array bit_strs_arr;
                if (!get_val(decode, "bit_strings", bit_strs_arr, err_loc, Get_Type::Mandatory)) return false;

                // setting default care_mask based on type:
                if (!packed_register && (bit_field || Enum))
                {
                    care_mask = std::numeric_limits<u64>::max();
                }
                // if (individual_bits || enum_field || individual_enums)
                if (individual_bits) // NOTE(WALKER): in the future delete this line and uncomment the above one
                {
                    care_mask = 0; // we don't care about any bits unless they have a string associated with it for these types
                }

                for (auto bit_str : bit_strs_arr)
                {
                    auto& curr_bit_str = bit_strings.emplace_back();
                    if (bit_field)
                    {
                        if (!curr_bit_str.load_bit_field(bit_str, err_loc, curr_bit_idx, max_num_bits)) return false;
                    }
                    else if (individual_bits)
                    {
                        if (!curr_bit_str.load_individual_bit(bit_str, err_loc, curr_bit_idx, max_num_bits, care_mask)) return false;
                        // if (curr_bit_str.num_unknown > 0)
                        // {

                        // }
                    }
                    else if (Enum) // also random_enum
                    {
                        if (!curr_bit_str.load_enum(bit_str, err_loc, curr_enum_val, max_enum_val)) return false;
                        ++curr_enum_val;
                    }
                    // else if (enum_field)
                    // {

                    // }
                    // else if (individual_enums)
                    // {

                    // }
                    ++err_loc.bit_strings_idx;
                }
                if (!check_bit_strings(err_loc)) return false;
                err_loc.bit_strings_idx = 0;
                compress_bit_strings();
            }
        }

        return true;
    }

    bool check_offset(bool off_by_one, Error_Location& err_loc)
    {
        if (offset > std::numeric_limits<u16>::max())
        {
            err_loc.err_msg = fmt::format("offset (currently: {}, off_by_one: {}) must be between 0 and {}", offset, off_by_one, std::numeric_limits<u16>::max());
            return false;
        }
        return true;
    }

    bool check_size(bool is_coil_or_discrete_input, Error_Location& err_loc)
    {
        if (is_coil_or_discrete_input && size != 1)
        {
            err_loc.err_msg = fmt::format("size (currently: {}) must be 1 for Coils or Discrete Inputs", size);
            return false;
        }
        if (!(size == 1 || size == 2 || size == 4))
        {
            err_loc.err_msg = fmt::format("size (currently: {}) must be 1, 2 or 4 for Holding and Input registers", size);
            return false;
        }
        if (offset + size - 1UL > std::numeric_limits<u16>::max()) // overflow check
        {
            err_loc.err_msg = fmt::format("offset {} with size {} would exceed the max offset of {}", offset, size, std::numeric_limits<u16>::max());
            return false;
        }
        if (Float && size == 1)
        {
            err_loc.err_msg = fmt::format("size (currently: {}) must be 2 or 4 for float to be true", size);
            return false;
        }
        return true;
    }

    bool check_signed_float(Error_Location& err_loc)
    {
        if (Signed && Float)
        {
            err_loc.err_msg = "signed and float cannot both be true at the same time";
            return false;
        }
        return true;
    }

    bool check_bitwise_float(Error_Location& err_loc)
    {
        if (Float && starting_bit_pos != 0)
        {
            err_loc.err_msg = "starting bit position must be 0 or unconfigured when float is true";
            return false;
        }
        return true;
    }

    bool check_bit_str_flags(Error_Location& err_loc)
    {
        auto bit_str_type_counter = bit_field + individual_bits + Enum;
        if (bit_str_type_counter > 1)
        {
            err_loc.err_msg = "More than one bit_strings flag among: bit_field, individual_bits, and enum/random_enum is set to true";
            return false;
        }
        if (bit_str_type_counter == 1)
        {
            // NOTE(WALKER): In the future, might remove the scale and shift restrictions, if need be
            // for now they are here for bit_strings, because they are meant for unchanged unsigned values
            // that we just interpret as is
            if (Signed)
            {
                err_loc.err_msg = "signed cannot be set to true for any bit_strings type";
                return false;
            }
            else if (Float)
            {
                err_loc.err_msg = "float cannot be set to true for any bit_strings type";
                return false;
            }
            // NOTE(WALKER): If we need "scale" and "shift" to work alongside bit_strings then delete these two if checks
            else if (scale != 0.0)
            {
                err_loc.err_msg = fmt::format("scale (currently: {}) cannot be set to anything other than 0 for any bit_strings type", scale);
                return false;
            }
            else if (shift != 0)
            {
                err_loc.err_msg = fmt::format("shift (currently: {}) cannot be set to anything other than 0 for any bit_strings type", shift);
                return false;
            }
            has_bit_strings = true;
        }
        return true;
    }

    bool check_bit_strings(Error_Location& err_loc)
    {
        err_loc.bit_strings_idx = 0;

        if (bit_strings.empty())
        {
            err_loc.err_msg = fmt::format("bit_strings array is empty");
            return false;
        }

        err_loc.key = "";
        err_loc.type = Required_Types::None;

        bool has_at_least_one_legitimate_bit_string = false;
        for (const auto& bit_str : bit_strings)
        {
            if (!bit_str.is_unknown && bit_str.num_ignored == 0)
            {
                has_at_least_one_legitimate_bit_string = true;
                break;
            }
        }
        if (!has_at_least_one_legitimate_bit_string)
        {
            err_loc.err_msg = fmt::format("The bit_strings array contains no mappings (is all null/unknown/ignored)");
            return false;
        }

        if (bit_field && !check_bit_field(err_loc)) return false;
        if (individual_bits && !check_individual_bits(err_loc)) return false;
        if (Enum && !check_enums(err_loc)) return false;
        // experimental (for the future):
        // if (enum_field && !check_enum_field(err_loc)) return false;
        // if (individual_enums && !check_individual_enums(err_loc)) return false;
        return true;
    }

    // TODO(WALKER): In the future make sure all of these account for "ignored" bits as well

    bool check_bit_field(Error_Location& err_loc)
    {
        const auto max_num_bits = size * 16UL;
        if (bit_strings.size() > max_num_bits)
        {
            err_loc.err_msg = fmt::format("The maximum number of bit_strings (currently: {}) for a register of this size (currently: {}) has been exceeded", max_num_bits, size);
            return false;
        }

        // NOTE(WALKER): Dupes are allowed in certain cases, so this check is commented out for now
        // If in the future it is needed again, simply uncomment it.

        // std::unordered_map<std::string_view, bool> taken_map;
        // for (const auto& bit_str : bit_strings)
        // {
        //     if (bit_str.is_unknown || bit_str.num_ignored > 0) {++err_loc.bit_strings_idx; continue; }
        //     if (taken_map[bit_str.id])
        //     {
        //         err_loc.err_msg = fmt::format("bit_field string \"{}\" has already been mapped", bit_str.id);
        //         return false;
        //     }
        //     ++err_loc.bit_strings_idx;
        //     taken_map[bit_str.id] = true; // claim it
        // }

        return true;
    }

    bool check_individual_bits(Error_Location& err_loc)
    {
        const auto max_num_bits = size * 16;
        if (bit_strings.size() > max_num_bits)
        {
            err_loc.err_msg = fmt::format("The maximum number of bit_strings (currently: {}) for a register of this size (currently: {}) has been exceeded", max_num_bits, size);
            return false;
        }

        std::unordered_map<std::string_view, bool> taken_map;
        for (const auto& bit_str : bit_strings)
        {
            if (bit_str.is_unknown || bit_str.num_ignored > 0) { ++err_loc.bit_strings_idx; continue; }
            if (taken_map[bit_str.id])
            {
                err_loc.err_msg = fmt::format("individual_bits string \"{}\" has already been mapped", bit_str.id);
                return false;
            }
            ++err_loc.bit_strings_idx;
            taken_map[bit_str.id] = true; // claim it
        }

        return true;
    }

    bool check_enums(Error_Location& err_loc)
    {
        // std::unordered_map<std::string_view, bool> string_taken_map;
        std::unordered_map<u64, bool> val_taken_map;

        if (bit_strings.size() > Max_Num_Enums)
        {
            err_loc.err_msg = fmt::format("The number of enums (currently: {}) has exceeded the maximum of {}", bit_strings.size(), Max_Num_Enums);
            return false;
        }

        // sort enums in ascending order:
        std::sort(bit_strings.begin(), bit_strings.end());

        for (const auto& enum_str : bit_strings)
        {
            // dupe value check:
            if (val_taken_map[enum_str.enum_pair.enum_val])
            {
                err_loc.err_msg = fmt::format("enum value {} has already been mapped", enum_str.enum_pair.enum_val);
                return false;
            }
            val_taken_map[enum_str.enum_pair.enum_val] = true; // claim value

            // NOTE(WALKER): For now this check doesn't exist
            // but if in the future we need it then uncomment it and the map above

            // dupe string check:
            // if (string_taken_map[enum_str.enum_pair.enum_string])
            // {
            //     err_loc.err_msg = fmt::format("enum string \"{}\" has already been mapped", enum_str.enum_pair.enum_string);
            //     return false;
            // }
            // string_taken_map[enum_str.enum_pair.enum_string] = true; // claim string

            ++err_loc.bit_strings_idx;
        }
        return true;
    }

    // bool check_enum_field(Error_Location& err_loc)
    // {
    //     return true;
    // }

    // bool check_individual_enums(Error_Location& err_loc)
    // {
    //     return true;
    // }

    void compress_bit_strings()
    {
        if (bit_field) compress_bit_field();
        if (individual_bits) compress_individual_bits();
        if (Enum) compress_enums();
        // Experimental (to be used in the future):
        // if (enum_field) compress_enum_field();
        // if (individual_enums) compress_individual_enums();
    }

    void compress_bit_field()
    {
        u64 last_bit_idx = 0;
        u64 curr_bit_idx = 0;
        u64 unknown_counter = 0;
        u64 ignored_counter = 0;
        for (const auto& bit_str : bit_strings)
        {
            if (bit_str.is_unknown)
            {
                if (ignored_counter > 0)
                {
                    auto& curr_bit_str = compressed_bit_strings.emplace_back();
                    curr_bit_str.is_ignored = true;
                    curr_bit_str.begin_bit = last_bit_idx;
                    curr_bit_str.end_bit = bit_str.begin_bit - 1;
                    ignored_counter = 0;
                    last_bit_idx = bit_str.end_bit + 1;
                }
                ++unknown_counter;
                ++curr_bit_idx;
            }
            else if (bit_str.num_ignored > 0)
            {
                if (unknown_counter > 0)
                {
                    auto& curr_bit_str = compressed_bit_strings.emplace_back();
                    curr_bit_str.is_unknown = true;
                    curr_bit_str.begin_bit = last_bit_idx;
                    curr_bit_str.end_bit = bit_str.begin_bit - 1;
                    unknown_counter = 0;
                    last_bit_idx = bit_str.end_bit + 1;
                }
                ignored_counter += bit_str.num_ignored;
                curr_bit_idx += bit_str.num_ignored;
            }
            else // normal:
            {
                if (unknown_counter > 0)
                {
                    auto& curr_bit_str = compressed_bit_strings.emplace_back();
                    curr_bit_str.is_unknown = true;
                    curr_bit_str.begin_bit = last_bit_idx;
                    curr_bit_str.end_bit = bit_str.begin_bit - 1;
                    unknown_counter = 0;
                }
                if (ignored_counter > 0)
                {
                    auto& curr_bit_str = compressed_bit_strings.emplace_back();
                    curr_bit_str.is_ignored = true;
                    curr_bit_str.begin_bit = last_bit_idx;
                    curr_bit_str.end_bit = bit_str.begin_bit - 1;
                    ignored_counter = 0;
                }
                auto& curr_bit_str = compressed_bit_strings.emplace_back();
                curr_bit_str.begin_bit = bit_str.begin_bit;
                curr_bit_str.end_bit = bit_str.end_bit;
                curr_bit_str.str = bit_str.id;
                last_bit_idx = bit_str.end_bit + 1;
                ++curr_bit_idx;
            }
        }
        // take care of last bit of "ignored" bits if we have them at the end ("unknown" is automatic at the end anyway):
        if (ignored_counter > 0)
        {
            auto& curr_bit_str = compressed_bit_strings.emplace_back();
            curr_bit_str.is_ignored = true;
            curr_bit_str.begin_bit = last_bit_idx;
            curr_bit_str.end_bit = curr_bit_idx - 1;
            ignored_counter = 0;
        }
    }

    void compress_individual_bits()
    {
        for (const auto& bit_str : bit_strings)
        {
            if (bit_str.num_ignored > 0) continue;
            auto& curr_bit_str = compressed_bit_strings.emplace_back();
            curr_bit_str.begin_bit = bit_str.begin_bit;
            curr_bit_str.end_bit = bit_str.end_bit;
            curr_bit_str.str = bit_str.id;
        }
    }

    void compress_enums()
    {
        for (const auto& bit_str : bit_strings)
        {
            auto& curr_bit_str = compressed_bit_strings.emplace_back();
            curr_bit_str.enum_val = bit_str.enum_pair.enum_val;
            curr_bit_str.str = bit_str.enum_pair.enum_string;
        }
    }

    // void compress_enum_field()
    // {
    //     for (const auto& bit_str : bit_strings)
    //     {

    //     }
    // }

    // void compress_individual_enums()
    // {
    //     for (const auto& bit_str : bit_strings)
    //     {

    //     }
    // }
};

}

// formatters:

template<>
struct fmt::formatter<config_loader::Compressed_Bit_String>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template<typename FormatContext>
    auto format(const config_loader::Compressed_Bit_String& bs, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), R"({{ "begin_bit": {}, "end_bit": {}, "string/id": "{}", "enum_value": {}, "is_unknown": {}, "is_ignored": {} }})", bs.begin_bit, bs.end_bit, bs.str, bs.enum_val, bs.is_unknown, bs.is_ignored);
    }
};

template<>
struct fmt::formatter<config_loader::Bit_String>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template<typename FormatContext>
    auto format(const config_loader::Bit_String& bs, FormatContext& ctx) const
    {
        // null format (change this in the future):
        if (bs.is_unknown || bs.num_ignored > 0) return fmt::format_to(ctx.out(), "null");
        // ignore format (TODO: in the future, use this):
        // if (bs.num_ignored > 0) return fmt::format_to(ctx.out(), R"({{"IGNORE":{}}})", bs.num_ignored);
        // regular string:
        if (!bs.id.empty()) return fmt::format_to(ctx.out(), R"("{}")", bs.id);
        // enum key/string pairs (change this in the future):
        return fmt::format_to(ctx.out(), R"({{"value": {}, "string": "{}"}})", bs.enum_pair.enum_val, bs.enum_pair.enum_string);
        // if (!bs.enum_pair.enum_string.empty()) return fmt::format_to(ctx.out(), R"({{"value": {}, "string": "{}"}})", bs.enum_pair.enum_val, bs.enum_pair.enum_string);
        // TODO(WALKER), finish this when I get around to individual_enum/enum_field:
        // if (!bs.enum_strings.empty())
        // {

        // }
    }
};

template<>
struct fmt::formatter<config_loader::Decode>
{
    char presentation = 'c'; // default to client formatting

    constexpr auto parse(format_parse_context& ctx)
    {
        // Parse the presentation format and store it in the formatter:
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 's')) presentation = *it; // only accept c or s (no errors please)

        return end;
    }

    template<typename FormatContext>
    auto format(const config_loader::Decode& dc, FormatContext& ctx) const
    {
        if (presentation == 'c') // client style (default):
        {
            return fmt::format_to(ctx.out(), R"(
                        {{ "id": "{}", "offset": {}, "size": {}, "scale": {}, "shift": {}, "starting_bit_pos": {}, "number_of_bits": {}, "multi_write_op_code": {}, "signed": {}, "float": {}, "invert_mask": "{}", "bit_field": {}, "individual_bits": {}, "random_enum": {}, "uses_masks": {}, "packed_register": {},"bit_strings": [{}], "compressed_bit_strings": [{}] }})", 
                            dc.id,
                            dc.offset,
                            dc.size,
                            dc.scale,
                            dc.shift,
                            dc.starting_bit_pos,
                            dc.number_of_bits,
                            dc.multi_write_op_code,
                            dc.Signed,
                            dc.Float,
                            dc.invert_mask_str,
                            dc.bit_field,
                            dc.individual_bits,
                            dc.Enum,
                            dc.uses_masks,
                            dc.packed_register,
                            fmt::join(dc.bit_strings, ", "),
                            fmt::join(dc.compressed_bit_strings, ", "));
        }
        else // server style (includes uri):
        {
            return fmt::format_to(ctx.out(), R"(
            {{ "id": "{}", "uri": "{}", "offset": {}, "size": {}, "scale": {}, "shift": {}, "starting_bit_pos": {}, "number_of_bits": {},"multi_write_op_code": {}, "signed": {}, "float": {}, "invert_mask": "{}", "bit_field": {}, "individual_bits": {}, "random_enum": {}, "uses_masks": {}, "packed_register": {}, "bit_strings": [{}], "compressed_bit_strings": [{}] }})", 
                dc.id,
                dc.uri,
                dc.offset,
                dc.size,
                dc.scale,
                dc.shift,
                dc.starting_bit_pos,
                dc.number_of_bits,
                dc.multi_write_op_code,
                dc.Signed,
                dc.Float,
                dc.invert_mask_str,
                dc.bit_field,
                dc.individual_bits,
                dc.Enum,
                dc.uses_masks,
                dc.packed_register,
                fmt::join(dc.bit_strings, ", "),
                fmt::join(dc.compressed_bit_strings, ", "));
        }
    }
};
