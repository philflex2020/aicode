#ifndef _LOGGER_H
#define _LOGGER_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>



constexpr size_t LOG_BUF_SIZE = 1024 * 1024; // 1 MB

#pragma pack(push, 1)
struct LogHeader {
    uint32_t packet_size;  // bytes after this header
    uint64_t timestamp_us;
    uint8_t data[1];       // start of the packed payload
};


#pragma pack(pop)

struct LogBuffer {
    uint8_t buffer[LOG_BUF_SIZE];
    std::atomic<size_t> write_idx{0};  // next free position
    std::atomic<size_t> read_idx{0};   // next unread position
    std::function<void(const LogHeader*)> on_evict = nullptr;

    // Write a packed record into the circular buffer
    bool write(const uint8_t* payload, size_t size, uint64_t timestamp_us) {
        const size_t total_size = sizeof(LogHeader) - 1 + size;
        if (total_size > LOG_BUF_SIZE) return false;  // too big to ever fit

        size_t w = write_idx.load(std::memory_order_relaxed);
        size_t r = read_idx.load(std::memory_order_relaxed);

        auto space_remaining = [&]() {
            return (r <= w) ? (LOG_BUF_SIZE - (w - r)) : (r - w);
        };

        // Evict if needed
        while (space_remaining() < total_size + 1) {
            if (r == w) break;  // fully full and can't evict

            // Read current record at read_idx
            LogHeader* old_hdr = reinterpret_cast<LogHeader*>(buffer + r);
            size_t old_total = sizeof(LogHeader) - 1 + old_hdr->packet_size;

            // If old record crosses boundary, reset read_idx
            if (r + old_total > LOG_BUF_SIZE) {
                r = 0;
                if (on_evict) {
                    on_evict(old_hdr);
                }
                old_hdr = reinterpret_cast<LogHeader*>(buffer + r);
                old_total = sizeof(LogHeader) - 1 + old_hdr->packet_size;
                if (r + old_total > w) break; // collision if writer hasn't wrapped
            }

            r += old_total;
            if (r >= LOG_BUF_SIZE) r = 0;
            read_idx.store(r, std::memory_order_release);
        }

        if (space_remaining() < total_size + 1) return false;

        // Wrap writer if needed
        if (w + total_size > LOG_BUF_SIZE) {
            w = 0;
            write_idx.store(w, std::memory_order_release);
        }

        // Final write
        LogHeader* hdr = reinterpret_cast<LogHeader*>(buffer + w);
        hdr->timestamp_us = timestamp_us;
        hdr->packet_size = size;
        std::memcpy(hdr->data, payload, size);

        write_idx.store(w + total_size, std::memory_order_release);
        return true;
    }

    // bool write(const uint8_t* payload, size_t size, uint64_t timestamp_us) {
    //     size_t total_size = sizeof(LogHeader) - 1 + size; // -1 for flexible array

    //     // Wrap-around logic
    //     if (total_size > LOG_BUF_SIZE) return false;

    //     size_t w = write_idx.load(std::memory_order_relaxed);
    //     size_t r = read_idx.load(std::memory_order_acquire);

    //     size_t space = (r <= w) ? (LOG_BUF_SIZE - (w - r)) : (r - w);
    //     if (space < total_size + 1) return false;  // not enough space

    //     // Handle wrap if needed
    //     if (w + total_size > LOG_BUF_SIZE) {
    //         if (r <= total_size) return false; // can't wrap, would collide
    //         w = 0;
    //         write_idx.store(w, std::memory_order_release);
    //     }

    //     LogHeader* hdr = reinterpret_cast<LogHeader*>(buffer + w);
    //     hdr->timestamp_us = timestamp_us;
    //     hdr->packet_size = size;
    //     std::memcpy(hdr->data, payload, size);

    //     write_idx.store(w + total_size, std::memory_order_release);
    //     return true;
    // }

    // Read a packet (copy into `out` buffer)
    bool read(std::vector<uint8_t>& out, uint64_t& timestamp_out) {
        size_t r = read_idx.load(std::memory_order_relaxed);
        size_t w = write_idx.load(std::memory_order_acquire);

        if (r == w) return false;  // nothing to read

        LogHeader* hdr = reinterpret_cast<LogHeader*>(buffer + r);
        size_t total_size = sizeof(LogHeader) - 1 + hdr->packet_size;

        if (r + total_size > LOG_BUF_SIZE) {
            r = 0;  // try wrap
            hdr = reinterpret_cast<LogHeader*>(buffer + r);
            total_size = sizeof(LogHeader) - 1 + hdr->packet_size;
            if (r + total_size > w) return false; // not yet written
        }

        timestamp_out = hdr->timestamp_us;
        out.assign(hdr->data, hdr->data + hdr->packet_size);

        read_idx.store(r + total_size, std::memory_order_release);
        return true;
    }
};

// logbuf.on_evict = [](const LogHeader* hdr) {
//     printf("[evict] %lu byte packet evicted at %lu us\n", 
//            (unsigned long)hdr->packet_size, hdr->timestamp_us);
// };


typedef struct
{
    uint16_t ID_1;
    uint16_t ID_2;
    uint32_t len;
    uint32_t nlen;
    uint16_t xbms_num;
    uint16_t rack_num;
    uint16_t seq;    // this is now a sequence number
    uint16_t ack;    // this is now the ack
    uint8_t cmd;     // 
    uint8_t mtype;   // 0 rtos, 1 rack 

    uint16_t control_size;
    uint16_t data_size;
    uint32_t crc;  // put crc in here 
} Header;

#endif