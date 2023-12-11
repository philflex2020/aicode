#pragma once

#include <string_view>
#include <charconv>

#include "modbus/modbus.h" // for modbus connection

#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/bundled/format.h"
#include "parallel_hashmap/phmap.h"

#include "simdjson_noexcept.hpp"

#include "fims/libfims.h"

#include "fims/defer.hpp"
#include "Jval_buif.hpp"
#include "string_storage.hpp"

using namespace std::string_view_literals;

inline constexpr uint8_t bit_all_index = 64; // for setting/getting "individual_bits". This set means it is a regular register or is the uri that belongs to all the bits for that register
inline constexpr uint8_t enum_all_index = std::numeric_limits<uint8_t>::max(); // this is set to all in the case where we don't really use "enum_id" (NOT really used, but could be useful for consistency)
inline constexpr uint8_t max_enum_bit_strings_allowed = std::numeric_limits<uint8_t>::max(); // maximum number of "bit strings" (and/or nested "enum strings") allowed in the config
inline constexpr auto ignore_keyword = "IGNORE"sv; // this is the keyword we look for in "bit strings" to ignore certain bits for "bit_fields".
inline constexpr auto forbidden_chars = R"({ \/"%})"sv; // these are characters that are forbidden from begin in ids (or "bit strings" for "individual bits" and "individual enums")
inline constexpr auto forbidden_chars_err_str = R"('%', '{', '}', '\', '/', '"', or ' ')"sv; // error string for forbidden chars


#define NEW_FPS_ERROR_PRINT(fmt_str, ...) fmt::print(stderr, FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define NEW_FPS_ERROR_PRINT_NO_COMPILE(fmt_str, ...) fmt::print(stderr, fmt_str, ##__VA_ARGS__) 
#define NEW_FPS_ERROR_PRINT_NO_ARGS(fmt_str) fmt::print(stderr, FMT_COMPILE(fmt_str))

// helper functions:

auto str_view_hash(const std::string_view view) noexcept
{
    return std::hash<std::string_view>{}(view);
}

// returns a 64 bit mask from begin_bit to end_bit (produces arbirary 64 bit masks)
// NOTE(WALKER): Begin bit starts at 0 (0 -> 63) NOT (1 -> 64)
// CREDIT: https://www.geeksforgeeks.org/set-bits-given-range-number/
constexpr uint64_t gen_mask(const uint8_t begin_bit, const uint8_t end_bit) noexcept
{
    return (((1UL << (begin_bit)) - 1UL) ^ ((1UL << (end_bit + 1UL)) - 1UL));
}

// this returns whether or not a string ends with the specified string (used in listener)
// this is taken straight from the C++ standard library and outfitted to work between two strings for C++17
constexpr bool str_ends_with(const std::string_view str, const std::string_view end_str) noexcept
{
    const auto str_len = str.size();
    const auto end_str_len = end_str.size();
    return str_len >= end_str_len
        && std::string_view::traits_type::compare(str.end() - end_str_len, end_str.data(), end_str_len) == 0;
}

// NOTE(WALKER): use these in combination with send_event for proper severity levels
enum Event_Severity : uint8_t
{
    Debug = 0,
    Info = 1,
    Status = 2,
    Alarm = 3,
    Fault = 4
};
// [DEBUG, INFO, STATUS, ALARM, FAULT] 0..4 (from Kyle on slack thread)

// this is for when things go bad
bool emit_event(fims& fims_gateway, const std::string_view source, const std::string_view message, Event_Severity severity)
{
    fmt::basic_memory_buffer<char, 100> send_buf;
    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"source":"{}","message":"{}","severity":{}}})"), source, message, severity);

    return fims_gateway.send_post("/events", std::string_view{send_buf.data(), send_buf.size()});
}

struct decode_flags
{
    uint16_t flags = 0;

    // add more indices as needed (increase size type of flags as needed as well):
    // IMPORTANT(WALKER): Make sure these bits are always next to each other:
    static constexpr uint8_t is_size_one_index = 0;
    static constexpr uint8_t is_size_two_index = 1;
    static constexpr uint8_t is_size_four_index = 2;
    static constexpr uint16_t size_mask = 0b111; // size mask for get_size(): DO NOT CHANGE THIS OR THE ABOVE INDICES
    // IMPORTANT(WALKER): DO NOT change the above flags, they are necessary for the get_size() method to return a proper whole number
    static constexpr uint8_t is_word_swapped_index = 3;
    static constexpr uint8_t is_signed_index = 4;
    static constexpr uint8_t is_float_index = 5;
    // these are for "bit_strings":
    static constexpr uint8_t is_individual_bits_index = 6;
    static constexpr uint8_t is_individual_enums_index = 7;
    static constexpr uint8_t is_bit_field_index = 8;
    static constexpr uint8_t is_enum_field_index = 9;
    static constexpr uint8_t is_enum_index = 10; // this doubles as both "enum" and "random_enum"
    static constexpr uint16_t goes_into_bits_mask = (1UL << is_individual_bits_index) | 
                                                    (1UL << is_bit_field_index) |
                                                    (1UL << is_individual_enums_index) | 
                                                    (1UL << is_enum_field_index); // goes_into_bits_mask for is_goes_into_bits_type() 
    static constexpr uint16_t bit_strings_mask = (1UL << is_individual_bits_index) | 
                                                    (1UL << is_bit_field_index) | 
                                                    (1UL << is_individual_enums_index) | 
                                                    (1UL << is_enum_field_index) | 
                                                    (1UL << is_enum_index); // bit_strings_mask for is_bit_string_type()

    // setters (only called once during the config phase):
    constexpr void set_size(const int64_t size) noexcept
    {
        if (size == 2)
        {
            flags |= (1 << is_size_two_index);
        }
        else if (size == 4)
        {
            flags |= (1 << is_size_four_index);
        }
        else // size of 1 by default
        {
            flags |= (1 << is_size_one_index);
        }
    }
    constexpr void set_word_swapped(const bool is_word_swapped) noexcept
    {
        flags |= (1 & is_word_swapped) << is_word_swapped_index;
    }
    constexpr void set_signed(const bool is_signed) noexcept
    {
        flags |= (1 & is_signed) << is_signed_index;
    }
    constexpr void set_float(const bool is_float) noexcept
    {
        flags |= (1 & is_float) << is_float_index;
    }
    // these use "bit_strings":
    constexpr void set_individual_bits(const bool is_individual_bits) noexcept
    {
        flags |= (1 & is_individual_bits) << is_individual_bits_index;
    }
    constexpr void set_individual_enums(const bool is_individual_enums) noexcept
    {
        flags |= (1 & is_individual_enums) << is_individual_enums_index;
    }
    constexpr void set_bit_field(const bool is_bit_field) noexcept
    {
        flags |= (1 & is_bit_field) << is_bit_field_index;
    }
    constexpr void set_enum_field(const bool is_enum_field) noexcept
    {
        flags |= (1 & is_enum_field) << is_enum_field_index;
    }
    constexpr void set_enum(const bool is_enum) noexcept
    {
        flags |= (1 & is_enum) << is_enum_index;
    }

    // getters:
    constexpr uint8_t get_size() const noexcept // this returns size as a whole number (1, 2, or 4)
    {
        return (flags & size_mask); // mask out the first three bits
    }
    constexpr bool is_size_one() const noexcept
    {
        return (flags >> is_size_one_index) & 1;
    }
    constexpr bool is_size_two() const noexcept
    {
        return (flags >> is_size_two_index) & 1;
    }
    constexpr bool is_size_four() const noexcept
    {
        return (flags >> is_size_four_index) & 1;
    }
    constexpr bool is_word_swapped() const noexcept
    {
        return (flags >> is_word_swapped_index) & 1;
    }
    constexpr bool is_signed() const noexcept
    {
        return (flags >> is_signed_index) & 1;
    }
    constexpr bool is_float() const noexcept
    {
        return (flags >> is_float_index) & 1;
    }
    // all these below use "bit_strings":
    constexpr bool is_individual_bits() const noexcept
    {
        return (flags >> is_individual_bits_index) & 1;
    }
    constexpr bool is_individual_enums() const noexcept
    {
        return (flags >> is_individual_enums_index) & 1;
    }
    constexpr bool is_bit_field() const noexcept
    {
        return (flags >> is_bit_field_index) & 1;
    }
    constexpr bool is_enum_field() const noexcept
    {
        return (flags >> is_enum_field_index) & 1;
    }
    constexpr bool is_enum() const noexcept
    {
        return (flags >> is_enum_index) & 1;
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
// auto x = sizeof(decode_flags);

struct decode_info
{
    uint8_t bit_str_index = 126; //  126 means no bit_str for this decode
    uint16_t offset{};
    decode_flags flags{};
    uint64_t invert_mask{};
    double scale{};
    int64_t shift{};
};
// size info:
// auto x = sizeof(decode_info);

struct enum_bit_string_info
{
    uint8_t begin_bit; // for generating masks and shifting
    uint8_t end_bit; // for generating masks
    uint8_t last_index; // when iterating over the array if we arrive at the last index for this enum (bit range) then we know it is "unknown" and we can't account for this enum value
    Str_Storage_Handle enum_bit_string; // handle that indexes into str_storage
    uint64_t care_mask; // which bits in the bits range we need we care about (for the case where bits are split)
    uint64_t enum_bit_val = 1UL; // the whole number we expect if we use "enum"/"random_enum" or enums that are stuffed inside of "bit_field"s (for single bits it is 1UL)
};
// size info:
// auto x = sizeof(enum_bit_string_info);

struct enum_bit_string_info_vec
{
    uint64_t care_mask = std::numeric_limits<uint64_t>::max(); // for masking out the bits that we are about (so we don't count bits we don't care about as changing)
    uint64_t changed_mask = 0; // for when we have "bits" that we want to check for change or not. Really used for "individual_bits", "individual_enums", and "enum_field"
    std::vector<enum_bit_string_info> vec;
};

constexpr void decode(const uint16_t* raw_registers, const decode_info& current_decode, Jval_buif& current_jval, const uint64_t care_mask = std::numeric_limits<uint64_t>::max()) noexcept
{
    uint64_t current_unsigned_val = 0UL;
    int64_t current_signed_val = 0;
    double current_float_val = 0.0;

    if (current_decode.flags.is_size_one())
    {
        current_unsigned_val = raw_registers[0];
        // invert mask stuff:
        current_unsigned_val ^= current_decode.invert_mask;
        // do signed stuff (size 1 cannot be float):
        if (current_decode.flags.is_signed())
        {
            int16_t to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
    }
    else if (current_decode.flags.is_size_two())
    {
        if (!current_decode.flags.is_word_swapped())
        {
            current_unsigned_val = (static_cast<uint64_t>(raw_registers[0]) << 16) +
                                        (static_cast<uint64_t>(raw_registers[1]) << 0);
        }
        else
        {
            current_unsigned_val = (static_cast<uint64_t>(raw_registers[0]) << 0) + 
                                        (static_cast<uint64_t>(raw_registers[1]) << 16);
        }
        // invert mask stuff:
        current_unsigned_val ^= current_decode.invert_mask;
        // do signed/float stuff:
        if (current_decode.flags.is_signed())
        {
            int32_t to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            float to_reinterpret = 0.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }
    else // size of 4:
    {
        if (!current_decode.flags.is_word_swapped())
        {
            current_unsigned_val = (static_cast<uint64_t>(raw_registers[0]) << 48) +
                                        (static_cast<uint64_t>(raw_registers[1]) << 32) +
                                        (static_cast<uint64_t>(raw_registers[2]) << 16) +
                                        (static_cast<uint64_t>(raw_registers[3]) << 0);
        }
        else
        {
            current_unsigned_val = (static_cast<uint64_t>(raw_registers[0]) << 0) +
                                        (static_cast<uint64_t>(raw_registers[1]) << 16) +
                                        (static_cast<uint64_t>(raw_registers[2]) << 32) +
                                        (static_cast<uint64_t>(raw_registers[3]) << 48);
        }
        // invert mask stuff:
        current_unsigned_val ^= current_decode.invert_mask;
        // do signed/float stuff:
        if (current_decode.flags.is_signed()) // this is only for whole numbers really (signed but not really float):
        {
            int64_t to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            double to_reinterpret = 0.0;
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
        current_float_val += current_decode.shift; // add shift
        current_jval = current_float_val;
    }
    else if (!current_decode.flags.is_signed()) // unsigned whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_unsigned_val += current_decode.shift; // add shift
            current_jval = current_unsigned_val;
        }
        else
        {
            auto scaled = static_cast<double>(current_unsigned_val) / current_decode.scale;
            scaled += current_decode.shift; // add shift
            current_jval = scaled;
        }
    }
    else // signed whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_signed_val += current_decode.shift; // add shift
            current_jval = current_signed_val;
        }
        else
        {
            auto scaled = static_cast<double>(current_signed_val) / current_decode.scale;
            scaled += current_decode.shift; // add shift
            current_jval = scaled;
        }
    }
}

constexpr void encode(uint16_t* raw_registers, const decode_info& current_decode, const Jval_buif& to_encode, 
    const uint8_t bit_id = bit_all_index, const uint64_t enum_str_care_mask = 0UL, const uint64_t decoded_unsigned_val = 0UL) noexcept
{
    uint64_t current_unsigned_val = 0;
    int64_t current_signed_val = 0;
    double current_float_val = 0.0;

    if (bit_id == bit_all_index) // normal set:
    {
        // extract out set_val's value (signed check, float check, shift, scaling, etc.):
        if (to_encode.holds_uint())
        {
            current_unsigned_val = to_encode.get_uint_unsafe();
            current_unsigned_val -= current_decode.shift;
            if (current_decode.scale) // scale != 0
            {
                current_unsigned_val *= current_decode.scale;
            }
        }
        else if (to_encode.holds_int()) // negative values only:
        {
            current_signed_val = to_encode.get_int_unsafe();
            current_signed_val -= current_decode.shift;
            if (current_decode.scale) // scale != 0
            {
                current_unsigned_val *= current_decode.scale;
            }
        }
        else // float:
        {
            current_float_val = to_encode.get_float_unsafe();
            current_float_val -= current_decode.shift;
            if (current_decode.scale) // scale != 0
            {
                current_float_val *= current_decode.scale;
            }
        }
    }
    else // set a particular bit/bits (leaving all other bits we don't care about unchanged):
    {
        auto current_care_mask = 1UL; // set to 1 for "individual_bits"
        if (current_decode.flags.is_individual_enums()) 
        {
            current_care_mask = enum_str_care_mask;
        }
        current_unsigned_val = decoded_unsigned_val; // store the previous decode jval's uint here so we don't overwrite too much
        // NOTE(WALKER): In the future, if we need to check for proper bits being set, etc. then this might need to change. However, "overflow" protection is not the primary concern of modbus_client
        // so if they set a larger value than is within the range of these bits then that is their own fault (or they change the bits of an enum in the current range by accident (like the "inbetween" bits) -> maybe they have a situation where they want to do this on purpose)
        current_unsigned_val = (current_unsigned_val & ~(current_care_mask << bit_id)) | (to_encode.get_uint_unsafe() << bit_id);
    }

    // set registers based on size:
    if (current_decode.flags.is_size_one())
    {
        uint16_t current_u16_val = 0;
        if (to_encode.holds_uint())
        {
            current_u16_val = static_cast<uint16_t>(current_unsigned_val);
        }
        else if (to_encode.holds_int())
        {
            current_u16_val = static_cast<uint16_t>(current_signed_val);
        }
        else // float:
        {
            if (!current_decode.flags.is_signed()) // unsigned:
            {
                current_u16_val = static_cast<uint16_t>(current_float_val);
            }
            else // signed:
            {
                current_u16_val = static_cast<uint16_t>(static_cast<int16_t>(current_float_val));
            }
        }
        current_u16_val ^= current_decode.invert_mask; // invert before byte swapping and sending out
        raw_registers[0] = current_u16_val;
    }
    else if (current_decode.flags.is_size_two())
    {
        uint32_t current_u32_val = 0U;
        if (to_encode.holds_uint())
        {
            if (!current_decode.flags.is_float())
            {
                current_u32_val = static_cast<uint32_t>(current_unsigned_val);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<float>(current_unsigned_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else if (to_encode.holds_int())
        {
            if (!current_decode.flags.is_float())
            {
                current_u32_val = static_cast<uint32_t>(current_signed_val);
            }
            else // is_float:
            {
                const auto current_float32_val = static_cast<float>(current_signed_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
        }
        else // float:
        {
            if (current_decode.flags.is_float())
            {
                const auto current_float32_val = static_cast<float>(current_float_val);
                memcpy(&current_u32_val, &current_float32_val, sizeof(current_u32_val));
            }
            else if (!current_decode.flags.is_signed())
            {
                current_u32_val = static_cast<uint32_t>(current_float_val);
            }
            else // signed:
            {
                current_u32_val = static_cast<uint32_t>(static_cast<int32_t>(current_float_val));
            }
        }
        current_u32_val ^= current_decode.invert_mask; // invert before word swapping and sending out
        if (!current_decode.flags.is_word_swapped())
        {
            raw_registers[0] = current_u32_val >> 16;
            raw_registers[1] = current_u32_val >> 0;
        }
        else
        {
            raw_registers[0] = current_u32_val >> 0;
            raw_registers[1] = current_u32_val >> 16;
        }
    }
    else // size 4:
    {
        uint64_t current_u64_val = 0UL;
        if (to_encode.holds_uint())
        {
            if (!current_decode.flags.is_float())
            {
                current_u64_val = current_unsigned_val;
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<double>(current_unsigned_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_u64_val));
            }
        }
        else if (to_encode.holds_int())
        {
            if (!current_decode.flags.is_float())
            {
                current_u64_val = static_cast<uint64_t>(current_signed_val);
            }
            else // is_float:
            {
                const auto current_float64_val = static_cast<double>(current_signed_val);
                memcpy(&current_u64_val, &current_float64_val, sizeof(current_u64_val));
            }
        }
        else // float:
        {
            if (current_decode.flags.is_float())
            {
                memcpy(&current_u64_val, &current_float_val, sizeof(current_u64_val));
            }
            else if (!current_decode.flags.is_signed())
            {
                current_u64_val = static_cast<uint64_t>(current_float_val);
            }
            else // signed:
            {
                current_u64_val = static_cast<uint64_t>(static_cast<int64_t>(current_float_val));
            }
        }
        current_u64_val ^= current_decode.invert_mask;
        if (!current_decode.flags.is_word_swapped())
        {
            raw_registers[0] = current_u64_val >> 48;
            raw_registers[1] = current_u64_val >> 32;
            raw_registers[2] = current_u64_val >> 16;
            raw_registers[3] = current_u64_val >> 0;
        }
        else // word swap:
        {
            raw_registers[0] = current_u64_val >> 0;
            raw_registers[1] = current_u64_val >> 16;
            raw_registers[2] = current_u64_val >> 32;
            raw_registers[3] = current_u64_val >> 48;
        }
    }
}




// config related stuff:

struct config_enum_bit_str
{
    uint64_t enum_val;
    std::string enum_str;
};
// temp formatter for debug purposes (not in the final product):
template<>
struct fmt::formatter<config_enum_bit_str>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_enum_bit_str& ebs, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"(
        {{ "enum_val": {}, "enum_string": {} }})"), ebs.enum_val, ebs.enum_str);
    }
};

struct config_enum_bit_str_info
{
    std::string id; // for individual_enums, individual_bits, bit_field, enum
    uint8_t begin_bit{};
    uint8_t end_bit{};
    uint64_t enum_val{}; // for enum/random_enum
    uint64_t care_mask{}; // or ~ignore_mask -> by default this is = gen_mask(begin_bit, end_bit) >> begin_bit
    std::vector<config_enum_bit_str> enum_strings;
};
// temp formatter for debug purposes (not in the final product):
template<>
struct fmt::formatter<config_enum_bit_str_info>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_enum_bit_str_info& ebsi, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"(
    {{ "id": "{}", "begin_bit": {}, "end_bit": {}, "enum_val": {}, "care_mask": {:b}, "enum_strings": [{}] }})"),
            ebsi.id, ebsi.begin_bit, ebsi.end_bit, ebsi.enum_val, ebsi.care_mask, fmt::join(ebsi.enum_strings, ","));
    }
};

struct config_bit_strings_array_info
{
    uint64_t final_care_mask{}; // we accumulate this as we go along
    std::vector<config_enum_bit_str_info> final_array;
};
// temp formatter for debug purposes (not in the final product):
template<>
struct fmt::formatter<config_bit_strings_array_info>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_bit_strings_array_info& bsai, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"({{ "final_care_mask": {:b}, "enum_bit_strings": [{}] }})"), 
            bsai.final_care_mask, fmt::join(bsai.final_array, ","));
    }
};

struct config_decode_info
{
    std::string id;
    std::string uri; // server only
    uint16_t offset{};
    uint8_t size = 1;
    double scale{};
    int64_t shift{};
    uint64_t invert_mask{};
    bool is_signed{};
    bool is_float{};
    // bit strings stuff:
    bool is_individual_bits{};
    bool is_bit_field{};
    bool is_individual_enums{};
    bool is_enum_field{};
    bool is_enum{};
    config_bit_strings_array_info bit_strings_array;

    // for sorting purposes:
    constexpr bool operator<(const config_decode_info& rhs) const noexcept
    {
        return offset < rhs.offset;
    }
};
// temp formatter for debug purposes (not in the final product):
template<>
struct fmt::formatter<config_decode_info>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_decode_info& dc, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"({{ "id": "{}", "offset": {}, "size": {}, "scale": {}, "shift": {}, "invert": {}, "signed": {}, "float": {}, "enum": {}, "bit_field": {}, "individual_bits": {}, "enum_field": {}, "individual_enums": {}, bit_strings_info: {} }})"), 
            dc.id, dc.offset, dc.size, dc.scale, dc.shift, dc.invert_mask, dc.is_signed, dc.is_float, dc.is_enum, dc.is_bit_field, dc. is_individual_bits, dc.is_enum_field, dc.is_individual_enums, dc.bit_strings_array);
    }
};

// basic config_range abstraction:
struct config_range
{
    int64_t begin{};
    int64_t end{};
    int64_t step = 1; // step of this range if it has one (used in component_offset_range and decode_offset_range)
    std::string_view replace_str;
};


enum class Register_Types : uint8_t
{
    Holding,
    Input,
    Coil,
    Discrete_Input
};
// formatter for Register_Types (used when we get a config request on client):
template<>
struct fmt::formatter<Register_Types>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Register_Types& rt, FormatContext& ctx) const
    {
        switch (rt)
        {
            case Register_Types::Holding:
                return fmt::format_to(ctx.out(), FMT_COMPILE("Holding"));
            case Register_Types::Input:
                return fmt::format_to(ctx.out(), FMT_COMPILE("Input"));
            case Register_Types::Coil:
                return fmt::format_to(ctx.out(), FMT_COMPILE("Coil"));
            default:
                return fmt::format_to(ctx.out(), FMT_COMPILE("Discrete Input"));
        }
    }
};


bool parse_decode_objects_array(simdjson::ondemand::array& json_array, std::vector<config_decode_info>& decode_array, const Register_Types reg_type, const bool is_off_by_negative_one = false, std::string_view main_uri = "", uint16_t* start_offset = nullptr, uint16_t* end_offset = nullptr, const config_range* decode_offset_range = nullptr) noexcept
{
    simdjson::error_code err;

    for (auto decode : json_array)
    {
        auto& current_decode_info = decode_array.emplace_back();

        bool decode_has_range_str = false; // this determines whether or not we need to check each "individual_x"'s "ids"/"bit_strings" for rangification (only in the case of individual_enums and individual_bits)

        if (err = decode.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get decode_object #{} for uri {}, err = {}\n", decode_array.size(), main_uri, simdjson::error_message(err));
            return false;
        }

        auto decode_obj = decode.get_object();
        if (err = decode_obj.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get decode object #{} as an object for uri {}, err = {}\n", decode_array.size(), main_uri, simdjson::error_message(err));
            return false;
        }

        if (main_uri.empty()) // this is for the server
        {
            if(decode_obj["uri"].get(main_uri))
            {
                NEW_FPS_ERROR_PRINT("can't get uri as a string for decode object #{} (previous uri = {}), err = {}\n", decode_array.size(), main_uri, simdjson::error_message(err));
                return false;
            }
            // TODO(WALKER): Depending on the server and how string storage is used
            // this check might need to be combined with the id check on the server side
            // we'll have to see
            if (main_uri.size() > 255)
            {
                NEW_FPS_ERROR_PRINT("uri: {} exceeds 255 characters, please reduce the string's size\n", main_uri);
                return false;
            }
            current_decode_info.uri = main_uri;
        }

        auto decode_id = decode_obj["id"].get_string();
        if (err = decode_id.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get id as a string inside decode object #{} for uri {}, err = {}\n", decode_array.size(), main_uri, simdjson::error_message(err));
            return false;
        }
        const auto decode_id_view = decode_id.value_unsafe();

        if (decode_id_view.empty())
        {
            NEW_FPS_ERROR_PRINT("decode id is empty inside decode object #{} for uri {}. Please provide an id\n", decode_array.size(), main_uri);
            return false;
        }
        if (decode_id_view.find_first_of(forbidden_chars) != std::string_view::npos)
        {
            NEW_FPS_ERROR_PRINT("decode id for uri {}/{} cannot contain the characters {} as they are reserved. Please remove them\n", main_uri, decode_id_view, forbidden_chars_err_str);
            return false;
        }
        // TODO(WALKER): When it comes time to rewrite the server make sure this check gets in for uri/id
        // instead of just id because that will go into string storage ultimately (that or uri and id are separate 
        // will have to think about it)
        if (decode_id_view.size() > 255)
        {
            NEW_FPS_ERROR_PRINT("decode is for uri {}/{} exceeds 255 characters, please reduce the ids string size\n", main_uri, decode_id_view);
            return false;
        }
        // TODO(WALKER): Get range checking done here as well:
        if (decode_offset_range && decode_id_view.find(decode_offset_range->replace_str) != std::string_view::npos)
        {
            decode_has_range_str = true; // this decoce is a range, if it is "individual_bits" or "individual_enums" then it is required that all of its "bit_strings" also be rangified so uris remain unique in the system
        }
        current_decode_info.id = decode_id_view;

        // offset:
        auto offset = decode_obj["offset"].get_int64();
        if (err = offset.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get offset as an int for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        const auto offset_val = offset.value_unsafe() - (1 * is_off_by_negative_one);

        if (offset_val < 0 || offset_val > std::numeric_limits<uint16_t>::max())
        {
            NEW_FPS_ERROR_PRINT("offset: {} is < 0 or > 65535 for uri {}/{}. Please make sure it is in the valid offset range\n", offset_val, main_uri, decode_id_view);
            return false;
        }
        current_decode_info.offset = static_cast<uint16_t>(offset_val);

        // set start_offset
        // NOTE(WALKER): Client only
        if (start_offset)
        {
            if (current_decode_info.offset < *start_offset)
            {
                *start_offset = current_decode_info.offset;
            }
        }

        auto size = decode_obj["size"].get_int64();
        if (err = size.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get size as an int for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!size.error())
        {
            const auto size_val = size.value_unsafe();
            if ((reg_type == Register_Types::Coil || reg_type == Register_Types::Discrete_Input) && size_val != 1) // cannot have size other than 1 for coils and discrete inputs
            {
                NEW_FPS_ERROR_PRINT("cannot have size other than 1 for uri {}/{} as the register mapping it belongs to is of type Coil or Discrete Input\n", main_uri, decode_id_view);
                return false;
            }
            if (!(size_val == 1 || size_val == 2 || size_val == 4))
            {
                NEW_FPS_ERROR_PRINT("size: {} is not 1, 2, or 4 for uri {}/{}. Only 1, 2 and 4 are accepted\n", size_val, main_uri, decode_id_view);
                return false;
            }
            if (offset_val + size_val - 1 > std::numeric_limits<uint16_t>::max())
            {
                NEW_FPS_ERROR_PRINT("offset + size: {} extends beyond the maximum register offset of 65535 for uri {}/{}.\n", offset_val + size_val - 1, main_uri, decode_id_view);
                return false;
            }
            current_decode_info.size = static_cast<uint8_t>(size_val);
        }

        // NOTE(WALKER): Client only
        if (end_offset)
        {
            // set end_offset
            if ((current_decode_info.offset + current_decode_info.size - 1) > *end_offset)
            {
                *end_offset = current_decode_info.offset + current_decode_info.size - 1;
            }

            // check for legitimate range for this mapping:
            if ((*end_offset - *start_offset + 1) > MODBUS_MAX_READ_REGISTERS)
            {
                NEW_FPS_ERROR_PRINT("The maximum number of registers of 215 has been exceeded at uri {}/{}. Please make this mapping smaller, or check for typos\n", main_uri, decode_id_view);
                return false;
            }
        }

        // don't bother decoding the rest of the stuff for coils or discrete inputs (only offset and size of 1 matters):
        if (reg_type == Register_Types::Coil || reg_type == Register_Types::Discrete_Input)
        {
            continue;
        }

        // scale:
        auto scale = decode_obj["scale"].get_double();
        if (err = scale.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get scale as a double/float64 for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!scale.error())
        {
            const auto scale_val = scale.value_unsafe();
            current_decode_info.scale = scale_val * (scale_val != 1.0); // scale of 1 might as well == 0
        }

        // shift:
        auto shift = decode_obj["shift"].get_int64();
        if (err = shift.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get shift as an int for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!shift.error())
        {
            const auto shift_val = shift.value_unsafe();
            current_decode_info.shift = shift_val;
        }

        // invert mask (special case -> hex/binary string, will use charconv for this instead of strtoull for better error checking):
        auto invert_mask = decode_obj["invert_mask"].get_string();
        if (err = invert_mask.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get invert_mask as a string for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!invert_mask.error())
        {
            const auto mask_view = invert_mask.value_unsafe();

            if (mask_view.size() < 2) // must contain at least two characters (0x or 0b)
            {
                NEW_FPS_ERROR_PRINT("for uri {}/{}, invert_mask is less than two characters (must begin with 0x)\n", main_uri, decode_id_view);
                return false;
            }

            std::string final_str;
            final_str = mask_view.substr(2);
            const auto mask_prefix = mask_view.substr(0,2);
            const auto is_hex = mask_prefix == "0x";
            const auto is_binary = mask_prefix == "0b";

            if (!(is_hex || is_binary) || final_str.empty())
            {
                NEW_FPS_ERROR_PRINT("invert_mask: {} for uri {}/{} is not a proper hex/binary string. It must begin with 0x or 0b, and must contain hex characters after 0x or binary characters after 0b\n", mask_view, main_uri, decode_id_view);
                return false;
            }

            uint64_t final_mask = 0;
            const auto [ptr, ec] = is_hex ? std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 16) : std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 2);

            if (ec == std::errc())
            {
                if (*ptr != '\0') // non-null-terminated end (found some invalid characters)
                {
                    NEW_FPS_ERROR_PRINT("the invalid hex/binary character: '{}' was found in invert_mask: {} for uri {}/{}. Please remove it\n", *ptr, mask_view, main_uri, decode_id_view);
                    return false;
                }

                if (final_mask > gen_mask(0, current_decode_info.size * 16 - 1))
                {
                    NEW_FPS_ERROR_PRINT("the invert_mask: {} for uri {}/{} is greater than {} bits and is out of range for this register (size of: {}). Please change it\n", mask_view, main_uri, decode_id_view, current_decode_info.size * 16, current_decode_info.size);
                    return false;
                }
                current_decode_info.invert_mask = final_mask; // success
            }
            else if (ec == std::errc::invalid_argument)
            {
                NEW_FPS_ERROR_PRINT("the invert_mask: {} for uri {}/{} is not a valid hex/binary string. Please provide a valid hex/binary string\n", mask_view, main_uri, decode_id_view);
                return false;
            }
            else if (ec == std::errc::result_out_of_range)
            {
                NEW_FPS_ERROR_PRINT("the invert_mask: {} for uri {}/{} is out of bounds for the absolute maximum number of bits: 64. Please remove the excess hex/binary characters\n", mask_view, main_uri, decode_id_view);
                return false;
            }
        }

        // signed:
        auto is_signed = decode_obj["signed"].get_bool();
        if (err = is_signed.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get signed as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_signed.error())
        {
            current_decode_info.is_signed = is_signed.value_unsafe();
        }

        // float:
        auto is_float = decode_obj["float"].get_bool();
        if (err = is_float.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get float as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_float.error())
        {
            current_decode_info.is_float = is_float.value_unsafe();

            if (current_decode_info.is_signed && current_decode_info.is_float) // can't have both signed and float for the same decode:
            {
                NEW_FPS_ERROR_PRINT("for uri {}/{}, both signed and float cannot be set to true. Please pick one\n", main_uri, decode_id_view);
                return false;
            }
            if (current_decode_info.size == 1)
            {
                NEW_FPS_ERROR_PRINT("for uri {}/{}, cannot have a size of 1 and be a float. Only sizes of 2 or 4 are allowed to be floats\n", main_uri, decode_id_view);
                return false;
            }
        }

        // "bit strings" stuff:
        uint8_t bit_string_type_counter = 0; // this cannot be > 1 at the end of the flags checks:

        // individual_bits:
        auto is_individual_bits = decode_obj["individual_bits"].get_bool();
        if (err = is_individual_bits.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get individual_bits as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_individual_bits.error())
        {
            current_decode_info.is_individual_bits = is_individual_bits.value_unsafe();
            bit_string_type_counter += 1 * current_decode_info.is_individual_bits;
        }

        // bit_field:
        auto is_bit_field = decode_obj["bit_field"].get_bool();
        if (err = is_bit_field.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get bit_field as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_bit_field.error())
        {
            current_decode_info.is_bit_field = is_bit_field.value_unsafe();
            bit_string_type_counter += 1 * current_decode_info.is_bit_field;
        }

        // individual_enums:
        auto is_individual_enums = decode_obj["individual_enums"].get_bool();
        if (err = is_individual_enums.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get individual_enums as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_individual_enums.error())
        {
            current_decode_info.is_individual_enums = is_individual_enums.value_unsafe();
            bit_string_type_counter += 1 * current_decode_info.is_individual_enums;
        }

        // enum_field:
        auto is_enum_field = decode_obj["enum_field"].get_bool();
        if (err = is_enum_field.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get enum_field as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_enum_field.error())
        {
            current_decode_info.is_enum_field = is_enum_field.value_unsafe();
            bit_string_type_counter += 1 * current_decode_info.is_enum_field;
        }

        // enum:
        auto is_enum = decode_obj["enum"].get_bool();
        if (err = is_enum.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get enum as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_enum.error())
        {
            current_decode_info.is_enum = is_enum.value_unsafe();
            bit_string_type_counter += 1 * current_decode_info.is_enum;
        }

        // "random_enum" (NOTE(WALKER): this also counts as "enum" in the new system, so this will just combine with enum by itself):
        auto is_random_enum = decode_obj["random_enum"].get_bool();
        if (err = is_random_enum.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get random_enum as a bool for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_random_enum.error())
        {
            bit_string_type_counter += 1 * is_random_enum.value_unsafe() * !current_decode_info.is_enum; // only add one if is_enum flag is already false
            current_decode_info.is_enum = is_random_enum.value_unsafe() || current_decode_info.is_enum; // keep true status if they have already labeled it as true through "enum" flag
        }

        if (bit_string_type_counter > 1) // can't have more than one bit_strings type
        {
            NEW_FPS_ERROR_PRINT("for uri {}/{}, only one bit_strings type flag can be labeled true among: individual_bits, bit_field, individual_enums, enum_field and enum/random_enum. Please pick one\n", main_uri, decode_id_view);
            return false;
        }
        else if (bit_string_type_counter == 1) // extract out "bit_strings"
        {
            if (current_decode_info.is_signed || current_decode_info.is_float || current_decode_info.scale) // cannot be signed, float or have scale
            {
                NEW_FPS_ERROR_PRINT("for uri {}/{}, a register that uses bit_strings is only allowed to be unsigned with no scale. signed, float, and scale must all be equal to false/0. Please remove them, or do not label this register as a bit string type\n", main_uri, decode_id_view);
                return false;
            }

            auto bit_str_array = decode_obj["bit_strings"].get_array();
            if (err = bit_str_array.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't get bit_strings as an array for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
                return false;
            }

            auto is_bit_str_array_empty = bit_str_array.is_empty();
            if (err = is_bit_str_array_empty.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't determine if bit_strings array is empty or not for uri {}/{}, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
                return false;
            }

            if (is_bit_str_array_empty.value_unsafe())
            {
                NEW_FPS_ERROR_PRINT("for uri {}/{}, bit_strings array is empty, even though a bit_strings type has been set to true, what are you smoking?\n", main_uri, decode_id_view);
                return false;
            }

            phmap::flat_hash_map<uint64_t, bool> enum_val_map; // for checking the existence of an enum value existing/being occupied
            phmap::flat_hash_map<std::string_view, bool> enum_str_map; // for checking the existence of an emum string existing/being occupied
            uint64_t current_enum_val = 0; // so we keep enums going along (and they can skip ahead) -> adds by one from the most recently set value
            const uint64_t max_enum_val = current_decode_info.size * static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()); // the maximum enum value we're allowed to have for a register of this size (uint16_t, uint32_t, or uint64_t)
            const uint8_t max_num_bits = current_decode_info.size * 16; // what the maximum number of bits is for this uri/register
            uint8_t current_bit_index = 0; // keeps track of which bit we are currently on for begin_bit and masking purposes (bit_fields, and individual_bits use this)
            uint8_t bit_str_counter = 0; // keeps track of the number of strings/objects in the bit_strings array (not going into enum_strings) -> first level only
            uint64_t overall_enum_bit_str_counter = 0; // this cannot reach the absolute max of 255

            if (current_decode_info.is_bit_field || current_decode_info.is_enum) // bit_field, and enum/random_enum
            {
                current_decode_info.bit_strings_array.final_care_mask = std::numeric_limits<uint64_t>::max(); // we care about all the bits
            }
            else // individual_bits, enum_field, and individual_enums
            {
                current_decode_info.bit_strings_array.final_care_mask = 0UL; // we only care about the bits we specify
            }

            // iterate over the array and do interpretation here:
            for (auto bit_str : bit_str_array)
            {
                defer { ++bit_str_counter; };
                if (bit_str_counter >= max_enum_bit_strings_allowed) // cannot exceed 255 bit strings absolutely (will increase this if needed)
                {
                    NEW_FPS_ERROR_PRINT("for uri {}/{}, the maximum number of bit_string strings/objects allowed: {}, has been exceeded. Please reduce the number of strings/objects in the bit_strings array, or increase the maximum number of bit_strings strings/objects allowed\n", main_uri, decode_id_view, max_enum_bit_strings_allowed);
                    return false;
                }

                auto& current_care_mask = current_decode_info.bit_strings_array.final_care_mask;
                auto& current_bit_str_arr = current_decode_info.bit_strings_array.final_array;
                auto& current_enum_bit_str_info = current_bit_str_arr.emplace_back();

                if (err = bit_str.error(); err)
                {
                    NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get bit_string string/object #{}, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                    return false;
                }

                if (current_decode_info.is_bit_field || current_decode_info.is_individual_bits)
                {
                    if (bit_str.is_null()) // null check
                    {
                        current_bit_str_arr.pop_back(); // remove false element
                        current_care_mask &= ~(1UL << current_bit_index); // set this particular bit to false/0 -> we don't care about it
                        ++current_bit_index;
                        if (current_bit_index > max_num_bits) // cannot exceed bits beyond your size
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the number of bit strings (after ignore indexing/null indexing) exceeds the number of bits ({}) for this register's size ({}). Please remove the excess bit strings\n", main_uri, decode_id_view, max_num_bits, current_decode_info.size);
                            return false;
                        }
                        continue;
                    }
                    auto current_bit_str = bit_str.get_string();
                    if (err = current_bit_str.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get a bit string as a string, err = {}\n", main_uri, decode_id_view, simdjson::error_message(err));
                        return false;
                    }
                    if (!current_bit_str.error())
                    {
                        const auto bit_str_view = current_bit_str.value_unsafe();
                        if (bit_str_view == ignore_keyword)
                        {
                            current_bit_str_arr.pop_back(); // remove false element
                            current_care_mask &= ~(1UL << current_bit_index); // set this particular bit to false/0 -> we don't care about it
                        }
                        else // this is a string we care about
                        {
                            if (current_decode_info.is_individual_bits) // individual_bits id checks:
                            {
                                if (bit_str_view.find_first_of(forbidden_chars) != std::string_view::npos) // cannot contain forbidden_chars for individual_bits
                                {
                                    NEW_FPS_ERROR_PRINT("for uri {}/{}, the bit string: {} for individual_bits contains the reserved characters: {}, please remove them\n", main_uri, decode_id_view, bit_str_view, forbidden_chars_err_str);
                                    return false;
                                }
                                if (decode_id_view == bit_str_view) // cannot have both the same base id and a bit_strings id for individual_bits
                                {
                                    NEW_FPS_ERROR_PRINT("for uri {}/{}, the bit string: {} for individual_bits would conflict with the base uri for this register. Please change one\n", main_uri, decode_id_view, bit_str_view);
                                    return false;
                                }
                                if (decode_has_range_str) // if this decode is rangified
                                {
                                    if (bit_str_view.find(decode_offset_range->replace_str) == std::string_view::npos) // then it has to have the same replace string in it as the base uri
                                    {
                                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get decode_offset_range's replace string: {} for individual_bits bit_string uri string: {}, even though this register's main uri is labeled one that is to be rangified. Please check for typos\n", main_uri, decode_id_view, decode_offset_range->replace_str, bit_str_view);
                                        return false;
                                    }
                                }
                            }
                            // everything checks out add the string:
                            current_enum_bit_str_info.id = bit_str_view;
                            current_enum_bit_str_info.begin_bit = current_bit_index;
                            current_enum_bit_str_info.end_bit = current_bit_index;
                            current_care_mask |= (1UL << current_bit_index); // we care about this bit
                        }
                        ++current_bit_index;
                        if (current_bit_index > max_num_bits) // cannot exceed bits beyond your size
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the number of bit strings (after ignore indexing) exceeds the number of bits ({}) for this register's size ({}). Please remove the excess bit strings\n", main_uri, decode_id_view, max_num_bits, current_decode_info.size);
                            return false;
                        }
                        continue;
                    }
                    auto current_bit_str_object = bit_str.get_object();
                    if (err = current_bit_str_object.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get bit_string #{} as null, a string, or an object, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    if (!current_bit_str_object.error())
                    {
                        auto ignore_bits = current_bit_str_object[ignore_keyword].get_uint64();
                        if (err = ignore_bits.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get the ignore keyword {}'s value as an unsigned integer inside of bit_string object #{} when trying to ignore bits for individual_bits/bit_field. Only {} is allowed to be a key in an object in this context, err = {}\n", main_uri, decode_id_view, ignore_keyword, bit_str_counter, ignore_keyword, simdjson::error_message(err));
                            return false;
                        }
                        const auto ignore_bits_val = ignore_bits.value_unsafe();
                        const uint64_t new_bits_index = current_bit_index + ignore_bits_val;
                        if (new_bits_index < current_bit_index || new_bits_index > max_num_bits) // maximum and overflow check
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, an ignore bits object inside the bit strings array would exceed past the maximum allowed bits for this uri/register ({}, size of {}). Please fix this\n", main_uri, decode_id_view, max_num_bits, current_decode_info.size);
                            return false;
                        }
                        current_bit_str_arr.pop_back(); // remove false element
                        const auto temp_ignore_mask = gen_mask(current_bit_index, new_bits_index - 1);
                        current_bit_index = new_bits_index; // skip the specified bits
                        current_care_mask &= ~temp_ignore_mask; // mask out the bits we don't care about
                    }
                }
                else if (current_decode_info.is_enum_field || current_decode_info.is_individual_enums)
                {
                    auto enum_str_object = bit_str.get_object();
                    if (err = enum_str_object.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get bit_string object #{} as an object for an enum_field/individual_enum, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }

                    if (current_decode_info.is_individual_enums) // only individual_enums have id, the rest is shared between both bit_strings types
                    {
                        defer { ++overall_enum_bit_str_counter; }; // increment counter for id slot for individual_enums
                        if (overall_enum_bit_str_counter >= max_enum_bit_strings_allowed)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the maximum number of bit_strings/enum_strings: {} has been reached. Please either increase the maximum allowed or decrease the number of bit_strings/enum_strings\n", main_uri, decode_id_view, bit_str_counter, max_enum_bit_strings_allowed);
                            return false;
                        }

                        auto enum_str_id = enum_str_object["id"].get_string();
                        if (err = enum_str_id.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get individual_enum #{}'s id as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                            return false;
                        }

                        const auto enum_str_id_view = enum_str_id.value_unsafe();

                        if (enum_str_id_view.empty())
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the individual_enums id string for object #{} is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                        if (enum_str_id_view.find_first_of(forbidden_chars) != std::string_view::npos) // cannot contain forbidden_chars for individual_bits
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the individual_enums id string: {} contains the reserved characters: {}, please remove them\n", main_uri, decode_id_view, enum_str_id_view, forbidden_chars_err_str);
                            return false;
                        }
                        if (decode_id_view == enum_str_id_view) // cannot have both the same base id and a enum_strings id for individual_enums
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the individual_eums id string: {} would conflict with the base uri for this register. Please change one\n", main_uri, decode_id_view, enum_str_id_view);
                            return false;
                        }
                        if (decode_has_range_str) // if this decode is rangified
                        {
                            if (enum_str_id_view.find(decode_offset_range->replace_str) == std::string_view::npos) // then it has to have the same replace string in it as the base uri
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{}, can't find decode_offset_range's replace string: {} for individual_enums id uri string: {}, even though this register's main uri is labeled one that is to be rangified. Please check for typos\n", main_uri, decode_id_view, decode_offset_range->replace_str, enum_str_id_view);
                                return false;
                            }
                        }
                        // everything checks out put it in the current_enum_bit_str_info
                        current_enum_bit_str_info.id = enum_str_id_view;
                    }

                    auto enum_str_begin_bit = enum_str_object["begin_bit"].get_uint64();
                    if (err = enum_str_begin_bit.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get individual_enum/enum_field #{}'s begin_bit as an unsigned integer, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    auto enum_str_end_bit = enum_str_object["end_bit"].get_uint64();
                    if (err = enum_str_end_bit.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get individual_enum/enum_field #{}'s end_bit as an unsigned integer, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    const auto begin_bit_val = enum_str_begin_bit.value_unsafe();
                    const auto end_bit_val = enum_str_end_bit.value_unsafe();
                    if (begin_bit_val >= max_num_bits || end_bit_val >= max_num_bits) // out of bounds check:
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, individual_enum/enum_field object #{}'s begin_bit or end_bit would exceed the maximum number of bits ({}) for this uri/register's size ({}). NOTE: bit indexes start at 0, NOT 1. Please fix this bit range\n", main_uri, decode_id_view, bit_str_counter, max_num_bits, current_decode_info.size);
                        return false;
                    }
                    if (end_bit_val < begin_bit_val) // proper bit range check:
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, individual_enum/enum_field object #{}'s end_bit ({}) is < begin_bit ({}). Please fix this\n", main_uri, decode_id_view, bit_str_counter, end_bit_val, begin_bit_val);
                        return false;
                    }
                    auto current_enum_mask = gen_mask(begin_bit_val, end_bit_val) >> begin_bit_val; // default care_mask unless they specify one of the two below:

                    // NOTE(WALKER): these two are optional:
                    auto enum_str_care_mask = enum_str_object["care_mask"].get_string();
                    if (err = enum_str_care_mask.error(); simdj_optional_key_error(err))
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get individual_enum/enum_field object #{}'s care_mask as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    if (!enum_str_care_mask.error())
                    {
                        const auto mask_view = enum_str_care_mask.value_unsafe();

                        if (mask_view.size() < 2) // must contain at least two characters (0x or 0b)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, individual_enum/enum_field object #{}'s care_mask is less than two characters (must begin with 0x or 0b)\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        std::string final_str;
                        final_str = mask_view.substr(2);
                        const auto mask_prefix = mask_view.substr(0,2);
                        const auto is_hex = mask_prefix == "0x";
                        const auto is_binary = mask_prefix == "0b";

                        if (!(is_hex || is_binary) || final_str.empty())
                        {
                            NEW_FPS_ERROR_PRINT("care_mask: {} for uri {}/{} individual_enum/enum_field object #{} is not a proper hex or binary string. It must begin with 0x or 0b, and must contain hex characters after 0x or binary characters after 0b\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        uint64_t final_mask = 0;
                        const auto [ptr, ec] = is_hex ? std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 16) : std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 2);

                        if (ec == std::errc())
                        {
                            if (*ptr != '\0') // non-null-terminated end (found some invalid characters)
                            {
                                NEW_FPS_ERROR_PRINT("the invalid hex/binary character: '{}' was found in care_mask: {} for uri {}/{} individual_enum/enum_field object #{}. Please remove it\n", *ptr, mask_view, main_uri, decode_id_view, bit_str_counter);
                                return false;
                            }

                            if (final_mask == 0) // they cannot "care" about nothing
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} individual_enums/enum_field object #{}, the care_mask: {} would mask out all bits and this is a contradiction. Please fix this\n", main_uri, decode_id_view, bit_str_counter, mask_view);
                                return false;
                            }
                            if (final_mask > gen_mask(0, max_num_bits - 1)) //  out of max bits range check:
                            {
                                NEW_FPS_ERROR_PRINT("the care_mask: {} for uri {}/{} individual_enums/enum_field object #{} is greater than {} bits and is out of range for this register (size of: {}). Please change it\n", mask_view, main_uri, decode_id_view, bit_str_counter, max_num_bits, current_decode_info.size);
                                return false;
                            }
                            if ((current_enum_mask | final_mask) > current_enum_mask) // out of begin_bit/end_bit range check:
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} individual_enums/enum_field object #{}, the care_mask: {} provided goes outside of the bounds for the number of bits in this individual_enums/enum_field object's bit range (begin_bit: {}, end_bit: {}, num_bits: {}). Please check for typos\n", main_uri, decode_id_view, bit_str_counter, mask_view, begin_bit_val, end_bit_val, end_bit_val - begin_bit_val + 1);
                                return false;
                            }
                            current_enum_mask = final_mask; // success
                        }
                        else if (ec == std::errc::invalid_argument)
                        {
                            NEW_FPS_ERROR_PRINT("the care_mask: {} for uri {}/{} individual_enums/enum_field object #{} is not a valid hex/binary string. Please provide a valid hex/binary string\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                        else if (ec == std::errc::result_out_of_range)
                        {
                            NEW_FPS_ERROR_PRINT("the care_mask: {} for uri {}/{} individual_enums/enum_field object #{} is out of bounds for the absolute maximum number of bits: 64. Please remove the excess hex/binary characters\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                    }
                    auto enum_str_ignore_mask = enum_str_object["ignore_mask"].get_string();
                    if (err = enum_str_ignore_mask.error(); simdj_optional_key_error(err))
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, can't get individual_enum/enum_field #{}'s ignore_mask as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    if (!enum_str_ignore_mask.error())
                    {
                        if (!enum_str_care_mask.error()) // can't have both optional masks in this context:
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, can't have both optional care_mask and optional ignore_mask for individual_enum/enum_field object #{}. Please pick one\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                        const auto mask_view = enum_str_ignore_mask.value_unsafe();

                        if (mask_view.size() < 2) // must contain at least two characters (0x or 0b)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, individual_enum/enum_field object #{}'s ignore_mask is less than two characters (must begin with 0x or 0b)\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        std::string final_str;
                        final_str = mask_view.substr(2);
                        const auto mask_prefix = mask_view.substr(0,2);
                        const auto is_hex = mask_prefix == "0x";
                        const auto is_binary = mask_prefix == "0b";

                        if (!(is_hex || is_binary) || final_str.empty())
                        {
                            NEW_FPS_ERROR_PRINT("ignore_mask: {} for uri {}/{} individual_enum/enum_field object #{} is not a proper hex/binary string. It must begin with 0x or 0b, and must contain hex characters after 0x or binary characters after 0b\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        uint64_t final_mask = 0;
                        const auto [ptr, ec] = is_hex ? std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 16) : std::from_chars(final_str.data(), final_str.data() + final_str.size(), final_mask, 2);

                        if (ec == std::errc())
                        {
                            if (*ptr != '\0') // non-null-terminated end (found some invalid characters)
                            {
                                NEW_FPS_ERROR_PRINT("the invalid hex/binary character: '{}' was found in ignore_mask: {} for uri {}/{} individual_enum/enum_field object #{}. Please remove it\n", *ptr, mask_view, main_uri, decode_id_view, bit_str_counter);
                                return false;
                            }

                            if (final_mask == current_enum_mask) // they cannot "ignore" everything
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} individual_enums/enum_field object #{}, the ignore_mask: {} would mask out all bits and this is a contradiction. Please fix this\n", main_uri, decode_id_view, bit_str_counter, mask_view);
                                return false;
                            }
                            if (final_mask > gen_mask(0, max_num_bits - 1)) //  out of max bits range check:
                            {
                                NEW_FPS_ERROR_PRINT("the ignore_mask: {} for uri {}/{} individual_enums/enum_field object #{} is greater than {} bits and is out of range for this register (size of: {}). Please change it\n", mask_view, main_uri, decode_id_view, bit_str_counter, max_num_bits, current_decode_info.size);
                                return false;
                            }
                            if ((current_enum_mask | final_mask) > current_enum_mask) // out of begin_bit/end_bit range check:
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} individual_enums/enum_field object #{}, the ignore_mask: {} provided goes outside of the bounds for the number of bits in this individual_enum/enum_field object's bit range (begin_bit: {}, end_bit: {}, num_bits: {}). Please check for typos\n", main_uri, decode_id_view, bit_str_counter, mask_view, begin_bit_val, end_bit_val, end_bit_val - begin_bit_val + 1);
                                return false;
                            }
                            current_enum_mask = ~final_mask & current_enum_mask; // success (invert it to a care_mask in our bit range)
                        }
                        else if (ec == std::errc::invalid_argument)
                        {
                            NEW_FPS_ERROR_PRINT("the ignore_mask: {} for uri {}/{} individual_enums/enum_field object #{} is not a valid hex/binary string. Please provide a valid hex/binary string\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                        else if (ec == std::errc::result_out_of_range)
                        {
                            NEW_FPS_ERROR_PRINT("the ignore_mask: {} for uri {}/{} individual_enums/enum_field object #{} is out of bounds for the absolute maximum number of bits: 64. Please remove the excess hex/binary characters\n", mask_view, main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }
                    }
                    current_enum_bit_str_info.begin_bit = begin_bit_val; // set begin_bit
                    current_enum_bit_str_info.end_bit = end_bit_val; // set end_bit
                    current_enum_bit_str_info.care_mask = current_enum_mask; // set current bit_range's care mask after begin_bit, end_bit, and optional care_mask/ignore_mask parsing
                    current_care_mask |= current_enum_mask << begin_bit_val; // set bits we care about from this enum_bit_range to true (the overall mask for decoding)

                    auto enum_strings = enum_str_object["enum_strings"].get_array();
                    if (err = enum_strings.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, cannot get enum_strings as an array, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    auto is_enum_strings_empty = enum_strings.is_empty();
                    if (err = is_enum_strings_empty.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, cannot determine whether or not the enum_strings array is empty or not, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    if (is_enum_strings_empty.value_unsafe())
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, enum_strings array is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter);
                        return false;
                    }
                    decltype(enum_val_map) inner_enum_val_map;
                    decltype(enum_str_map) inner_enum_str_map;
                    decltype(current_enum_val) inner_current_enum_val;
                    auto inner_enum_id_counter = 0UL;
                    const auto inner_max_enum_val = gen_mask(begin_bit_val, end_bit_val) >> begin_bit_val;

                    for (auto enum_string : enum_strings)
                    {
                        defer {
                            ++inner_enum_id_counter;
                            ++overall_enum_bit_str_counter;
                        };

                        if (overall_enum_bit_str_counter >= max_enum_bit_strings_allowed)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{} enum_string #{}, the maximum number of bit_strings/enum_strings: {} has been reached. Please either increase the maximum allowed or decrease the number of bit_strings/enum_strings\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter, max_enum_bit_strings_allowed);
                            return false;
                        }

                        auto& current_enum_str = current_enum_bit_str_info.enum_strings.emplace_back();

                        auto enum_str = enum_string.get_string();
                        if (err = enum_str.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, could not get enum_string #{} as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter, simdjson::error_message(err));
                            return false;
                        }
                        if (!enum_str.error()) // it is a normal string
                        {
                            if (enum_str.value_unsafe().empty())
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, enum string #{}'s string is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter);
                                return false;
                            }

                            if (inner_enum_val_map[inner_current_enum_val]) // check for value already having a string associated with it in this bit_strings array
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum value: {} has already been taken in this enum_strings array. Please remove the duplicate\n", main_uri, decode_id_view, bit_str_counter, inner_current_enum_val);
                                return false;
                            }
                            if (inner_enum_str_map[enum_str.value_unsafe()])
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum string: {} has already been taken in this enum_strings array. Please remove the duplicate\n", main_uri, decode_id_view, bit_str_counter, enum_str.value_unsafe());
                                return false;
                            }
                            if (inner_current_enum_val > inner_max_enum_val)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum value: {} associated with enum string: {} will exceed the maximum enum value of: {} for this bit range (begin_bit: {}, end_bit: {}). Please check for typos\n", main_uri, decode_id_view, bit_str_counter, inner_current_enum_val, enum_str.value_unsafe(), inner_max_enum_val, begin_bit_val, end_bit_val);
                                return false;
                            }
                            if ((inner_current_enum_val | current_enum_bit_str_info.care_mask) > current_enum_bit_str_info.care_mask)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enum object #{}, the enum value: {} associated with enum string: {} is outside of the care_mask: {:b} for this bit range. Please check your values\n", main_uri, decode_id_view, bit_str_counter, inner_current_enum_val, enum_str.value_unsafe(), current_enum_bit_str_info.care_mask);
                                return false;
                            }
                            inner_enum_val_map[inner_current_enum_val] = true; // claim this value (can't have a duplicate anymore)
                            inner_enum_str_map[enum_str.value_unsafe()] = true; // claim this string (can't have a duplicate anymore)
                            current_enum_str.enum_val = inner_current_enum_val;
                            current_enum_str.enum_str = enum_str.value_unsafe();
                            ++inner_current_enum_val; // increment inner current value by one (like C/C++ enums)
                        }
                        else // try and get it as an object (in the case of something like random_enums):
                        {
                            // TODO(WALKER): Get proper error messages here, copy paste nonsense occuring:
                            auto enum_str_obj = enum_string.get_object();
                            if (err = enum_str_obj.error(); err)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, could not get enum string #{} as a string or an object (value/string pair), err = {}\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter, simdjson::error_message(err));
                                return false;
                            }
                            auto enum_str_value = enum_str_obj["value"].get_uint64();
                            if (err = enum_str_value.error(); err)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, could not get enum string #{} object's value as an unsigned integer, err = {}\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter, simdjson::error_message(err));
                                return false;
                            }
                            auto enum_str_string = enum_str_obj["string"].get_string();
                            if (err = enum_str_string.error(); err)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, could not get enum string #{} object's string as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter, simdjson::error_message(err));
                                return false;
                            }
                            if (enum_str_string.value_unsafe().empty())
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, enum string #{} object's string is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter, inner_enum_id_counter);
                                return false;
                            }

                            const auto inner_enum_str_value_val = enum_str_value.value_unsafe();
                            if (inner_enum_val_map[inner_enum_str_value_val])
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum value: {} has already been taken in this enum_strings array. Please remove the duplicate\n", main_uri, decode_id_view, bit_str_counter, inner_enum_str_value_val);
                                return false;
                            }
                            if (inner_enum_str_map[enum_str_string.value_unsafe()])
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum string: {} has already been taken in this enum_strings array. Please remove the duplicate\n", main_uri, decode_id_view, bit_str_counter, enum_str_string.value_unsafe());
                                return false;
                            }
                            if (inner_enum_str_value_val > inner_max_enum_val)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enums object #{}, the enum value: {} associated with enum string: {} will exceed the maximum enum value of: {} for this bit range (begin_bit: {}, end_bit: {}). Please check for typos\n", main_uri, decode_id_view, bit_str_counter, inner_enum_str_value_val, enum_str_string.value_unsafe(), inner_max_enum_val, begin_bit_val, end_bit_val);
                                return false;
                            }
                            if ((inner_enum_str_value_val | current_enum_bit_str_info.care_mask) > current_enum_bit_str_info.care_mask)
                            {
                                NEW_FPS_ERROR_PRINT("for uri {}/{} enum_field/individual_enum object #{}, the enum value: {} associated with enum string: {} is outside of the care_mask: {:b} for this bit range. Please check your values\n", main_uri, decode_id_view, bit_str_counter, inner_enum_str_value_val, enum_str_string.value_unsafe(), current_enum_bit_str_info.care_mask);
                                return false;
                            }
                            inner_enum_val_map[inner_enum_str_value_val] = true; // claim value
                            inner_enum_str_map[enum_str_string.value_unsafe()] = true; // claim string
                            current_enum_str.enum_val = inner_enum_str_value_val;
                            current_enum_str.enum_str = enum_str_string.value_unsafe();
                            inner_current_enum_val = inner_enum_str_value_val + 1; // jump to the next value (this means values can be skipped if desired)                            
                        }
                    }
                }
                else // enum/random_enum (same as "enum_strings" array above)
                {
                    auto enum_str = bit_str.get_string();
                    if (err = enum_str.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                    {
                        NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get enum string #{} as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                        return false;
                    }
                    if (!enum_str.error()) // it is a normal string
                    {
                        if (enum_str.value_unsafe().empty())
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, enum string #{}'s string is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        if (enum_val_map[current_enum_val]) // check for value already having a string associated with it in this bit_strings array
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum value: {} has already been taken in this bit_strings array. Please remove the duplicate\n", main_uri, decode_id_view, current_enum_val);
                            return false;
                        }
                        if (enum_str_map[enum_str.value_unsafe()]) // check for string already having a value associated with it in this bit_strings array
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum string: {} has already been mapped to a value in this bit_strings array. Please remove the duplicate\n", main_uri, decode_id_view, enum_str.value_unsafe());
                            return false;
                        }
                        if (current_enum_val > max_enum_val)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum value: {} will exceed the maximum enum value for this register: {} (size of: {}). Please fix this\n", main_uri, decode_id_view, current_enum_val, max_enum_val, current_decode_info.size);
                            return false;
                        }
                        enum_val_map[current_enum_val] = true; // claim this value (can't have a duplicate anymore)
                        enum_str_map[enum_str.value_unsafe()] = true; // claim this string so no other value can be assigned it
                        current_enum_bit_str_info.enum_val = current_enum_val;
                        current_enum_bit_str_info.id = enum_str.value_unsafe();
                        ++current_enum_val; // increment current value by one (like C/C++ enums)
                    }
                    else // try and get it as an object (in the case of something like random_enums):
                    {
                        auto enum_str_obj = bit_str.get_object();
                        if (err = enum_str_obj.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get enum string #{} as a string or an object (value/string pair), err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                            return false;
                        }
                        auto enum_str_value = enum_str_obj["value"].get_uint64();
                        if (err = enum_str_value.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get enum string #{} object's value as an unsigned integer, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                            return false;
                        }
                        auto enum_str_string = enum_str_obj["string"].get_string();
                        if (err = enum_str_string.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, could not get enum string #{} object's string as a string, err = {}\n", main_uri, decode_id_view, bit_str_counter, simdjson::error_message(err));
                            return false;
                        }

                        if (enum_str_string.value_unsafe().empty())
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, enum string #{} object's string is empty, what are you smoking?\n", main_uri, decode_id_view, bit_str_counter);
                            return false;
                        }

                        const auto enum_str_value_val = enum_str_value.value_unsafe();
                        if (enum_val_map[enum_str_value_val])
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum value: {} has already been taken in this bit_strings array. Please remove the duplicate\n", main_uri, decode_id_view, enum_str_value_val);
                            return false;
                        }
                        if (enum_str_map[enum_str_string.value_unsafe()]) // check for string already having a value associated with it in this bit_strings array
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum string: {} has already been mapped to a value in this bit_strings array. Please remove the duplicate\n", main_uri, decode_id_view, enum_str_string.value_unsafe());
                            return false;
                        }
                        if (enum_str_value_val > max_enum_val)
                        {
                            NEW_FPS_ERROR_PRINT("for uri {}/{}, the enum value: {} will exceed the maximum enum value for this register: {} (size of: {}). Please fix this\n", main_uri, decode_id_view, enum_str_value_val, max_enum_val, current_decode_info.size);
                            return false;
                        }
                        enum_val_map[enum_str_value_val] = true; // claim value
                        enum_str_map[enum_str_string.value_unsafe()] = true; // claim string
                        current_enum_bit_str_info.id = enum_str_string.value_unsafe();
                        current_enum_bit_str_info.enum_val = enum_str_value_val;
                        current_enum_val = enum_str_value_val + 1; // jump to the next value (this means values can be skipped if desired)                            
                    }
                }
            }
        }
        // NOTE(WALKER): For server, resets main_uri if we need to parse it
        if (!current_decode_info.uri.empty())
        {
            main_uri = ""sv;
        }
    }
    return true;
}
