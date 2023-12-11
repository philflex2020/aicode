#pragma once

// IMPORTANT: Please include this version of simdjson
// instead of the normal one in order to code without exceptions
// this way we are writing optimal code
// please see the basics of simdjson oh how to code without exceptions
// and with proper error checking

#define SIMDJSON_EXCEPTIONS 0
#include "simdjson.h"

static constexpr auto Simdj_Padding = simdjson::SIMDJSON_PADDING;

constexpr bool simdj_optional_key_error(simdjson::error_code err) noexcept
{
    return !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::NO_SUCH_FIELD);
}

constexpr bool simdj_optional_type_error(simdjson::error_code err) noexcept
{
    return !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE);
}
