#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

// just some definitions:
// for tags (basics - no recursive crap):
// NOTE(WALKER): DO NOT CHANGE THESE
enum class jval_types : uint8_t
{
    Null,
    Bool,
    UInt,
    Int,
    Float,
    String
};

// IMPORTANT(WALKER): DO NOT CHANGE THESE
using jval_n_type = std::monostate;
using jval_b_type = bool;
using jval_u_type = uint64_t;
using jval_i_type = int64_t;
using jval_f_type = double;
using jval_s_type = std::string;
using jval_sv_type = std::string_view; // for non-owning and trivial versions of Jval that use strings
