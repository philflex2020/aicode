#pragma once

#include <string>

#include "spdlog/fmt/bundled/format.h"

#include "shared_utils.hpp"
#include "simdjson_noexcept.hpp"

namespace config_loader
{

enum class Required_Types : uint8_t
{
    None,
    Bool,
    Int,
    Uint,
    Float,
    String,
    Object,
    Array
};

template<typename T>
constexpr Required_Types to_required_type() noexcept
{
    using Type = std::decay_t<T>;
         if constexpr (std::is_same_v<Type, bool>)                       { return Required_Types::Bool; }
    else if constexpr (std::is_same_v<Type, s64>)                        { return Required_Types::Int; }
    else if constexpr (std::is_same_v<Type, u64>)                        { return Required_Types::Uint; }
    else if constexpr (std::is_same_v<Type, double>)                     { return Required_Types::Float; }
    else if constexpr (std::is_same_v<Type, std::string>)                { return Required_Types::String; }
    else if constexpr (std::is_same_v<Type, simdjson::ondemand::object>) { return Required_Types::Object; }
    else if constexpr (std::is_same_v<Type, simdjson::ondemand::array>)  { return Required_Types::Array; }
    else                                                                 { static_assert(std::is_same_v<Type, bool>); return Required_Types::Bool; } // fail out
}

// helper struct that tells you where things went wrong in the config file:
struct Error_Location
{
    std::size_t component_idx = 0;
    std::string_view component_id;
    std::size_t register_idx = 0;
    std::string_view register_type;
    std::size_t decode_idx = 0;
    std::string_view decode_id;
    std::size_t bit_strings_idx = 0;
    std::string_view key;
    Required_Types type;
    std::string err_msg;

    void reset()
    {
        component_idx = 0;
        component_id = "";
        register_idx = 0;
        register_type = "";
        decode_idx = 0;
        decode_id = "";
        bit_strings_idx = 0;
        key = "";
        type = Required_Types::None;
        err_msg.clear();
    }
};

template<typename T>
static bool get_mandatory_val(simdjson::simdjson_result<simdjson::ondemand::value> val, T& to_get, Error_Location& err_loc)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::string_view str;
        if (const auto err = val.get(str); err)
        {
            err_loc.err_msg = simdjson::error_message(err);
            return false;
        }
        to_get = str;
    }
    else
    {
        if (const auto err = val.get(to_get); err)
        {
            err_loc.err_msg = simdjson::error_message(err);
            return false;
        }
    }
    return true;
}

template<typename T>
static bool get_optional_val(simdjson::simdjson_result<simdjson::ondemand::value> val, T& to_get, Error_Location& err_loc)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::string_view str;
        if (const auto err = val.get(str); simdj_optional_key_error(err))
        {
            err_loc.err_msg = simdjson::error_message(err);
            return false;
        }
        else if (!err) // for the case where it doesn't exist
        {
            to_get = str;
        }
    }
    else
    {
        if (const auto err = val.get(to_get); simdj_optional_key_error(err))
        {
            err_loc.err_msg = simdjson::error_message(err);
            return false;
        }
    }
    return true;
}

enum class Get_Type
{
    Mandatory,
    Optional
};

template<typename T>
static bool get_val(simdjson::ondemand::object& obj, std::string_view key, T& to_get, Error_Location& err_loc, Get_Type type)
{
    err_loc.key = key;
    err_loc.type = to_required_type<T>();
    auto val = obj[key];
    return type == Get_Type::Mandatory ? get_mandatory_val(val, to_get, err_loc) : get_optional_val(val, to_get, err_loc);
}

// for array iteration:
// TODO(WALKER): Get this to work with "bit_strings" stuff at some point (introduce optional types -> iterate over "type" list then try those?)
template<typename T>
static bool get_array_val(simdjson::simdjson_result<simdjson::ondemand::value> val, T& to_get, Error_Location& err_loc)
{
    err_loc.key = "";
    err_loc.type = to_required_type<T>();
    return get_mandatory_val(val, to_get, err_loc);
}

static bool check_str_for_error(const std::string_view str, Error_Location& err_loc, const std::string_view Forbidden_Chars = R"({}\/ "%)", const std::size_t Max_Str_Size = std::numeric_limits<u8>::max())
{
    if (str.empty())
    {
        err_loc.err_msg = fmt::format("string is empty");
        return false;
    }
    if (str.find_first_of(Forbidden_Chars) != std::string_view::npos)
    {
        err_loc.err_msg = fmt::format("string (currently: \"{}\") contains one of the forbidden characters: '{}'", str, fmt::join(Forbidden_Chars, "', '"));
        return false;
    }
    if (str.size() > Max_Str_Size)
    {
        err_loc.err_msg = fmt::format("string (currently: \"{}\", size: {}) has exceeded the maximum character limit of {}", str, str.size(), Max_Str_Size);
        return false;
    }

    return true;
}

}

// formatters:

template<>
struct fmt::formatter<config_loader::Required_Types>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Required_Types& jt, FormatContext& ctx) const
    {
        using namespace config_loader;
        switch (jt)
        {
            case Required_Types::Bool:    return fmt::format_to(ctx.out(), "boolean");
            case Required_Types::Int:     return fmt::format_to(ctx.out(), "integer");
            case Required_Types::Uint:    return fmt::format_to(ctx.out(), "unsigned integer");
            case Required_Types::Float:   return fmt::format_to(ctx.out(), "float");
            case Required_Types::String:  return fmt::format_to(ctx.out(), "string");
            case Required_Types::Object:  return fmt::format_to(ctx.out(), "object");
            case Required_Types::Array:   return fmt::format_to(ctx.out(), "array");
            default:                      return fmt::format_to(ctx.out(), ""); // blank type ("none" )
        }
    }
};

template<>
struct fmt::formatter<config_loader::Error_Location>
{
    char presentation = 'c'; // default to client formatting

    constexpr auto parse(format_parse_context& ctx)
    {
        // Parse the presentation format and store it in the formatter:
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'c' || *it == 's')) presentation = *it; // only accept c or s (no errors please)

        return end;
    }

    template <typename FormatContext>
    auto format(const config_loader::Error_Location& err_loc, FormatContext& ctx) const
    {
        if (presentation == 'c') // client
        {
            return fmt::format_to(ctx.out(), R"(
Error Location:

Current Component index:     {}
Last Known Component id:     {}
Current Register index:      {}
Last Known Register type:    {}
Current Decode index:        {}
Last Known Decode id:        {}
Current Bit String index:    {}
Current Key/Field:           {}
Current Required type:       {}
Error:                       {}

)",             err_loc.component_idx, 
                err_loc.component_id, 
                err_loc.register_idx,
                err_loc.register_type, 
                err_loc.decode_idx, 
                err_loc.decode_id, 
                err_loc.bit_strings_idx,
                err_loc.key, 
                err_loc.type, 
                err_loc.err_msg);
        }
        else // server:
        {
            return fmt::format_to(ctx.out(), R"(
Error Location:

Register type:               {}
Current Decode index:        {}
Last Known Decode id:        {}
Current Bit String index:    {}
Current Key/Field:           {}
Current Required type:       {}
Error:                       {}

)",             err_loc.register_type, 
                err_loc.decode_idx, 
                err_loc.decode_id, 
                err_loc.bit_strings_idx, 
                err_loc.key, 
                err_loc.type, 
                err_loc.err_msg);
        }
    }
};
