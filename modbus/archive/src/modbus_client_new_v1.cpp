#include <bitset>
#include <thread>
#include <future>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <string_view>

#include "simdjson_noexcept.hpp" // for json parsing
#include "spdlog/details/fmt_helper.h" // for Timestamp formattting
#include "spdlog/fmt/compile.h"
#include "spdlog/fmt/bundled/ranges.h"
#include "spdlog/fmt/chrono.h" // for chrono formatting
#include "parallel_hashmap/phmap.h" // for storing the data in a flat hashtable
#include "rigtorp/SPSCQueue.h" // for "channels"
#include "tscns.h" // for a more robust and less time consuming steady clock. Source: https://github.com/MengRao/tscns

#include "fims/libfims_special.h" // for fims connection
#include "fims/defer.hpp" // for some basic defer calls to make my life easier
#include "Jval_buif.hpp" // class that we store so we can stringify it later (for gets and pubs[on change]) -> tagged union
#include "string_storage.hpp" // for storing ids found in the config
#include "new_modbus_utils.hpp"

using namespace std::chrono_literals;

enum class Conn_Types : uint8_t
{
    TCP,
    RTU
};

enum class Method_Types : uint8_t
{
    Set,
    Get,
};
// temp formatter for debug purposes (not in the final product)
template<>
struct fmt::formatter<Method_Types>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const Method_Types& mt, FormatContext& ctx) const
    {
        switch (mt)
        {
            case Method_Types::Get:
                return fmt::format_to(ctx.out(), "Get");
            default:
                return fmt::format_to(ctx.out(), "Set");
        }
    }
};

// POSSIBLE TODO(WALKER): This might or might not get configs from dbi, we'll have to see in the future
// if modbus_client is going to do that or not
enum class Config_Types : uint8_t
{
    File,
    Dbi
};

// constexpr constants:
inline constexpr uint8_t max_num_hardware = 33; // maximum number of ess controllers site controller can connect to really (all hardware for ip ranges must be identical)
inline constexpr uint8_t num_default_hardware = 1; // default number of hardware we have in our hardware vector before we start heap allocating ("short hardware optimization"). Might change to two for dual bms systems
inline constexpr uint8_t decode_all_index = MODBUS_MAX_READ_REGISTERS; // set for a uri that we know contains multiple threads in its thread mask (for either set all or get all)
inline constexpr uint8_t max_num_components = MAX_SUBSCRIPTIONS; // maximum number of fims uri subscriptions
inline constexpr uint8_t max_num_threads = 64; // this is really the maximum number of worker threads
inline constexpr uint8_t max_num_conns = 32;
inline constexpr uint8_t max_chan_msgs = 16; // the maximum number of messages that are accepted over channels before 
inline constexpr std::size_t max_expected_msg_size = 10000; // should be good enough (increase as needed by about 2000 increments)
inline constexpr std::size_t max_str_storage_size = 30000; // increase as needed by about 2000 increments (shared between all pieces of hardware -> keeps track of duplicate strings and prevents heap allocations)
inline constexpr std::size_t component_uri_split_index = 12; // for spliting uris that begin with /components/... (12 is index of last /)
inline constexpr auto sleep_time = 20ms; // how often worker threads sleep for while checking channels (after the initial check)
inline constexpr auto raw_suffix = "_raw"sv; // what string we expect at the end of a uri if it is a raw request
inline constexpr auto config_suffix = "_config"sv; // what string we expect at the end of a uri if it is a config request
inline constexpr auto modbus_default_timeout = 50ms; // the default timeout we want for modbus polls -> if we don't specify this it is 500ms for newly created contexts
// these are specific modbus codes to look out for (errno):
inline constexpr auto modbus_errno_disconnect = 104; // "Connection reset by peer" -> disconnect
inline constexpr auto modbus_errno_cant_connect = 115; // "Operation now in progress" -> no connection
// POSSIBLE TODO(WALKER): Look into the particular error codes for RTU to make sure threads exit properly on RTU disconnects

struct mod_conn_raii
{
    modbus_t* conn;

    constexpr mod_conn_raii(modbus_t* mc = nullptr) noexcept
        : conn(mc)
    {}

    ~mod_conn_raii() noexcept
    {
        if (conn)
        {
            modbus_close(conn);
            modbus_free(conn);
        }
    }

    constexpr operator modbus_t*() noexcept
    {
        return conn;
    }
};

// NOTE(WALKER): This might not be used due to how
// we need to unlock very quickly
// manual locking and unlocking might be necessary
// especially for reader threads
// We will have to see
struct conn_lock_guard
{
    std::mutex* lock;
    mod_conn_raii* conn;

    conn_lock_guard(std::array<std::mutex, max_num_conns>& locks, std::array<mod_conn_raii, max_num_conns>& conn_array, const uint8_t num_locks, const uint8_t thread_id) noexcept
    {
        uint8_t index = thread_id % num_locks; // start at somewhere appropriate for your index
        locks[index].lock();
        lock = &locks[index];
        conn = &conn_array[index];
    }

    ~conn_lock_guard() noexcept
    {
        lock->unlock();
    }
};

// NOTE(WALKER): This is necessary because sets in the json object I receive might not be in the order they
// are in on the other side
// example: set body = {"day": x, "year": y, "second": z}
// real order is: year, day, second in register map
// but if we didn't have "decode_id" next to set then
// we couldn't use the decode_id and assume order
struct set_info
{
    uint8_t decode_id; // which decode id in the threads array this set belongs to
    uint8_t bit_id; // which bit in the "individual_bits" we are setting or how much to shift by when setting "individual enums"
    uint8_t enum_id;  // which index in the "enum_bit_strings_array" this set belongs to (so we can go grab the care_mask for setting this "individual_enum")
    Jval_buif set_val; // the value that is being "set" (uint, int, float/double, bool)
};
// size info:
// auto x = sizeof(set_info);

// temp formatter for set pair (for debugging purposes):
template<>
struct fmt::formatter<set_info>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const set_info& sp, FormatContext& ctx) const
    {
        if (sp.set_val.holds_bool())
        {
            return fmt::format_to(ctx.out(), FMT_COMPILE(R"({{ "decode id": {}, "bit_id": {}, "value": {} }})"), sp.decode_id, sp.bit_id, sp.set_val.get_bool_unsafe());
        }
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"({{ "decode id": {}, "bit_id": {}, "value": {} }})"), sp.decode_id, sp.bit_id, sp.set_val);
    }
};

// struct for special flags sent over channels (like if it is a raw request or not):
struct chan_msg_flags
{
    uint8_t flags = 0;

    static constexpr uint8_t is_raw_request_index = 0;

    // setters (only called once during config phase):
    constexpr void set_raw_request(const bool is_raw_request) noexcept
    {
        flags |= (1 & is_raw_request) << is_raw_request_index;
    }

    // getters:
    constexpr bool is_raw_request() const noexcept
    {
        return (flags >> is_raw_request_index) & 1;
    }
};

// how this works:
// example: /components/x/y
// receiving only x = last bit of decode_id will be set to true for gets only
//                    and for sets particular bits are set along with vector being filled with the values
// receiving x and y = certain decode_id is set (NOT decode_all_index) and vector could contain one value (if it is a set)
// method_type determines what the thread does and its logic (gets always check for last bit, set will loop through mask)
struct chan_msg
{
    Method_Types method_type; // set or get
    uint8_t decode_id; // gets use this and replyto only
    uint8_t bit_id; // gets use this and replyto only
    uint8_t enum_id; // gets use this and replyto only
    chan_msg_flags flags; // when responding to gets if it is raw then just give them back the uintx_t's in the threads raw_input buffers
    std::vector<set_info> set_info_array; // for when setting (a) particular value(s)/bit(s)
    std::string replyto; // where to "set" to when receiving gets (possibly used during testing mode when receiving raw sets)

    chan_msg() noexcept = default;
    // moves:
    // for when popping off the channels (thread consumes incoming message
    chan_msg(chan_msg&& other) noexcept
        : method_type(other.method_type), decode_id(other.decode_id), bit_id(other.bit_id), flags(other.flags), 
            set_info_array(std::move(other.set_info_array)), replyto(std::move(other.replyto))
    {}
    chan_msg& operator=(chan_msg&& other) noexcept
    {
        method_type = other.method_type;
        decode_id = other.decode_id;
        bit_id = other.bit_id;
        flags = other.flags;
        set_info_array = std::move(other.set_info_array);
        replyto = std::move(other.replyto);
        return *this;
    }
};
// size info:
// auto x = sizeof(chan_msg);

// temp formatter for channel messages (for debugging purposes):
template<>
struct fmt::formatter<chan_msg>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const chan_msg& msg, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), FMT_COMPILE(R"({{ "method type": {}, "replyto": {}, "set_info_array": [{}] }})"), msg.method_type, msg.replyto, fmt::join(msg.set_info_array, ", "));
    }
};

template <std::size_t Channel_Size = max_chan_msgs>
struct msg_channel
{
    rigtorp::SPSCQueue<chan_msg> chan{Channel_Size};

    constexpr auto* operator->() noexcept
    {
        return &chan;
    }
};
// size info:
// auto x = sizeof(msg_channel<>);

struct thread_uri_mapping_flags
{
    uint8_t flags = 0;

    static constexpr uint8_t is_individual_bit_index = 0;
    static constexpr uint8_t is_individual_enum_index = 1;

    // setters (called once during the config stage):
    constexpr void set_individual_bit(const bool is_individual_bit) noexcept
    {
        flags |= (1 & is_individual_bit) << is_individual_bit_index;
    }
    constexpr void set_individual_enum(const bool is_individual_enum) noexcept
    {
        flags |= (1 & is_individual_enum) << is_individual_enum_index;
    }

    // getters:
    constexpr bool is_individual_bit() const noexcept
    {
        return (flags >> is_individual_bit_index) & 1;
    }
    constexpr bool is_individual_enum() const noexcept
    {
        return (flags >> is_individual_enum_index) & 1;
    }
};

// this is used like this:
// /components/x/y (y is optional)
// x = multiple threads
// y = particular uri for single thread
// id_mask resolves to one or more threads (in case we just receive an x and no y)
// decode_id = index of that thread's decode_array that this uri belongs to 
//      in the case we have a y, if we only have x it == decode_all_index
struct thread_uri_mapping_info
{
    std::bitset<max_num_threads> thread_id_mask{}; // which thread(s) this uri belongs to in the list (if just x then this acts as a bitmask)
    Register_Types reg_type; // which register type this uri is (so listener can deny sets on input and discrete_input registers)
    thread_uri_mapping_flags flags; // to store whether or not this uri belongs to an "individual_bit" or NOT (experimental: "individual_enums")
    uint8_t decode_id; // decode_all_index means all (in the case where we have more than one thread to resolve to -> only x, but no y)
    uint8_t bit_id; // which bit this uri belongs to in the case where we have "individual_bits"
    uint8_t enum_id; // which "enum" this uri belongs to in the case where we have "individual_enums" (so we can grab the "care_mask" during sets and gets)
};
// size info:
// auto x = sizeof(thread_uri_mapping_info);
// size info for pairs inside of map:
// auto y = sizeof(std::pair<std::size_t, thread_uri_mapping_info>);

struct conn_info
{
    bool is_fully_connected = false; // whether or not the hardware workspace this info belongs to has connected the first time or not
    Conn_Types conn_type = Conn_Types::TCP; // TCP connection type by default
    fmt::basic_memory_buffer<char, 50> ip_serial_buf; // stores ip for TCP connections and serial name for RTU connections
    // TCP info:
    uint16_t port;
    // RTU info:
    int baud_rate = 115200; // TODO(WALKER): Get proper error checking here for baud rates in the load_config function
    char parity = 'N'; // "none", "even", or "odd"
    uint8_t data_bits = 8; // between 5 and 8 inclusive
    uint8_t stop_bits = 1; // 1 or 2
    // name of this piece of hardware:
    fmt::basic_memory_buffer<char, 50> hardware_name_buf; // stores the "name" for this hardware connection
};
// size info
// auto x = sizeof(conn_info);

// NOTE(WALKER): This is a workspace that threads look to for thread synchornization stuff
// this means that all threads have a pointer to their respective component_workspaces
// This is mainly used for defragmenting pubs and get replies
// but is also used for heartbeat logic
struct component_workspace
{
    std::atomic_bool component_connected{}; // = false by default (threads exit on component disconnect -> main can start them back up)
    uint8_t num_threads; // how many threads belong to this component (this includes the heartbeat thread for publishing purposes)
    uint8_t start_thread_index; // the start worker thread index in the thread_workspaces (so main can start them up again)

    std::mutex pub_lock;
    bool pub_heartbeat_contributed = false; // threads need to check this ONLY if this component has a heartbeat
    uint8_t pub_thread_counter = 0;
    uint64_t pub_threads_status = 0UL;
    fmt::memory_buffer pub_buf;

    std::mutex get_lock;
    uint8_t get_thread_counter = 0;
    uint64_t get_threads_status = 0UL;
    fmt::memory_buffer get_buf;

    // heartbeat related stuff (for heartbeat thread if we have one):
    bool has_heartbeat = false; // by default components have no heartbeat
    std::atomic_bool heartbeat_thread_needs_to_stop{}; // so main can tell heartbeat thread to stop
    Register_Types heartbeat_read_reg_type = Register_Types::Input; // the register type for heartbeat reads -> must be Holding for writes if we have one
    uint16_t heartbeat_slave_address; // slave address heartbeat thread needs to poll on and write on
    uint16_t heartbeat_read_offset; // mandatory
    int32_t heartbeat_write_offset = -1; // optional (> -1 means we have a write offset)
    uint16_t heartbeat_current_val = 0; // poll server and put it into this (also when we set with write -> we do this with)
    std::chrono::milliseconds heartbeat_poll_frequency; // how often the heartbeat thread polls the server
    std::chrono::milliseconds heartbeat_timeout_frequency; // how long we can go without the value changing before considering the component disconnected -> "component_heartbeat_timeout_ms"
    int64_t heartbeat_last_check_time; // the last time we checked for timeout
    fmt::basic_memory_buffer<char, 75> component_uri_buf; // stores main component uri (so heartbeat can get to it really)
};
// size info:
// auto x = sizeof(component_workspace);

struct thread_workspace
{
    Register_Types reg_type{};
    uint8_t slave_address{};
    uint16_t start_offset{};
    uint8_t num_registers{};
    uint8_t num_decode{};
    uint8_t heartbeat_read_index = decode_all_index; // this set to 0->124 means we have a heartbeat we need to read
    uint8_t heartbeat_write_index = decode_all_index; // this set to 0->124 means we have a heartbeat we need to write to using prev_val + 1
    std::bitset<MODBUS_MAX_READ_REGISTERS> changed_mask{};
    std::array<uint16_t, MODBUS_MAX_READ_REGISTERS> raw_input;
    std::array<decode_info, MODBUS_MAX_READ_REGISTERS> decode_info_array;
    std::array<Jval_buif, MODBUS_MAX_READ_REGISTERS> decoded_cache;
    std::array<enum_bit_string_info_vec, MODBUS_MAX_READ_REGISTERS> enum_bit_strings_array;
    std::array<Str_Storage_Handle, MODBUS_MAX_READ_REGISTERS> ids;
    std::bitset<MODBUS_MAX_READ_REGISTERS> debounce_to_set_mask{}; // stores whether or not we have set a value after its debounce period or not
    std::array<Jval_buif, MODBUS_MAX_READ_REGISTERS> debounce_to_set_vals; // These are the last values we tried to set within the debounce timeframe (they are set when the debounce period is up)
    std::array<int64_t, MODBUS_MAX_READ_REGISTERS> debounce_last_set_times; // used in pair with debounce (to not spam server with sets)
    fmt::basic_memory_buffer<char, 75> main_component_uri_buf; // stores /components/x uri for this thread (the uri to publish on)
    // fmt::basic_memory_buffer<char, 500> send_buf; // to send their message out on fims (also stores most recent pubbed information)
    component_workspace* comp_workspace; // this is where send_buf is going to be located at for the immediate future until fragmented pubs/gets are acceptable. Also heartbeat info
    std::chrono::milliseconds frequency; // how often we "poll" the modbus server
    std::chrono::milliseconds debounce{}; // how much time we have to wait before sending a set to the server (keeps track of the previous set values)
    msg_channel<max_chan_msgs> channel; // listener_thread pushes on to this (threads check it every now and then or at some set interval)
    std::future<bool> thread_status; // this tells whether or not the thread is currently working or not
};
// size info:
// auto x = sizeof(thread_workspace) % 64;


// thread workspace exist per piece of hardware:
// max of 64 mappings per piece of hardware (64 threads)
struct hardware_workspace
{
    // connection info for this piece of hardware (for reconnection purposes):
    conn_info connection_info; // if this hardware workspace was able to establish a connection or not to the modbus server
    // worker threads stuff:
    uint8_t num_actual_conns{};
    std::array<std::mutex, max_num_conns> conn_locks{};
    std::array<mod_conn_raii, max_num_conns> conn_array{};

    uint8_t num_actual_components; // so main can reach into the component_workspaces appropriately
    std::array<thread_workspace, max_num_threads> thread_workspaces;
    std::array<component_workspace, max_num_components> component_workspaces;

    // listener stuff:
    fims fims_gateway;
    std::array<char, max_expected_msg_size + simdj_padding> msg_recv_buf;
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;
    phmap::flat_hash_map<std::size_t, thread_uri_mapping_info> thread_uri_index_map;
    uint8_t num_actual_threads{};
    std::array<chan_msg, max_num_threads> msg_staging_area;
};
// size info:
// auto x = sizeof(hardware_workspace);

// NOTE(WALKER): This is used because std::vector will not allow rigtorps spscqueue to work
// due to its copy assignment operator and constructor being deleted.
// so I use this to single time allocate the array that we need depending on how much hardware
// we're currently using
// still memory leak free
struct hardware_vector
{
    // NOTE(WALKER): I might change this to two for the number of hardware workspaces by default
    std::array<hardware_workspace, num_default_hardware> default_workspaces; // "Short Hardware Optimization" for the case where we have a couple pieces of hardware to connect to (1 for now)
    hardware_workspace* vector; // to store hardware workspaces in the case where we have more than the number of hardware supported by default

    ~hardware_vector() noexcept
    {
        if (vector)
        {
            delete[] vector;
        }
    }

    // run this one time and one time only:
    void initialize_vector(const std::size_t size) noexcept
    {
        // if we need more hardware then we have in our default array then we heap allocate:
        if (size > num_default_hardware)
        {
            vector = new hardware_workspace[size];
        }
        return;
    }

    constexpr hardware_workspace& operator[](const std::size_t index) noexcept
    {
        if (!vector)
        {
            return default_workspaces[index];
        }
        return vector[index];
    }
};
// size info:
// auto x = sizeof(hardware_vector);

struct modbus_global_workspace
{
    TSCNS proper_steady_clock{}; // this is used by all threads to get elapsed time during their execution without the overhead of get_time() on linux (or std::chrono::steady_clock::now())
    Str_Storage<max_str_storage_size> str_storage; // shared between all pieces of hardware
    uint8_t num_actual_hardware = 1;
    hardware_vector hardware_workspaces; // contains 1 stack allocated hardware workspace to optimize for single ip connections -> heap allocates array of hardware workspaces if ip_range is found

    modbus_global_workspace() noexcept
    {
        proper_steady_clock.init();
    }
};
// size info:
// auto x = sizeof(modbus_global_workspace);

modbus_global_workspace main_workspace;

// helper structs for the load_config stage:

// connection info and ip_range stuff (at x's level):
struct config_conn_info
{
    std::string hardware_name;
    std::vector<std::string> hardware_name_expansion_array;
    std::string ip_serial_string;
    std::vector<std::string> ip_expansion_array;
    Conn_Types conn_type{};
    uint16_t port = 502;
    // ip_range:
    bool has_ip_range = false;
    config_range ip_range;
    // rtu stuff:
    int baud_rate = 115200; // TODO(WALKER): Get proper error checking here for baud rates in the load_config function
    char parity = 'N'; // "none", "even", or "odd"
    uint8_t data_bits = 8; // between 5 and 8 inclusive
    uint8_t stop_bits = 1; // 1 or 2
    // inheritance flags/data (all components will inherit these if they specified them up here in the "connection" object):
    // these can be overriden at the components object level:
    std::optional<bool> is_word_swapped;
    std::optional<bool> is_off_by_negative_one;
    std::optional<std::chrono::milliseconds> frequency;
    std::optional<uint8_t> device_id;
    std::optional<std::chrono::milliseconds> debounce;
};

// register object info and y range info (decode_offset_range):
struct config_register_info
{
    Register_Types reg_type{};
    uint16_t start_offset = std::numeric_limits<uint16_t>::max();
    uint16_t end_address{};
    std::vector<config_decode_info> decode_vals;
    // decode_offset_range:
    bool has_decode_offset_range = false;
    config_range decode_offset_range;
};

// component info and x range info (device_id_range and component_offset_range):
struct config_component_info
{
    std::string component_id;
    uint8_t device_id{};
    std::chrono::milliseconds frequency;
    std::chrono::milliseconds debounce{};

    // heartbeat stuff:
    bool heartbeat_enabled = false;
    // std::chrono::milliseconds modbus_heartbeat_timeout; // mandatory -> checking frequency -> NO LONGER NECESSARY
    std::chrono::milliseconds component_heartbeat_timeout; // mandatory -> actual timeout
    // both of these below need to be checked post expansion per piece of hardware:
    std::string component_heartbeat_read_uri; // mandatory (this is for change only)
    std::string component_heartbeat_write_uri; // optional (this is for internal writing -> we do the register change ourselves)

    bool is_off_by_negative_one = false; // "off_by_one"
    bool is_word_swapped = false; // "byte_swap"
    std::vector<config_register_info> register_info_array;
    // device_id_range:
    bool has_device_id_range = false;
    config_range device_id_range;
    // component_offset_range:
    bool has_component_offset_range = false;
    config_range component_offset_range;
};

// NOTE(WALKER): I am probably going to make a "config_workspace" struct in the future
// to better manage the load_config function below
// so we can introduce more generalized ways to do ranges, etc.
// Also, in the future there might be a "hardware" array that we pull all of our configs into
// so there is only ever one modbus client running which controls all of the pieces of hardware
// we would take the configs of each one and put those objects in the "hardware" array
// so, in short, we would have one giant mega combined config and one modbus client that runs all of that
// boom, cool, huh? A weird idea, but something that could happen quite easily with that way the client has been rewritten

// reads config from either dbi or file (depending on command line arguments -> dbi by default in future)
bool load_config(const char* config_file, Config_Types config_type) noexcept
{
    // fims config_gateway; // for getting the config from dbi
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;

    // these store everything that we need
    config_conn_info conn_info;
    std::vector<config_component_info> components_array;
    components_array.reserve(10);

    std::vector<config_component_info> post_expansion_array; // what we expand into after the initial config phase right before we start putting into thread workspaces
    post_expansion_array.reserve(10);

    std::vector<std::vector<config_component_info>> final_expansion;
    std::vector<bool> connection_status_array; // to store if we can connect to pieces of hardware (for error reporting, etc.)

    // extra info needed during config stage (string storage and fims subs):
    fmt::basic_memory_buffer<char, 75> decode_uri_buf; // to store x/y stuff after expansion as we put them into the thread_uri_index_map for each hardware workspace
    phmap::flat_hash_map<std::string_view, Str_Storage_Handle> id_map; // determines if we have already stored this key in str_storage or not
    std::vector<std::vector<std::string>> sub_arrays; // to store /components/x stuff per piece of hardware

    // load file -> TODO(WALKER): Replace this with a get from dbi
    // using fims to receive that directly
    // will have to subscribe to /dbi/x_modbus_client first temporarily
    // I will probably just make a temporary fims connection to get that information
    // and then immediately close it
    const auto json = simdjson::padded_string::load(config_file);
    auto err = json.error();
    if (err)
    {
        NEW_FPS_ERROR_PRINT("Can't open json file {}, err = {}. Please check your config path\n", config_file, simdjson::error_message(err));
        return false;
    }

    if (err = parser.iterate(json).get(doc); err)
    {
        NEW_FPS_ERROR_PRINT("can't parse config json, err = {}\n", simdjson::error_message(err));
        return false;
    }

    auto connection_info = doc["connection"].get_object();
    if (err = connection_info.error(); err)
    {
        NEW_FPS_ERROR_PRINT("can't get connection as an object in this config file, err = {}\n", simdjson::error_message(err));
        return false;
    }

    const auto name = connection_info["name"].get_string();
    if (err = name.error(); err)
    {
        NEW_FPS_ERROR_PRINT("can't get name as a string, err = {}\n", simdjson::error_message(err));
        return false;
    }
    const auto name_view = name.value_unsafe();
    if (name_view.empty())
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("name is empty, what are you smoking?\n");
        return false;
    }
    if (name_view.find_first_of(R"({}\")") != std::string_view::npos) // NOTE(WALKER): Special case for name, because some people include spaces in the name (doesn't effect fims so it's ok)
    {
        NEW_FPS_ERROR_PRINT("name: {} cannot contains the characters '{{', '}}', '\\', or '\"' as they are reserved. Please remove them\n", name_view);
        return false;
    }
    conn_info.hardware_name = name.value_unsafe();

    std::string_view ip_view;
    const auto ip = connection_info["ip_address"].get_string();
    if (err = ip.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get ip_address as a string, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!ip.error())
    {
        ip_view = ip.value_unsafe();
        if (ip_view.empty())
        {
            NEW_FPS_ERROR_PRINT_NO_ARGS("ip is empty, what are you smoking?\n");
            return false;
        }
        if (ip_view.find_first_of(forbidden_chars) != std::string_view::npos)
        {
            NEW_FPS_ERROR_PRINT("ip: {} cannot contains the characters {} as they are reserved. Please remove them\n", ip_view, forbidden_chars_err_str);
            return false;
        }
    }

    std::string_view serial_device_view;
    auto serial_device = connection_info["serial_device"].get_string();
    if (err = serial_device.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get serial_device as a string, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!serial_device.error())
    {
        serial_device_view = serial_device.value_unsafe();
        if (serial_device_view.empty())
        {
            NEW_FPS_ERROR_PRINT_NO_ARGS("serial_device is empty, what are you smoking?\n");
            return false;
        }
        if (serial_device_view.find_first_of(forbidden_chars) != std::string_view::npos)
        {
            NEW_FPS_ERROR_PRINT("serial_device: {} cannot contain the characters {} as they are reserved. Please remove them\n", serial_device_view, forbidden_chars_err_str);
            return false;
        }
    }

    if (ip.error() && serial_device.error()) // They didn't provide either:
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("neither ip_address nor serial_device were provided for a connection. Please provide at least one\n");
        return false;
    }

    if (!ip.error() && !serial_device.error()) // they cannot have both TCP and RTU connections at the same time:
    {
        NEW_FPS_ERROR_PRINT("both an ip_address: {} and a serial_device: {} were provided, you must choose between TCP and RTU. Please remove one\n", ip_view, serial_device_view);
        return false;
    }

    if (!ip.error())
    {
        conn_info.conn_type = Conn_Types::TCP;
        conn_info.ip_serial_string = ip_view;

        const auto port = connection_info["port"].get_int64();
        if (err = port.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get port as an integer, err = {}\n", simdjson::error_message(err));
            return false;
        }
        if (!port.error())
        {
            const auto port_val = port.value_unsafe();
            if (port_val < 0 || port_val > std::numeric_limits<uint16_t>::max())
            {
                NEW_FPS_ERROR_PRINT("port is: {}, but it must be between 0 and 65535\n", port_val);
                return false;
            }
            conn_info.port = static_cast<uint16_t>(port_val);
        }

        auto ip_range = connection_info["ip_range"].get_object();
        if (err = ip_range.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get ip_range as an object, err = {}\n", simdjson::error_message(err));
            return false;
        }

        if (!ip_range.error())
        {
            auto begin = ip_range["begin"].get_int64();
            if (err = begin.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. Can't get begin as an integer, err = {}\n", simdjson::error_message(err));
                return false;
            }
            auto end = ip_range["end"].get_int64();
            if (err = end.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. Can't get end as an integer, err = {}\n", simdjson::error_message(err));
                return false;
            }
            auto replace_str = ip_range["replace"].get_string();
            if (err = replace_str.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. Can't get replace as a string, err = {}\n", simdjson::error_message(err));
                return false;
            }

            const auto begin_val = begin.value_unsafe();
            const auto end_val = end.value_unsafe();
            const auto replace_str_view = replace_str.value_unsafe();

            if (begin_val < 1 || begin_val > 255)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. begin: {} is < 1 or > 255. Please provide a valid ip_range\n", begin_val);
                return false;
            }
            if (end_val < 1 || end_val > 255)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. end: {} is < 1 or > 255. Please provide a valid ip_range\n", end_val);
                return false;
            }
            if (end_val <= begin_val)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. end ({}) <= begin ({}). Please provide a valid ip_range\n", end_val, begin_val);
                return false;
            }
            if (end_val - begin_val + 1 > max_num_hardware)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. The total number of hardware for this range: {} exceeds the maximum number of supported hardware: {}. Please provide a valid ip_range\n", (end_val - begin_val + 1), max_num_hardware);
                return false;
            }

            if (replace_str_view.empty())
            {
                NEW_FPS_ERROR_PRINT_NO_ARGS("In ip_range. replace is empty, please provide at least one character\n");
                return false;
            }
            if (replace_str_view.find_first_of(forbidden_chars) != std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. replace: {} cannot contain the characters {} as they are reserved. Please remove them\n", replace_str_view, forbidden_chars_err_str);
                return false;
            }

            if (name_view.find(replace_str_view) == std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. name: {} does not contain the replace string: {}. Please check for typos\n", name_view, replace_str_view);
                return false;
            }
            if (ip_view.find(replace_str_view) == std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("In ip_range. ip: {} does not contain the replace string: {}. Please check for typos\n", ip_view, replace_str_view);
                return false;
            }

            // put values into struct:
            conn_info.has_ip_range = true;
            conn_info.ip_range.begin = begin_val;
            conn_info.ip_range.end = end_val;
            conn_info.ip_range.replace_str = replace_str_view;
        }
    }
    else // RTU:
    {
        conn_info.conn_type = Conn_Types::RTU;
        conn_info.ip_serial_string = serial_device_view;

        auto baud_rate = connection_info["baud_rate"].get_int64();
        if (err = baud_rate.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get baud_rate as an integer, err = {}\n", simdjson::error_message(err));
            return false;
        }
        if (!baud_rate.error())
        {
            const auto baud_rate_val = baud_rate.value_unsafe();
            if (baud_rate_val < 1 || baud_rate_val > std::numeric_limits<int>::max()) // POSSIBLE TODO(WALKER): Maybe we do error checking for these numbers?: https://lucidar.me/en/serialib/most-used-baud-rates-table/
            {
                NEW_FPS_ERROR_PRINT("baud_rate: {} cannot be < 1 or > {}. Please provide a valid baud_rate\n", baud_rate_val, std::numeric_limits<int>::max());
                return false;
            }
            conn_info.baud_rate = static_cast<int>(baud_rate_val);
        }

        auto parity = connection_info["parity"].get_string();
        if (err = parity.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get parity as a string, err = {}\n", simdjson::error_message(err));
            return false;
        }
        if (!parity.error())
        {
            const auto parity_view = parity.value_unsafe();
            if (parity_view == "none")
            {
                conn_info.parity = 'N';
            }
            else if (parity_view == "even")
            {
                conn_info.parity = 'E';
            }
            else if (parity_view == "odd")
            {
                conn_info.parity = 'O';
            }
            else
            {
                NEW_FPS_ERROR_PRINT("parity: {} is not accepted for serial RTU communications. Only none, even, and odd are accepted\n", parity_view);
                return false;
            }
        }

        auto data_bits = connection_info["data_bits"].get_int64();
        if (err = data_bits.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get data_bits as an integer, err = {}\n", simdjson::error_message(err));
            return false;
        }
        if (!data_bits.error())
        {
            const auto data_bits_val = data_bits.value_unsafe();
            if (data_bits_val < 5 || data_bits_val > 8)
            {
                NEW_FPS_ERROR_PRINT("data_bits: {} cannot be < 5 or > 8. Please provide a valid data_bits number\n", data_bits_val);
                return false;
            }
            conn_info.data_bits = static_cast<uint8_t>(data_bits_val);
        }

        auto stop_bits = connection_info["stop_bits"].get_int64();
        if (err = stop_bits.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get stop_bits as an integer, err = {}\n", simdjson::error_message(err));
            return false;
        }
        if (!stop_bits.error())
        {
            const auto stop_bits_val = stop_bits.value_unsafe();
            if (!(stop_bits_val == 1 || stop_bits_val == 2))
            {
                NEW_FPS_ERROR_PRINT("stop_bits: {} can only be 1 or 2. Please provide a valid stop_bits number\n", stop_bits_val);
                return false;
            }
            conn_info.stop_bits = static_cast<uint8_t>(stop_bits_val);
        }
    }

    // "inheritance" stuff here:

    auto connection_off_by_one = connection_info["off_by_one"].get_bool();
    if (err = connection_off_by_one.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get connection's objects off_by_one as a bool, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!connection_off_by_one.error()) // they are providing inheritance:
    {
        conn_info.is_off_by_negative_one = connection_off_by_one.value_unsafe();
    }

    auto connection_word_swapped = connection_info["byte_swap"].get_bool(); // "byte_swap" is actually "word_swap" but is kept here for legacy reasons
    if (err = connection_word_swapped.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get connection's object's byte_swap as a bool, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!connection_word_swapped.error()) // they are providing inheritance:
    {
        conn_info.is_word_swapped = connection_word_swapped.value_unsafe();
    }

    auto connection_frequency = connection_info["frequency"].get_int64();
    if (err = connection_frequency.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get connection's objects frquency as an integer, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!connection_frequency.error()) // they are providing inheritance:
    {
        const auto conn_frequency_val = connection_frequency.value_unsafe();
        if (conn_frequency_val < 1)
        {
            NEW_FPS_ERROR_PRINT("connection object's frequency: {} is < 1. This is not possible, what are you smoking?\n", conn_frequency_val);
            return false;
        }
        conn_info.frequency = std::chrono::milliseconds{conn_frequency_val};
    }

    auto connection_debounce = connection_info["debounce"].get_int64();
    if (err = connection_debounce.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get connection's objects debounce as an integer, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!connection_debounce.error()) // they are providing inheritance:
    {
        const auto conn_debounce_val = connection_debounce.value_unsafe();
        if (conn_debounce_val < 1)
        {
            NEW_FPS_ERROR_PRINT("connection object's debounce: {} is < 1. This is not possible, what are you smoking?\n", conn_debounce_val);
            return false;
        }
        conn_info.debounce = std::chrono::milliseconds{conn_debounce_val};
    }

    auto connection_device_id = connection_info["device_id"].get_int64();
    if (err = connection_device_id.error(); simdj_optional_key_error(err))
    {
        NEW_FPS_ERROR_PRINT("can't get connection's objects device_id as an int, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (!connection_device_id.error()) // they are providing inheritance:
    {
        const auto connection_device_id_val = connection_device_id.value_unsafe();
        if (connection_device_id_val < 0 || connection_device_id_val > std::numeric_limits<uint8_t>::max())
        {
            NEW_FPS_ERROR_PRINT("connection object's device_id: {} is < 0 or > 255. Please change it\n", connection_device_id_val);
            return false;
        }
        conn_info.device_id = static_cast<uint8_t>(connection_device_id_val);
    }

    // print out connection info (debug info):
    NEW_FPS_ERROR_PRINT("connection info (name: {}) = \nprotocol: {}\n", name_view, conn_info.conn_type == Conn_Types::TCP ? "TCP" : "RTU");
    if (conn_info.conn_type == Conn_Types::TCP)
    {
        NEW_FPS_ERROR_PRINT("port: {}\nip: {}\nhas_ip_range: {}\n", conn_info.port, conn_info.ip_serial_string, conn_info.has_ip_range);
    }
    else // RTU:
    {
        NEW_FPS_ERROR_PRINT("serial_device: {}\nbaud_rate: {}\nparity: {}\ndata_bits: {}\nstop_bits: {}\n", conn_info.ip_serial_string, conn_info.baud_rate, conn_info.parity, conn_info.data_bits, conn_info.stop_bits);
    }
    if (conn_info.has_ip_range)
    {
        NEW_FPS_ERROR_PRINT("begin: {}\nend: {}\nreplace_str: {}\n", conn_info.ip_range.begin, conn_info.ip_range.end, conn_info.ip_range.replace_str);
    }
    // inheritance stuff:
    if (conn_info.is_word_swapped)
    {
        NEW_FPS_ERROR_PRINT("connection object has is_word_swapped: {}\n", conn_info.is_word_swapped.value());
    }
    if (conn_info.is_off_by_negative_one)
    {
        NEW_FPS_ERROR_PRINT("connection object has is_off_by_negative_one: {}\n", conn_info.is_off_by_negative_one.value());
    }
    if (conn_info.frequency)
    {
        NEW_FPS_ERROR_PRINT("connection object has frequency: {}\n", conn_info.frequency.value());
    }
    if (conn_info.debounce)
    {
        NEW_FPS_ERROR_PRINT("connection object has debouce: {}\n", conn_info.debounce.value());
    }
    if (conn_info.device_id)
    {
        NEW_FPS_ERROR_PRINT("connection object has device_id: {}\n", conn_info.device_id.value());
    }

    // return false;

    // start to loop over components array:
    auto components = doc["components"].get_array();
    if (err = components.error(); err)
    {
        NEW_FPS_ERROR_PRINT("could not get components as an array, err = {}\n", simdjson::error_message(err));
        return false;
    }
    const auto is_components_empty = components.is_empty();
    if (err = is_components_empty.error(); err)
    {
        NEW_FPS_ERROR_PRINT("could not determine whether or not components array is empty or not, err = {}\n", simdjson::error_message(err));
        return false;
    }
    if (is_components_empty.value_unsafe())
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("components array is empty, please provide at least one component\n");
        return false;
    }

    for (auto component : components)
    {
        auto& current_component = components_array.emplace_back();

        // inheritance stuff from connection object (basic flags and frequency/device_id):
        if (conn_info.is_word_swapped)
        {
            current_component.is_word_swapped = conn_info.is_word_swapped.value();
        }
        if (conn_info.is_off_by_negative_one)
        {
            current_component.is_off_by_negative_one = conn_info.is_off_by_negative_one.value();
        }
        if (conn_info.frequency)
        {
            current_component.frequency = conn_info.frequency.value();
        }
        if (conn_info.debounce)
        {
            current_component.debounce = conn_info.debounce.value();
        }
        if (conn_info.device_id)
        {
            current_component.device_id = conn_info.device_id.value();
        }

        if (err = component.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get a component during components iteration at component #{}, err = {}\n", components_array.size(), simdjson::error_message(err));
            return false;
        }

        auto component_obj = component.get_object();
        if (err = component_obj.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get a component as an object in components array at component object #{}, err = {}\n", components_array.size(), simdjson::error_message(err));
            return false;
        }

        auto component_id = component_obj["id"].get_string();
        if (err = component_id.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get a component id as a string in component object #{}, err = {}\n", components_array.size(), simdjson::error_message(err));
            return false;
        }
        const auto component_id_view = component_id.value_unsafe();

        // component_id_view error checking (empty and must contain replace characters):
        if (component_id_view.empty())
        {
            NEW_FPS_ERROR_PRINT("id is empty for component object #{}, please provide an id\n", components_array.size());
            return false;
        }
        if (component_id_view.find_first_of(forbidden_chars) != std::string_view::npos)
        {
            NEW_FPS_ERROR_PRINT("id for uri /components/{} cannot contain the characters {} as they are reserved. Please remove them\n", component_id_view, forbidden_chars_err_str);
            return false;
        }
        if (conn_info.has_ip_range && component_id_view.find(conn_info.ip_range.replace_str) == std::string_view::npos)
        {
            NEW_FPS_ERROR_PRINT("an ip_range was provided but could not find the replace string: {} for uri /components/{}, please check for typos\n", conn_info.ip_range.replace_str, component_id_view);
            return false;
        }
        current_component.component_id = component_id_view;

        auto frequency = component_obj["frequency"].get_int64();
        if (err = frequency.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get frequency as in int for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (!frequency.error()) // override the inherited one:
        {
            if (frequency.value_unsafe() < 1)
            {
                NEW_FPS_ERROR_PRINT("frequency for uri /components/{} is < 1. This is not possible, what are you smoking?\n", component_id_view);
                return false;
            }
            current_component.frequency = std::chrono::milliseconds{frequency.value_unsafe()};
        }
        if (frequency.error() && !conn_info.frequency) // they did NOT inherit anything and they did NOT provide one at this level:
        {
            NEW_FPS_ERROR_PRINT("frequency was not provided at either the connection object level or at the component level for uri /components/{}. Please provide one at this level, or provide one to be inherited for all components inside the connection object\n", component_id_view);
            return false;
        }

        auto debounce = component_obj["debounce"].get_int64();
        if (err = debounce.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get debounce as an integer for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (!debounce.error()) // override inherited one:
        {
            // POSSIBLE TODO(WALKER): figure out why the original debounce was a minimum of 50ms?
            if (debounce.value_unsafe() < 1)
            {
                NEW_FPS_ERROR_PRINT("debounce for uri /components/{} is < 1. This is not possible, what are you smoking?\n", component_id_view);
                return false;
            }
            current_component.debounce = std::chrono::milliseconds{debounce.value_unsafe()};
        }

        auto off_by_negative_one = component_obj["off_by_one"].get_bool();
        if (err = off_by_negative_one.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get off_by_one as a bool for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (!off_by_negative_one.error()) // override inherited one:
        {
            current_component.is_off_by_negative_one = off_by_negative_one.value_unsafe();
        }
        // they don't have to provide one (this is truly optional)

        auto is_word_swapped = component_obj["byte_swap"].get_bool();
        if (err = is_word_swapped.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get byte_swap as a bool for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (!is_word_swapped.error()) // override inherited one:
        {
            current_component.is_word_swapped = is_word_swapped.value_unsafe();
        }
        // they don't have to provide one (this is truly optional)

        auto device_id = component_obj["device_id"].get_int64();
        if (err = device_id.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get device_id as an int for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (!device_id.error()) // override the inherited one:
        {
            const auto device_id_val = device_id.value_unsafe();
            if (device_id_val < 1 || device_id_val > std::numeric_limits<uint8_t>::max())
            {
                NEW_FPS_ERROR_PRINT("device_id: {} for uri /components/{} is < 1 or > 255. Please change it\n", device_id_val, component_id_view);
                return false;
            }
            current_component.device_id = static_cast<uint8_t>(device_id_val);
        }

        auto device_id_range = component_obj["device_id_range"].get_object();
        if (err = device_id_range.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get device_id_range as an object for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        // check for device_id and device_id_range conflicts:
        if (!device_id.error() && !device_id_range.error()) // can't have both
        {
            NEW_FPS_ERROR_PRINT("can't have both device_id and device_id_range for uri /components/{}, please pick one\n", component_id_view);
            return false;
        }
        else if (device_id.error() && device_id_range.error() && !conn_info.device_id) // they did not inherit it and they did not provide a device_id or device_id_range (none of the three):
        {
            current_component.device_id = static_cast<uint8_t>(-1); // default to -1/255 for device_id
        }

        // device_id_range info extraction:
        if (!device_id_range.error())
        {
            auto begin = device_id_range["begin"].get_int64();
            if (err = begin.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}, can't get begin as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            auto end = device_id_range["end"].get_int64();
            if (err = end.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}, can't get end as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            auto replace_str = device_id_range["replace"].get_string();
            if (err = replace_str.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}, can't get replace as a string, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            const auto begin_val = begin.value_unsafe();
            const auto end_val = end.value_unsafe();
            const auto replace_str_view = replace_str.value_unsafe();

            if (begin_val < 0 || begin_val > 255)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}. begin: {} is < 0 or > 255. Please provide a valid device_id_range\n", component_id_view, begin_val);
                return false;
            }
            if (end_val < 0 || end_val > 255)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}. end: {} is < 0 or > 255. Please provide a valid device_id_range\n", component_id_view, end_val);
                return false;
            }
            if (end_val <= begin_val)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}. end ({}) <= begin ({}). Please provide a valid device_id_range\n", component_id_view, end_val, begin_val);
                return false;
            }

            if (replace_str_view.empty())
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}. replace is empty, please provide a proper replace string\n", component_id_view);
                return false;
            }
            if (replace_str_view.find_first_of(forbidden_chars) != std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("In device_id_range for uri /components/{}. replace: {} cannot contain the characters {} as they are reserved. Please remove them\n", component_id_view, replace_str_view, forbidden_chars_err_str);
                return false;
            }

            if (component_id_view.find(replace_str_view) == std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("a device_id_range was provided but could not find the replace string: {} for uri /components/{}, please check for typos\n", replace_str_view, component_id_view);
                return false;
            }

            current_component.has_device_id_range = true;
            current_component.device_id_range.begin = begin_val;
            current_component.device_id_range.end = end_val;
            current_component.device_id_range.replace_str = replace_str_view;
        }

        auto component_offset_range = component_obj["component_offset_range"].get_object();
        if (err = component_offset_range.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("can't get component_offset_range as an object for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }

        // component_offset_range info extraction:
        if (!component_offset_range.error())
        {
            auto begin = component_offset_range["begin"].get_int64();
            if (err = begin.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}, can't get begin as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            auto end = component_offset_range["end"].get_int64();
            if (err = end.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}, can't get end as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            auto step = component_offset_range["step"].get_int64();
            if (err = step.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}, can't get step as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            auto replace_str = component_offset_range["replace"].get_string();
            if (err = replace_str.error(); err)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}, can't get replace as a string, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }

            const auto begin_val = begin.value_unsafe();
            const auto end_val = end.value_unsafe();
            const auto step_val = step.value_unsafe();
            const auto replace_str_view = replace_str.value_unsafe();

            // for offsets we at least need a positive range, don't really care how big
            if (begin_val < 1)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. begin: {} is < 1. Please provide a valid component_offset_range\n", component_id_view, begin_val);
                return false;
            }
            if (end_val < 1)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. end: {} is < 1. Please provide a valid component_offset_range\n", component_id_view, end_val);
                return false;
            }
            if (end_val <= begin_val)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. end ({}) <= begin ({}). Please provide a valid component_offset_range\n", component_id_view, end_val, begin_val);
                return false;
            }

            if (step_val < 1)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. step: {} is < 1. Please provide a valid component_offset_range\n", component_id_view, step_val);
                return false;
            }

            if (replace_str_view.empty())
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. replace is empty, please provide a proper replace string\n", component_id_view);
                return false;
            }
            if (replace_str_view.find_first_of(forbidden_chars) != std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("In component_offset_range for uri /components/{}. replace: {} cannot contain the characters {} as they are reserved. Please remove them\n", component_id_view, replace_str_view, forbidden_chars_err_str);
                return false;
            }

            if (component_id_view.find(replace_str_view) == std::string_view::npos)
            {
                NEW_FPS_ERROR_PRINT("a component_offset_range was provided but could not find the replace string: {} for uri /components/{}, please check for typos\n", replace_str_view, component_id_view);
                return false;
            }

            current_component.has_component_offset_range = true;
            current_component.component_offset_range.begin = begin_val;
            current_component.component_offset_range.end = end_val;
            current_component.component_offset_range.step = step_val;
            current_component.component_offset_range.replace_str = replace_str_view;
        }

        // NOTE(WALKER): I am enforcing this for now because we haven't seen them combined before
        // if they are combined in the future then I will have to modify the config stage to allow them to work
        // together. That will be a giant pain in the ass, so I'll think about it.
        // I'll probably expand out offset first then device_id as opposed to the other way around
        // we'll have to see how they might interact if that becomes a possibility
        if (!component_offset_range.error() && !device_id_range.error())
        {
            NEW_FPS_ERROR_PRINT("cannot have both device_id_range and component_offset_range for uri /components/{}, please pick one\n", component_id_view);
            return false;
        }

        // heartbeat stuff (optional):
        auto heartbeat_enabled = component_obj["heartbeat_enabled"].get_bool();
        if (err = heartbeat_enabled.error(); simdj_optional_key_error(err))
        {
            NEW_FPS_ERROR_PRINT("for uri /components/{}, can't get optional key heartbeat_enabled as a bool, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        current_component.heartbeat_enabled = heartbeat_enabled.value_unsafe();
        if (current_component.heartbeat_enabled)
        {
            auto heartbeat_timeout = component_obj["component_heartbeat_timeout_ms"].get_int64(); // this is mandatory
            if (err = heartbeat_timeout.error(); err)
            {
                NEW_FPS_ERROR_PRINT("fpr uri /components/{}, can't get component_heartbeat_timeout_ms as an int, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }
            current_component.component_heartbeat_timeout = std::chrono::milliseconds{heartbeat_timeout.value_unsafe()};

            // frequency cannot be less than how often the heartbeat thread would poll the server
            if (current_component.component_heartbeat_timeout > current_component.frequency)
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, frequency ({}) < ({}) component_heartbeat_timeout_ms. This is illegal, please increase the timeout for heartbeat or change this component's frequency\n", component_id_view, current_component.frequency, current_component.component_heartbeat_timeout);
                return false;
            }

            auto heartbeat_read_uri = component_obj["component_heartbeat_read_uri"].get_string(); // this is mandatory
            if (err = heartbeat_read_uri.error(); err)
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, can't get component_heartbeat_read_uri as a string, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }
            current_component.component_heartbeat_read_uri = heartbeat_read_uri.value_unsafe(); // NOTE(WALKER): This is checked post expansion instead of as we go along

            if (current_component.component_heartbeat_read_uri.empty())
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_read_uri is empty, what are you smoking?\n", component_id_view);
                return false;
            }
            if (current_component.component_heartbeat_read_uri.find_first_of(forbidden_chars) != std::string::npos)
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_read_uri: {} contains the forbidden characters: {}, please remove them\n", component_id_view, current_component.component_heartbeat_read_uri, forbidden_chars_err_str);
                return false;
            }

            auto heartbeat_write_uri = component_obj["component_heartbeat_write_uri"].get_string(); // this is optional
            if (err = heartbeat_write_uri.error(); simdj_optional_key_error(err))
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, can't get component_heartbeat_read_uri as a string, err = {}\n", component_id_view, simdjson::error_message(err));
                return false;
            }
            if (!heartbeat_write_uri.error())
            {
                current_component.component_heartbeat_write_uri = heartbeat_write_uri.value_unsafe(); // NOTE(WALKER): This is checked post expansion instead of as we go along

                if (current_component.component_heartbeat_write_uri.empty())
                {
                    NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_write_uri is empty, what are you smoking?\n", component_id_view);
                    return false;
                }
                if (current_component.component_heartbeat_write_uri.find_first_of(forbidden_chars) != std::string::npos)
                {
                    NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_write_uri: {} contains the forbidden characters: {}, please remove them\n", component_id_view, current_component.component_heartbeat_write_uri, forbidden_chars_err_str);
                    return false;
                }
            }
        }

        // do registers array iteration:
        auto registers = component_obj["registers"].get_array();
        if (err = registers.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get registers as an array for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        const auto is_registers_empty = registers.is_empty();
        if (err = is_registers_empty.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't determine if registers array is empty or not for uri /components/{}, err = {}\n", component_id_view, simdjson::error_message(err));
            return false;
        }
        if (is_registers_empty.value_unsafe())
        {
            NEW_FPS_ERROR_PRINT("registers array is empty for uri /components/{}, please provide at least one register object\n", component_id_view);
            return false;
        }

        for (auto reg_mapping : registers.value_unsafe())
        {
            auto& current_reg_mapping = current_component.register_info_array.emplace_back();

            if (err = reg_mapping.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't get register mapping #{} for uri /components/{}, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                return false;
            }

            auto reg_mapping_obj = reg_mapping.get_object();
            if (err = reg_mapping_obj.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't get regster mapping #{} as an object for uri /components/{}, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                return false;
            }

            auto register_type = reg_mapping_obj["type"].get_string();
            if (err = register_type.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't get register_type as a string inside mapping #{} for uri /components/{}, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                return false;
            }
            const auto register_type_view = register_type.value_unsafe();

            // extract our register_type stuff:
            if (register_type_view == "Holding" || register_type_view == "Holding Registers")
            {
                current_reg_mapping.reg_type = Register_Types::Holding;
            }
            else if (register_type_view == "Input" || register_type_view == "Input Registers")
            {
                current_reg_mapping.reg_type = Register_Types::Input;
            }
            else if (register_type_view == "Coil" || register_type_view == "Coils")
            {
                current_reg_mapping.reg_type = Register_Types::Coil;
            }
            else if (register_type_view == "Discrete Input" || register_type_view == "Discrete Inputs")
            {
                current_reg_mapping.reg_type = Register_Types::Discrete_Input;
            }
            else
            {
                NEW_FPS_ERROR_PRINT("register type: {} is unsupported inside mapping #{} for uri /components/{}. Only Holding/Holding Registers, Input/Input Registers, Coil/Coils, and Discrete Input/Discrete Inputs are accepted\n", register_type_view, current_component.register_info_array.size(), component_id_view);
                return false;
            }

            auto decode_offset_range = reg_mapping_obj["decode_offset_range"].get_object();
            if (err = decode_offset_range.error(); simdj_optional_key_error(err))
            {
                NEW_FPS_ERROR_PRINT("can't get decode_offset_range as an object inside mapping #{} for uri /components/{}, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                return false;
            }

            if (!decode_offset_range.error())
            {
                auto begin = decode_offset_range["begin"].get_int64();
                if (err = begin.error(); err)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}, can't get begin as an int, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                    return false;
                }

                auto end = decode_offset_range["end"].get_int64();
                if (err = end.error(); err)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}, can't get end as an int, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                    return false;
                }

                auto step = decode_offset_range["step"].get_int64();
                if (err = step.error(); err)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}, can't get step as an int, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                    return false;
                }

                auto replace_str = decode_offset_range["replace"].get_string();
                if (err = replace_str.error(); err)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}, can't get replace as a string, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                    return false;
                }

                const auto begin_val = begin.value_unsafe();
                const auto end_val = end.value_unsafe();
                const auto step_val = step.value_unsafe();
                const auto replace_str_view = replace_str.value_unsafe();

                // for decode_id we at least need a positive range, don't really care how big
                if (begin_val < 1)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. begin: {} is < 1. Please provide a valid decode_offset_range\n", current_component.register_info_array.size(), component_id_view, begin_val);
                    return false;
                }
                if (end_val < 1)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. end: {} is < 1. Please provide a valid decode_offset_range\n", current_component.register_info_array.size(), component_id_view, end_val);
                    return false;
                }
                if (end_val <= begin_val)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. end ({}) <= begin ({}). Please provide a valid decode_offset_range\n", current_component.register_info_array.size(), component_id_view, end_val, begin_val);
                    return false;
                }

                if (step_val < 1)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. step: {} is < 1. Please provide a valid decode_offset_range\n", current_component.register_info_array.size(), component_id_view, step_val);
                    return false;
                }

                if (replace_str_view.empty())
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. replace is empty, please provide a proper replace string\n", current_component.register_info_array.size(), component_id_view);
                    return false;
                }
                if (replace_str_view.find_first_of(forbidden_chars) != std::string_view::npos)
                {
                    NEW_FPS_ERROR_PRINT("In decode_offset_range inside mapping #{} for uri /components/{}. replace: {} cannot contain the characters {} as they are reserved. Please remove them\n", current_component.register_info_array.size(), component_id_view, replace_str_view, forbidden_chars_err_str);
                    return false;
                }

                current_reg_mapping.has_decode_offset_range = true;
                current_reg_mapping.decode_offset_range.begin = begin_val;
                current_reg_mapping.decode_offset_range.end = end_val;
                current_reg_mapping.decode_offset_range.step = step_val;
                current_reg_mapping.decode_offset_range.replace_str = replace_str_view;
            }

            auto map = reg_mapping_obj["map"].get_array();
            if (err = map.error(); err)
            {
                NEW_FPS_ERROR_PRINT("can't get map as an array inside mapping #{} for uri /components/{}, err = {}\n", current_component.register_info_array.size(), component_id_view, simdjson::error_message(err));
                return false;
            }
            auto map_arr = map.value_unsafe();

            const auto main_uri_str = fmt::format("/components/{}", component_id_view);

            if (!parse_decode_objects_array(map_arr, 
                                            current_reg_mapping.decode_vals, 
                                            current_reg_mapping.reg_type,
                                            current_component.is_off_by_negative_one, 
                                            main_uri_str,
                                            &current_reg_mapping.start_offset,
                                            &current_reg_mapping.end_address,
                                            current_reg_mapping.has_decode_offset_range ? &current_reg_mapping.decode_offset_range : nullptr))
            {
                NEW_FPS_ERROR_PRINT("encountered parsing error at mapping #{}\n", current_component.register_info_array.size());
                return false;
            }

            // do size check and sort check here:
            if (current_reg_mapping.decode_vals.size() > 125)
            {
                NEW_FPS_ERROR_PRINT("the number of decode objects of 125 has been exceeded inside the mapping that contains the uri /components/{}/{}. There shouldn't be that many objects in a single mapping", component_id_view, current_reg_mapping.decode_vals.back().id);
                return false;
            }
            else if (current_reg_mapping.decode_vals.size() == 1) // don't bother checking array of size 1
            {
                continue;
            }

            std::sort(current_reg_mapping.decode_vals.begin(), current_reg_mapping.decode_vals.end());
            const auto* current_sort_decode = &(current_reg_mapping.decode_vals[0]);
            for (std::size_t i = 1; i < current_reg_mapping.decode_vals.size(); ++i)
            {
                if ((current_sort_decode->offset + current_sort_decode->size - 1) >= current_reg_mapping.decode_vals[i].offset)
                {
                    NEW_FPS_ERROR_PRINT("offset conflict between uri's /components/{0}/{1} and /components/{0}/{2}, please make sure offsets are not overlapping\n", component_id_view, current_sort_decode->id, current_reg_mapping.decode_vals[i].id);
                    return false;
                }
                current_sort_decode = &(current_reg_mapping.decode_vals[i]);
            }
        }
    }

    // print out components array info:
    NEW_FPS_ERROR_PRINT_NO_ARGS("\nbefore expansion:");
    for (const auto& component : components_array)
    {
        NEW_FPS_ERROR_PRINT("\nuri /components/{} = \nfrequency: {}\ndebounce: {}\ndevice_id: {}\nis_off_by_negative_one: {}\nis_word_swapped: {}\nhas_device_id_range: {}\nhas_component_offset_range: {}\n", 
            component.component_id, component.frequency, component.debounce, component.device_id, component.is_off_by_negative_one, component.is_word_swapped, component.has_device_id_range, component.has_component_offset_range);
        if (component.has_device_id_range)
        {
            NEW_FPS_ERROR_PRINT("begin: {}\nend: {}\nreplace_str: {}\n", component.device_id_range.begin, component.device_id_range.end, component.device_id_range.replace_str);
        }
        if (component.has_component_offset_range)
        {
            NEW_FPS_ERROR_PRINT("begin: {}\nend: {}\nstep: {}\nreplace_str: {}\n", component.component_offset_range.begin, component.component_offset_range.end, component.component_offset_range.step, component.component_offset_range.replace_str);
        }
        NEW_FPS_ERROR_PRINT("heartbeat_enabled: {}\n", component.heartbeat_enabled);
        if (component.heartbeat_enabled)
        {
            NEW_FPS_ERROR_PRINT("heartbeat_read_uri: {}\nheartbeat_write_uri: {}\nheartbeat_timeout: {}\n", component.component_heartbeat_read_uri, component.component_heartbeat_write_uri, component.component_heartbeat_timeout);
        }

        for (const auto& register_info : component.register_info_array)
        {
            NEW_FPS_ERROR_PRINT("reg_type: {}\nstart_offset: {}\nend_address: {}\nnum_registers: {}\nnum_decode: {}\ndecode_vals: [\n{}]\nhas_decode_offset_range: {}\n",
                register_info.reg_type, register_info.start_offset, register_info.end_address, (register_info.end_address - register_info.start_offset + 1), register_info.decode_vals.size(), fmt::join(register_info.decode_vals, ",\n"), register_info.has_decode_offset_range);
            if (register_info.has_decode_offset_range)
            {
            NEW_FPS_ERROR_PRINT("begin: {}\nend: {}\nstep: {}\nreplace_str: {}\n", register_info.decode_offset_range.begin, register_info.decode_offset_range.end, register_info.decode_offset_range.step, register_info.decode_offset_range.replace_str);
            }
        }
    }
    NEW_FPS_ERROR_PRINT_NO_ARGS("\n");


    // start to expand out (the tricky part):
    for (auto& component : components_array)
    {
        auto* current_expansion = &(post_expansion_array.emplace_back()); // really current_component_expansion

        // do y expansions first before going up to the x level:
        for (auto& register_info : component.register_info_array)
        {
            auto* current_reg_expansion = &(current_expansion->register_info_array.emplace_back());

            if (!register_info.has_decode_offset_range) // no expansion just copy info across
            {
                current_reg_expansion->reg_type = register_info.reg_type;
                current_reg_expansion->start_offset = register_info.start_offset;
                current_reg_expansion->end_address = register_info.end_address;
                current_reg_expansion->decode_vals = register_info.decode_vals;
            }
            else // do expansion here:
            {
                std::vector<config_decode_info> to_expand_array; // these are the ones we need to expand into the below array
                std::vector<config_decode_info> full_exp_decode_array; // stores all of the expanded decode_info structs in a single array
                full_exp_decode_array.reserve(register_info.decode_vals.size() * (register_info.decode_offset_range.end - register_info.decode_offset_range.begin + 1));

                // replace all of the replace_str characters with "{0>2}" for fmt formatting
                for (auto& decode : register_info.decode_vals)
                {
                    const auto replace_index = decode.id.find(register_info.decode_offset_range.replace_str);
                    if (replace_index != std::string::npos) // only replace for ids that have the replacement characters
                    {
                        decode.id.replace(replace_index, register_info.decode_offset_range.replace_str.size(), "{:0>2}");
                        // if this decode is one of type individual_bits or individual_enums then we need to replace the "id" of each one with {:0>2} as well
                        if (decode.is_individual_enums || decode.is_individual_bits)
                        {
                            for (auto& enum_bit_id : decode.bit_strings_array.final_array)
                            {
                                const auto inner_replace_index = enum_bit_id.id.find(register_info.decode_offset_range.replace_str);
                                enum_bit_id.id.replace(inner_replace_index, register_info.decode_offset_range.replace_str.size(), "{:0>2}");
                            }
                        }
                        auto& current_to_expand_decode_info = to_expand_array.emplace_back();
                        current_to_expand_decode_info = decode;
                    }
                    else // copy over that decode like a normal decode info:
                    {
                        auto& current_decode_info = full_exp_decode_array.emplace_back();
                        current_decode_info = decode;
                    }
                }

                // now expand them out:
                for (const auto& decode : to_expand_array)
                {
                    for (auto i = register_info.decode_offset_range.begin; i <= register_info.decode_offset_range.end; ++i)
                    {
                        auto& current_decode_info = full_exp_decode_array.emplace_back();

                        // copy information over to the new decode infos and do formatting:
                        // TODO(WALKER): Check expansion offset for valid expansion range (0 - 65535)
                        // and make sure there is no overflow into the zero range (going past 65535)
                        // although: I might not do this as this is almost never the case and it mostly their own fault
                        // so I'll forgo doing this for now
                        // that or there might be the one weird case where we want the loop around for some odd reason
                        // who knows (maybe a weirdchamp vendor)
                        current_decode_info = decode; // copy over all info
                        current_decode_info.id = fmt::format(current_decode_info.id, i); // format main uri
                        // if it is of bit strings type individual_bits/individual_enums then format each of the ids:
                        if (current_decode_info.is_individual_bits || current_decode_info.is_individual_enums)
                        {
                            for (auto& enum_bit_str_info : current_decode_info.bit_strings_array.final_array)
                            {
                                enum_bit_str_info.id = fmt::format(enum_bit_str_info.id, i);
                            }
                        }
                        current_decode_info.offset += register_info.decode_offset_range.step * (i - register_info.decode_offset_range.begin); // change offset
                    }
                }

                // sort them all:
                std::sort(full_exp_decode_array.begin(), full_exp_decode_array.end());

                // check for weird overlaps in offsets post expansion (probably needs a larger offset step):
                const auto* current_sort_decode = &(full_exp_decode_array[0]);
                for (std::size_t i = 1; i < full_exp_decode_array.size(); ++i)
                {
                    if ((current_sort_decode->offset + current_sort_decode->size - 1) >= full_exp_decode_array[i].offset)
                    {
                        NEW_FPS_ERROR_PRINT("offset conflict between uri's /components/{0}/{1} and /components/{0}/{2} post expansion, please make sure offsets are not overlapping after they expand out. Your step is probably too small\n", component.component_id, current_sort_decode->id, full_exp_decode_array[i].id);
                        return false;
                    }
                    current_sort_decode = &(full_exp_decode_array[i]);
                }

                // start to push them into the real array in 125 register chunks at most:
                const auto* prev_exp_decode = &(full_exp_decode_array.front());
                auto current_offset_min = prev_exp_decode->offset;
                for (auto& decode_expanded : full_exp_decode_array)
                {
                    auto current_offset_range = (decode_expanded.offset + decode_expanded.size - 1) - current_offset_min;
                    if (current_offset_range > MODBUS_MAX_READ_REGISTERS) // create new range of 125 if we are about to go over
                    {
                        // set values up for the current_reg_expansion before shifting to the new one:
                        current_reg_expansion->reg_type = register_info.reg_type;
                        current_reg_expansion->start_offset = current_offset_min;
                        current_reg_expansion->end_address = prev_exp_decode->offset + prev_exp_decode->size - 1;

                        current_offset_min = decode_expanded.offset;
                        current_reg_expansion = &(current_expansion->register_info_array.emplace_back());
                    }
                    // otherwise add it to this set of 125:
                    current_reg_expansion->decode_vals.emplace_back(std::move(decode_expanded));
                    prev_exp_decode = &decode_expanded;
                }
                // set last expansion's information:
                current_reg_expansion->reg_type = register_info.reg_type;
                current_reg_expansion->start_offset = current_reg_expansion->decode_vals.front().offset;
                current_reg_expansion->end_address = current_reg_expansion->decode_vals.back().offset + current_reg_expansion->decode_vals.back().size - 1;
            }
        }

        // then expand out the x's level:
        if (!component.has_component_offset_range && !component.has_device_id_range) // normal, copy stuff over
        {
            current_expansion->component_id = component.component_id;
            current_expansion->frequency = component.frequency;
            current_expansion->debounce = component.debounce;
            current_expansion->device_id = component.device_id;
            current_expansion->is_word_swapped = component.is_word_swapped;
            // heartbeat stuff:
            current_expansion->heartbeat_enabled = component.heartbeat_enabled;
            current_expansion->component_heartbeat_read_uri = component.component_heartbeat_read_uri;
            current_expansion->component_heartbeat_write_uri = component.component_heartbeat_write_uri;
            current_expansion->component_heartbeat_timeout = component.component_heartbeat_timeout;
        }
        if (component.has_component_offset_range) // expand out component_offset_range
        {
            const auto replace_index = component.component_id.find(component.component_offset_range.replace_str);
            component.component_id.replace(replace_index, component.component_offset_range.replace_str.size(), "{:0>2}");
            const auto first_current_exp_register_array_index = post_expansion_array.size() - 1;

            // iterate over and start to copy stuff over for the range:
            for (auto i = component.component_offset_range.begin; i <= component.component_offset_range.end; ++i)
            {
                current_expansion->component_id = fmt::format(component.component_id, i);
                current_expansion->frequency = component.frequency;
                current_expansion->debounce = component.debounce;
                current_expansion->device_id = component.device_id;
                current_expansion->is_word_swapped = component.is_word_swapped;
                // heartbeat stuff:
                current_expansion->heartbeat_enabled = component.heartbeat_enabled;
                current_expansion->component_heartbeat_read_uri = component.component_heartbeat_read_uri;
                current_expansion->component_heartbeat_write_uri = component.component_heartbeat_write_uri;
                current_expansion->component_heartbeat_timeout = component.component_heartbeat_timeout;

                // copy over register_info_array from first current expansion and start adding offsets:
                current_expansion->register_info_array = post_expansion_array[first_current_exp_register_array_index].register_info_array;
                for (auto& register_info : current_expansion->register_info_array)
                {
                    // iterate through and add offets:
                    for (auto& decode : register_info.decode_vals)
                    {
                        decode.offset += component.component_offset_range.step * (i - component.component_offset_range.begin);
                    }
                    register_info.start_offset += component.component_offset_range.step * (i - component.component_offset_range.begin);
                    register_info.end_address += component.component_offset_range.step * (i - component.component_offset_range.begin);
                }

                if (i != component.component_offset_range.end)
                {
                    current_expansion = &(post_expansion_array.emplace_back());
                }
            }
        }
        if (component.has_device_id_range) // expand out device_id_range last
        {
            const auto replace_index = component.component_id.find(component.device_id_range.replace_str);
            component.component_id.replace(replace_index, component.device_id_range.replace_str.size(), "{:0>2}");
            const auto first_current_exp_register_array_index = post_expansion_array.size() - 1;

            // iterate over and start to copy stuff over for the range:
            for (auto i = component.device_id_range.begin; i <= component.device_id_range.end; ++i)
            {
                current_expansion->component_id = fmt::format(component.component_id, (i - component.device_id_range.begin + 1));
                current_expansion->frequency = component.frequency;
                current_expansion->debounce = component.debounce;
                current_expansion->device_id = static_cast<uint8_t>(i);
                current_expansion->is_word_swapped = component.is_word_swapped;
                // heartbeat stuff:
                current_expansion->heartbeat_enabled = component.heartbeat_enabled;
                current_expansion->component_heartbeat_read_uri = component.component_heartbeat_read_uri;
                current_expansion->component_heartbeat_write_uri = component.component_heartbeat_write_uri;
                current_expansion->component_heartbeat_timeout = component.component_heartbeat_timeout;
                // regular array stuff:
                current_expansion->register_info_array = post_expansion_array[first_current_exp_register_array_index].register_info_array; // copy over the first expansion's array (so we keep post-device_id_range expansion info)

                if (i != component.device_id_range.end)
                {
                    current_expansion = &(post_expansion_array.emplace_back());
                }
            }
        }
    }

    // do basic error checking post expansion right before ip ranges:
    if (post_expansion_array.size() > max_num_components)
    {
        NEW_FPS_ERROR_PRINT("post expansion there are: {} total components, this is more than 64. This is illegal for fims, please reduce the number of total components.\n", post_expansion_array.size());
        return false;
    }

    std::size_t total_mappings = 0;
    for (const auto& expansion : post_expansion_array)
    {
        total_mappings += expansion.register_info_array.size();
    }
    if (total_mappings > max_num_threads)
    {
        NEW_FPS_ERROR_PRINT("The total mappings: {} post expansion has exceeded the maximum number of mappings: {}. Please decrease the total number of mappings or increase the maximum number of mappings allowed\n", total_mappings, max_num_threads);
        return false;
    }

    if (conn_info.has_ip_range)
    {
        // replace for ip address:
        const auto ip_replace_index = conn_info.ip_serial_string.find(conn_info.ip_range.replace_str);
        conn_info.ip_serial_string.replace(ip_replace_index, conn_info.ip_range.replace_str.size(), "{}");

        // replace for hardware name:
        const auto name_replace_index = conn_info.hardware_name.find(conn_info.ip_range.replace_str);
        conn_info.hardware_name.replace(name_replace_index, conn_info.ip_range.replace_str.size(), "{:0>2}");

        // replace the final replace str for ip inside each component in our component array:
        for (auto& component : post_expansion_array)
        {
            const auto comp_replace_index = component.component_id.find(conn_info.ip_range.replace_str);
            component.component_id.replace(comp_replace_index, conn_info.ip_range.replace_str.size(), "{:0>2}");
        }

        for (auto i = conn_info.ip_range.begin; i <= conn_info.ip_range.end; ++i)
        {
            auto& current_expansion = final_expansion.emplace_back();
            current_expansion = post_expansion_array;

            for (auto& component : current_expansion)
            {
                component.component_id = fmt::format(component.component_id, (i - conn_info.ip_range.begin + 1));
            }
        }

        // one final loop for final ip expansion array:
        for (auto i = conn_info.ip_range.begin; i <= conn_info.ip_range.end; ++i)
        {
            auto& current_expansion = conn_info.ip_expansion_array.emplace_back();
            current_expansion = fmt::format(conn_info.ip_serial_string, i);
        }

        for (auto i = conn_info.ip_range.begin; i <= conn_info.ip_range.end; ++i)
        {
            auto& current_expansion = conn_info.hardware_name_expansion_array.emplace_back();
            current_expansion = fmt::format(conn_info.hardware_name, (i - conn_info.ip_range.begin + 1));
        }
    }
    else
    {
        auto& current_expansion = final_expansion.emplace_back();
        current_expansion = post_expansion_array;
    }

    // setup subscriptions inside our sub arrays:
    for (const auto& expansion : final_expansion)
    {
        auto& current_array = sub_arrays.emplace_back();

        for (const auto& component : expansion)
        {
            auto& current_sub = current_array.emplace_back();
            current_sub = fmt::format(FMT_COMPILE("/components/{}"), component.component_id);
        }
    }

    // print out components array info:
    NEW_FPS_ERROR_PRINT_NO_ARGS("\nfinal post expansion:\n");
    if (conn_info.has_ip_range)
    {
        NEW_FPS_ERROR_PRINT("ips = [\n{}]\n", fmt::join(conn_info.ip_expansion_array, ",\n"));
        NEW_FPS_ERROR_PRINT("names = [\n{}]\n", fmt::join(conn_info.hardware_name_expansion_array, ",\n"));
    }
    else
    {
        NEW_FPS_ERROR_PRINT("ip = {}\n", std::string_view{conn_info.ip_serial_string.data(), conn_info.ip_serial_string.size()});
        NEW_FPS_ERROR_PRINT("name = {}\n", std::string_view{conn_info.hardware_name.data(), conn_info.hardware_name.size()});
    }
    NEW_FPS_ERROR_PRINT("subs = [\n{}]\n", fmt::join(sub_arrays, ",\n"));
    for (const auto& expansion : final_expansion)
    {
        for (const auto& component : expansion)
        {
            NEW_FPS_ERROR_PRINT("\nuri /components/{} = \nfrequency: {}\ndebounce: {}\ndevice_id: {}\nis_word_swapped: {}\nregisters_array_size: {}\n", 
                component.component_id, component.frequency, component.debounce, component.device_id, component.is_word_swapped, component.register_info_array.size());
            for (const auto& register_info : component.register_info_array)
            {
                NEW_FPS_ERROR_PRINT("reg_type: {}\nstart_offset: {}\nend_address: {}\nnum_registers: {}\nnum_decode: {}\ndecode_vals: [\n{}]\n",
                    register_info.reg_type, register_info.start_offset, register_info.end_address, (register_info.end_address - register_info.start_offset + 1), register_info.decode_vals.size(), fmt::join(register_info.decode_vals, ",\n"));
            }
        }
    }
    NEW_FPS_ERROR_PRINT_NO_ARGS("\nAfter final expansion:\n");

    // setup hardware_workspaces:
    main_workspace.hardware_workspaces.initialize_vector(final_expansion.size());
    main_workspace.num_actual_hardware = static_cast<uint8_t>(final_expansion.size()); // might change this to be down below instead

    // now do the rest of the config stage (putting info into thread workspaces per hardware workspace):
    for (std::size_t i = 0; i < final_expansion.size(); ++i)
    {
        auto& current_components_array = final_expansion[i];
        auto& current_hardware_workspace = main_workspace.hardware_workspaces[i];
        current_hardware_workspace.num_actual_components = static_cast<uint8_t>(current_components_array.size());

        uint8_t current_thread_index = 0;
        uint8_t current_thread_num = 0;
        // setup thread workspaces for this piece of hardware:
        for (std::size_t x = 0; x < current_components_array.size(); ++x)
        {
            auto& current_component = current_components_array[x];
            auto& current_component_workspace = current_hardware_workspace.component_workspaces[x];

            // we check heartbeat stuff post expansion here:
            if (current_component.heartbeat_enabled)
            {
                // copy over basic info:
                current_component_workspace.has_heartbeat = true;
                fmt::format_to(std::back_inserter(current_component_workspace.component_uri_buf), FMT_COMPILE("/components/{}"), current_component.component_id);
                current_component_workspace.heartbeat_poll_frequency = current_component.frequency;
                current_component_workspace.heartbeat_timeout_frequency = current_component.component_heartbeat_timeout;
                current_component_workspace.heartbeat_slave_address = current_component.device_id;

                // iterative over each of the registers array and check for read and write uri's respectively:
                bool found_read_uri = false;
                bool found_write_uri = false;
                for (const auto& reg_info_obj : current_component.register_info_array)
                {
                    for (const auto& decode_obj : reg_info_obj.decode_vals)
                    {
                        if (current_component.component_heartbeat_read_uri == decode_obj.id && !found_read_uri)
                        {
                            found_read_uri = true;
                            if (!(reg_info_obj.reg_type == Register_Types::Holding || reg_info_obj.reg_type == Register_Types::Input))
                            {
                                NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_read_uri {} can only be one of Holding or Input registers.\n", current_component.component_id, current_component.component_heartbeat_read_uri);
                                return false;
                            }
                            current_component_workspace.heartbeat_read_reg_type = reg_info_obj.reg_type; // copy over register type
                            current_component_workspace.heartbeat_read_offset = decode_obj.offset;
                            if (decode_obj.size > 1)
                            {
                                if (!current_component.is_word_swapped) // get the last word instead of the first one
                                {
                                    current_component_workspace.heartbeat_read_offset += decode_obj.size - 1U; // go to the last "word" (the one we care about)
                                }
                            }
                        }
                        if (current_component.component_heartbeat_write_uri == decode_obj.id && !found_write_uri)
                        {
                            found_write_uri = true;
                            if (reg_info_obj.reg_type != Register_Types::Holding)
                            {
                                NEW_FPS_ERROR_PRINT("for uri /components/{}, component_heartbeat_write_uri {} can only be a Holding register.\n", current_component.component_id, current_component.component_heartbeat_write_uri);
                                return false;
                            }
                            current_component_workspace.heartbeat_write_offset = decode_obj.offset;
                            if (decode_obj.size > 1)
                            {
                                if (!current_component.is_word_swapped) // get the last word instead of the first one
                                {
                                    current_component_workspace.heartbeat_write_offset += decode_obj.size - 1U; // go to the last "word" (the one we care about)
                                }
                            }
                        }
                    }
                }
                if (!found_read_uri)
                {
                    NEW_FPS_ERROR_PRINT("for uri /components/{} could not find the component_heartbeat_read_uri {}. Please check for typos\n", current_component.component_id, current_component.component_heartbeat_read_uri);
                    return false;
                }
                if (!current_component.component_heartbeat_write_uri.empty() && !found_write_uri)
                {
                    NEW_FPS_ERROR_PRINT("for uri /components/{} could not find the component_heartbeat_write_uri {}, even though one was provided. Please check for typos\n", current_component.component_id, current_component.component_heartbeat_write_uri);
                    return false;
                }
            }

            auto current_component_uri_index_pair_hash = str_view_hash(current_component.component_id);
            if (current_hardware_workspace.thread_uri_index_map[current_component_uri_index_pair_hash].decode_id == decode_all_index) // check that /components/x hasn't already been mapped:
            {
                NEW_FPS_ERROR_PRINT("uri /components/{} has already been mapped, please remove the duplicate\n", current_component.component_id);
                return false;
            }
            current_hardware_workspace.thread_uri_index_map[current_component_uri_index_pair_hash].decode_id = decode_all_index; // claim it

            current_component_workspace.num_threads = static_cast<uint8_t>(current_component.register_info_array.size() + (1UL * current_component.heartbeat_enabled));
            current_component_workspace.start_thread_index = current_thread_num;
            current_thread_num += current_component.register_info_array.size();

            for (std::size_t y = 0; y < current_component.register_info_array.size(); ++y)
            {
                current_hardware_workspace.thread_uri_index_map[current_component_uri_index_pair_hash].thread_id_mask[current_thread_index] = true;
                auto& current_thread_workspace = current_hardware_workspace.thread_workspaces[current_thread_index];

                auto& current_register = current_component.register_info_array[y];

                // set fims workspace:
                current_thread_workspace.comp_workspace = &current_component_workspace;

                // copy over basic info from component:
                current_thread_workspace.slave_address = current_component.device_id;
                current_thread_workspace.frequency = current_component.frequency;
                current_thread_workspace.debounce = current_component.debounce;
                current_thread_workspace.reg_type = current_register.reg_type;
                fmt::format_to(std::back_inserter(current_thread_workspace.main_component_uri_buf), FMT_COMPILE("/components/{}"), current_component.component_id);
                // copy over basic info from register:
                current_thread_workspace.start_offset = current_register.start_offset;
                current_thread_workspace.num_registers = current_register.end_address - current_register.start_offset + 1;
                current_thread_workspace.num_decode = static_cast<uint8_t>(current_register.decode_vals.size());

                // copy over decode array stuff (ids and decode_info):
                for (std::size_t z = 0; z < current_register.decode_vals.size(); ++z)
                {
                    auto& current_decode = current_register.decode_vals[z];
                    auto& current_thread_decode = current_thread_workspace.decode_info_array[z];
                    auto& current_thread_enum_bit_str_info = current_thread_workspace.enum_bit_strings_array[z];
                    auto& current_thread_id = current_thread_workspace.ids[z];

                    // copy over basic decode info:
                    current_thread_decode.flags.set_word_swapped(current_component.is_word_swapped);
                    current_thread_decode.flags.set_size(current_decode.size);
                    current_thread_decode.offset = current_decode.offset;
                    current_thread_decode.scale = current_decode.scale;
                    current_thread_decode.shift = current_decode.shift;
                    current_thread_decode.invert_mask = current_decode.invert_mask;
                    current_thread_decode.flags.set_signed(current_decode.is_signed);
                    current_thread_decode.flags.set_float(current_decode.is_float);
                    current_thread_decode.flags.set_enum(current_decode.is_enum);
                    current_thread_decode.flags.set_bit_field(current_decode.is_bit_field);
                    current_thread_decode.flags.set_individual_bits(current_decode.is_individual_bits);
                    current_thread_decode.flags.set_enum_field(current_decode.is_enum_field);
                    current_thread_decode.flags.set_individual_enums(current_decode.is_individual_enums);

                    // put id into string storage and setup key:
                    auto& current_id = id_map[current_decode.id];
                    if (current_id.str_len == 0) // put it into storage:
                    {
                        current_id = main_workspace.str_storage.append_str(current_decode.id);
                        if (current_id.str_len == 0)
                        {
                            NEW_FPS_ERROR_PRINT("ran out of string storage for uri /components/{}/{}, please increase string storage size or reduce the size/number of strings for this modbus client config\n", current_component.component_id, current_decode.id);
                            return false;
                        }
                    }
                    current_thread_id = current_id;

                    uint64_t current_last_index = 0; // keeps track of the "last_index" for sub arrays. Needed for bit_fields, individual_enums, and enum_fields so we can skip ahead to the next sub array during runtime (or "Unknown bits can be determined")

                    // TODO(WALKER): get the bit_strings stuff in here:
                    current_thread_enum_bit_str_info.care_mask = current_decode.bit_strings_array.final_care_mask; // copy over care mask
                    for (const auto& enum_bit_str_info : current_decode.bit_strings_array.final_array) // iterate over final_array
                    {
                        // these three use id as the enum/bit string:
                        if (current_decode.is_bit_field || current_decode.is_individual_bits || current_decode.is_enum)
                        {
                            auto& current_enum_bit_str = current_thread_enum_bit_str_info.vec.emplace_back();
                            current_enum_bit_str.begin_bit = enum_bit_str_info.begin_bit;
                            current_enum_bit_str.end_bit = enum_bit_str_info.end_bit;
                            current_enum_bit_str.care_mask = enum_bit_str_info.care_mask;
                            current_enum_bit_str.enum_bit_val = enum_bit_str_info.enum_val;
                            current_enum_bit_str.last_index = static_cast<uint8_t>(current_decode.bit_strings_array.final_array.size() - 1); // set last index to the last index in the actual array
                            // put id into string storage and setup key:
                            auto& inner_current_id = id_map[enum_bit_str_info.id];
                            if (inner_current_id.str_len == 0) // put it into storage:
                            {
                                inner_current_id = main_workspace.str_storage.append_str(enum_bit_str_info.id);
                                if (inner_current_id.str_len == 0) // ran out of storage space:
                                {
                                    NEW_FPS_ERROR_PRINT("ran out of string storage for bit_string {}, please increase string storage size or reduce the size/number of strings for this modbus client config\n", enum_bit_str_info.id);
                                    return false;
                                }
                            }
                            current_enum_bit_str.enum_bit_string = inner_current_id;
                        }
                        // these go into enum_strings (individual_enums uses id as well)
                        else if (current_decode.is_enum_field || current_decode.is_individual_enums)
                        {
                            // last index recording:
                            defer { ++current_last_index; }; // increment to beginning of next sub array at the end:
                            current_last_index += enum_bit_str_info.enum_strings.size() - (1 * current_decode.is_enum_field);
                            if (current_last_index >= max_enum_bit_strings_allowed) // safety/sanity check (should NOT occur in the real system because I already kept track of this during the config reading phase above, but is here just in case)
                            {
                                NEW_FPS_ERROR_PRINT("error when determining the last_index post expansion during thread workspace setup, the current_last_index (starting at 0) = {}, which exceeds the maximum of: {}. The programmer probably needs to fix this, sorry!\n", current_last_index, max_enum_bit_strings_allowed);
                                return false;
                            }

                            if (current_decode.is_individual_enums) // copy over main id stuff first for individual_enums (so key slot is the first in the sub_array for this bit strings type):
                            {
                                auto& current_enum_bit_str = current_thread_enum_bit_str_info.vec.emplace_back();
                                current_enum_bit_str.begin_bit = enum_bit_str_info.begin_bit;
                                current_enum_bit_str.end_bit = enum_bit_str_info.end_bit;
                                current_enum_bit_str.care_mask = enum_bit_str_info.care_mask;
                                current_enum_bit_str.enum_bit_val = enum_bit_str_info.enum_val;
                                current_enum_bit_str.last_index = static_cast<uint8_t>(current_last_index);
                                // put id into string storage and setup key for this sub array:
                                auto& inner_current_id = id_map[enum_bit_str_info.id];
                                if (inner_current_id.str_len == 0) // put it into storage:
                                {
                                    inner_current_id = main_workspace.str_storage.append_str(enum_bit_str_info.id);
                                    if (inner_current_id.str_len == 0) // ran out of storage space:
                                    {
                                        NEW_FPS_ERROR_PRINT("ran out of string storage for individual_enum id {}, please increase string storage size or reduce the size/number of strings for this modbus client config\n", enum_bit_str_info.id);
                                        return false;
                                    }
                                }
                                current_enum_bit_str.enum_bit_string = inner_current_id;
                            }
                            for (const auto& enum_str : enum_bit_str_info.enum_strings) // copy over enum_strings array stuff:
                            {
                                auto& current_enum_bit_str = current_thread_enum_bit_str_info.vec.emplace_back();
                                current_enum_bit_str.begin_bit = enum_bit_str_info.begin_bit;
                                current_enum_bit_str.end_bit = enum_bit_str_info.end_bit;
                                current_enum_bit_str.care_mask = enum_bit_str_info.care_mask;
                                current_enum_bit_str.enum_bit_val = enum_str.enum_val; // copy over enum_str's enum_val
                                current_enum_bit_str.last_index = static_cast<uint8_t>(current_last_index);
                                // put id into string storage and setup key for this sub array:
                                auto& inner_current_id = id_map[enum_str.enum_str];
                                if (inner_current_id.str_len == 0) // put it into storage:
                                {
                                    inner_current_id = main_workspace.str_storage.append_str(enum_str.enum_str);
                                    if (inner_current_id.str_len == 0) // ran out of storage space:
                                    {
                                        NEW_FPS_ERROR_PRINT("ran out of string storage for enum_string {}, please increase string storage size or reduce the size/number of strings for this modbus client config\n", enum_str.enum_str);
                                        return false;
                                    }
                                }
                                current_enum_bit_str.enum_bit_string = inner_current_id;
                            }
                        }
                    }

                    // setup thread_uri_index_map that uri (x/y) pair:
                    decode_uri_buf.clear();
                    fmt::format_to(std::back_inserter(decode_uri_buf), FMT_COMPILE("{}/{}"), current_component.component_id, current_decode.id);
                    auto& current_decode_uri_index_pair = current_hardware_workspace.thread_uri_index_map[str_view_hash(std::string_view{decode_uri_buf.data(), decode_uri_buf.size()})];
                    if (current_decode_uri_index_pair.thread_id_mask.any()) // check that x/y hasn't already been mapped:
                    {
                        NEW_FPS_ERROR_PRINT("uri /components/{}/{} has already been mapped, please remove the duplicate\n", current_component.component_id, current_decode.id);
                        return false;
                    }
                    current_decode_uri_index_pair.thread_id_mask[current_thread_index] = true;
                    current_decode_uri_index_pair.decode_id = static_cast<uint8_t>(z); // claim decode
                    current_decode_uri_index_pair.bit_id = bit_all_index; // all bits
                    current_decode_uri_index_pair.enum_id = enum_all_index; // all enums
                    // individual_bits setup:
                    if (current_decode.is_individual_bits) // get bits uris and "bit_ids" in
                    {
                        for (const auto& bit_str : current_thread_enum_bit_str_info.vec)
                        {
                            decode_uri_buf.clear();
                            fmt::format_to(std::back_inserter(decode_uri_buf), FMT_COMPILE("{}/{}"), current_component.component_id, main_workspace.str_storage.get_str(bit_str.enum_bit_string));
                            auto& inner_current_decode_uri_index_pair = current_hardware_workspace.thread_uri_index_map[str_view_hash(std::string_view{decode_uri_buf.data(), decode_uri_buf.size()})];
                            if (inner_current_decode_uri_index_pair.thread_id_mask.any()) // check that x/y hasn't already been mapped:
                            {
                                NEW_FPS_ERROR_PRINT("individual_bit uri /components/{}/{} has already been mapped, please remove the duplicate\n", current_component.component_id, main_workspace.str_storage.get_str(bit_str.enum_bit_string));
                                return false;
                            }
                            inner_current_decode_uri_index_pair.thread_id_mask[current_thread_index] = true;
                            inner_current_decode_uri_index_pair.decode_id = static_cast<uint8_t>(z); // claim decode
                            inner_current_decode_uri_index_pair.bit_id = bit_str.begin_bit; // claim bit
                            inner_current_decode_uri_index_pair.enum_id = enum_all_index; // all enums
                            inner_current_decode_uri_index_pair.flags.set_individual_bit(true); // label this uri as an individual_bit for listener
                        }
                    }
                    // individual_enums setup:
                    else if (current_decode.is_individual_enums)
                    {
                        for (std::size_t index = 0; index < current_thread_enum_bit_str_info.vec.size(); ++index)
                        {
                            const auto& enum_str = current_thread_enum_bit_str_info.vec[index];

                            decode_uri_buf.clear();
                            fmt::format_to(std::back_inserter(decode_uri_buf), FMT_COMPILE("{}/{}"), current_component.component_id, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                            auto& inner_current_decode_uri_index_pair = current_hardware_workspace.thread_uri_index_map[str_view_hash(std::string_view{decode_uri_buf.data(), decode_uri_buf.size()})];
                            if (inner_current_decode_uri_index_pair.thread_id_mask.any()) // check that x/y hasn't already been mapped:
                            {
                                NEW_FPS_ERROR_PRINT("individual_enum uri /components/{}/{} has already been mapped, please remove the duplicate\n", current_component.component_id, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                return false;
                            }
                            inner_current_decode_uri_index_pair.thread_id_mask[current_thread_index] = true;
                            inner_current_decode_uri_index_pair.decode_id = static_cast<uint8_t>(z); // claim decode
                            inner_current_decode_uri_index_pair.bit_id = enum_str.begin_bit; // claim bit
                            inner_current_decode_uri_index_pair.enum_id = static_cast<uint8_t>(index); // claim enum
                            inner_current_decode_uri_index_pair.flags.set_individual_enum(true); // label this uri as an individual_enum for listener
                            index = enum_str.last_index; // jump to the beginning of the next sub_array
                        }
                    }
                }
                ++current_thread_index;
            }
        }
        current_hardware_workspace.num_actual_threads = current_thread_index;

        auto& current_hardware_conn_info = current_hardware_workspace.connection_info;
        // add in connection info for the current hardware workspace:        
        if (conn_info.conn_type == Conn_Types::TCP)
        {
            if (conn_info.has_ip_range) // multi connection (multiple hardware workspaces):
            {
                fmt::format_to(std::back_inserter(current_hardware_conn_info.ip_serial_buf), "{}", conn_info.ip_expansion_array[i]);
                fmt::format_to(std::back_inserter(current_hardware_conn_info.hardware_name_buf), "{}", conn_info.hardware_name_expansion_array[i]);
            }
            else // single hardware workspace:
            {
                fmt::format_to(std::back_inserter(current_hardware_conn_info.ip_serial_buf), "{}", conn_info.ip_serial_string);
                fmt::format_to(std::back_inserter(current_hardware_conn_info.hardware_name_buf), "{}", conn_info.hardware_name);
            }
            current_hardware_conn_info.ip_serial_buf.push_back('\0');
            current_hardware_conn_info.port = conn_info.port;
            current_hardware_conn_info.conn_type = Conn_Types::TCP;
        }
        else // rtu (only single piece of hardware accepted -> no ranges):
        {
            // serial name and hardware name:
            fmt::format_to(std::back_inserter(current_hardware_conn_info.ip_serial_buf), "{}", conn_info.ip_serial_string);
            fmt::format_to(std::back_inserter(current_hardware_conn_info.hardware_name_buf), "{}", conn_info.hardware_name);

            // other basic info:
            current_hardware_conn_info.baud_rate = conn_info.baud_rate;
            current_hardware_conn_info.parity = conn_info.parity;
            current_hardware_conn_info.data_bits = conn_info.data_bits;
            current_hardware_conn_info.stop_bits = conn_info.stop_bits;
            current_hardware_conn_info.conn_type = Conn_Types::RTU;
        }

        const auto current_hardware_name = std::string_view{current_hardware_conn_info.hardware_name_buf.data(), current_hardware_conn_info.hardware_name_buf.size()};

        // connection steps (to fims and to modbus server):

        // connect fims:
        if(!current_hardware_workspace.fims_gateway.Connect())
        {
            NEW_FPS_ERROR_PRINT("couldn't connect to fims for hardware #{} (name: {})\n", i + 1, current_hardware_name);
            return false;
        }

        // subscribe to that hardware's sub list:
        if(!current_hardware_workspace.fims_gateway.new_subscribe(sub_arrays[i]))
        {
            NEW_FPS_ERROR_PRINT("couldn't subscribe over fims for hardware #{} (name: {})\n", i + 1, current_hardware_name);
            return false;
        }

        // setup modbus connection contexts for the current piece of hardware (try to go for one per thread):
        connection_status_array.emplace_back();
        for (std::size_t c = 0; c < current_thread_index; ++c)
        {
            if (c >= max_num_conns) break; // cannot exceed max_num_conns for threads

            mod_conn_raii current_conn;
            auto connected = -1;
            if (current_hardware_conn_info.conn_type == Conn_Types::TCP)
            {
                current_conn.conn = modbus_new_tcp(current_hardware_conn_info.ip_serial_buf.data(), current_hardware_conn_info.port);
            }
            else // RTU:
            {
                current_conn.conn = modbus_new_rtu(current_hardware_conn_info.ip_serial_buf.data(), 
                                                    current_hardware_conn_info.baud_rate, 
                                                    current_hardware_conn_info.parity, 
                                                    current_hardware_conn_info.data_bits,
                                                    current_hardware_conn_info.stop_bits);
            }
            connected = modbus_connect(current_conn);
            if (connected == -1 && c == 0) // we failed on first connection for this piece of hardware (can't connect for that hardware)
            {
                if (current_hardware_conn_info.conn_type == Conn_Types::TCP)
                {
                    NEW_FPS_ERROR_PRINT("could not establish TCP connection to modbus server for hardware #{} (name: {}), on ip: {}, with port: {}, err = {}\n", 
                        i + 1, 
                        current_hardware_name,
                        current_hardware_conn_info.ip_serial_buf.data(), 
                        current_hardware_conn_info.port, 
                        modbus_strerror(errno));
                }
                else // RTU:
                {
                    NEW_FPS_ERROR_PRINT("could not establish serial RTU connection to modbus server for hardware #{} (name: {}), with serial_device: {}, baud_rate: {}, parity: {}, data_bits: {}, and stop_bits: {}, err = {}\n", 
                        i + 1, 
                        current_hardware_name,
                        current_hardware_conn_info.ip_serial_buf.data(), 
                        current_hardware_conn_info.baud_rate, 
                        current_hardware_conn_info.parity, 
                        current_hardware_conn_info.data_bits,
                        current_hardware_conn_info.stop_bits, 
                        modbus_strerror(errno));
                }
                connection_status_array.back() = false;
                break;
            }
            if (connected == -1) // reached our limit (we have a limited number of connections for this piece of hardware):
            {
                break;
            }
            // successful connection (add it to the array):
            modbus_set_error_recovery(current_conn, MODBUS_ERROR_RECOVERY_PROTOCOL);
            ++current_hardware_workspace.num_actual_conns;
            current_hardware_workspace.conn_array[c].conn = current_conn.conn;
            current_conn.conn = nullptr;
            connection_status_array.back() = true;
        }
    }

    // see if we have an ethernet cable plugged in at all:
    bool has_single_hardware_connection = false;
    for (std::size_t i = 0; i < connection_status_array.size(); ++i)
    {
        auto& current_hardware_workspace = main_workspace.hardware_workspaces[i];

        const bool connected = connection_status_array[i];
        if (connected)
        {
            has_single_hardware_connection = true; // we are good to go
            current_hardware_workspace.connection_info.is_fully_connected = true; // so main can check that hardware workspaces futures
        }
    }
    if (!has_single_hardware_connection)
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("couldn't establish a single connection to any piece of hardware, erroring out\n");
        return false;
    }

    return true;
}

bool heartbeat_thread(const uint8_t hardware_id, const uint8_t component_id) noexcept
{
    auto& my_hardware_workspace = main_workspace.hardware_workspaces[hardware_id];
    auto& comp_workspace = my_hardware_workspace.component_workspaces[component_id];

    auto& conn_locks = my_hardware_workspace.conn_locks;
    auto& conn_array = my_hardware_workspace.conn_array;
    auto& fims_gateway = my_hardware_workspace.fims_gateway;

    const auto component_uri_view = std::string_view{comp_workspace.component_uri_buf.data(), comp_workspace.component_uri_buf.size()};
    const auto component_name = component_uri_view.substr(component_uri_split_index);

    uint16_t prev_val;
    bool heartbeat_val_changed = true; // considered changed the first time (one free pass before timeout is considered)
    bool component_connected = true;
    bool has_errno = false;
    int64_t begin_work;
    auto remainder_time = 0ns; // positive or negative (always adds to the time_left -> positive = sleep more, negative = sleep less)

    comp_workspace.heartbeat_last_check_time = main_workspace.proper_steady_clock.rdns(); // we begin with checking now and succeeding

    // main loop:
    while (true)
    {
        begin_work = main_workspace.proper_steady_clock.rdns();
        if (comp_workspace.heartbeat_thread_needs_to_stop.load(std::memory_order_acquire)) return false; // heartbeat threads exit if they are told to exit by main
        prev_val = comp_workspace.heartbeat_current_val;

        { // empty scope for conn_lock_guard destructor call
            conn_lock_guard my_lock{conn_locks, conn_array, my_hardware_workspace.num_actual_conns, component_id};

            // do actual modbus poll after setting parameters:
            modbus_set_slave(*my_lock.conn, comp_workspace.heartbeat_slave_address);
            modbus_set_response_timeout(*my_lock.conn, 0, ((comp_workspace.heartbeat_poll_frequency < modbus_default_timeout) ? comp_workspace.heartbeat_poll_frequency.count() : modbus_default_timeout.count()) * 1000);

            // NOTE(WALKER): This only reads the least significant word for heartbeat (whichever word is responsible for 0-65535) regardless of size (determined during load_config phase)
            if (comp_workspace.heartbeat_read_reg_type == Register_Types::Holding)
            {
                has_errno = modbus_read_registers(*my_lock.conn, comp_workspace.heartbeat_read_offset, 1, &comp_workspace.heartbeat_current_val) == -1;
            }
            else // input registers:
            {
                has_errno = modbus_read_input_registers(*my_lock.conn, comp_workspace.heartbeat_read_offset, 1, &comp_workspace.heartbeat_current_val) == -1;
            }
        }

        if (has_errno)
        {
            const auto current_errno = errno;
            NEW_FPS_ERROR_PRINT("hardware #{} component {}'s heartbeat thread, error when reading modbus_registers, err = {}\n", hardware_id + 1, component_name, modbus_strerror(current_errno));
            if (current_errno == modbus_errno_disconnect || current_errno == modbus_errno_cant_connect)
            {
                comp_workspace.component_connected.store(false, std::memory_order_release); // set this component to be disconnected
                component_connected = false;
            }
            has_errno = false; // reset errno
        }
        // NOTE(WALKER): Might have main do this instead of heartbeat thread (so main can start the threads up again)
        else if (!component_connected) // we read something properly
        {
            comp_workspace.component_connected.store(true, std::memory_order_release); // set this component to be connected again
            component_connected = true;

            // send event out here for reconnection (severity == 1?):
            fmt::basic_memory_buffer<char, 75> event_message;
            fmt::format_to(std::back_inserter(event_message), FMT_COMPILE("Component {} reconnected."), component_name);
            if (!emit_event(fims_gateway, "Modbus Client"sv, std::string_view{event_message.data(), event_message.size()}, Event_Severity::Info))
            {
                NEW_FPS_ERROR_PRINT("can't send fims post to /events for hardware #{} component {}'s heartbeat thread, erroring out.\n", hardware_id + 1, component_name);
                return false;
            }
        }

        if (prev_val != comp_workspace.heartbeat_current_val) heartbeat_val_changed = true; // this means we are getting an update

        const auto now = main_workspace.proper_steady_clock.rdns();
        if (std::chrono::nanoseconds{now - comp_workspace.heartbeat_last_check_time} >= comp_workspace.heartbeat_timeout_frequency) // time to check whether or not the heartbeat value has changed since last we checked it
        {
            if (component_connected && !heartbeat_val_changed)
            {
                comp_workspace.component_connected.store(false, std::memory_order_release); // set this component to be disconnected -> we timed out
                component_connected = false;

                fmt::basic_memory_buffer<char, 75> event_message;
                fmt::format_to(std::back_inserter(event_message), FMT_COMPILE("Component {} disconnected; heartbeat no longer detected."), component_name);
                if(!emit_event(fims_gateway, "Modbus Client"sv, std::string_view{event_message.data(), event_message.size()}, Event_Severity::Info))
                {
                    NEW_FPS_ERROR_PRINT("can't send fims post to /events for hardware #{} component {}'s heartbeat thread, erroring out.\n", hardware_id + 1, component_name);
                    return false;
                }
            }
            heartbeat_val_changed = false; // reset check

            // time to do write if we have one:
            if (component_connected && comp_workspace.heartbeat_write_offset > -1)
            {
                conn_lock_guard my_lock{conn_locks, conn_array, my_hardware_workspace.num_actual_conns, component_id};

                // do modbus stuff:
                modbus_set_slave(*my_lock.conn, comp_workspace.heartbeat_slave_address);
                modbus_set_response_timeout(*my_lock.conn, 0, ((comp_workspace.heartbeat_poll_frequency < modbus_default_timeout) ? comp_workspace.heartbeat_poll_frequency.count() : modbus_default_timeout.count()) * 1000);

                has_errno = modbus_write_register(*my_lock.conn, comp_workspace.heartbeat_write_offset, comp_workspace.heartbeat_current_val + 1U) == -1;
                if (has_errno)
                {
                    const auto current_errno = errno;
                    NEW_FPS_ERROR_PRINT("hardware #{} component {}'s heartbeat thread, error when setting modbus registers, err = {}\n", hardware_id + 1, component_name, modbus_strerror(current_errno));
                    if (current_errno == modbus_errno_disconnect || current_errno == modbus_errno_cant_connect)
                    {
                        comp_workspace.component_connected.store(false, std::memory_order_release); // set this component to be disconnected
                        component_connected = false;
                    }
                    has_errno = false; // reset errno
                }
                // next time it goes through it should be +1 anyway
                // else // no errno (we wrote properly)
                // {
                //     ++comp_workspace.heartbeat_current_val;
                // }
            }
        }

        if (component_connected) // only do normal pubs if we are properly connected (this means other threads should be started up)
        {
            // contribute to pub step:
            bool has_contributed_to_pub = false;

            while (!has_contributed_to_pub)
            {
                comp_workspace.pub_lock.lock();
                if (comp_workspace.pub_heartbeat_contributed)
                {
                    // this means we have "wrapped-around" and need to spin for a while -> things are backed up
                    comp_workspace.pub_lock.unlock();
                    NEW_FPS_ERROR_PRINT("warning from hardware #{}, component {}'s heartbeat thread, pubs are getting backed up. We have wrapped around. Please consider having a higher frequency or less mappings\n", hardware_id + 1, component_name);
                    std::this_thread::sleep_for(2ms); // sleep for about 2ms before trying again
                    continue;
                }
                has_contributed_to_pub = true;
                defer { comp_workspace.pub_lock.unlock(); };
                ++comp_workspace.pub_thread_counter; // increment counter
                comp_workspace.pub_heartbeat_contributed = true; // set this heartbeat thread to be done

                auto& send_buf = comp_workspace.pub_buf;
                if (comp_workspace.pub_thread_counter == 1) // this means this thread is the first thread to get there. They can clear the buffer and start the object
                {
                    send_buf.clear();
                    send_buf.push_back('{');
                }

                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("modbus_heartbeat":{},"component_connected":true,)"), comp_workspace.heartbeat_current_val);

                if (comp_workspace.pub_thread_counter == comp_workspace.num_threads) // this is the last thread to finish formatting to the send_buf (this thread needs to publish the message)
                {
                    comp_workspace.pub_heartbeat_contributed = false; // reset heartbeat contribution
                    comp_workspace.pub_thread_counter = 0; // reset counter
                    comp_workspace.pub_threads_status = 0UL; // reset threads status for wrap-around

                    const auto timestamp       = std::chrono::system_clock::now();
                    const auto timestamp_micro = spdlog::details::fmt_helper::time_fraction<std::chrono::microseconds>(timestamp);
                    fmt::format_to(std::back_inserter(send_buf), R"("Timestamp":"{:%m-%d-%Y %T}.{}"}})", timestamp, timestamp_micro.count());

                    // send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                    // send_buf.push_back('}'); // finish pub object

                    if (!fims_gateway.send_pub(component_uri_view, std::string_view{send_buf.data(), send_buf.size()}))
                    {
                        NEW_FPS_ERROR_PRINT("can't send fims pub for hardware #{} component {}'s heartbeat thread, erroring out.\n", hardware_id + 1, component_name);
                        return false;
                    }
                }
            }
        }
        else // we are disconnected just send it out like normal:
        {
            // NOTE(WALKER): Does this need "Timestamp"? -> is that really important on a disconnect? For now, no
            fmt::basic_memory_buffer<char, 50> send_buf;
            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"modbus_heartbeat":{},"component_connected":false}})"), comp_workspace.heartbeat_current_val);

            if (!fims_gateway.send_pub(component_uri_view, std::string_view{send_buf.data(), send_buf.size()}))
            {
                NEW_FPS_ERROR_PRINT("can't send fims pub for hardware #{} component {}'s heartbeat thread, erroring out.\n", hardware_id + 1, component_name);
                return false;
            }
        }

        // sleep for some time_left:
        std::this_thread::sleep_for(comp_workspace.heartbeat_poll_frequency - std::chrono::nanoseconds{main_workspace.proper_steady_clock.rdns() - begin_work} + remainder_time);
        remainder_time = std::chrono::nanoseconds{main_workspace.proper_steady_clock.rdns() - begin_work} - comp_workspace.heartbeat_poll_frequency;
    }

    return true;
}

// this will be the base for the basic worker thread:
// uses if constexpr magic to generate functions for us
// so we don't have to repeat ourselves (code dupe is lame):
template<Register_Types Reg_Type>
bool worker_thread(const uint8_t hardware_id, const uint8_t thread_id) noexcept
{
    auto& my_hardware_workspace = main_workspace.hardware_workspaces[hardware_id];
    auto& my_workspace = my_hardware_workspace.thread_workspaces[thread_id];

    auto& conn_locks = my_hardware_workspace.conn_locks;
    auto& conn_array = my_hardware_workspace.conn_array;
    auto& fims_gateway = my_hardware_workspace.fims_gateway;
    // auto& send_buf = my_workspace.send_buf;
    const auto component_uri_view = std::string_view{my_workspace.main_component_uri_buf.data(), my_workspace.main_component_uri_buf.size()};
    auto& my_channel = my_workspace.channel;


    fmt::memory_buffer debug_buf;
    // print out thread_info
    fmt::format_to(std::back_inserter(debug_buf), R"(
"main_uri": {},
"hardware_id": {},
"thread_id": {},
"slave_address": {},
"start_offset": {},
"num_registers": {},
"register_type": {},
"frequency": {},
"debounce": {},
"num_decode": {},
"decode_info_array": [
)",
        component_uri_view,
        hardware_id + 1,
        thread_id,
        my_workspace.slave_address,
        my_workspace.start_offset,
        my_workspace.num_registers,
        my_workspace.reg_type,
        my_workspace.frequency,
        my_workspace.debounce,
        my_workspace.num_decode);

    for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
    {
        fmt::format_to(std::back_inserter(debug_buf), "{},\n", my_workspace.decode_info_array[i]);
    }
    fmt::format_to(std::back_inserter(debug_buf), "],\n\"ids\": [");
    for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
    {
        fmt::format_to(std::back_inserter(debug_buf), "{}, ", main_workspace.str_storage.get_str(my_workspace.ids[i]));
    }
    fmt::format_to(std::back_inserter(debug_buf), "],\n\"bit_strings_info\": [");
    for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
    {
        fmt::format_to(std::back_inserter(debug_buf), R"(
{{ "care_mask": {}, "bit_strings_array": [)", my_workspace.enum_bit_strings_array[i].care_mask);
        for (const auto& ebsi : my_workspace.enum_bit_strings_array[i].vec)
        {
            fmt::format_to(std::back_inserter(debug_buf), FMT_COMPILE(R"(
    {{ "begin_bit": {}, "end_bit": {}, "care_mask": {}, "last_index": {}, "value": {}, "string": {} }})"), ebsi.begin_bit, ebsi.end_bit, ebsi.care_mask, ebsi.last_index, ebsi.enum_bit_val, main_workspace.str_storage.get_str(ebsi.enum_bit_string));
        }
        debug_buf.resize(debug_buf.size() - 1 * (my_workspace.enum_bit_strings_array[i].vec.size() > 0));
        debug_buf.append(std::string_view{"] },"}); // finish array and object
    }
    fmt::format_to(std::back_inserter(debug_buf), "]\n");
    NEW_FPS_ERROR_PRINT("{}", std::string_view{debug_buf.data(), debug_buf.size()});

    // return false;

    // flush out our channel if this thread just started (in the case where threads need to be restarted) -> prevents messages from getting backed up
    {
        auto flush_chan_size = my_channel->size();
        while (flush_chan_size-- != 0UL) { my_channel->pop(); } // flush all the messages off the channel
    }

    bool has_errno = false;
    int64_t begin_work;
    auto overshoot_time = 0ns;

    // main loop:
    while (true)
    {
        begin_work = main_workspace.proper_steady_clock.rdns();
        if (!my_workspace.comp_workspace->component_connected.load(std::memory_order_acquire)) { return true; } // component disconnected

        { // empty scope for conn_lock_guard destructor call
            conn_lock_guard my_lock{conn_locks, conn_array, my_hardware_workspace.num_actual_conns, thread_id};

            // do actual modbus poll after setting parameters:
            modbus_set_slave(*my_lock.conn, my_workspace.slave_address);
            if (my_workspace.frequency < modbus_default_timeout)
            {
                modbus_set_response_timeout(*my_lock.conn, 0, my_workspace.frequency.count() * 1000);
            }
            else
            {
                modbus_set_response_timeout(*my_lock.conn, 0, modbus_default_timeout.count() * 1000);
            }
            if constexpr (Reg_Type == Register_Types::Holding)
            {
                has_errno = modbus_read_registers(*my_lock.conn, my_workspace.start_offset, my_workspace.num_registers, my_workspace.raw_input.data()) == -1;
            }
            else if constexpr (Reg_Type == Register_Types::Input)
            {
                has_errno = modbus_read_input_registers(*my_lock.conn, my_workspace.start_offset, my_workspace.num_registers, my_workspace.raw_input.data()) == -1;
            }
            // NOTE(WALKER): These two below use the regular buffer except we reinterpet the first uint16_t* as uint8_t*
            else if constexpr (Reg_Type == Register_Types::Coil)
            {
                has_errno = modbus_read_bits(*my_lock.conn, my_workspace.start_offset, my_workspace.num_registers, reinterpret_cast<uint8_t*>(my_workspace.raw_input.data())) == -1;
            }
            else // Discrete Inputs:
            {
                has_errno = modbus_read_input_bits(*my_lock.conn, my_workspace.start_offset, my_workspace.num_registers, reinterpret_cast<uint8_t*>(my_workspace.raw_input.data())) == -1;
            }
        }
        if (has_errno)
        {
            const auto current_errno = errno;
            NEW_FPS_ERROR_PRINT("hardware #{} thread #{}, error when reading modbus_registers, err = {}\n", hardware_id + 1, thread_id, modbus_strerror(current_errno));
            if (current_errno == modbus_errno_disconnect || current_errno == modbus_errno_cant_connect)
            {
                return true; // error out on disconnect or can't connect (main should attempt to restart this workspace)
            }
            has_errno = false; // reset errno
        }

        // NOTE(WALKER): This bool* here allows us to treat our raw input array as if it 
        // was an array of uint8_t (which modbus treats as when we pass it in to read_bits etc.)
        // this is for the case where we are reading in coils and discrete inputs
        bool* raw_input_as_bools = reinterpret_cast<bool*>(my_workspace.raw_input.data());
        for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
        {
            auto& current_decode = my_workspace.decode_info_array[i];
            auto& current_decoded_cache_val = my_workspace.decoded_cache[i];

            if constexpr (Reg_Type == Register_Types::Coil || Reg_Type == Register_Types::Discrete_Input)
            {
                const auto to_pub = raw_input_as_bools[current_decode.offset - my_workspace.start_offset];
                // my_workspace.changed_mask[i] = (current_decoded_cache_val != to_pub);
                my_workspace.changed_mask[i] = true; // NOTE(WALKER): delete this line and uncomment the above line for proper if changed logic
                current_decoded_cache_val = to_pub;
            }
            else // Holding and Input registers:
            {
                my_workspace.changed_mask[i] = decode(&my_workspace.raw_input[current_decode.offset - my_workspace.start_offset],
                                                        current_decode,
                                                        current_decoded_cache_val,
                                                        current_decode.flags.is_goes_into_bits_type() ? my_workspace.enum_bit_strings_array[i].care_mask : std::numeric_limits<uint64_t>::max(),
                                                        current_decode.flags.is_goes_into_bits_type() ? &my_workspace.enum_bit_strings_array[i].changed_mask : nullptr);
            }
        }
        bool has_contributed_to_pub = false;

        while (!has_contributed_to_pub)
        {
            my_workspace.comp_workspace->pub_lock.lock();
            if (((my_workspace.comp_workspace->pub_threads_status >> thread_id) & 1UL) == 1UL)
            {
                // this means we have "wrapped-around" and need to spin for a while -> things are backed up
                my_workspace.comp_workspace->pub_lock.unlock();
                NEW_FPS_ERROR_PRINT("warning from hardware #{}, thread #{}, pubs are getting backed up. We have wrapped around. Please consider having a higher frequency or less mappings\n", hardware_id + 1, thread_id);
                std::this_thread::sleep_for(2ms); // sleep for about 2ms before trying again
                if (!my_workspace.comp_workspace->component_connected.load(std::memory_order_acquire)) return true; // prevents spinning infinitely when one thread disconnects before it can contribute to a pub
                continue;
            }
            has_contributed_to_pub = true;
            defer { my_workspace.comp_workspace->pub_lock.unlock(); };
            ++my_workspace.comp_workspace->pub_thread_counter; // increment counter
            my_workspace.comp_workspace->pub_threads_status |= 1UL << thread_id; // set this thread to be done

            auto& send_buf = my_workspace.comp_workspace->pub_buf;
            if (my_workspace.comp_workspace->pub_thread_counter == 1) // this means this thread is the first person to get there. They can clear the buffer and start the object
            {
                send_buf.clear();
                send_buf.push_back('{');
            }

            // start the pub step:
            if (my_workspace.changed_mask.any()) // only pub on change
            {
                // send_buf.clear();
                // send_buf.push_back('{');
                for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
                {
                    if (my_workspace.changed_mask[i])
                    {
                        const auto& current_decoded_cache_val = my_workspace.decoded_cache[i];

                        if constexpr (Reg_Type == Register_Types::Coil || Reg_Type == Register_Types::Discrete_Input)
                        {
                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_decoded_cache_val.get_bool_unsafe());
                        }
                        else // Holding and Input registers (uint, int, float, "bit_strings"):
                        {
                            const auto& current_decode = my_workspace.decode_info_array[i];

                            if (!current_decode.flags.is_bit_string_type()) // normal formatting:
                            {
                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_decoded_cache_val);
                            }
                            else // format using "bit_strings":
                            {
                                const auto current_unsigned_val = current_decoded_cache_val.get_uint_unsafe(); // we only have "enum_bit_strings" for unsigned values (no scale, signed, or float)
                                auto& current_bits_changed_mask = my_workspace.enum_bit_strings_array[i].changed_mask; // this is only used for "individual_bits", "individual_enums", and "enum_field" really.
                                const auto& current_enum_bit_string_array = my_workspace.enum_bit_strings_array[i].vec;

                                // TODO(WALKER): get "ignore_mask" stuff working properly during the config phase for the below two (the "ignore" bits are a part of the "care_mask" during the first decode pass through)
                                if (current_decode.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit) and also pub the whole raw output (uses care_mask before -> change assumes at least one bit we care about has changed):
                                {
                                    // NOTE(WALKER): Uses bits_changed_mask
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val); // this is the whole individual_bits as a raw number (after basic decoding)
                                    for (const auto& bit_str : current_enum_bit_string_array)
                                    {
                                        if (((current_bits_changed_mask >> bit_str.begin_bit) & 1UL) == 1UL) // only pub this bit on change:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(bit_str.enum_bit_string), ((current_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
                                        }
                                    }
                                }
                                // TODO(WALKER): Get "changed mask for bits" working here in the future:
                                else if (current_decode.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
                                {
                                    // This does NOT use bits_changed_mask (repubs the whole thing as if it was one whole "object" (including bits that haven't actually changed))
                                    // NOTE(WALKER): In the future, change this to be like "enum_field" where we publish the whole thing as an object instead of an array (so "value" will store the whole thing)
                                    // so the final thing will be: "bit_field_name":{"value":1,"bit_strings":[{"bit":0,"value":true,"string":"some_bit_string"}, ...]} (make it like enum_field)
                                    // in the final thing pubs will be more sparse (will NOT include bits that haven't changed in the array itself)
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[)"), main_workspace.str_storage.get_str(my_workspace.ids[i]));
                                    for (const auto& bit_str : current_enum_bit_string_array)
                                    {
                                        // NOTE(WALKER): All other bits that aren't in the array by now should have been masked out by care_mask or are "Unknown" (only at the end really)
                                        // if we have "inbetween" bits that aren't labeled as "IGNORE" then something has gone wrong during the config phase (should throw a proper error)
                                        if (((current_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"{}"}},)"), 
                                                                                                bit_str.begin_bit, 
                                                                                                main_workspace.str_storage.get_str(bit_str.enum_bit_string));
                                        }
                                    }
                                    // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown":
                                    for (uint8_t last_bit = (current_enum_bit_string_array.back().end_bit + 1); last_bit < (current_decode.flags.get_size() * 16); ++last_bit)
                                    {
                                        if (((current_unsigned_val >> last_bit) & 1UL) == 1UL) // only format bits that are high:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"Unknown"}},)"), last_bit);
                                        }
                                    }
                                    send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                    send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                                }
                                // enum stuff:
                                else if (current_decode.flags.is_individual_enums()) // resolves to multiple whole number uris:
                                {
                                    // NOTE(WALKER): Uses bits_changed_mask
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val); // send out the whole raw register (all the enums in one)
                                    auto current_id = main_workspace.str_storage.get_str(current_enum_bit_string_array[0].enum_bit_string);
                                    for (uint8_t j = 1; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                    {
                                        auto& enum_str = current_enum_bit_string_array[j];
                                        const auto current_changed_bits = (current_bits_changed_mask >> enum_str.begin_bit) & enum_str.care_mask;
                                        if (current_changed_bits == 0) // No change for these bits:
                                        {
                                            j = enum_str.last_index + 1; // jump to the next sub_array
                                            current_id = j < current_enum_bit_string_array.size() ? main_workspace.str_storage.get_str(current_enum_bit_string_array[j].enum_bit_string) : ""sv; // set the current id
                                            continue;
                                        }
                                        const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                        if (current_enum_val == enum_str.enum_bit_val) // we found the enum value:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"{}"}}],)"), current_id, current_enum_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                            j = enum_str.last_index + 1; // jump to the next sub_array
                                            current_id = j < current_enum_bit_string_array.size() ? main_workspace.str_storage.get_str(current_enum_bit_string_array[j].enum_bit_string) : ""sv;
                                        }
                                        else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub array and it is NOT == our current_enum_val)
                                        {
                                            // format the current enum value for this key (non-ignored value that has changed that is "Unknown"):
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"Unknown"}}],)"), current_id, current_enum_val);
                                            ++j; // jump to the next sub_array
                                            current_id = j < current_enum_bit_string_array.size() ? main_workspace.str_storage.get_str(current_enum_bit_string_array[j].enum_bit_string) : ""sv;
                                        }
                                    }
                                }
                                else if (current_decode.flags.is_enum_field()) // resolve to object of {"value":x,"enum_strings":[...]}
                                {
                                    // NOTE(WALKER): Uses bits_changed_mask
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[)"), main_workspace.str_storage.get_str(my_workspace.ids[i]));
                                    for (uint8_t j = 0; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                    {
                                        auto& enum_str = current_enum_bit_string_array[j];
                                        const auto current_changed_bits = (current_bits_changed_mask >> enum_str.begin_bit) & enum_str.care_mask;
                                        if (current_changed_bits == 0) // No change for these bits:
                                        {
                                            j = enum_str.last_index; // jump to the next sub_array
                                            continue;
                                        }
                                        const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                        if (current_enum_val == enum_str.enum_bit_val) // we have found the enum
                                        {
                                            j = enum_str.last_index; // we have finished iterating over this sub array, go to the next one
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"{}"}},)"), 
                                                                                                        enum_str.begin_bit,
                                                                                                        enum_str.end_bit,
                                                                                                        enum_str.care_mask,
                                                                                                        current_enum_val,
                                                                                                        main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                        }
                                        else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub array and it is NOT == our current_enum_val)
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"Unknown"}},)"), 
                                                                                                        enum_str.begin_bit,
                                                                                                        enum_str.end_bit,
                                                                                                        enum_str.care_mask,
                                                                                                        current_enum_val);
                                        }
                                    }
                                    send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                    send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                                }
                                else if (current_decode.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                                {
                                    bool enum_found = false;
                                    for (const auto& enum_str : current_enum_bit_string_array)
                                    {
                                        if (current_unsigned_val == enum_str.enum_bit_val)
                                        {
                                            enum_found = true;
                                            // TODO(WALKER): In the future, change this to just be: {"value":x,"string":"some_enum_string"} and get rid of the array braces
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"{}"}}],)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                            break;
                                        }
                                    }
                                    if (!enum_found) // config did not account for this value:
                                    {
                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"Unknown"}}],)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val);
                                    }
                                }
                                // current_bits_changed_mask = 0UL; // uncomment this to have proper if changed logic
                            }
                        }
                    }
                }

                if (my_workspace.comp_workspace->pub_thread_counter == my_workspace.comp_workspace->num_threads) // this is the last thread to finish formatting to the send_buf (this thread needs to publish the message)
                {
                    my_workspace.comp_workspace->pub_heartbeat_contributed = false; // reset heartbeat contribution
                    my_workspace.comp_workspace->pub_thread_counter = 0; // reset counter
                    my_workspace.comp_workspace->pub_threads_status = 0UL; // reset threads status for wrap-around

                    const auto timestamp       = std::chrono::system_clock::now();
                    const auto timestamp_micro = spdlog::details::fmt_helper::time_fraction<std::chrono::microseconds>(timestamp);
                    fmt::format_to(std::back_inserter(send_buf), R"("Timestamp":"{:%m-%d-%Y %T}.{}"}})", timestamp, timestamp_micro.count());

                    // send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                    // send_buf.push_back('}'); // finish pub object

                    if (!fims_gateway.send_pub(component_uri_view, std::string_view{send_buf.data(), send_buf.size()}))
                    {
                        NEW_FPS_ERROR_PRINT("can't send fims pub for hardware #{} thread #{}, erroring out.\n", hardware_id + 1, thread_id);
                        return true;
                    }
                }
            }
            // my_workspace.changed_mask.reset(); // NOTE(WALKER): uncomment this line for proper if changed logic
        }

        // start checking channels until it is time to poll modbus again:
        chan_msg current_msg;
        std::size_t current_chan_size;
        std::chrono::nanoseconds time_left; // time_left until we need to poll modbus again
        do
        {
            current_chan_size = my_channel->size();
            if (current_chan_size > 0)
            {
                --current_chan_size;
                current_msg = std::move(*my_channel->front());
                my_channel->pop();

                if (current_msg.method_type == Method_Types::Set)
                {
                    if constexpr (Reg_Type == Register_Types::Holding || Reg_Type == Register_Types::Coil) // we are a type that can set values:
                    {
                        conn_lock_guard my_lock{conn_locks, conn_array, my_hardware_workspace.num_actual_conns, thread_id};

                        // do modbus stuff:
                        modbus_set_slave(*my_lock.conn, my_workspace.slave_address);
                        modbus_set_response_timeout(*my_lock.conn, 0, ((my_workspace.frequency < modbus_default_timeout) ? my_workspace.frequency.count() : modbus_default_timeout.count()) * 1000);
                        uint16_t mod_send_buf[4];
                        for (const auto& to_set : current_msg.set_info_array)
                        {
                            const auto& current_decode = my_workspace.decode_info_array[to_set.decode_id];

                            // NOTE(WALKER): The debounce feature only works for whole registers and NOT for individual_bits/individual_enums
                            // this feature only exists for the sake of working with other ess controllers that need it (which don't use individual_bits/individual_enums features -> i.e. NOT our software)
                            if (my_workspace.debounce > 0ms) // do debouce logic
                            {
                                const auto now = main_workspace.proper_steady_clock.rdns();
                                if (std::chrono::nanoseconds{now - my_workspace.debounce_last_set_times[to_set.decode_id]} < my_workspace.debounce)
                                {
                                    my_workspace.debounce_to_set_mask[to_set.decode_id] = true; // this means we need to set this value later (we have a debounced value)
                                    my_workspace.debounce_to_set_vals[to_set.decode_id] = to_set.set_val; // store value we want to set later once debounce timer goes off
                                    continue; // skip this set
                                }
                            }

                            if constexpr (Reg_Type == Register_Types::Coil)
                            {
                                if (0) { auto something_blah_blah = mod_send_buf[0]; ++something_blah_blah; }
                                // NOTE(WALKER): get_bool_unsafe() should be fine in this case for the union
                                // even though listener sets booleans as 1/0 this should still reinterept the bits properly (type punning)
                                // if for some odd reason this might be an issue then change it to get_uint_unsafe() and cast to uint8_t
                                has_errno = modbus_write_bit(*my_lock.conn, current_decode.offset, to_set.set_val.get_bool_unsafe()) == -1;
                            }
                            else if constexpr (Reg_Type == Register_Types::Holding)
                            {
                                encode(mod_send_buf, current_decode, to_set.set_val, to_set.bit_id, 
                                            current_decode.flags.is_individual_enums() ? my_workspace.enum_bit_strings_array[to_set.decode_id].vec[to_set.enum_id].care_mask : 0UL,
                                            to_set.bit_id != bit_all_index ? my_workspace.decoded_cache[to_set.decode_id].get_uint_unsafe() : 0UL);

                                has_errno = modbus_write_registers(*my_lock.conn, current_decode.offset, current_decode.flags.get_size(), mod_send_buf) == -1;
                            }
                            if (has_errno)
                            {
                                const auto current_errno = errno;
                                NEW_FPS_ERROR_PRINT("hardware #{} thread #{}, error when setting modbus registers, err = {}\n", hardware_id + 1, thread_id, modbus_strerror(current_errno));
                                if (current_errno == modbus_errno_disconnect || current_errno == modbus_errno_cant_connect)
                                {
                                    return true; // error out on disconnect or can't connect (main should attempt to restart this workspace)
                                }
                                has_errno = false; // reset errno
                            }
                            else if (my_workspace.debounce > 0ms) // no error (we set the value properly) -> make sure debounce time is reset
                            {
                                my_workspace.debounce_last_set_times[to_set.decode_id] = main_workspace.proper_steady_clock.rdns();
                                my_workspace.debounce_to_set_mask[to_set.decode_id] = false; // we set it properly, don't need to set the value again unless we get backed up
                            }
                        }
                    }
                }
                else // gets:
                {
                    // send_buf.clear();
                    if (current_msg.decode_id == decode_all_index) // we have a get "all" scenario
                    {
                        bool has_contributed_to_get = false;
                        while (!has_contributed_to_get)
                        {
                            my_workspace.comp_workspace->get_lock.lock();
                            if (((my_workspace.comp_workspace->get_threads_status >> thread_id) & 1UL) == 1UL)
                            {
                                // this means we have "wrapped-around" and need to spin for a while -> things are backed up
                                my_workspace.comp_workspace->get_lock.unlock();
                                NEW_FPS_ERROR_PRINT("warning from hardware #{}, thread #{}, gets are getting backed up. We have wrapped around. Please do NOT spam modbus client with so many get requests\n", hardware_id + 1, thread_id);
                                std::this_thread::sleep_for(2ms); // sleep for about 2ms before trying again
                                if (!my_workspace.comp_workspace->component_connected.load(std::memory_order_acquire)) return true; // prevents spinning infinitely when one thread disconnects before it can contribute to a get
                                continue;
                            }
                            has_contributed_to_get = true;
                            defer { my_workspace.comp_workspace->get_lock.unlock(); };
                            ++my_workspace.comp_workspace->get_thread_counter; // increment counter
                            my_workspace.comp_workspace->get_threads_status |= 1UL << thread_id; // set this thread to be done

                            auto& send_buf = my_workspace.comp_workspace->get_buf;
                            if (my_workspace.comp_workspace->get_thread_counter == 1) // this means this thread is the first person to get there. They can clear the buffer and start the object
                            {
                                send_buf.clear();
                                send_buf.push_back('{');
                            }
                            // send_buf.push_back('{');
                            if constexpr (Reg_Type == Register_Types::Coil || Reg_Type == Register_Types::Discrete_Input)
                            {
                                for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
                                {
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), my_workspace.decoded_cache[i].get_bool_unsafe());
                                }
                            }
                            else // Holding and Input registers (uint, int, float, "bit_strings"):
                            {
                                if (!current_msg.flags.is_raw_request()) // we have a normal request
                                {
                                    for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
                                    {
                                        const auto& current_decode = my_workspace.decode_info_array[i];
                                        const auto& current_decoded_cache_val = my_workspace.decoded_cache[i];

                                        if (!current_decode.flags.is_bit_string_type()) // normal formatting:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_decoded_cache_val);
                                        }
                                        else // format using "bit_strings":
                                        {
                                            const auto current_unsigned_val = current_decoded_cache_val.get_uint_unsafe(); // we only have "enum_bit_strings" for unsigned values (no scale, signed, or float)
                                            const auto& current_enum_bit_string_array = my_workspace.enum_bit_strings_array[i].vec;

                                            if (current_decode.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit) and also pub the whole raw output:
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val); // this is the whole individual_bits as a raw number (after basic decoding)
                                                for (const auto& bit_str : current_enum_bit_string_array)
                                                {
                                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(bit_str.enum_bit_string), ((current_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
                                                }
                                            }
                                            else if (current_decode.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == 1UL:
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[)"), main_workspace.str_storage.get_str(my_workspace.ids[i]));
                                                for (const auto& bit_str : current_enum_bit_string_array)
                                                {
                                                    // NOTE(WALKER): All other bits that aren't in the array by now should have been masked out by care_mask or are "Unknown" (only at the end really)
                                                    // if we have "inbetween" bits that aren't labeled as "IGNORE" then something has gone wrong during the config phase (should throw a proper error)
                                                    if (((current_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                                    {
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"{}"}},)"), 
                                                                                                            bit_str.begin_bit, 
                                                                                                            main_workspace.str_storage.get_str(bit_str.enum_bit_string));
                                                    }
                                                }
                                                // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                                                for (uint8_t last_bit = (current_enum_bit_string_array.back().end_bit + 1); last_bit < (current_decode.flags.get_size() * 16); ++last_bit)
                                                {
                                                    if (((current_unsigned_val >> last_bit) & 1UL) == 1UL) // only format bits that are high:
                                                    {
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"Unknown"}},)"), last_bit);
                                                    }
                                                }
                                                send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                                send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                                            }
                                            // enum stuff:
                                            else if (current_decode.flags.is_individual_enums()) // resolves to multiple whole number uris:
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":{},)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val); // send out the whole raw register (all the enums in one)
                                                auto current_id = main_workspace.str_storage.get_str(current_enum_bit_string_array[0].enum_bit_string);
                                                for (uint8_t j = 1; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                                {
                                                    auto& enum_str = current_enum_bit_string_array[j];
                                                    const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                                    if (current_enum_val == enum_str.enum_bit_val) // we found the enum value:
                                                    {
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"{}"}}],)"), current_id, current_enum_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                                        j = enum_str.last_index + 1; // jump to the next sub_array
                                                        current_id = j < current_enum_bit_string_array.size() ? main_workspace.str_storage.get_str(current_enum_bit_string_array[j].enum_bit_string) : ""sv;
                                                    }
                                                    else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub_array and it is NOT == our current_enum_val)
                                                    {
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"Unknown"}}],)"), current_id, current_enum_val);
                                                        ++j; // jump to the next sub_array
                                                        current_id = j < current_enum_bit_string_array.size() ? main_workspace.str_storage.get_str(current_enum_bit_string_array[j].enum_bit_string) : ""sv;
                                                    }
                                                }
                                            }
                                            else if (current_decode.flags.is_enum_field()) // resolve to object of {"value":x,"enum_strings":[...]}
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[)"), main_workspace.str_storage.get_str(my_workspace.ids[i])); // send out the whole raw register (all the enums in one)
                                                for (uint8_t j = 0; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                                {
                                                    auto& enum_str = current_enum_bit_string_array[j];
                                                    const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                                    if (current_enum_val == enum_str.enum_bit_val) // we have found the enum
                                                    {
                                                        j = enum_str.last_index; // we have finished iterating over this sub array, go to the next one
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"{}"}},)"), 
                                                                                                                    enum_str.begin_bit,
                                                                                                                    enum_str.end_bit,
                                                                                                                    enum_str.care_mask,
                                                                                                                    current_enum_val,
                                                                                                                    main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                                    }
                                                    else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub array and it is NOT == our current_enum_val and also hasn't been ignored)
                                                    {
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"Unknown"}},)"), 
                                                                                                                    enum_str.begin_bit,
                                                                                                                    enum_str.end_bit,
                                                                                                                    enum_str.care_mask,
                                                                                                                    current_enum_val);
                                                    }
                                                }
                                                send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                                send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                                            }
                                            else if (current_decode.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                                            {
                                                bool enum_found = false;
                                                for (const auto& enum_str : current_enum_bit_string_array)
                                                {
                                                    if (current_unsigned_val == enum_str.enum_bit_val)
                                                    {
                                                        enum_found = true;
                                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"{}"}}],)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                                        break;
                                                    }
                                                }
                                                if (!enum_found) // config did not account for this value:
                                                {
                                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{}":[{{"value":{},"string":"Unknown"}}],)"), main_workspace.str_storage.get_str(my_workspace.ids[i]), current_unsigned_val);
                                                }
                                            }
                                        }
                                    }
                                }
                                else // we have a raw request:
                                {
                                    for (uint8_t i = 0; i < my_workspace.num_decode; ++i)
                                    {
                                        const auto& current_decode = my_workspace.decode_info_array[i];
                                        auto* current_input = &my_workspace.raw_input[current_decode.offset - my_workspace.start_offset];

                                        uint64_t current_raw_val;
                                        if (current_decode.flags.is_size_one())
                                        {
                                            current_raw_val = current_input[0];
                                        }
                                        else if (current_decode.flags.is_size_two())
                                        {
                                            current_raw_val = (static_cast<uint64_t>(current_input[0]) << 16) +
                                                                (static_cast<uint64_t>(current_input[1]) << 0);
                                        }
                                        else // size four:
                                        {
                                            current_raw_val = (static_cast<uint64_t>(current_input[0]) << 48) +
                                                                (static_cast<uint64_t>(current_input[1]) << 32) +
                                                                (static_cast<uint64_t>(current_input[2]) << 16) +
                                                                (static_cast<uint64_t>(current_input[3]) << 0);
                                        }
                                        fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"("{0}":{{"offset":{1},"size":{2},"value":{3},"hex":"{3:0>{4}X}","binary":"{3:0>{5}b}"}},)"), 
                                                                                                    main_workspace.str_storage.get_str(my_workspace.ids[i]), current_decode.offset, current_decode.flags.get_size(), 
                                                                                                    current_raw_val, current_decode.flags.get_size() * 4, current_decode.flags.get_size() * 16);
                                    }
                                }
                            }
                            if (my_workspace.comp_workspace->get_thread_counter == my_workspace.comp_workspace->num_threads) // this is the last thread to finish formatting to the send_buf
                            {
                                my_workspace.comp_workspace->get_thread_counter = 0; // reset counter
                                my_workspace.comp_workspace->get_threads_status = 0UL; // reset threads status for wrap-around

                                send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                send_buf.push_back('}'); // finish pub object

                                if(!fims_gateway.send_set(current_msg.replyto, std::string_view{send_buf.data(), send_buf.size()}))
                                {
                                    NEW_FPS_ERROR_PRINT("can't send fims set to replyto uri: {} for hardware #{} thread #{}, erroring out.\n", current_msg.replyto, hardware_id + 1, thread_id);
                                    return true;
                                }
                            }
                        }
                    }
                    else // get single value only:
                    {
                        fmt::memory_buffer send_buf; // threads can have their own send buffer for this (no giant object needed)
                        if constexpr (Reg_Type == Register_Types::Coil || Reg_Type == Register_Types::Discrete_Input)
                        {
                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), my_workspace.decoded_cache[current_msg.decode_id].get_bool_unsafe());
                        }
                        else // Holding and Input registers (uint, int, float, "bit_strings"):
                        {
                            const auto& current_decode = my_workspace.decode_info_array[current_msg.decode_id];

                            if (!current_msg.flags.is_raw_request()) // we have a normal request:
                            {
                                const auto& current_decoded_cache_val = my_workspace.decoded_cache[current_msg.decode_id];

                                if (!current_decode.flags.is_bit_string_type()) // normal formatting:
                                {
                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), current_decoded_cache_val);
                                }
                                else // format using "bit_strings":
                                {
                                    const auto current_unsigned_val = current_decoded_cache_val.get_uint_unsafe(); // we only have "enum_bit_strings" for unsigned values (no scale, signed, or float)
                                    const auto& current_enum_bit_string_array = my_workspace.enum_bit_strings_array[current_msg.decode_id].vec;

                                    if (current_decode.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit) and also pub the whole raw output:
                                    {
                                        if (current_msg.bit_id == bit_all_index) // they want the whole thing
                                        {
                                            // POSSIBLE TODO(WALKER): The raw id pub might or might not need to include the binary string as a part of its pub (maybe wrap it in an object?)
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), current_unsigned_val); // this is the whole individual_bits as a raw number (after basic decoding)
                                        }
                                        else // they want the individual true/false bit (that belongs to this bit_id)
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), ((current_unsigned_val >> current_msg.bit_id) & 1UL) == 1UL); // this is the individual bit itself (true/false)
                                        }
                                    }
                                    else if (current_decode.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
                                    {
                                        send_buf.push_back('[');
                                        for (const auto& bit_str : current_enum_bit_string_array)
                                        {
                                            // NOTE(WALKER): All other bits that aren't in the array by now should have been masked out by care_mask or are "Unknown" (only at the end really)
                                            // if we have "inbetween" bits that aren't labeled as "IGNORE" then something has gone wrong during the config phase (should throw a proper error)
                                            if (((current_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"{}"}},)"), 
                                                                                                    bit_str.begin_bit, 
                                                                                                    main_workspace.str_storage.get_str(bit_str.enum_bit_string));
                                            }
                                        }
                                        // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                                        for (uint8_t last_bit = (current_enum_bit_string_array.back().end_bit + 1); last_bit < (current_decode.flags.get_size() * 16); ++last_bit)
                                        {
                                            if (((current_unsigned_val >> last_bit) & 1UL) == 1UL) // only format bits that are high:
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"value":{},"string":"Unknown"}},)"), last_bit);
                                            }
                                        }
                                        send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                        send_buf.push_back(']'); // finish the array
                                    }
                                    else if (current_decode.flags.is_individual_enums()) // resolves to multiple uris:
                                    {
                                        if (current_msg.bit_id == bit_all_index) // they want the whole thing
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE("{}"), current_unsigned_val); // send out the whole raw register (all the enums in one)
                                        }
                                        else // they want a particular enum inside the bits themselves:
                                        {
                                            // only iterate on that particular sub_array (from the beginning using enum_id):
                                            for (uint8_t j = current_msg.enum_id + 1; j < current_enum_bit_string_array[current_msg.enum_id].last_index + 1; ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                            {
                                                auto& enum_str = current_enum_bit_string_array[j];
                                                const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                                if (current_enum_val == enum_str.enum_bit_val) // we found the enum value:
                                                {
                                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"{}"}}])"), current_enum_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                                }
                                                else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub_array and it is NOT == our current_enum_val)
                                                {
                                                    fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"Unknown"}}])"), current_enum_val);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    else if (current_decode.flags.is_enum_field())
                                    {
                                        send_buf.push_back('[');
                                        for (uint8_t j = 0; j < current_enum_bit_string_array.size(); ++j) // NOTE(WALKER): Change j's type to uint16_t if we need more than 255 enum strings (unlikely)
                                        {
                                            auto& enum_str = current_enum_bit_string_array[j];
                                            const auto current_enum_val = (current_unsigned_val >> enum_str.begin_bit) & enum_str.care_mask;
                                            if (current_enum_val == enum_str.enum_bit_val) // we have found the enum
                                            {
                                                j = enum_str.last_index; // we have finished iterating over this sub array, go to the next one
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"{}"}},)"), 
                                                                                                            enum_str.begin_bit,
                                                                                                            enum_str.end_bit,
                                                                                                            enum_str.care_mask,
                                                                                                            current_enum_val,
                                                                                                            main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                            }
                                            else if (j == enum_str.last_index) // we could NOT find this enum (we reached the last value in the sub array and it is NOT == our current_enum_val and also hasn't been ignored)
                                            {
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":"Unknown"}},)"), 
                                                                                                            enum_str.begin_bit,
                                                                                                            enum_str.end_bit,
                                                                                                            enum_str.care_mask,
                                                                                                            current_enum_val);
                                            }
                                        }
                                        send_buf.resize(send_buf.size() - 1); // this gets rid of the last excessive ',' character
                                        send_buf.append(std::string_view{"],"}); // append "]," to finish the array
                                    }
                                    else if (current_decode.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                                    {
                                        bool enum_found = false;
                                        for (const auto& enum_str : current_enum_bit_string_array)
                                        {
                                            if (current_unsigned_val == enum_str.enum_bit_val)
                                            {
                                                enum_found = true;
                                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"{}"}}])"), current_unsigned_val, main_workspace.str_storage.get_str(enum_str.enum_bit_string));
                                                break;
                                            }
                                        }
                                        if (!enum_found) // config did not account for this value:
                                        {
                                            fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"([{{"value":{},"string":"Unknown"}}])"), current_unsigned_val);
                                        }
                                    }
                                }
                            }
                            else // give them the raw value back:
                            {
                                auto* current_input = &my_workspace.raw_input[current_decode.offset - my_workspace.start_offset];

                                uint64_t current_raw_val;
                                if (current_decode.flags.is_size_one())
                                {
                                    current_raw_val = current_input[0];
                                }
                                else if (current_decode.flags.is_size_two())
                                {
                                    current_raw_val = (static_cast<uint64_t>(current_input[0]) << 16) +
                                                        (static_cast<uint64_t>(current_input[1]) << 0);
                                }
                                else // size four:
                                {
                                    current_raw_val = (static_cast<uint64_t>(current_input[0]) << 48) +
                                                        (static_cast<uint64_t>(current_input[1]) << 32) +
                                                        (static_cast<uint64_t>(current_input[2]) << 16) +
                                                        (static_cast<uint64_t>(current_input[3]) << 0);
                                }
                                fmt::format_to(std::back_inserter(send_buf), FMT_COMPILE(R"({{"offset":{0},"size":{1},"value":{2},"hex":"{2:0>{3}X}","binary":"{2:0>{4}b}"}})"), 
                                                                                            current_decode.offset, current_decode.flags.get_size(), current_raw_val, 
                                                                                            current_decode.flags.get_size() * 4, current_decode.flags.get_size() * 16);
                            }
                        }
                        if(!fims_gateway.send_set(current_msg.replyto, std::string_view{send_buf.data(), send_buf.size()}))
                        {
                            NEW_FPS_ERROR_PRINT("can't send fims set to replyto uri: {} for hardware #{} thread #{}, erroring out.\n", current_msg.replyto, hardware_id + 1, thread_id);
                            return true;
                        }
                    }

                    // NOTE(WALKER): In the future this will be the way to go for fragmented pubs and get requests so for now keep this commented out

                    // if(!fims_gateway.send_set(current_msg.replyto, std::string_view{send_buf.data(), send_buf.size()}))
                    // {
                    //     NEW_FPS_ERROR_PRINT("can't send fims set to replyto uri: {} for hardware #{} thread #{}, erroring out.\n", current_msg.replyto, hardware_id + 1, thread_id);
                    //     return true;
                    // }
                }
            }

            // NOTE(WALKER): debounce stuff
            if constexpr (Reg_Type == Register_Types::Holding || Reg_Type == Register_Types::Coil) // we are a type that can set values (only these two types have debounce really):
            {
                if (my_workspace.debounce > 0ms)
                {
                    uint16_t mod_send_buf[4];
                    for (uint8_t current_debounce_index = 0; current_debounce_index < my_workspace.num_decode; ++current_debounce_index)
                    {
                        const auto now = main_workspace.proper_steady_clock.rdns();
                        if (std::chrono::nanoseconds{now - my_workspace.debounce_last_set_times[current_debounce_index]} >= my_workspace.debounce && my_workspace.debounce_to_set_mask[current_debounce_index])
                        {
                            conn_lock_guard my_lock{conn_locks, conn_array, my_hardware_workspace.num_actual_conns, thread_id};

                            // do modbus stuff:
                            modbus_set_slave(*my_lock.conn, my_workspace.slave_address);
                            modbus_set_response_timeout(*my_lock.conn, 0, ((my_workspace.frequency < modbus_default_timeout) ? my_workspace.frequency.count() : modbus_default_timeout.count()) * 1000);

                            const auto& current_decode = my_workspace.decode_info_array[current_debounce_index];
                            const auto& to_set = my_workspace.debounce_to_set_vals[current_debounce_index];

                            if constexpr (Reg_Type == Register_Types::Coil)
                            {
                                if (0) { auto something_blah_blah = mod_send_buf[0]; ++something_blah_blah; }
                                // NOTE(WALKER): get_bool_unsafe() should be fine in this case for the union
                                // even though listener sets booleans as 1/0 this should still reinterept the bits properly (type punning)
                                // if for some odd reason this might be an issue then change it to get_uint_unsafe() and cast to uint8_t
                                has_errno = modbus_write_bit(*my_lock.conn, current_decode.offset, to_set.get_bool_unsafe()) == -1;
                            }
                            else if constexpr (Reg_Type == Register_Types::Holding)
                            {
                                encode(mod_send_buf, current_decode, to_set);

                                has_errno = modbus_write_registers(*my_lock.conn, current_decode.offset, current_decode.flags.get_size(), mod_send_buf) == -1;
                            }
                            if (has_errno)
                            {
                                const auto current_errno = errno;
                                NEW_FPS_ERROR_PRINT("hardware #{} thread #{}, error when setting modbus registers, err = {}\n", hardware_id + 1, thread_id, modbus_strerror(current_errno));
                                if (current_errno == modbus_errno_disconnect || current_errno == modbus_errno_cant_connect)
                                {
                                    return true; // error out on disconnect or can't connect (main should attempt to restart this workspace)
                                }
                                has_errno = false; // reset errno
                            }
                            my_workspace.debounce_last_set_times[current_debounce_index] = main_workspace.proper_steady_clock.rdns(); // record successfully set time
                            my_workspace.debounce_to_set_mask[current_debounce_index] = false; // we have set the value (don't set it again unless we get more spam)
                        }
                    }
                }
            }

            time_left = my_workspace.frequency - std::chrono::nanoseconds{main_workspace.proper_steady_clock.rdns() - begin_work} - overshoot_time;
            std::this_thread::sleep_for(((time_left * (time_left < sleep_time)) + (sleep_time * (time_left >= sleep_time)) - 100us) * (current_chan_size == 0)); // sleep for sleep_time, time_left, or no time if we have another message to process
            time_left = my_workspace.frequency - std::chrono::nanoseconds{main_workspace.proper_steady_clock.rdns() - begin_work} - overshoot_time;
        } while (time_left > 0ns); // go until we have no time left

        overshoot_time = -time_left; // set excess time_left to overshoot time (play catchup next time)
    }

    return false;
}

// main modbus_client listener
// responsible for puting msg_info onto channels
// returns true upon error (fims error really)
bool listener_thread(const uint8_t hardware_id) noexcept
{
    // return true;

    auto& my_hardware_workspace = main_workspace.hardware_workspaces[hardware_id];
    auto& thread_workspaces = my_hardware_workspace.thread_workspaces;

    auto& fims_gateway = my_hardware_workspace.fims_gateway;
    auto& msg_recv_buf = my_hardware_workspace.msg_recv_buf;
    auto& parser = my_hardware_workspace.parser;
    auto& doc = my_hardware_workspace.doc;
    auto& thread_uri_index_map = my_hardware_workspace.thread_uri_index_map;
    const auto num_actual_threads = my_hardware_workspace.num_actual_threads;
    auto& msg_staging_area = my_hardware_workspace.msg_staging_area;

    while (true)
    {
        if (!fims_gateway.Receive_raw(fims::buffer_view{msg_recv_buf.begin(), msg_recv_buf.size()}, msg_recv_buf.size() - simdj_padding))
        {
            NEW_FPS_ERROR_PRINT("Listener for hardware #{}: Error receiving message over fims, erroring out.\n", hardware_id + 1);
            return true;
        }
        const auto msg_len = std::strlen(msg_recv_buf.data()); // does receive give you strlen automatically? These calls are annoying and time consuming
        // NEW_FPS_ERROR_PRINT("incoming message = {}\n", std::string_view{msg_recv_buf.data(), msg_len});
        // continue;
        auto err = parser.iterate(msg_recv_buf.data(), msg_len, msg_len + simdj_padding).get(doc);
        if (err)
        {
            NEW_FPS_ERROR_PRINT("could not parse an incoming message, err = {}", simdjson::error_message(err));
            continue;
        }

        // get method -> guaranteed by server
        const auto method = doc["method"].get_string().value_unsafe();
        Method_Types method_type;

        if (method == "set") // 99.99% of the time (MUST support this)
        {
            method_type = Method_Types::Set;
        }
        else if (method == "get") // this is really here for gets in the future when we have a "changed" mask
        {
            method_type = Method_Types::Get;
        }
        else // unsupported method type
        {
            NEW_FPS_ERROR_PRINT("modbus_client does not supported the method type: {}\n", method);
            continue;
        }

        // split uri and resolve to thread_id(s)
        auto uri = doc["uri"].get_string();
        if (err = uri.error(); err)
        {
            NEW_FPS_ERROR_PRINT("can't get uri of message, err = {}\n", simdjson::error_message(err));
            continue;
        }
        // explanation:
        // / c o m p o n e n  t  s  / x/y
        // 1 2 3 4 5 6 7 8 9 10 11 12 (split at 12 you are left with x/y or just x)
        auto uri_split = uri.value_unsafe().substr(component_uri_split_index);
        const auto ends_in_raw = str_ends_with(uri_split, raw_suffix);
        const auto ends_in_config = str_ends_with(uri_split, config_suffix);
        uri_split.remove_suffix(raw_suffix.size() * ends_in_raw);
        uri_split.remove_suffix(config_suffix.size() * ends_in_config);

        const auto uri_thread_info = thread_uri_index_map.find(str_view_hash(uri_split));
        if (uri_thread_info == thread_uri_index_map.end())
        {
            NEW_FPS_ERROR_PRINT("can't find uri /components/{}. Message dropped\n", uri_split);
            continue;
        }

        if (method_type == Method_Types::Set)
        {
            if (ends_in_raw || ends_in_config)
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, raw and config uris cannot be set. Message dropped\n", uri_split);
                continue;
            }

            if (uri_thread_info->second.reg_type == Register_Types::Input || uri_thread_info->second.reg_type == Register_Types::Discrete_Input)
            {
                NEW_FPS_ERROR_PRINT("for uri /components/{}, Input registers and Discrete Inputs do not accepts sets. Message dropped\n", uri_split);
                continue;
            }

            // copy over uri_split here so we don't lose it when re-parsing body:
            fmt::basic_memory_buffer<char, 75> main_frag_buf; // this is so we maintain the uri after a set (which moves the "body" string over to msg_recv_buf again to be parsed)
            main_frag_buf.append(uri_split);
            const auto uri_last_half = std::string_view{main_frag_buf.data(), main_frag_buf.size()};

            // unescape body and reuse the buffer for parsing:
            std::string_view body_view;
            auto* msg_recv_buf_begin = reinterpret_cast<uint8_t*>(msg_recv_buf.data());
            if (err = doc["body"].get_raw_json_string().unescape(msg_recv_buf_begin).get(body_view); err)
            {
                NEW_FPS_ERROR_PRINT("couldn't parse fims_msg body when trying to set (a) variable(s)/bit(s) for uri /components/{}, err = {}\n", uri_split, simdjson::error_message(err));
                continue;
            } 
            std::memset(&msg_recv_buf[body_view.size()], '\0', simdj_padding); // set padding bits of new "body" to '\0' to prevent possible errors

            if (err = parser.iterate(msg_recv_buf.begin(), body_view.size(), body_view.size() + simdj_padding).get(doc); err)
            {
                NEW_FPS_ERROR_PRINT("error when parsing body on set for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                continue;
            }

            Jval_buif to_set;
            if (uri_thread_info->second.decode_id == decode_all_index) // we have multiple uri's to resolve to (x but no y):
            {
                fmt::basic_memory_buffer<char, 100> uri_buf; // so we can store x/y as we go along for multi sets
                bool inner_json_it_error = false;
                std::bitset<max_num_threads> send_mask;

                auto msg_obj = doc.get_object();
                for (auto pair : msg_obj) // iterate over each key/value pair
                {
                    if (err = pair.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set -> getting key/value pair) for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                        inner_json_it_error = true;
                        break;
                    }
                    auto key = pair.unescaped_key();
                    if (err = key.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set -> getting key) for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                        inner_json_it_error = true;
                        break;
                    }
                    auto val = pair.value();
                    if (err = val.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set -> getting value) for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                        inner_json_it_error = true;
                        break;
                    }

                    const auto key_view = key.value_unsafe();
                    uri_buf.clear();
                    fmt::format_to(std::back_inserter(uri_buf), FMT_COMPILE("{}/{}"), uri_last_half, key_view);
                    const auto current_set_uri_view = std::string_view{uri_buf.data(), uri_buf.size()};

                    auto final_val = std::move(val);
                    auto val_clothed = final_val.get_object();
                    if (err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set -> getting clothed value as an object) for uri /components/{}, err = {}\n", current_set_uri_view, simdjson::error_message(err));
                        inner_json_it_error = true;
                        break;
                    }
                    if (!val_clothed.error())
                    {
                        auto inner_val = val_clothed.find_field("value");
                        if (err = inner_val.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("inner json error (multi set -> getting value inside clothed value object) for uri /components/{}, err = {}\n", current_set_uri_view, simdjson::error_message(err));
                            continue;
                        }
                        final_val = std::move(inner_val);
                    }

                    if (auto val_uint = final_val.get_uint64(); !val_uint.error())
                    {
                        to_set = val_uint.value_unsafe();
                    }
                    else if (auto val_int = final_val.get_int64(); !val_int.error())
                    {
                        to_set = val_int.value_unsafe();
                    }
                    else if (auto val_float = final_val.get_double(); !val_float.error())
                    {
                        to_set = val_float.value_unsafe();
                    }
                    else if (auto val_bool = final_val.get_bool(); !val_bool.error())
                    {
                        to_set = static_cast<uint64_t>(val_bool.value_unsafe()); // just set booleans equal to the whole numbers 1/0 for sets
                    }
                    else
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set -> setting a value) for uri /components/{}, only uints, ints, floats, and bools are supported, skipping that uri's set\n", current_set_uri_view);
                        continue;
                    }

                    const auto inner_uri_thread_info = thread_uri_index_map.find(str_view_hash(current_set_uri_view));
                    if (inner_uri_thread_info == thread_uri_index_map.end())
                    {
                        NEW_FPS_ERROR_PRINT("inner json error (multi set) for uri /components/{}, it doesn't exist, skipping that uri's set.\n", current_set_uri_view);
                        continue;
                    }

                    // type check for setting individual_bits (and coils):
                    if (inner_uri_thread_info->second.reg_type == Register_Types::Coil || inner_uri_thread_info->second.flags.is_individual_bit())
                    {
                        // Must contain the whole number 1 or 0 only for these registers:
                        if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL))
                        {
                            NEW_FPS_ERROR_PRINT("inner json error (multi set) for uri /components/{}, cannot set Coil or individual_bit register to the value: {}, only true/false and 1/0 are accepted, skipping that uri's set.\n", current_set_uri_view, to_set);
                            continue;
                        }
                    }
                    // type check for setting individual_enums:
                    else if (uri_thread_info->second.flags.is_individual_enum())
                    {
                        // Must contain only whole unsigned numbers for setting "individual_enums"
                        if (!to_set.holds_uint())
                        {
                            NEW_FPS_ERROR_PRINT("inner json error (multi set) for uri /components/{}, cannot set individual_enum register to the value: {}, only unsigned whole numbers are accepted, skipping that uri's set.\n", current_set_uri_view, to_set);
                            continue;
                        }
                    }

                    // set send mask bit to true (so we know which channels to send off on):
                    send_mask |= inner_uri_thread_info->second.thread_id_mask;

                    // push onto that msg in the staging area:
                    for (uint8_t i = 0; i < num_actual_threads; ++i)
                    {
                        if (inner_uri_thread_info->second.thread_id_mask[i])
                        {
                            msg_staging_area[i].method_type = Method_Types::Set;
                            msg_staging_area[i].set_info_array.emplace_back(set_info{inner_uri_thread_info->second.decode_id, inner_uri_thread_info->second.bit_id, inner_uri_thread_info->second.enum_id, to_set});
                            break;
                        }
                    }
                }
                if (inner_json_it_error) // skip sending over channels (bad incoming message -> parsing error)
                {
                    // clear each of the msg_staging_areas (their vectors)
                    for (uint8_t i = 0; i < num_actual_threads; ++i)
                    {
                        if (send_mask[i])
                        {
                            msg_staging_area[i].set_info_array.clear();
                        }
                    }
                    continue;
                }
                // send out messages on channels
                for (uint8_t i = 0; i < num_actual_threads; ++i)
                {
                    if (send_mask[i])
                    {
                        if (!thread_workspaces[i].channel->try_emplace(std::move(msg_staging_area[i])))
                        {
                            NEW_FPS_ERROR_PRINT("can't push onto channel (multi set) for hardware #{} thread #{}, skipping it. Message lost\n", hardware_id, i);
                        }
                        // clear msg in staging area:
                        msg_staging_area[i].set_info_array.clear();
                    }
                }
            }
            else // we have x/y (a single thread uri to resolve to):
            {
                // get single thread_id index
                uint8_t thread_id = 0;
                for (uint8_t i = 0; i < num_actual_threads; ++i)
                {
                    if (uri_thread_info->second.thread_id_mask[i]) // search for true
                    {
                        thread_id = i;
                        break;
                    }
                }

                decltype(doc.get_value()) final_val;
                auto val_clothed = doc.get_object();
                if (err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                {
                    NEW_FPS_ERROR_PRINT("couldn't get clothed value as an object (single set) for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                    continue;
                }
                if (!val_clothed.error()) // they sent a clothed value:
                {
                    auto inner_val = val_clothed.find_field("value");
                    if (err = inner_val.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("couldn't get clothed value inside object (single set) for uri /components/{}, err = {}\n", uri_last_half, simdjson::error_message(err));
                        continue;
                    }
                    final_val = std::move(inner_val);
                }

                // NOTE(WALKER): This logic has to be like this instead of like the multi set
                // because simdjson has a special case where if the json string itself is just a scalar
                // then you can't use doc.get_value() (will return an error code). So this is custom tailored 
                // to take that into account. Do NOT try and duplicate the multi set logic for this.
                if (auto val_uint = val_clothed.error() ? doc.get_uint64() : final_val.get_uint64(); !val_uint.error()) // it is an unsigned integer
                {
                    to_set = val_uint.value_unsafe();
                }
                else if (auto val_int = val_clothed.error() ? doc.get_int64() : final_val.get_int64(); !val_int.error()) // it is an integer
                {
                    to_set = val_int.value_unsafe();
                }
                else if (auto val_float = val_clothed.error() ? doc.get_double() : final_val.get_double(); !val_float.error()) // it is a float
                {
                    to_set = val_float.value_unsafe();
                }
                else if (auto val_bool = val_clothed.error() ? doc.get_bool() : final_val.get_bool(); !val_bool.error()) // it is a bool
                {
                    to_set = static_cast<uint64_t>(val_bool.value_unsafe()); // just set booleans to the whole numbers 1/0
                }
                else // unsupported type:
                {
                    NEW_FPS_ERROR_PRINT("for single set on uri /components/{}, only uints, ints, floats and bools are supported. Message lost\n", uri_last_half);
                    continue;
                }

                // type check for setting individual_bits (and coils):
                if (uri_thread_info->second.reg_type == Register_Types::Coil || uri_thread_info->second.flags.is_individual_bit())
                {
                    // Must contain the whole number 1 or 0 only for these registers:
                    if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL))
                    {
                        NEW_FPS_ERROR_PRINT("single set on uri /components/{}, cannot set Coil or individual_bit register to the value: {}, only true/false and 1/0 are accepted. Message lost\n", uri_last_half, to_set);
                        continue;
                    }
                }
                // type check for setting individual_enums:
                else if (uri_thread_info->second.flags.is_individual_enum())
                {
                    // Must contain only whole unsigned numbers for setting "individual_enums"
                    if (!to_set.holds_uint())
                    {
                        NEW_FPS_ERROR_PRINT("single set on uri /components/{}, cannot set individual_enum register to the value: {}, only unsigned whole numbers are accepted. Message lost\n", uri_last_half, to_set);
                        continue;
                    }
                }

                // TODO(WALKER): consider setting a variable here so that the previous message is not flushed if we can't push onto a channel
                // that way we can find a way to keep the most recent set message available and fresh.
                // Maybe flush out the previous sets, if the modbus_client is acting bad.
                msg_staging_area[thread_id].method_type = Method_Types::Set;
                msg_staging_area[thread_id].set_info_array.emplace_back(set_info{uri_thread_info->second.decode_id, uri_thread_info->second.bit_id, uri_thread_info->second.enum_id, to_set});
                // push value
                if (!thread_workspaces[thread_id].channel->try_emplace(std::move(msg_staging_area[thread_id])))
                {
                    NEW_FPS_ERROR_PRINT("can't push onto channel (single set) for uri /components/{} as it is full. Message lost.\n", uri_last_half);
                }
                // reset msg in staging area
                msg_staging_area[thread_id].set_info_array.clear();
            }
        }
        else // gets:
        {
            auto replyto = doc["replyto"].get_string();
            if (err = replyto.error(); err) // we need a replyto for gets
            {
                NEW_FPS_ERROR_PRINT("couldn't get replyto when trying to get (a) variable(s) for uri /components/{}, err = {}\n", uri_split, simdjson::error_message(err));
                continue;
            }

            if (uri_thread_info->second.decode_id == decode_all_index) // we have multiple uri's to resolve to (x but no y):
            {
                if (ends_in_config) // create a "components" object:
                {
                    fmt::basic_memory_buffer<char, 20000> config_send_buf; // big buffer for sending out entire component worth of config data
                    fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"id":"{}","registers":[)"), uri_split);
                    for (uint8_t i = 0; i < num_actual_threads; ++i)
                    {
                        if (uri_thread_info->second.thread_id_mask[i])
                        {
                            auto& thread_workspace = my_hardware_workspace.thread_workspaces[i];
                            fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"type":"{}","frequency":{},"device_id":{},"starting_offset":{},"number_of_registers":{}"map":[)"), 
                                                                                                thread_workspace.reg_type, thread_workspace.frequency.count(), thread_workspace.slave_address,
                                                                                                thread_workspace.start_offset, thread_workspace.num_registers);
                            for (uint8_t j = 0; j < thread_workspace.num_decode; ++j)
                            {
                                auto& current_decode = thread_workspace.decode_info_array[j];
                                auto& current_enum_bit_strings_arr = thread_workspace.enum_bit_strings_array[j];
                                auto& current_id = thread_workspace.ids[j];

                                fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"id":"{}","offset":{},"size":{},"scale":{},"shift":{},"invert_mask":"0x{:X}","byte_swap":{},"signed":{},"float":{},"individual_bits":{},"bit_field":{},"individual_enums":{},"enum_field":{},"enum":{},"bit_strings":[)"),
                                                                                        main_workspace.str_storage.get_str(current_id), current_decode.offset, current_decode.flags.get_size(), current_decode.scale, current_decode.shift, current_decode.invert_mask,
                                                                                        current_decode.flags.is_word_swapped(), current_decode.flags.is_signed(), current_decode.flags.is_float(), current_decode.flags.is_individual_bits(), current_decode.flags.is_bit_field(), 
                                                                                        current_decode.flags.is_individual_enums(), current_decode.flags.is_enum_field(), current_decode.flags.is_enum());
                                for (const auto& ebsi : current_enum_bit_strings_arr.vec)
                                {
                                    fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":{}}},)"), ebsi.begin_bit, ebsi.end_bit, ebsi.care_mask, ebsi.enum_bit_val, main_workspace.str_storage.get_str(ebsi.enum_bit_string));
                                }
                                config_send_buf.resize(config_send_buf.size() - 1 * (current_enum_bit_strings_arr.vec.size() > 0));
                                config_send_buf.append(std::string_view{"]},"}); // finish array and object
                            }
                            config_send_buf.resize(config_send_buf.size() - 1); // this clears out the last excessive ',' character
                            config_send_buf.append(std::string_view{"]},"}); // end the array and object
                        }
                    }
                    config_send_buf.resize(config_send_buf.size() - 1); // this clears out the last excessive ',' character
                    config_send_buf.append(std::string_view{"]}"}); // end the array and object

                    if (!fims_gateway.send_set(replyto.value_unsafe(), std::string_view{config_send_buf.data(), config_send_buf.size()}))
                    {
                        NEW_FPS_ERROR_PRINT("error sending fims set to replyto uri: {} (multi config get for uri /components/{}), erroring out\n", replyto.value_unsafe(), uri_split);
                        return true;
                    }
                    continue;
                }

                // setup messages:
                for (uint8_t i = 0; i < num_actual_threads; ++i)
                {
                    if (uri_thread_info->second.thread_id_mask[i])
                    {
                        msg_staging_area[i].method_type = Method_Types::Get;
                        msg_staging_area[i].decode_id = decode_all_index;
                        msg_staging_area[i].bit_id = bit_all_index;
                        msg_staging_area[i].enum_id = enum_all_index;
                        msg_staging_area[i].flags.set_raw_request(ends_in_raw);
                        msg_staging_area[i].replyto = replyto.value_unsafe();
                    }
                }
                // send out messages:
                for (uint8_t i = 0; i < num_actual_threads; ++i)
                {
                    if (uri_thread_info->second.thread_id_mask[i])
                    {
                        if (!thread_workspaces[i].channel->try_emplace(std::move(msg_staging_area[i])))
                        {
                            NEW_FPS_ERROR_PRINT("can't push onto channel (multi get) for uri /components/{} and thread #{} as it is full. Message lost\n", uri_split, i);
                        }
                        // reset message in staging area after sending it out:
                        msg_staging_area[i].replyto.clear();
                    }
                }
            }
            else // we have x/y (a single thread uri to resolve to):
            {
                // get single thread_id index
                uint8_t thread_id = 0;
                for (uint8_t i = 0; i < num_actual_threads; ++i)
                {
                    if (uri_thread_info->second.thread_id_mask[i]) // search for true
                    {
                        thread_id = i;
                        break;
                    }
                }

                if (ends_in_config) // go grab decode info from that particular thread workspace
                {
                    fmt::basic_memory_buffer<char, 100> config_send_buf;
                    auto& thread_workspace = my_hardware_workspace.thread_workspaces[thread_id];
                    auto& current_decode = thread_workspace.decode_info_array[uri_thread_info->second.decode_id];
                    auto& current_id = thread_workspace.ids[uri_thread_info->second.decode_id];
                    auto& current_enum_bit_strings_arr = thread_workspace.enum_bit_strings_array[uri_thread_info->second.decode_id];

                    // POSSIBLE TODO(WALKER): Reduce the size of this maybe by having if statements for all the flags (so you don't get this ginormous thing)
                    fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"id":"{}","offset":{},"size":{},"scale":{},"shift":{},"invert_mask":"0x{:X}","byte_swap":{},"signed":{},"float":{},"bit_field":{},"enum":{},"individual_bits":{},"bit_strings":[)"),
                                                                            main_workspace.str_storage.get_str(current_id), current_decode.offset, current_decode.flags.get_size(), current_decode.scale, current_decode.shift, current_decode.invert_mask,
                                                                            current_decode.flags.is_word_swapped(), current_decode.flags.is_signed(), current_decode.flags.is_float(), current_decode.flags.is_bit_field(), current_decode.flags.is_enum(), 
                                                                            current_decode.flags.is_individual_bits());
                    for (const auto& ebsi : current_enum_bit_strings_arr.vec)
                    {
                        fmt::format_to(std::back_inserter(config_send_buf), FMT_COMPILE(R"({{"begin_bit":{},"end_bit":{},"care_mask":{},"value":{},"string":{}}},)"), ebsi.begin_bit, ebsi.end_bit, ebsi.care_mask, ebsi.enum_bit_val, main_workspace.str_storage.get_str(ebsi.enum_bit_string));
                    }
                    config_send_buf.resize(config_send_buf.size() - 1 * (current_enum_bit_strings_arr.vec.size() > 0));
                    config_send_buf.append(std::string_view{"]}"}); // finish array and object

                    if (!fims_gateway.send_set(replyto.value_unsafe(), std::string_view{config_send_buf.data(), config_send_buf.size()}))
                    {
                        // couldn't sent set back to the receipient
                        NEW_FPS_ERROR_PRINT("error sending fims set to replyto uri: {} (single config get for uri /components/{}), erroring out\n", replyto.value_unsafe(), uri_split);
                        return true;
                    }
                    continue;
                }

                msg_staging_area[thread_id].method_type = Method_Types::Get;
                msg_staging_area[thread_id].decode_id = uri_thread_info->second.decode_id;
                msg_staging_area[thread_id].bit_id = uri_thread_info->second.bit_id;
                msg_staging_area[thread_id].flags.set_raw_request(ends_in_raw);
                msg_staging_area[thread_id].replyto = replyto.value_unsafe();
                if (!thread_workspaces[thread_id].channel->try_emplace(std::move(msg_staging_area[thread_id])))
                {
                    NEW_FPS_ERROR_PRINT("can't push onto channel (single get) for uri /components/{} as it is full. Message lost\n", uri_split);
                }
                // reset message in staging area
                msg_staging_area[thread_id].replyto.clear();
            }
        }
    }
    return false;
}

int main(const int argc, const char* argv[]) noexcept
{
    // check for argument count and non-null config path
    // TODO(WALKER): Get -d, -h, -e, and -f working
    // -h means help (nothing else)
    // -f means file (requires file name)
    // -d for dbi (requires file name)
    // -e means output an echo file (requires file name)
    if (argc < 2 || argv[1] == nullptr)
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("Config file argument not found. Please provide one with -f or -d (WIP do not actually say -f or -d)\n");
        return EXIT_FAILURE;
    }
    if (strlen(argv[1]) > 200) // this is for buffer overflow junk -> max config path size
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("Config file path exceeds 200 characters, do you really need a path that long?\n");
        return EXIT_FAILURE;
    }

    // load config
    // TODO(WALKER): get -e, -f, -d working here
    if (!load_config(argv[1], Config_Types::File)) // -d will pass in dbi enum here instead
    {
        return EXIT_FAILURE;
    }

    // sleep for about a second after the config phase to let our proper_steady_clock finish calibration:
    NEW_FPS_ERROR_PRINT_NO_ARGS("sleeping for roughly two seconds to finish calibrating clock shared by threads\n");
    std::this_thread::sleep_for(2s); // sleep for two seconds before doing final clock calibration
    NEW_FPS_ERROR_PRINT("finished calibrating clock, this hardware's clock frequency is: {} GHz. Beginning main program\n", main_workspace.proper_steady_clock.calibrate());

    std::array<uint8_t, max_num_hardware> retry_array; // when this reaches 5 we stop trying again for that connection (if everything is shutdown then we exit out)
    std::array<std::array<std::future<bool>, max_num_threads + 1>, max_num_hardware> future_array;
    std::array<std::array<std::future<bool>, max_num_components>, max_num_hardware> heartbeat_array; // specific futures for heartbeat threads

    // setup main worker threads, heartbeat threads, and listener thread per piece of hardware:
    for (uint8_t i = 0; i < main_workspace.num_actual_hardware; ++i)
    {
        auto& current_hardware_workspace = main_workspace.hardware_workspaces[i];
        auto num_actual_threads = current_hardware_workspace.num_actual_threads;
        auto& thread_workspaces = main_workspace.hardware_workspaces[i].thread_workspaces;
        const auto current_hardware_name = std::string_view{current_hardware_workspace.connection_info.hardware_name_buf.data(), current_hardware_workspace.connection_info.hardware_name_buf.size()};

        if (current_hardware_workspace.connection_info.is_fully_connected) // only start workspaces up if they have an actual connection
        {
            fmt::basic_memory_buffer<char, 75> event_buf;
            for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
            {
                auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                const auto component_uri_view = std::string_view{current_comp_workspace.component_uri_buf.data(), current_comp_workspace.component_uri_buf.size()};
                const auto component_name = component_uri_view.substr(component_uri_split_index);

                event_buf.clear();
                fmt::format_to(std::back_inserter(event_buf), FMT_COMPILE("{}: Modbus Connection established to device {}"), component_name, current_hardware_name);
                if (!emit_event(current_hardware_workspace.fims_gateway, "Modbus Client"sv, std::string_view{event_buf.data(), event_buf.size()}, Event_Severity::Info))
                {
                    NEW_FPS_ERROR_PRINT_NO_ARGS("main could not send fims event, erroring out\n");
                    return EXIT_FAILURE;
                }
            }
            for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
            {
                auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                current_comp_workspace.component_connected.store(true, std::memory_order_release); // consider all the components connected by default
            }
            // startup all the threads
            for (uint8_t j = 0; j < num_actual_threads; ++j)
            {
                if (thread_workspaces[j].reg_type == Register_Types::Holding)
                {
                    future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Holding>, i, j);
                }
                else if (thread_workspaces[j].reg_type == Register_Types::Input)
                {
                    future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Input>, i, j);
                }
                else if (thread_workspaces[j].reg_type == Register_Types::Coil)
                {
                    future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Coil>, i, j);
                }
                else // discrete input:
                {
                    future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Discrete_Input>, i, j);
                }
            }
            // start listener thread (last thread_id == num_actual_threads)
            future_array[i][num_actual_threads] = std::async(std::launch::async, listener_thread, i);
            // startup heartbeat threads per component if they have one (this only happens the first time -> no time after):
            for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
            {
                auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                if (current_comp_workspace.has_heartbeat)
                {
                    heartbeat_array[i][comp_idx] = std::async(std::launch::async, heartbeat_thread, i, comp_idx);
                }
            }
        }
    }

    // TODO(WALKER): Get this done properly after finishin up heartbeat and the other mandatory features

    while (true) // main sticks around checking if threads are ok
    {
        // continue; // for debugging purposes whenever you just want things to print out, comment out in the real system
        for (uint8_t i = 0; i < main_workspace.num_actual_hardware; ++i)
        {
            auto& current_hardware_workspace = main_workspace.hardware_workspaces[i];
            auto& current_hardware_conn_info = current_hardware_workspace.connection_info;
            auto& thread_workspaces = current_hardware_workspace.thread_workspaces;
            const auto num_actual_threads = current_hardware_workspace.num_actual_threads;
            const auto current_hardware_name = std::string_view{current_hardware_conn_info.hardware_name_buf.data(), current_hardware_conn_info.hardware_name_buf.size()};

            if (current_hardware_conn_info.is_fully_connected) // only check workspaces that are connected:
            {
                for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
                {
                    auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                    for (uint8_t current_thread_index = current_comp_workspace.start_thread_index; i < current_comp_workspace.start_thread_index + current_comp_workspace.num_threads; ++current_thread_index)
                    {
                        if (future_array[i][current_thread_index].wait_for(100ms) == std::future_status::ready) // threads should not be returning (error if they do)
                        {
                            current_comp_workspace.component_connected.store(false, std::memory_order_release); // tell all the threads in this component to stop (a disconnect occured somewhere)
                        }
                    }
                    if (current_comp_workspace.has_heartbeat) // check heartbeat thread if we have one:
                    {
                        if (heartbeat_array[i][comp_idx].wait_for(100ms) == std::future_status::ready) // this should NEVER happen
                        {
                            current_comp_workspace.component_connected.store(false, std::memory_order_release); // disconnect all the threads in that workspace
                        }
                    }
                }
                // check listener thread:
                if (future_array[i][num_actual_threads].wait_for(100ms) == std::future_status::ready) // this should NEVER happen
                {
                    for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
                    {
                        auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                        current_comp_workspace.component_connected.store(false, std::memory_order_release); // disconnect all the threads in all the components (and the heartbeat threads)
                        current_comp_workspace.heartbeat_thread_needs_to_stop.store(true, std::memory_order_release); // tell heartbeat thread to stop
                    }
                    for (uint8_t j = 0; j < num_actual_threads; ++j)
                    {
                        future_array[i][j].get(); // wait for all the threads to exit properly
                    }
                    hardware_ok_array[i] = false;
                }
                // TODO(WALKER): Get logic for restarting components here (and restarting the client connections):
                if (!hardware_ok_array[i])
                {

                }
                for ()
                {

                }
            }
            else // attempt to startup that workspace using its conn_info (for the first time):
            {
                // we spam the log with these messages. 
                // the retry time should be say 1 second and the log complaint every say 10 or 30 seconds.

                current_hardware_workspace.num_actual_conns = 0; // reset num conns = 0;

                // attempt to restart that hardware workspace's connections:
                for (std::size_t c = 0; c < num_actual_threads; ++c) // go for one connection per thread
                {
                    if (c >= max_num_conns) break; // cannot exceed max_num_conns for threads

                    mod_conn_raii current_conn;
                    auto connected = -1;
                    if (current_hardware_conn_info.conn_type == Conn_Types::TCP)
                    {
                        current_conn.conn = modbus_new_tcp(current_hardware_conn_info.ip_serial_buf.data(), current_hardware_conn_info.port);
                    }
                    else // RTU:
                    {
                        current_conn.conn = modbus_new_rtu(current_hardware_conn_info.ip_serial_buf.data(), 
                                                            current_hardware_conn_info.baud_rate, 
                                                            current_hardware_conn_info.parity, 
                                                            current_hardware_conn_info.data_bits,
                                                            current_hardware_conn_info.stop_bits);
                    }
                    connected = modbus_connect(current_conn);
                    if (connected == -1 && c == 0) // we failed on first connection for this piece of hardware (can't connect for that hardware)
                    {
                        if (current_hardware_conn_info.conn_type == Conn_Types::TCP)
                        {
                            NEW_FPS_ERROR_PRINT("could not re-establish TCP connection to modbus server for hardware #{} (name: {}), on ip: {}, with port: {}, err = {}\n", 
                                i + 1, 
                                current_hardware_name,
                                current_hardware_conn_info.ip_serial_buf.data(), 
                                current_hardware_conn_info.port, 
                                modbus_strerror(errno));
                        }
                        else // RTU:
                        {
                            NEW_FPS_ERROR_PRINT("could not re-establish serial RTU connection to modbus server for hardware #{} (name: {}), with serial_device: {}, baud_rate: {}, parity: {}, data_bits: {}, and stop_bits: {}, err = {}\n", 
                                i + 1, 
                                current_hardware_name,
                                current_hardware_conn_info.ip_serial_buf.data(), 
                                current_hardware_conn_info.baud_rate, 
                                current_hardware_conn_info.parity, 
                                current_hardware_conn_info.data_bits,
                                current_hardware_conn_info.stop_bits, 
                                modbus_strerror(errno));
                        }
                        break;
                    }
                    if (connected == -1) // reached our limit (we have a limited number of connections for this piece of hardware):
                    {
                        break;
                    }
                    // successful connection (add it to the array):
                    modbus_set_error_recovery(current_conn, MODBUS_ERROR_RECOVERY_PROTOCOL);
                    ++current_hardware_workspace.num_actual_conns;
                    current_hardware_workspace.conn_array[c].conn = current_conn.conn;
                    current_conn.conn = nullptr;
                    current_hardware_conn_info.is_fully_connected = true; // this hardware workspace is now connected properly for the first time
                }

                // if we could establish a connection for the first time then startup the worksapce properly:
                if (current_hardware_conn_info.is_fully_connected)
                {
                    fmt::basic_memory_buffer<char, 75> event_buf;
                    for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
                    {
                        auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                        const auto component_uri_view = std::string_view{current_comp_workspace.component_uri_buf.data(), current_comp_workspace.component_uri_buf.size()};
                        const auto component_name = component_uri_view.substr(component_uri_split_index);

                        event_buf.clear();
                        fmt::format_to(std::back_inserter(event_buf), FMT_COMPILE("{}: Modbus Connection established to device {}"), component_name, current_hardware_name);
                        if (!emit_event(current_hardware_workspace.fims_gateway, "Modbus Client"sv, std::string_view{event_buf.data(), event_buf.size()}, Event_Severity::Info))
                        {
                            NEW_FPS_ERROR_PRINT_NO_ARGS("main could not send fims event, erroring out\n");
                            return EXIT_FAILURE;
                        }
                    }
                    hardware_ok_array[i] = true; // set this piece of hardware to be ok by default
                    for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
                    {
                        auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                        current_comp_workspace.component_connected.store(true, std::memory_order_release); // consider all the components connected by default
                    }
                    for (uint8_t j = 0; j < num_actual_threads; ++j)
                    {
                        if (thread_workspaces[j].reg_type == Register_Types::Holding)
                        {
                            future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Holding>, i, j);
                        }
                        else if (thread_workspaces[j].reg_type == Register_Types::Input)
                        {
                            future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Input>, i, j);
                        }
                        else if (thread_workspaces[j].reg_type == Register_Types::Coil)
                        {
                            future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Coil>, i, j);
                        }
                        else // discrete input:
                        {
                            future_array[i][j] = std::async(std::launch::async, worker_thread<Register_Types::Discrete_Input>, i, j);
                        }
                    }
                    // start listener thread (last thread_id == num_actual_threads)
                    future_array[i][num_actual_threads] = std::async(std::launch::async, listener_thread, i);
                    // startup heartbeat threads if this workspace has them:
                    for (uint8_t comp_idx = 0; comp_idx < current_hardware_workspace.num_actual_components; ++comp_idx)
                    {
                        auto& current_comp_workspace = current_hardware_workspace.component_workspaces[comp_idx];
                        if (current_comp_workspace.has_heartbeat)
                        {
                            heartbeat_array[i][comp_idx] = std::async(std::launch::async, heartbeat_thread, i, comp_idx);
                        }
                    }
                }
            }
        }
    }
}
