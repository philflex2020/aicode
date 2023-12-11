#pragma once

#include <cstring>
#include <cstdint>
#include <string_view>
#include <limits>

// NOTE(WALKER): gets you an index into the string storage array
struct String_Storage_Handle
{
    uint8_t size;
    uint16_t idx;
};
// size info:
// auto x = sizeof(Str_Storage_Handle);

// NOTE(WALKER): This points inside the Simple_Arena
// also max number of chars is max of u16 -> 65535 chars
struct String_Storage
{
    static constexpr auto Max_Str_Storage_Size = std::numeric_limits<uint16_t>::max();

    uint16_t allocated = 0;
    uint16_t current_idx = 0;
    char* data = nullptr;

    String_Storage_Handle append_str(std::string_view str)
    {
        String_Storage_Handle handle;
        if (str.size() > std::numeric_limits<uint8_t>::max()) return handle; // no more than 255 characters for strings
        if (str.size() + current_idx > allocated) return handle; // ran out of storage space (allocate more memory)

        memcpy(data + current_idx, str.data(), str.size());
        handle.size = static_cast<uint8_t>(str.size());
        handle.idx = current_idx;
        current_idx += static_cast<uint16_t>(str.size());
        return handle;
    }

    std::string_view get_str(String_Storage_Handle handle)
    {
        return std::string_view{data + handle.idx, handle.size};
    }
};
// size info:
// auto x = sizeof(String_Storage);
