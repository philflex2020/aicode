#pragma once

#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/compile.h"

#include "math_utils.hpp"
#include "Jval_utils.hpp"

// basic json values (non-null, used in modbus client/server when pubbing and receiving values)
// pronounced: "beef"
struct Jval_buif final
{
    jval_types tag = jval_types::UInt;
    union
    {
        jval_u_type u{};
        jval_i_type i;
        jval_f_type f;
        jval_b_type b;
    };

public:
    constexpr Jval_buif() noexcept = default; 

    template<typename T>
    constexpr Jval_buif(const T arg) noexcept
    {
        using Type = std::decay_t<T>;
        if constexpr (is_unsigned_integral<Type> && !std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::UInt;
            u = arg;
        } 
        else if constexpr (is_signed_integral<Type> && !std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::Int;
            i = arg;
        }
        else if constexpr (std::is_same_v<Type, jval_f_type> || std::is_same_v<Type, float>)
        {
            tag = jval_types::Float;
            f = arg;
        }
        else if constexpr (std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::Bool;
            b = arg;
        }
        else
        {
            // this will always evaluate to false at this point
            static_assert(std::is_same_v<Type, jval_b_type>, "This type is not supported for Jval_buif");
        }
    }

    // assignment operator:
    template<typename T>
    constexpr Jval_buif& operator=(const T arg) noexcept
    {
        using Type = std::decay_t<T>;
        if constexpr (is_unsigned_integral<Type> && !std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::UInt;
            u = arg;
        }
        else if constexpr (is_signed_integral<Type> && !std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::Int;
            i = arg;
        }
        else if constexpr (std::is_same_v<Type, jval_f_type> || std::is_same_v<Type, float>)
        {
            tag = jval_types::Float;
            f = arg;
        }
        else if constexpr (std::is_same_v<Type, jval_b_type>)
        {
            tag = jval_types::Bool;
            b = arg;
        }
        else
        {
            static_assert(std::is_same_v<Type, jval_b_type>, "This type is not supported for Jval_buif");
        }
        return *this;
    }

    // get raw type:
    constexpr auto get_type() const noexcept
    {
        return tag;
    }

    // check which type it is:
    constexpr bool holds_uint() const noexcept
    {
        return tag == jval_types::UInt;
    } 
    constexpr bool holds_int() const noexcept
    {
        return tag == jval_types::Int;
    }
    constexpr bool holds_float() const noexcept
    {
        return tag == jval_types::Float;
    }
    constexpr bool holds_bool() const noexcept
    {
        return tag == jval_types::Bool;
    }

    // get values out (safe):
    constexpr auto get_uint() const noexcept
    {
        return this->holds_uint() ? u : jval_u_type{};
    }
    constexpr auto get_int() const noexcept
    {
        return this->holds_int() ? i : jval_i_type{};
    }
    constexpr auto get_float() const noexcept
    {
        return this->holds_float() ? f : jval_f_type{};
    }
    constexpr auto get_bool() const noexcept
    {
        return this->holds_bool() ? b : jval_b_type{};
    }

    // get_values_unsafe - NOTE(WALKER): With great power comes great responsbility
    constexpr auto get_uint_unsafe() const noexcept
    {
        return u;
    }
    constexpr auto get_int_unsafe() const noexcept
    {
        return i;
    }
    constexpr auto get_float_unsafe() const noexcept
    {
        return f;
    }
    constexpr auto get_bool_unsafe() const noexcept
    {
        return b;
    }

    // == operator overloads:
    template<typename T>
    constexpr bool operator==(const T rhs) const noexcept
    {
        using Type = std::decay_t<T>;
        if constexpr (is_unsigned_integral<Type> && !std::is_same_v<Type, jval_b_type>) // unsigned integral comparison
        {
            return this->holds_uint() ? u == rhs : false;
        }
        else if constexpr (is_signed_integral<Type> && !std::is_same_v<Type, jval_b_type>) // integral comparison
        {
            return this->holds_int() ? i == rhs : false;
        }
        else if constexpr (std::is_floating_point_v<Type>) // floating point comparison
        {
            return this->holds_float() ? f == rhs : false;
        }
        else if constexpr (std::is_same_v<Type, Jval_buif>) // another Jval_buif
        {
            if (tag != rhs.tag)
            {
                return false;
            }
            else
            {
                // they are the same (compare as normal):
                switch (tag)
                {
                case jval_types::UInt :
                    return u == rhs.u;
                case jval_types::Int :
                    return i == rhs.i;
                case jval_types::Float :
                    return f == rhs.f;
                default: // bool
                    return b == rhs.b;
                }
            }
        }
        else
        {
            static_assert(std::is_same_v<Type, jval_u_type>, "unspported type for Jval_buif's comparison operator");
        }
    }
    constexpr bool operator==(const bool rhs) const noexcept
    {
        return this->holds_bool() ? b == rhs : false;
    }

    // != operator overloads:
    template<typename T>
    constexpr bool operator!=(const T rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

// fmt::formatter specialization for Jval_buif:
// IMPORTANT(WALKER): This does NOT print out bools
// as when doing that for coils and discrete inputs the get_bool_unsafe()
// function is used directly inside the "worker_thread" function
// that way we don't get the extra branch penalty for Holding and Input register types
// It speeds things up a little bit
template<>
struct fmt::formatter<Jval_buif>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Jval_buif& jv, FormatContext& ctx) const
    {
        switch (jv.get_type())
        {
        case jval_types::UInt :
            return format_to(ctx.out(), FMT_COMPILE("{}"), jv.get_uint_unsafe());
        case jval_types::Int :
            return format_to(ctx.out(), FMT_COMPILE("{}"), jv.get_int_unsafe());
        default : // float
            return format_to(ctx.out(), FMT_COMPILE("{}"), jv.get_float_unsafe());
        }
    }
};
