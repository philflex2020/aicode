#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <utility>

// General purpose single allocated arena
// does NOT expand capacity (count how many bytes you need ahead of time)
struct Simple_Arena
{
    std::size_t allocated = 0;
    std::size_t current_idx = 0;
    void* data = nullptr;

    ~Simple_Arena()
    {
        if (data) free(data);
    }

    bool initialize(std::size_t amount)
    {
        if (data) return false; // cannot allocate again (single time allocation)
        allocated = amount;
        current_idx = 0; // for reload purposes (if we call initialize again after a destructor call)
        data = malloc(amount);
        if (data) memset(data, 0, allocated); // set all data to 0 for initialization purposes
        return data != nullptr; // make sure we actually allocated all the memory we needed
    }

    template<typename T, typename... Args>
    bool allocate(T*& to_get, std::size_t amount = 1, Args&&... args)
    {
        if (to_get != nullptr) return false; // no double allocation by accident
        if (amount == 0) return false; // no empty allocations allowed

        // calculate offset from current_idx based alignment of the Type:
        std::size_t offset = ((std::size_t) data + current_idx) % alignof(T);
        if (offset != 0) offset = alignof(T) - offset; // adjust offset based on remainder
        const std::size_t bytes_needed = sizeof(T) * amount + offset;
        if (current_idx + bytes_needed > allocated) return false; // not enough space

        T* current = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(data) + current_idx + offset);
        current_idx += bytes_needed;

        // create a new one (and a new one per amount needed beyond the first):
        to_get = new (current) T(std::forward<Args>(args) ...);
        for (std::size_t i = 1; i < amount; ++i)
        {
            new (&to_get[i]) T(std::forward<Args>(args) ...);
        }
        return true;
    }
};
