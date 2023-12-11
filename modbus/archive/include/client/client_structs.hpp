#pragma once

// standard/C:
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <bitset>

// third party:
#include "modbus/modbus.h"
#include "tscns.h"
#include "parallel_hashmap/phmap.h"
#include "rigtorp/SPSCQueue.h"
#include "rigtorp/MPMCQueue.h"

// fims:
#include "fims/libfims.h"

// my includes:
#include "shared_utils.hpp"
#include "Jval_buif.hpp"
#include "simple_arena.hpp"
#include "string_storage_v2.hpp"
#include "decode_encode_utils.hpp"
#include "simdjson_noexcept.hpp"
#include "config_loaders/client_config_loader.hpp"
#include "semaphore.hpp" // Linux semaphore wrapper with helper functions (use PCQ_Semaphore)

// constants:
static constexpr u32 Max_Expected_Data_Size = 100000; // at most 100000 characters for json data being send to client

// helper functions:
static std::size_t uri_map_hash(const std::string_view str) noexcept {
    return std::hash<std::string_view>{}(str);
}

// struct Set_Info
// {
//     Jval_buif set_val;
//     Register_Types reg_type;
//     u8 component_idx = Component_All_Idx;
//     u8 thread_idx = Thread_All_Idx;
//     u8 decode_idx = Decode_All_Idx;
//     u8 bit_idx = Bit_All_Idx;
//     u8 enum_idx = Enum_All_Idx; // unused right now (for individual_enums/enum_field in the future)
// };
// size info:
// auto x = sizeof(Set_Info);

// struct Set_Chan_Msg
// {
//     std::vector<Set_Info> set_vals;

//     Set_Chan_Msg() = default;

//     Set_Chan_Msg(Set_Chan_Msg&& other) noexcept
//         : set_vals(std::move(other.set_vals))
//     {}

//     Set_Chan_Msg& operator=(Set_Chan_Msg&& other) noexcept
//     {
//         set_vals = std::move(other.set_vals);
//         return *this;
//     }
// };
// size info:
// auto x = sizeof(Set_Chan_Msg);

struct Uri_Flags {
    u8 flags = 0;

    static constexpr u8 is_individual_bit_idx = 0;
    static constexpr u8 is_individual_enum_idx = 1;

    // setters (called once during the config stage):
    constexpr void set_individual_bit(const bool is_individual_bit) noexcept {
        flags |= static_cast<u8>((1UL & is_individual_bit) << is_individual_bit_idx);
    }
    constexpr void set_individual_enum(const bool is_individual_enum) noexcept {
        flags |= static_cast<u8>((1UL & is_individual_enum) << is_individual_enum_idx);
    }

    // getters:
    constexpr bool is_individual_bit() const noexcept {
        return (flags >> is_individual_bit_idx) & 1UL;
    }
    constexpr bool is_individual_enum() const noexcept {
        return (flags >> is_individual_enum_idx) & 1UL;
    }
};

struct Uri_Info {
    Register_Types reg_type;
    Uri_Flags flags;
    u8 component_idx = Component_All_Idx;
    u8 thread_idx = Thread_All_Idx;
    u8 decode_idx = Decode_All_Idx;
    u8 bit_idx = Bit_All_Idx;
    u8 enum_idx = Enum_All_Idx; // experimental (unused for now) -> "individual_enums" in the future
};
// size info:
// auto x = sizeof(Uri_Info);
// auto x = sizeof(std::pair<std::size_t, Uri_Info>);

// ########################## CONST STUFF ##########################

#define CONST static constexpr

// ########################################################


// ########################## QUEUES AND WORK TYPES ##########################

struct Get_Request_Flags {
    u8 flags = 0;

    static constexpr u8 is_raw_request_idx = 0;
    static constexpr u8 is_timings_request_idx = 1;
    static constexpr u8 is_reset_timings_request_idx = 2;

    // setters (called once during the config stage):
    constexpr void set_raw_request(const bool is_raw_request) noexcept {
        flags |= static_cast<u8>((1UL & is_raw_request) << is_raw_request_idx);
    }
    constexpr void set_timings_request(const bool is_timings_request) noexcept {
        flags |= static_cast<u8>((1UL & is_timings_request) << is_timings_request_idx);
    }
    constexpr void set_reset_timings_request(const bool is_reset_timings_request) noexcept {
        flags |= static_cast<u8>((1UL & is_reset_timings_request) << is_reset_timings_request_idx);
    }

    // getters:
    constexpr bool is_raw_request() const noexcept {
        return (flags >> is_raw_request_idx) & 1UL;
    }
    constexpr bool is_timings_request() const noexcept {
        return (flags >> is_timings_request_idx) & 1UL;
    }
    constexpr bool is_reset_timings_request() const noexcept {
        return (flags >> is_reset_timings_request_idx) & 1UL;
    }
};

// set stuff:
struct Set_Info {
    Jval_buif set_val;
    u64 prev_unsigned_val; // NOTE(WALKER): This is for the case where we need to set "individual_bits"
    u8 component_idx = Component_All_Idx;
    u8 thread_idx = Thread_All_Idx;
    u8 decode_idx = Decode_All_Idx;
    u8 bit_idx = Bit_All_Idx;
    u8 enum_idx = Enum_All_Idx; // unused right now (for individual_enums/enum_field in the future)
};

struct Set_Work {
    std::vector<Set_Info> set_vals;

    int errno_code = 0; // this set to anything other than zero means there is an error
    Set_Work() = default;

    Set_Work(Set_Work&& other) noexcept 
        : set_vals(std::move(other.set_vals)) {}

    Set_Work& operator=(Set_Work&& other) noexcept {
        set_vals = std::move(other.set_vals);
        return *this;
    }
};

// get stuff:
struct Get_Work {
    Get_Request_Flags flags;
    u8 comp_idx;
    u8 thread_idx; // "map" index inside the component
    u8 decode_idx;
    u8 bit_idx;
    u8 enum_idx; // unused right now (for individual_enums/enum_field in the future)
    std::string replyto;
    int errno_code = 0; // this set to anything other than zero means there is an error

    Get_Work() = default;

    Get_Work(Get_Work&& other) noexcept
        : flags(other.flags), comp_idx(other.comp_idx), thread_idx(other.thread_idx), 
            decode_idx(other.decode_idx), bit_idx(other.bit_idx), enum_idx(other.enum_idx), 
            replyto(std::move(other.replyto)) {}

    Get_Work& operator=(Get_Work&& other) noexcept {
        flags      = other.flags;
        comp_idx   = other.comp_idx;
        thread_idx = other.thread_idx;
        decode_idx = other.decode_idx;
        bit_idx    = other.bit_idx;
        enum_idx   = other.enum_idx;
        replyto    = std::move(other.replyto);
        return *this;
    }
};

// poll stuff:
struct Poll_Work {
    u8 comp_idx;
    u8 thread_idx; // "map" idx inside the component
    int errno_code = 0; // this set to anything other than zero means there is an error

};

// publish stuff:
struct Decoded_Val {
    Jval_buif decoded_val;
    u64 raw_data;
};
struct Pub_Work {
    u8 comp_idx;
    u8 thread_idx; // this is actually "map_idx"
    int errno_code = 0; // this set to anything other than zero means there is an error
    std::chrono::nanoseconds response_time; // how long server takes to respond to requests (determined during polling)
    std::vector<Decoded_Val> pub_vals;

    Pub_Work() = default;

    Pub_Work(Pub_Work&& other) noexcept 
        : comp_idx(other.comp_idx), thread_idx(other.thread_idx), 
            errno_code(other.errno_code), response_time(other.response_time), 
            pub_vals(std::move(other.pub_vals)) {}

    Pub_Work& operator=(Pub_Work&& other) noexcept {
        comp_idx      = other.comp_idx;
        thread_idx    = other.thread_idx;
        errno_code    = other.errno_code;
        response_time = other.response_time;
        pub_vals      = std::move(other.pub_vals);
        return *this;
    }
};

// For IO threads to consume off of (listener and main produce on these):
struct IO_Work_Q {
    PCQ_Semaphore signaller;                          // signals that work exists (shared between both q's)
    rigtorp::MPMCQueue<Set_Work>  set_q{1024};        // highest priority (must be empty before going to poll_q)
    rigtorp::MPMCQueue<Poll_Work> poll_q{256 * 256};  // lowest priority (set_q must be empty before popping off of this -> enqueued in batches one whole component at a time)
};
// For main to consume off of (listener and IO threads produce on these):
struct Main_Work_Q {
    PCQ_Semaphore signaller;                        // signals that work exists (shared between both q's)
    rigtorp::SPSCQueue<Set_Work> set_q{1024};       // highest priority (must be empty before going to any other q -> NOTE(WALKER): main does "debounce" logic before requing work in another set_vals array)
    rigtorp::MPMCQueue<Pub_Work> pub_q{256 * 256};  // IO threads produce on this q for main to publish when a batch of polls is done (on a per component basis)
    rigtorp::SPSCQueue<Get_Work> get_q{1024};       // listener produces on this q for main to respond to get requests with json 
};

// helper function for try_pop() on SPSCQueue:
template<typename T>
static bool
try_pop (rigtorp::SPSCQueue<T>& q, T& work) noexcept {
    auto top = q.front();
    if (!top) return false;
    work = std::move(*top);
    q.pop();
    return true;
}

// ########################################################


// NOTE(WALKER): these messages are sent to the "aggregator" threads by the listener thread which control heartbeat and component synchronization
// struct Get_Chan_Msg
// {
//     Get_Request_Flags flags;
//     u8 comp_idx;
//     u8 thread_idx;
//     u8 decode_idx;
//     u8 bit_idx;
//     u8 enum_idx;
//     std::string replyto;

//     Get_Chan_Msg() = default;

//     Get_Chan_Msg(Get_Chan_Msg&& other) noexcept
//         : flags(other.flags), thread_idx(other.thread_idx), decode_idx(other.decode_idx), 
//             bit_idx(other.bit_idx), enum_idx(other.enum_idx), replyto(std::move(other.replyto))
//     {}

//     Get_Chan_Msg& operator=(Get_Chan_Msg&& other) noexcept
//     {
//         flags      = other.flags;
//         thread_idx = other.thread_idx;
//         decode_idx = other.decode_idx;
//         bit_idx    = other.bit_idx;
//         enum_idx   = other.enum_idx;
//         replyto    = std::move(other.replyto);
//         return *this;
//     }
// };
// size info:
// auto x = sizeof(Get_Chan_Msg);

// struct Pub_Info
// {
//     u64 raw_data = 0;
//     Jval_buif decoded_val;
// };
// size info:
// auto x = sizeof(Pub_Info);

// NOTE(WALKER): This is sent to the "aggregator" thread by the worker threads
// struct Pub_Chan_Msg
// {
//     int errno_code = 0; // this set to anything other than zero means there is an error
//     // std::chrono::nanoseconds conn_get_time; // how long a thread takes to acquire a modbus connection before polling (the mutex lock)
//     std::chrono::nanoseconds response_time; // how long server takes to respond to requests (determined during polling)
//     std::vector<Pub_Info> pub_vals;

//     Pub_Chan_Msg() = default;

//     Pub_Chan_Msg(Pub_Chan_Msg&& other) noexcept
//         : errno_code(other.errno_code), 
//         // conn_get_time(other.conn_get_time),
//         response_time(other.response_time), 
//         pub_vals(std::move(other.pub_vals)) 
//     {}

//     Pub_Chan_Msg& operator=(Pub_Chan_Msg&& other) noexcept
//     {
//         errno_code = other.errno_code;
//         // conn_get_time = other.conn_get_time;
//         response_time = other.response_time;
//         pub_vals = std::move(other.pub_vals);
//         return *this;
//     }
// };
// size info:
// auto x = sizeof(Pub_Chan_Msg);

// template<typename T, std::size_t Size = 1024>
// struct Wakeup_Channel
// {
//     rigtorp::SPSCQueue<T> q{Size};
// };
// size info:
// auto x = sizeof(Wakeup_Channel<int>);

struct Bit_Str_Encoding_Info
{
    u64 prev_unsigned_val;
    // debounce stuff (we we can set bits itself):
    std::bitset<64> debounce_to_set_mask; // these are the bits that need to be "debounced" (these are masked out before |'ing using prev_unsigned_val for sets)
    s64* debounce_last_set_times; // This is one per bit or one per enum for "individual_bits/enums"
    // std::pair<u8, u64>* care_masks; // unused for now (will be used for "individual_enums" case)
};
// size info:
// auto x = sizeof(Bit_Str_Encoding_Info);

struct Thread_Workspace
{
    Register_Types reg_type;
    u8 slave_address; // "device_id"
    u16 start_offset;
    u8 num_regs;
    u8 num_decode;
    // std::size_t client_thread_idx; // Each worker thread is given a unique idx in order to make sure that context locks are spaced out (for the shared context pool in their client)
    std::chrono::nanoseconds frequency;
    // Holding and Input:
    u16* raw_registers = nullptr;
    Decode_Info* decode_array = nullptr;
    Bit_Str_Encoding_Info* bit_str_encoding_array = nullptr; // Holding && individual_bits/enums only -> encoding needs this to keep the other bits unchanged on sets
    // Coil and Discrete Input:
    u8* bool_registers = nullptr;
    u16* bool_offset_array = nullptr; // to help save memory (only stores offsets -> no decode info)

    // std::chrono::nanoseconds debounce; // NOTE(WALKER): If we have debounce we need to sleep until the next debounce time if we have one instead of the normal time_left (a litle bit tricky)
    // std::bitset<MODBUS_MAX_READ_REGISTERS> debounce_to_set_mask; // records which values need to be debounced (if we have one)
    // s64* debounce_last_set_times = nullptr;
    // Jval_buif* debounce_to_set_vals = nullptr; // for Holding registers only
    // u8* debounce_to_set_bools = nullptr; // for Coils only (helps save memory)

    // std::mutex worker_mutex;
    // std::condition_variable worker_cond;
    // Wakeup_Channel<Set_Chan_Msg>* set_channel = nullptr; // you only have this if you are of type Holding or Coil
    // rigtorp::SPSCQueue<Pub_Chan_Msg> pub_channel{2};

    ~Thread_Workspace()
    {
        // if (set_channel) set_channel->~Wakeup_Channel();
    }

    // Tell the worker thread to stop (sets keep_running to false then notifies the thread):
    // void stop_thread()
    // {
    //     keep_running = false;
    //     {
    //         std::lock_guard<std::mutex> lk{worker_mutex};
    //     }
    //     worker_cond.notify_one();
    // }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Registers_Mapping& reg_map, const bool has_debounce)
    {
        std::size_t needed = 0;

        // raw input and decode stuff:
        if (reg_map.reg_type == Register_Types::Holding || reg_map.reg_type == Register_Types::Input)
        {
            needed += sizeof(u16) * reg_map.num_registers + alignof(u16) + sizeof(Decode_Info) * reg_map.map.size() + alignof(Decode_Info);
            if (reg_map.reg_type == Register_Types::Holding)
            {
                bool has_done_align = false;
                for (const auto& decode : reg_map.map)
                {
                    // if (decode.individual_bits || decode.individual_enums)
                    if (decode.individual_bits) // NOTE(WALKER): in the future, delete this line and uncomment the above one
                    {
                        if (!has_done_align) // for the whole array
                        {
                            has_done_align = true;
                            needed += alignof(Bit_Str_Encoding_Info);
                        }
                        needed += sizeof(Bit_Str_Encoding_Info);
                        if (has_debounce) needed += sizeof(s64) * decode.size * 16 + alignof(s64); // If we have "debounce" then we need to set aside space for the debounce_last_set_times inside the encoding array
                        // TODO(WALKER): make sure to allocate enough space in the future for the care_masks as well
                        // if (decode.individual_enums)
                        // {

                        // }
                    }
                }
            }
        }
        else // coils and discrete inputs:
        {
            needed += sizeof(u8) * reg_map.num_registers + sizeof(u16) * reg_map.map.size() + alignof(u16);
        }

        // debounce stuff:
        if (has_debounce && (reg_map.reg_type == Register_Types::Holding || reg_map.reg_type == Register_Types::Coil))
        {
            needed += sizeof(s64) * reg_map.map.size() + alignof(s64); // for last set times
            if (reg_map.reg_type == Register_Types::Holding)
            {
                needed += sizeof(Jval_buif) * reg_map.map.size() + alignof(Jval_buif);
            }
            else // Coils:
            {
                needed += sizeof(u8) * reg_map.map.size(); // align not needed (alignof == 1)
            }
        }

        // set channel stuff:
        // if (reg_map.reg_type == Register_Types::Holding || reg_map.reg_type == Register_Types::Coil)
        // {
        //     needed += sizeof(Wakeup_Channel<Set_Chan_Msg>) + alignof(Wakeup_Channel<Set_Chan_Msg>);
        // }

        return needed;
    }
};
// size info:
// auto x = sizeof(Thread_Workspace);

struct Decoded_Info
{
    u8 bit_str_array_idx = Decode_All_Idx;
    Decode_Flags flags;
    u64 raw_data = 0;
    Jval_buif decoded_val;
};
// size info:
// auto x = sizeof(Decoded_Info);

struct Decoded_Cache {
    Register_Types reg_type;
    u8 num_decode = 0;
    Decoded_Info* decoded_vals = nullptr; // For Holding and Input registers
    bool* bool_vals = nullptr; // for Coils and Discrete Inputs
    std::bitset<Max_Num_Decode> changed_mask;
    String_Storage_Handle* decode_ids = nullptr;
    Bit_Strings_Info_Array* bit_strings_arrays = nullptr; // Decoded_Info has indexes that can go to the appropriate array (helps minimize space)
    Thread_Workspace* thread_workspace = nullptr;

    ~Decoded_Cache()
    {
        if (thread_workspace) thread_workspace->~Thread_Workspace();
    }

    // helper functions:
    // TODO(WALKER): Get "bit_strings" stuff allocated ahead of time appropriately (kinda tricky)
    static std::size_t get_bytes_needed(const config_loader::Registers_Mapping& reg_map)
    {
        std::size_t needed = 0;

        if (reg_map.reg_type == Register_Types::Holding || reg_map.reg_type == Register_Types::Input)
        {
            needed += sizeof(Decoded_Info) * reg_map.map.size() + alignof(Decoded_Info);

            // compressed bit strings stuff:
            for (const auto& decode : reg_map.map)
            {
                if (decode.has_bit_strings)
                {
                    needed += sizeof(Bit_Strings_Info_Array) + alignof(Bit_Strings_Info_Array);
                    needed += sizeof(Bit_Strings_Info) * decode.compressed_bit_strings.size() + alignof(Bit_Strings_Info);
                }
            }
        }
        else // Coils and Discrete Inputs:
        {
            needed += sizeof(bool) * reg_map.map.size(); // no extra alignment needed (align 1)
        }

        needed += sizeof(String_Storage_Handle) * reg_map.map.size() + alignof(String_Storage_Handle);
        needed += sizeof(Thread_Workspace) + alignof(Thread_Workspace);

        return needed;
    }
};
// size info:
// auto x = sizeof(Decoded_Cache);

// this is also "aggregator" threads workspace
using timings_duration_type = std::chrono::duration<f64, std::milli>;
struct Component_Workspace
{
    u8 num_threads = 0; // aka "num_mappings"
    String_Storage_Handle comp_uri; // this stores /components/x as opposed to just component id
    Decoded_Cache* decoded_caches = nullptr; // thread workspace pointers are in here

    // polling stuff:
    bool is_polling = false; // main keeps track of if we've queued the poll work or not for this component
    u8 num_polls_in_q_left = 0; // keeps track of number of poll requests we have left in the q when popping off (when this reaches 0 we reset it and then publish appropriately)
    bool has_error_checked_already = false;
    u8 num_poll_errors = 0; // for keeping track for the number of times we have an error in a poll cycle (if we have no errors for a poll cycle then this resets -> one error means this increments, then at 5 we error out)
    s64 last_poll_time = 0; // this gets updated to the current time after a poll is done
    std::chrono::nanoseconds poll_time_left{}; // how much time we have left before polling (frequency - {lasat_poll_time - current_time})

    // Heartbeat stuff:
    bool heartbeat_enabled = false;
    bool component_connected = true;
    u8 heartbeat_read_thread_idx = Thread_All_Idx;
    u8 heartbeat_read_decode_idx = Thread_All_Idx;
    u8 heartbeat_write_thread_idx = Thread_All_Idx;
    u8 heartbeat_write_decode_idx = Thread_All_Idx;
    // std::chrono::nanoseconds heartbeat_frequency; // how often we check
    std::chrono::nanoseconds heartbeat_timeout; // how often we can go without the heartbeat changing
    s64 heartbeat_last_change_time; // the last time that we recorded that heartbeat has changed

    // /_timings request stuff:
    // timings_duration_type min_conn_get_time{std::numeric_limits<f64>::max()};
    // timings_duration_type max_conn_get_time;
    // timings_duration_type avg_conn_get_time;
    timings_duration_type min_response_time{std::numeric_limits<f64>::max()};
    timings_duration_type max_response_time;
    timings_duration_type avg_response_time;
    f64 num_timings_recorded = 0; // for average tracking (always increments by 1 whole number)

    Component_Workspace() = default;

    ~Component_Workspace()
    {
        if (decoded_caches)
        {
            for (u8 i = 0; i < num_threads; ++i)
            {
                decoded_caches[i].~Decoded_Cache();
            }
        }
    }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Component& component)
    {
        std::size_t needed = 0;

        needed += sizeof(Decoded_Cache) * component.registers.size() + alignof(Decoded_Cache);

        return needed;
    }
};

struct Msg_Staging_Area
{
    // u8 num_threads = 0;
    Component_Workspace* comp_workspace = nullptr;

    ~Msg_Staging_Area()
    {
        if (comp_workspace) comp_workspace->~Component_Workspace();
    }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Component& component)
    {
        std::size_t needed = 0;

        if(0)needed += component.registers.size();
        needed += sizeof(Component_Workspace) + alignof(Component_Workspace);

        return needed;
    }
};
// size info:
// auto x = sizeof(Msg_Staging_Area);

struct IO_Thread_Workspace {
    alignas(64) modbus_t* ctx = nullptr;
    std::atomic_bool keep_running{false};
    std::future<bool> thread_future; // IO Thread future

    ~IO_Thread_Workspace() { if (ctx) { modbus_close(ctx); modbus_free(ctx); } }
};
// size info:
// auto x = sizeof(IO_Thread_Workspace);

struct IO_Thread_Pool
{
    u8 max_num_conns = 1; // This is either "max_num_conns" in the config or "total_num_threads" after reading the config
    u8 num_conns = 0;       // actual num_threads variable underneath this new system now
    IO_Thread_Workspace* io_thread_workspaces;
    IO_Work_Q io_work_qs;

    ~IO_Thread_Pool() {
        for (u8 i = 0; i < max_num_conns; ++i) {
            io_thread_workspaces[i].~IO_Thread_Workspace();
        }
    }

    void stop_threads() {
        // tell each thread to stop:
        for (u8 i = 0; i < max_num_conns; ++i) {
            io_thread_workspaces[i].keep_running = false;
        }
        // signal each thread after setting the keep_running variable to false:
        for (u8 i = 0; i < max_num_conns; ++i) {
            io_work_qs.signaller.signal();
        }
        // wait for each thread to stop:
        for (u8 i = 0; i < num_conns; ++i) {
            io_thread_workspaces[i].thread_future.wait();
        }
    }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Client_Config& config) {
        std::size_t needed = 0;

        needed += sizeof(IO_Thread_Workspace) * config.connection.max_num_conns + alignof(IO_Thread_Workspace);

        return needed;
    }
};
// size info:
// auto x = sizeof(IO_Thread_Pool);

struct Connection_Info {
    Conn_Types type;

    // TCP:
    u16 port;

    // RTU:
    s32 baud_rate;
    char parity;
    u8 data_bits;
    u8 stop_bits;

    String_Storage_Handle name;
    String_Storage_Handle ip_serial_str; // NOTE(WALKER): This requires size + 1 characters (modbus uses c strings -> '\0' termination)
};
// size info:
// auto x = sizeof(Connection_Info);

struct Connection_Workspace {
    bool connected = false; // To tell main whether or not this workspace is up or not
    std::size_t total_num_threads; // aka "total_num_mappings" only counts worker threads for this number (this is NOT the same as num_conns)
    Connection_Info conn_info;
    IO_Thread_Pool IO_thread_pool;
};
// size info:
// auto x = sizeof(Connection_Workspace);

struct Client_Workspace {
    Connection_Workspace conn_workspace; // this is where IO_Threads have their stuff
    u8 num_comps = 0;
    bool has_debounce_somewhere = false;
    Msg_Staging_Area* msg_staging_areas = nullptr; // client workspace pointers are in here

    // main stuff:
    Main_Work_Q main_work_qs;

    // listener stuff:
    fims fims_gateway; // "aggregator" threads also use this to send out on fims (pubs and get requests)
    Receiver_Bufs<Max_Expected_Data_Size, Simdj_Padding> receiver_bufs;
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;
    phmap::flat_hash_map<std::size_t, Uri_Info> uri_map;
    fmt::basic_memory_buffer<char, 256> uri_buf; // so we can store x/y as we go along for multi sets (listener uses this)

    std::atomic_bool keep_running{false};
    std::future<bool> listener_future;

    ~Client_Workspace() {
        if (msg_staging_areas) {
            for (u8 i = 0; i < num_comps; ++i) {
                msg_staging_areas[i].~Msg_Staging_Area();
            }
        }
    }

    // starts up the modbus stuff for this client:
    bool startup_modbus_conns(String_Storage& string_storage)
    {
        // teardown the old connections if we have any:
        for (u8 i = 0; i < conn_workspace.IO_thread_pool.num_conns; ++i)
        {
            auto ctx = conn_workspace.IO_thread_pool.io_thread_workspaces[i].ctx;
            if (ctx) { modbus_close(ctx); modbus_free(ctx); }
        }
        conn_workspace.IO_thread_pool.num_conns = 0;

        // Now connect the modbus contexts for this client:
        for (std::size_t i = 0; i < conn_workspace.total_num_threads; ++i)
        {
            if (i >= conn_workspace.IO_thread_pool.max_num_conns) break; // cannot exceed max_num_conns for threads

            auto& curr_conn = conn_workspace.IO_thread_pool.io_thread_workspaces[i];
            auto connected = -1;
            if (conn_workspace.conn_info.type == Conn_Types::TCP)
            {
                curr_conn.ctx = modbus_new_tcp(string_storage.get_str(conn_workspace.conn_info.ip_serial_str).data(), conn_workspace.conn_info.port);
            }
            else // RTU:
            {
                curr_conn.ctx = modbus_new_rtu(string_storage.get_str(conn_workspace.conn_info.ip_serial_str).data(), 
                                                    conn_workspace.conn_info.baud_rate, 
                                                    conn_workspace.conn_info.parity, 
                                                    conn_workspace.conn_info.data_bits,
                                                    conn_workspace.conn_info.stop_bits);
            }
            connected = modbus_connect(curr_conn.ctx);
            if (connected == -1 && i == 0) // we failed on first connection for this client (can't connect for this client)
            {
                if (conn_workspace.conn_info.type == Conn_Types::TCP)
                {
                    NEW_FPS_ERROR_PRINT("could not establish a single TCP connection to modbus server for client \"{}\", on ip: {}, with port: {}, err_code = {}, err_str = \"{}\"\n", 
                        string_storage.get_str(conn_workspace.conn_info.name),
                        string_storage.get_str(conn_workspace.conn_info.ip_serial_str), 
                        conn_workspace.conn_info.port, 
                        errno,
                        modbus_strerror(errno));
                }
                else // RTU:
                {
                    NEW_FPS_ERROR_PRINT("could not establish a single serial RTU connection to modbus server for client \"{}\", with serial_device: {}, baud_rate: {}, parity: {}, data_bits: {}, and stop_bits: {}, err_code = {}, err_str = \"{}\"\n", 
                        string_storage.get_str(conn_workspace.conn_info.name),
                        string_storage.get_str(conn_workspace.conn_info.ip_serial_str), 
                        conn_workspace.conn_info.baud_rate, 
                        conn_workspace.conn_info.parity,
                        conn_workspace.conn_info.data_bits,
                        conn_workspace.conn_info.stop_bits,
                        errno,
                        modbus_strerror(errno));
                }
                return false;
            }
            if (connected == -1) // reached our limit (we have a limited number of connections for this piece of hardware):
            {
                break;
            }
            // successful connection (setup the rest of the modbus context stuff):
            // PHIL tru this again
            modbus_set_error_recovery(curr_conn.ctx, MODBUS_ERROR_RECOVERY_PROTOCOL);
            //modbus_set_error_recovery(curr_conn.ctx,  (modbus_error_recovery_mode)(MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));

            ++conn_workspace.IO_thread_pool.num_conns;
            conn_workspace.connected = true;
        }
        return true;
    }

    void stop_client() {
        keep_running = false; // tell listener to stop
        conn_workspace.IO_thread_pool.stop_threads();
        // wait for listener to stop:
        listener_future.wait();
    }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Client_Config& config)
    {
        std::size_t needed = 0;

        needed += sizeof(Msg_Staging_Area) * config.components.size() + alignof(Msg_Staging_Area);

        return needed;
    }
};
// size info:
// auto x = sizeof(Client_Workspace);

// NOTE(WALKER): This replaces hardware vector (for now there is only 1 workspace)
// in the future there will be more than one "Client Workspace" (for ip_ranges)
// that way site controller can compress how much crap they have to deal with
// into one simple executable/config
struct Main_Workspace
{
    Simple_Arena arena;
    String_Storage string_storage;
    TSCNS mono_clock;
    u8 num_clients = 1;
    std::atomic_bool reload = false; // main runs a do/while loop and checks for this
    Client_Workspace** client_workspaces = nullptr;

    // main condition variable stuff:
    std::atomic_bool start_signal{false}; // to synchronize every single thread upon first startup
    std::mutex main_mutex;
    std::condition_variable main_cond;

    ~Main_Workspace()
    {
        cleanup();
    }

    void cleanup()
    {
        if (client_workspaces)
        {
            for (u8 i = 0; i < num_clients; ++i)
            {
                if (client_workspaces[i]) client_workspaces[i]->~Client_Workspace();
            }
        }
    }

    // helper functions:
    static std::size_t get_bytes_needed(const config_loader::Client_Configs_Array& configs_array)
    {
        std::size_t needed = 0;

        needed += configs_array.total_str_length;
        needed += sizeof(Client_Workspace*) * configs_array.configs.size() + alignof(Client_Workspace*);
        needed += sizeof(Client_Workspace)  * configs_array.configs.size() + alignof(Client_Workspace);

        return needed;
    }
};
// size info:
// auto x = sizeof(Main_Workspace);

// config loading integration stuff:
static bool initialize_arena_from_config(const config_loader::Client_Configs_Array& configs_array, Main_Workspace& main_workspace)
{
    std::size_t bytes_needed = 0;
    bytes_needed += Main_Workspace::get_bytes_needed(configs_array);
    for (const auto& config : configs_array.configs)
    {
        bytes_needed += Client_Workspace::get_bytes_needed(config);
        bytes_needed += IO_Thread_Pool::get_bytes_needed(config);
        for (const auto& comp : config.components)
        {
            bytes_needed += Component_Workspace::get_bytes_needed(comp);
            bytes_needed += Msg_Staging_Area::get_bytes_needed(comp);
            for (const auto& reg_map : comp.registers)
            {
                bytes_needed += Decoded_Cache::get_bytes_needed(reg_map);
                bytes_needed += Thread_Workspace::get_bytes_needed(reg_map, comp.debounce > 0);
            }
        }
    }
    return main_workspace.arena.initialize(bytes_needed);
}

static bool allocate_main_workspace_from_config(const config_loader::Client_Configs_Array& configs_array, Main_Workspace& main_workspace)
{
    if (!initialize_arena_from_config(configs_array, main_workspace))
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("cannot allocate the memory arena for modbus client for some reason. Something is very wrong, as this should never happen. Please check to make sure there isn't a rogue process running that is eating up all the memory\n");
        return false;
    }

    // string storage:
    if (!main_workspace.arena.allocate(main_workspace.string_storage.data, configs_array.total_str_length)) return false;
    main_workspace.string_storage.allocated = static_cast<u16>(configs_array.total_str_length);
    main_workspace.string_storage.current_idx = 0; // for reload purposes (in case this function is called again)

    // client workspaces (pointers):
    if (!main_workspace.arena.allocate(main_workspace.client_workspaces, configs_array.configs.size())) return false;
    main_workspace.num_clients = static_cast<u8>(configs_array.configs.size());

    // client workspaces (one whole workspace at a time):
    for (u8 client_idx = 0; client_idx < main_workspace.num_clients; ++client_idx)
    {
        auto& config = configs_array.configs[client_idx];

        if (!main_workspace.arena.allocate(main_workspace.client_workspaces[client_idx])) return false;
        auto& curr_client_workspace = *main_workspace.client_workspaces[client_idx];

        // msg staging areas for components (listener stuff):
        if (!main_workspace.arena.allocate(curr_client_workspace.msg_staging_areas, config.components.size())) return false;
        curr_client_workspace.num_comps = static_cast<u8>(config.components.size());

        for (u8 comp_idx = 0; comp_idx < curr_client_workspace.num_comps; ++comp_idx)
        {
            // if (!main_workspace.arena.allocate(curr_client_workspace.msg_staging_areas[comp_idx].set_msgs, config.components[comp_idx].registers.size())) return false;
            // curr_client_workspace.msg_staging_areas[comp_idx].num_threads = static_cast<u8>(config.components[comp_idx].registers.size());
        }

        // connection workspace stuff:

        // total_num_threads (for ctx pool):
        curr_client_workspace.conn_workspace.total_num_threads = config.total_num_threads;
        // max_num_conns (for ctx pool):
        curr_client_workspace.conn_workspace.IO_thread_pool.max_num_conns = static_cast<u8>(config.connection.max_num_conns);
        // connection context pool (for aggregator and worker threads -> IO_thread_pool stuff):
        if (!main_workspace.arena.allocate(curr_client_workspace.conn_workspace.IO_thread_pool.io_thread_workspaces, curr_client_workspace.conn_workspace.IO_thread_pool.max_num_conns)) return false;

        // allocate component and thread stuff:
        for (u8 comp_idx = 0; comp_idx < curr_client_workspace.num_comps; ++comp_idx)
        {
            const auto& curr_config_comp = config.components[comp_idx];

            if (!main_workspace.arena.allocate(curr_client_workspace.msg_staging_areas[comp_idx].comp_workspace)) return false;
            auto& curr_comp_workspace = *curr_client_workspace.msg_staging_areas[comp_idx].comp_workspace;

            if (!main_workspace.arena.allocate(curr_comp_workspace.decoded_caches, curr_config_comp.registers.size())) return false;
            curr_comp_workspace.num_threads = static_cast<u8>(curr_config_comp.registers.size());

            // initialize decoded caches (aggregator thread stuff):
            for (u8 thread_idx = 0; thread_idx < curr_comp_workspace.num_threads; ++thread_idx)
            {
                const auto& curr_config_reg_map = curr_config_comp.registers[thread_idx];

                auto& curr_decoded_cache = curr_comp_workspace.decoded_caches[thread_idx];
                if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                {
                    if (!main_workspace.arena.allocate(curr_decoded_cache.decoded_vals, curr_config_reg_map.map.size())) return false;
                }
                else // Coils and Discrete Inputs:
                {
                    if (!main_workspace.arena.allocate(curr_decoded_cache.bool_vals, curr_config_reg_map.map.size())) return false;
                }
                if (!main_workspace.arena.allocate(curr_decoded_cache.decode_ids, curr_config_reg_map.map.size())) return false;

                if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                {
                    if (curr_config_reg_map.num_bit_str_arrays > 0)
                    {
                        if (!main_workspace.arena.allocate(curr_decoded_cache.bit_strings_arrays, curr_config_reg_map.num_bit_str_arrays)) return false;
                    }
                }
            }

            // initializing each of the "bit_strings" arrays for each decoded_cache using the "compressed" versions:
            for (u8 thread_idx = 0; thread_idx < curr_comp_workspace.num_threads; ++thread_idx)
            {
                const auto& curr_config_reg_map = curr_config_comp.registers[thread_idx];

                auto& curr_decoded_cache = curr_comp_workspace.decoded_caches[thread_idx];

                if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                {
                    if (curr_config_reg_map.num_bit_str_arrays > 0)
                    {
                        u8 curr_bit_str_idx = 0;
                        for (const auto& decode : curr_config_reg_map.map)
                        {
                            if (decode.has_bit_strings)
                            {
                                if (!main_workspace.arena.allocate(curr_decoded_cache.bit_strings_arrays[curr_bit_str_idx].bit_strs, decode.compressed_bit_strings.size())) return false;
                                ++curr_bit_str_idx;
                            }
                        }
                    }
                }
            }

            // initialize thread workspaces:
            for (u8 thread_idx = 0; thread_idx < curr_comp_workspace.num_threads; ++thread_idx)
            {
                const auto& curr_config_reg_map = curr_config_comp.registers[thread_idx];

                auto& curr_decoded_cache = curr_comp_workspace.decoded_caches[thread_idx];
                if (!main_workspace.arena.allocate(curr_decoded_cache.thread_workspace)) return false;

                // allocate the stuff the thread needs (right next to the workspace itself):
                auto& curr_thread_workspace = *curr_decoded_cache.thread_workspace;
                if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                {
                    if (!main_workspace.arena.allocate(curr_thread_workspace.raw_registers, curr_config_reg_map.num_registers)) return false;
                    if (!main_workspace.arena.allocate(curr_thread_workspace.decode_array, curr_config_reg_map.map.size())) return false;
                    if (curr_config_reg_map.reg_type == Register_Types::Holding && curr_config_reg_map.num_bit_str_arrays > 0)
                    {
                        u8 total_individual_bit_str_types = 0;
                        for (const auto& decode : curr_config_reg_map.map)
                        {
                            // if (decode.individual_bits || decode.individual_enums)
                            if (decode.individual_bits) // NOTE(WALKER): in the future, replace this line with the above one
                            {
                                ++total_individual_bit_str_types;
                            }
                            // NOTE(WALKER): In the future make sure to allocate the space needed for the "care_masks" for "individual_enums"
                        }
                        if (total_individual_bit_str_types > 0)
                        {
                            if (!main_workspace.arena.allocate(curr_thread_workspace.bit_str_encoding_array, total_individual_bit_str_types)) return false;
                            // NOTE(WALKER): In the future make sure to allocate the space needed for the "care_masks" for "individual_enums"
                        }
                    }
                }
                else // coils and discrete inputs:
                {
                    if (!main_workspace.arena.allocate(curr_thread_workspace.bool_registers, curr_config_reg_map.num_registers)) return false;
                    if (!main_workspace.arena.allocate(curr_thread_workspace.bool_offset_array, curr_config_reg_map.map.size())) return false;
                }

                // debounce stuff:
                // if (curr_config_comp.debounce > 0)
                // {
                //     if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Coil)
                //     {
                //         if (!main_workspace.arena.allocate(curr_thread_workspace.debounce_last_set_times, curr_config_reg_map.map.size())) return false;
                //         if (curr_config_reg_map.reg_type == Register_Types::Holding)
                //         {
                //             if (!main_workspace.arena.allocate(curr_thread_workspace.debounce_to_set_vals, curr_config_reg_map.map.size())) return false;
                //             // Do debounce setup for each of the encoding arrays if we need it:
                //             u8 encoding_array_idx = 0;
                //             for (const auto& decode : curr_config_reg_map.map)
                //             {
                //                 if (decode.individual_bits)
                //                 {
                //                     if (!main_workspace.arena.allocate(curr_thread_workspace.bit_str_encoding_array[encoding_array_idx].debounce_last_set_times, decode.size * 16)) return false;
                //                     ++encoding_array_idx;
                //                 }
                //             }
                //         }
                //         else // Coil:
                //         {
                //             if (!main_workspace.arena.allocate(curr_thread_workspace.debounce_to_set_bools, curr_config_reg_map.map.size())) return false;
                //         }
                //     }
                // }

                // set channel stuff:
                // if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Coil)
                // {
                //     if(!main_workspace.arena.allocate(curr_thread_workspace.set_channel)) return false;
                // }
            }
        }
    }

    return true;
}

static bool initialize_main_workspace_from_config(const config_loader::Client_Configs_Array& configs_array, Main_Workspace& main_workspace)
{
    if (!allocate_main_workspace_from_config(configs_array, main_workspace))
    {
        NEW_FPS_ERROR_PRINT(R"(
Ran out of arena memory. Please contact the software team for modbus client, and tell them to GIT GUD, because this should NEVER happen.
memory allocated:   {}
memory in use:      {}
memory left:        {}
)",         main_workspace.arena.allocated,
            main_workspace.arena.current_idx,
            main_workspace.arena.allocated - main_workspace.arena.current_idx);
        return false;
    }

    std::unordered_map<std::string, String_Storage_Handle> str_handles_map;

    // helper lambda for setting string handles and checking stuff:
    auto str_handle_check = [&] (String_Storage_Handle& handle, const std::string& str, const bool is_c_str = false) -> bool
    {
        std::string_view to_append = str;
        if (is_c_str) to_append = std::string_view{str.data(), str.size() + 1}; // for the extra '\0' needed (assumes you're viewing a null terminated string)

        auto& map_handle = str_handles_map[str];
        if (map_handle.size == 0)
        {
            map_handle = main_workspace.string_storage.append_str(to_append);
            if (map_handle.size == 0)
            {
                NEW_FPS_ERROR_PRINT(R"(
Ran out of string storage space. Please contact the software team for modbus client, and tell them to GIT GUD, because this should NEVER happen.
storage amount:        {}
storage current index: {}
storage left:          {}
current string:        {}
is c_str:              {}
)",                     main_workspace.string_storage.allocated,
                        main_workspace.string_storage.current_idx,
                        main_workspace.string_storage.allocated - main_workspace.string_storage.current_idx,
                        str,
                        is_c_str);
                return false;
            }
        }
        handle = map_handle;
        return true;
    };

    for (u8 client_idx = 0; client_idx < main_workspace.num_clients; ++client_idx)
    {
        // std::size_t client_thread_idx = 0; // variable to give each worker thread a unique id for their workspace (for client wide access indexing -> for example: a client's context pool to the modbus server)

        const auto& curr_config_client = configs_array.configs[client_idx];
        auto& curr_client_workspace = *main_workspace.client_workspaces[client_idx];

        // stuff we need to fill:
        auto& conn_workspace = curr_client_workspace.conn_workspace;
        auto& uri_map = curr_client_workspace.uri_map;

        // fill in conn workspace stuff:

        // name:
        if (!str_handle_check(conn_workspace.conn_info.name, curr_config_client.connection.name)) return false;
        // type:
        conn_workspace.conn_info.type = curr_config_client.connection.conn_type;
        // tcp stuff:
        conn_workspace.conn_info.port = static_cast<u16>(curr_config_client.connection.port);
        // rtu stuff:
        conn_workspace.conn_info.baud_rate = static_cast<s32>(curr_config_client.connection.baud_rate);
        conn_workspace.conn_info.parity    = curr_config_client.connection.parity;
        conn_workspace.conn_info.data_bits = static_cast<u8>(curr_config_client.connection.data_bits);
        conn_workspace.conn_info.stop_bits = static_cast<u8>(curr_config_client.connection.stop_bits);
        // ip_serial stuff:
        if (!curr_config_client.connection.ip_address.empty()) // tcp:
        {
            if (!str_handle_check(conn_workspace.conn_info.ip_serial_str, curr_config_client.connection.ip_address, true)) return false;
        }
        else // rtu:
        {
            if (!str_handle_check(conn_workspace.conn_info.ip_serial_str, curr_config_client.connection.serial_device, true)) return false;
        }

        // components:
        for (u8 comp_idx = 0; comp_idx < curr_client_workspace.num_comps; ++comp_idx)
        {
            const auto& curr_config_comp = curr_config_client.components[comp_idx];
            auto& curr_comp_workspace = *curr_client_workspace.msg_staging_areas[comp_idx].comp_workspace;

            // heartbeat stuff:
            curr_comp_workspace.heartbeat_enabled          = curr_config_comp.heartbeat_enabled;
            // curr_comp_workspace.heartbeat_frequency        = std::chrono::milliseconds{curr_config_comp.heartbeat_check_frequency};
            curr_comp_workspace.heartbeat_timeout          = std::chrono::milliseconds{curr_config_comp.heartbeat_timeout};
            curr_comp_workspace.heartbeat_read_thread_idx  = curr_config_comp.heartbeat_read_reg_idx;
            curr_comp_workspace.heartbeat_read_decode_idx  = curr_config_comp.heartbeat_read_decode_idx;
            curr_comp_workspace.heartbeat_write_thread_idx = curr_config_comp.heartbeat_write_reg_idx;
            curr_comp_workspace.heartbeat_write_decode_idx = curr_config_comp.heartbeat_write_decode_idx;

            // comp uri stuff:
            const auto comp_uri = fmt::format("{}{}", Main_Uri_Frag, curr_config_comp.id);
            if (!str_handle_check(curr_comp_workspace.comp_uri, comp_uri)) return false;
            // uri map stuff:
            auto& curr_comp_uri_info = uri_map[uri_map_hash(curr_config_comp.id)];
            curr_comp_uri_info.component_idx = comp_idx;
            curr_comp_uri_info.thread_idx = Thread_All_Idx;
            curr_comp_uri_info.decode_idx = Decode_All_Idx;
            curr_comp_uri_info.bit_idx = Bit_All_Idx;
            curr_comp_uri_info.enum_idx = Enum_All_Idx;

            // thread_workspace stuff:
            for (u8 thread_idx = 0; thread_idx < curr_comp_workspace.num_threads; ++thread_idx)
            {
                u8 curr_bit_str_array_idx = 0;
                const auto& curr_config_reg_map = curr_config_comp.registers[thread_idx];
                auto& curr_decoded_cache = curr_comp_workspace.decoded_caches[thread_idx];
                auto& curr_thread_workspace = *curr_decoded_cache.thread_workspace;

                // setup client thread index for this thread's workspace:
                // curr_thread_workspace.client_thread_idx = client_thread_idx;
                // ++client_thread_idx;

                // decoded cache stuff:
                curr_decoded_cache.num_decode = static_cast<u8>(curr_config_reg_map.map.size());
                curr_decoded_cache.reg_type = curr_config_reg_map.reg_type;
                for (u8 decode_idx = 0; decode_idx < curr_decoded_cache.num_decode; ++decode_idx)
                {
                    const auto& curr_config_decode = curr_config_reg_map.map[decode_idx];

                    // id:
                    // TODO(WALKER): in the future make sure that we also check for "individual_enums" -> unless we also start pubbing the base value along with the bit_strings (instead of replacing it)
                    if (!curr_config_decode.individual_bits && !str_handle_check(curr_decoded_cache.decode_ids[decode_idx], curr_config_reg_map.map[decode_idx].id)) return false;

                    // Coils and Discrete Inputs just get the bool values themselves (no extra decoding is necessary for stringification)
                    if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                    {
                        auto& curr_decoded_val = curr_decoded_cache.decoded_vals[decode_idx];
                        // flags:
                        curr_decoded_val.flags.set_size(curr_config_decode.size);
                        curr_decoded_val.flags.set_word_swapped(curr_config_comp.word_swap);
                        curr_decoded_val.flags.set_multi_write_op_code(curr_config_decode.multi_write_op_code);
                        curr_decoded_val.flags.set_signed(curr_config_decode.Signed);
                        curr_decoded_val.flags.set_float(curr_config_decode.Float);
                        curr_decoded_val.flags.set_bit_field(curr_config_decode.bit_field);
                        curr_decoded_val.flags.set_individual_bits(curr_config_decode.individual_bits);
                        curr_decoded_val.flags.set_enum(curr_config_decode.Enum);
                        curr_decoded_val.flags.set_use_masks(curr_config_decode.uses_masks);

                        // bit_strings stuff:
                        if (!curr_config_decode.compressed_bit_strings.empty())
                        {
                            curr_decoded_val.bit_str_array_idx = curr_bit_str_array_idx; // set bit_strings_idx

                            auto& curr_bit_str_array = curr_decoded_cache.bit_strings_arrays[curr_bit_str_array_idx];
                            ++curr_bit_str_array_idx;
                            curr_bit_str_array.num_bit_strs = static_cast<u8>(curr_config_decode.compressed_bit_strings.size());
                            curr_bit_str_array.care_mask = curr_config_decode.care_mask;

                            // transfer over compressed bit strings:
                            for (u8 bit_str_idx = 0; bit_str_idx < curr_config_decode.compressed_bit_strings.size(); ++bit_str_idx)
                            {
                                const auto& bit_str = curr_config_decode.compressed_bit_strings[bit_str_idx];
                                auto& curr_bit_str  = curr_bit_str_array.bit_strs[bit_str_idx];

                                if (!bit_str.is_unknown && !bit_str.is_ignored && !str_handle_check(curr_bit_str.str, bit_str.str)) return false;
                                curr_bit_str.enum_val  = bit_str.enum_val;
                                curr_bit_str.begin_bit = static_cast<u8>(bit_str.begin_bit);
                                curr_bit_str.end_bit   = static_cast<u8>(bit_str.end_bit);
                                curr_bit_str.flags.set_unknown(bit_str.is_unknown);
                                curr_bit_str.flags.set_ignored(bit_str.is_ignored); // TODO(WALKER): remove this, as ignored bits will always be zero anyway (even end bits) -> they will be checked but they will never go to true anyway
                                // TODO(WALKER): In the future get "individual_enums"/"enum_field" stuff carried over

                            }
                        }
                    }
                }
                curr_bit_str_array_idx = 0; // reset bit_str_array_idx for the thread workspace

                // thread_workspace stuff:
                curr_thread_workspace.num_decode    = static_cast<u8>(curr_config_reg_map.map.size());
                curr_thread_workspace.num_regs      = curr_config_reg_map.num_registers;
                curr_thread_workspace.start_offset  = curr_config_reg_map.start_offset;
                curr_thread_workspace.reg_type      = curr_config_reg_map.reg_type;
                curr_thread_workspace.slave_address = static_cast<u8>(curr_config_comp.device_id);
                curr_thread_workspace.frequency     = std::chrono::milliseconds{curr_config_comp.frequency};
                // curr_thread_workspace.debounce      = std::chrono::milliseconds{curr_config_comp.debounce};

                for (u8 decode_idx = 0; decode_idx < curr_thread_workspace.num_decode; ++decode_idx)
                {
                    const auto& curr_config_decode = curr_config_reg_map.map[decode_idx];
                    auto& curr_decode_info = curr_thread_workspace.decode_array[decode_idx];
                    auto& curr_bool_offset = curr_thread_workspace.bool_offset_array[decode_idx];

                    if (curr_config_reg_map.reg_type == Register_Types::Holding || curr_config_reg_map.reg_type == Register_Types::Input)
                    {
                        curr_decode_info.offset           = static_cast<u16>(curr_config_decode.offset);
                        curr_decode_info.invert_mask      = curr_config_decode.invert_mask;
                        curr_decode_info.scale            = curr_config_decode.scale;
                        curr_decode_info.shift            = curr_config_decode.shift;
                        curr_decode_info.starting_bit_pos = curr_config_decode.starting_bit_pos;
                        curr_decode_info.care_mask        = curr_config_decode.care_mask;
                        // flags:
                        curr_decode_info.flags.set_size(curr_config_decode.size);
                        curr_decode_info.flags.set_word_swapped(curr_config_comp.word_swap);
                        curr_decode_info.flags.set_multi_write_op_code(curr_config_decode.multi_write_op_code);
                        curr_decode_info.flags.set_signed(curr_config_decode.Signed);
                        curr_decode_info.flags.set_float(curr_config_decode.Float);
                        curr_decode_info.flags.set_bit_field(curr_config_decode.bit_field);
                        curr_decode_info.flags.set_individual_bits(curr_config_decode.individual_bits);
                        curr_decode_info.flags.set_enum(curr_config_decode.Enum);
                        curr_decode_info.flags.set_use_masks(curr_config_decode.uses_masks);
                        
                        if (!curr_config_decode.compressed_bit_strings.empty())
                        {
                            // if (curr_config_decode.individual_bits || curr_config_decode.individual_enums)
                            if (curr_config_decode.individual_bits) // NOTE(WALKER): In the future delete this line and uncomment the above line
                            {
                                curr_decode_info.bit_str_array_idx = curr_bit_str_array_idx;
                                ++curr_bit_str_array_idx;
                                // TODO(WALKER): make sure to get in "care_masks" here in the future
                                // if (curr_config_decode.individual_enums)
                                // {

                                // }
                            }
                        }
                    }
                    else // Coil and Discrete Input:
                    {
                        curr_bool_offset = static_cast<u16>(curr_config_decode.offset);
                    }

                    // uri_map stuff:
                    if (curr_config_decode.individual_bits) // put individual_bits uris into the uri_map:
                    {
                        // NOTE(WALKER): There should be no "unknown" or "ignored" bits in the compressed array for this (that is for "bit_field" only)
                        for (const auto& bit_str : curr_config_decode.compressed_bit_strings)
                        {
                            const auto individual_bit_uri = fmt::format("{}/{}", curr_config_comp.id, bit_str.str);
                            auto& curr_decode_uri_info = uri_map[uri_map_hash(individual_bit_uri)];
                            curr_decode_uri_info.reg_type = curr_config_reg_map.reg_type;
                            curr_decode_uri_info.flags.set_individual_bit(true);
                            curr_decode_uri_info.component_idx = comp_idx;
                            curr_decode_uri_info.thread_idx    = thread_idx;
                            curr_decode_uri_info.decode_idx    = decode_idx;
                            curr_decode_uri_info.bit_idx       = static_cast<u8>(bit_str.begin_bit);
                            curr_decode_uri_info.enum_idx      = Enum_All_Idx;
                        }
                    }
                    // else if (curr_config_decode.individual_enums)
                    // {

                    // }
                    else // normal:
                    {
                        const auto decode_uri = fmt::format("{}/{}", curr_config_comp.id, curr_config_decode.id);
                        auto& curr_decode_uri_info = uri_map[uri_map_hash(decode_uri)];
                        curr_decode_uri_info.reg_type = curr_config_reg_map.reg_type;
                        curr_decode_uri_info.flags.set_individual_bit(false);
                        curr_decode_uri_info.component_idx = comp_idx;
                        curr_decode_uri_info.thread_idx = thread_idx;
                        curr_decode_uri_info.decode_idx = decode_idx;
                        curr_decode_uri_info.bit_idx = Bit_All_Idx;
                        curr_decode_uri_info.enum_idx = Enum_All_Idx;
                    }
                }
            }
        }
    }

    return true;
}
