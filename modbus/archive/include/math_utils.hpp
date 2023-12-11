#pragma once

// helper bool for constructing with integrals:
template<typename T>
inline constexpr bool is_signed_integral = std::is_signed_v<T> && std::is_integral_v<T>;
// helper bool for constructing with unsigned integrals:
template<typename T>
inline constexpr bool is_unsigned_integral = std::is_unsigned_v<T> && std::is_integral_v<T>;
// and for unsigned integrals that can be put in here without loss (up to 32 bits):
template<typename T>
inline constexpr bool is_tiny_unsigned_integral = std::is_same_v<uint8_t, T> 
    || std::is_same_v<uint16_t, T>
    || std::is_same_v<uint32_t, T>;

namespace flex
{

// credit to: https://stackoverflow.com/questions/58734034/how-to-properly-compare-an-integer-and-a-floating-point-value
// excellent way to get a three-way-compare between an integral and a float:
// template <typename I, typename F>
// constexpr const std::partial_ordering compare_int_float(I i, F f) noexcept
// {
//     if constexpr (std::is_integral_v<F> && std::is_floating_point_v<I>)
//     {
//         return 0 <=> compare_int_float(f, i);
//     }
//     else
//     {
//         static_assert(std::is_integral_v<I> && std::is_floating_point_v<F>);
//         static_assert(std::numeric_limits<F>::radix == 2);

//         // This should be exactly representable as F due to being a power of two.
//         constexpr F I_min_as_F = std::numeric_limits<I>::min();

//         // The `numeric_limits<I>::max()` itself might not be representable as F, so we use this instead.
//         constexpr F I_max_as_F_plus_1 = F(std::numeric_limits<I>::max()/2+1) * 2;

//         // Check if the constants above overflowed to infinity. Normally this shouldn't happen.
//         constexpr bool limits_overflow = I_min_as_F * 2 == I_min_as_F || I_max_as_F_plus_1 * 2 == I_max_as_F_plus_1;
//         if constexpr (limits_overflow)
//         {
//             // Manually check for special floating-point values.
//             if (std::isinf(f))
//                 return f > 0 ? std::partial_ordering::less : std::partial_ordering::greater;
//             if (std::isnan(f))
//                 return std::partial_ordering::unordered;
//         }

//         if (limits_overflow || f >= I_min_as_F)
//         {
//             // `f <= I_max_as_F_plus_1 - 1` would be problematic due to rounding, so we use this instead.
//             if (limits_overflow || f - I_max_as_F_plus_1 <= -1)
//             {
//                 I f_trunc = static_cast<I>(f);
//                 if (f_trunc < i)
//                     return std::partial_ordering::greater;
//                 if (f_trunc > i)
//                     return std::partial_ordering::less;

//                 F f_frac = f - static_cast<F>(f_trunc);
//                 if (f_frac < 0)
//                     return std::partial_ordering::greater;
//                 if (f_frac > 0)
//                     return std::partial_ordering::less;
                    
//                 return std::partial_ordering::equivalent;
//             }

//             return std::partial_ordering::less;
//         }
        
//         if (f < 0)
//             return std::partial_ordering::greater;
        
//         return std::partial_ordering::unordered;
//     }
// }

// taken straight from C++20 -> can be used in C++17
// source: https://en.cppreference.com/w/cpp/utility/intcmp
template< class T, class U >
constexpr bool cmp_equal( T t, U u ) noexcept
{
    using UT = std::make_unsigned_t<T>;
    using UU = std::make_unsigned_t<U>;
    if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
        return t == u;
    else if constexpr (std::is_signed_v<T>)
        return t < 0 ? false : UT(t) == u;
    else
        return u < 0 ? false : t == UU(u);
}

template< class T, class U >
constexpr bool cmp_not_equal( T t, U u ) noexcept
{
    return !cmp_equal(t, u);
}

template< class T, class U >
constexpr bool cmp_less( T t, U u ) noexcept
{
    using UT = std::make_unsigned_t<T>;
    using UU = std::make_unsigned_t<U>;
    if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
        return t < u;
    else if constexpr (std::is_signed_v<T>)
        return t < 0 ? true : UT(t) < u;
    else
        return u < 0 ? false : t < UU(u);
}

template< class T, class U >
constexpr bool cmp_greater( T t, U u ) noexcept
{
    return cmp_less(u, t);
}

template< class T, class U >
constexpr bool cmp_less_equal( T t, U u ) noexcept
{
    return !cmp_greater(t, u);
}

template< class T, class U >
constexpr bool cmp_greater_equal( T t, U u ) noexcept
{
    return !cmp_less(t, u);
}

} // end namespace flex