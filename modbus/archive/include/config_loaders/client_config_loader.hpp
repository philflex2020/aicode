#pragma once

#include <string>
#include <vector>
#include <limits>
#include <map>

#include "modbus/modbus.h"

#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/bundled/ranges.h"

#include "simdjson_noexcept.hpp"
#include "config_loaders/decode_config_loader.hpp"
#include "config_loaders/error_location.hpp"
#include "shared_utils.hpp"
#include "string_storage_v2.hpp"

#include "fims/fims.h"

// helper macros for error messages in modbus client:
#define NEW_FPS_ERROR_PRINT(fmt_str, ...)            fmt::print(stderr, FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define NEW_FPS_ERROR_PRINT_NO_COMPILE(fmt_str, ...) fmt::print(stderr, fmt_str, ##__VA_ARGS__) 
#define NEW_FPS_ERROR_PRINT_NO_ARGS(fmt_str)         fmt::print(stderr, FMT_COMPILE(fmt_str))

// constants:
static constexpr u8   Max_Num_Conns        = std::numeric_limits<u8>::max(); // aka: "Max_Num_Contexts"
static constexpr u8   Max_Num_Clients      = std::numeric_limits<u8>::max(); // TODO(WALKER): get max num clients from fims library directly instead of using u8 max (requires some library changes -> unused for now, MAX_CONNECTIONS)
static constexpr u8   Max_Num_Components   = std::size_t{MAX_SUBSCRIPTIONS} > std::numeric_limits<u8>::max() ? std::numeric_limits<u8>::max() : static_cast<u8>(MAX_SUBSCRIPTIONS);
static constexpr auto Max_Uri_Length       = std::numeric_limits<u8>::max();
static constexpr auto Main_Uri_Frag        = std::string_view{"/components/"};
static constexpr auto Main_Uri_Frag_Length = Main_Uri_Frag.size();
// runtime constants:
static constexpr u8 Max_Num_Threads    = std::numeric_limits<u8>::max();
static constexpr u8 Max_Num_Decode     = MODBUS_MAX_READ_REGISTERS;
// special indices for indication of all/multi:
static constexpr u8 Component_All_Idx  = std::numeric_limits<u8>::max();
static constexpr u8 Thread_All_Idx     = std::numeric_limits<u8>::max();
static constexpr u8 Decode_All_Idx     = std::numeric_limits<u8>::max();

namespace config_loader
{

static int hostname_to_ip(std::string_view hostname , char* ip, int iplen)
{
    struct hostent *he;
    struct in_addr **addr_list;

    if (he = gethostbyname(hostname.data()); he == NULL) 
    {
        // get the host info
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;
        
    for(int i = 0; addr_list[i] != NULL; i++) 
    {
        //Return the first one;
        strncpy(ip , inet_ntoa(*addr_list[i]), iplen);
        return 0;
    }
        
    return 1;
}

static int service_to_port(std::string_view service, u64& port)
{
    struct servent *serv;

    /* getservbyname() - opens the etc.services file and returns the */
    /* values for the requested service and protocol.                */

    serv = getservbyname(service.data(), "tcp");
    if (serv == NULL) 
    {
        NEW_FPS_ERROR_PRINT("port cannot be derived from service [{}] for protocol [tcp] (it doesn't exist), going back to port provided in config (or default)\n", service);
        return 1;
    }

    port = ntohs(serv->s_port);
    return 0;
}

// helper functions shared between structs:
static bool check_device_id(const u64 device_id, Error_Location& err_loc)
{
    if (device_id > 255)
    {
        err_loc.err_msg = fmt::format("device_id (currently: {}) must be between 0 and 255", device_id);
        return false;
    }
    return true;
}
static bool check_debounce(const s64 debounce, Error_Location& err_loc)
{
    if (debounce < 0)
    {
        err_loc.err_msg = fmt::format("debounce (currently: {}) must be greater than or equal to 0", debounce);
        return false;
    }
    return true;
}

struct Connection
{
    std::string name;

    // TCP stuff:
    std::string ip_address;
    std::string service;
    u64 port = 502;

    // RTU stuff:
    std::string serial_device;
    u64 baud_rate = 115200;
    std::string parity_str = "none";
    u64 data_bits = 8;
    u64 stop_bits = 1;

    // max_num_conns ("contexts" -> ctx_pool) stuff:
    u64 max_num_conns = 1;

    // inheritable stuff:
    bool off_by_one = false;
    bool word_swap = false; // "byte_swap" for key (bad terminology)
    bool multi_write_op_code = false; // this is a stupid flag, but some vendors are just as stupid so what can you do, eh.
    s64 frequency = 0;
    u64 device_id = 255; // aka: "slave_address"
    s64 debounce = 0;
    u64 connection_timeout = 2;

    // derived information:
    Conn_Types conn_type;
    char parity = 'N';

    bool load(simdjson::ondemand::object& connection, Error_Location& err_loc)
    {
        if (!get_val(connection, "name", name, err_loc, Get_Type::Mandatory)) return false;
        if (!check_str_for_error(name, err_loc, R"({}\/"%)")) return false; // name can have ' ' in it

        // TCP stuff:
        if (!get_val(connection, "ip_address", ip_address, err_loc, Get_Type::Optional)) return false;
        if (!ip_address.empty()) // service file override (if name for ip -> like "localhost")
        {
            char new_ip[HOST_NAME_MAX + 1];
            new_ip[HOST_NAME_MAX] = '\0';
            auto ret = hostname_to_ip(ip_address , new_ip, HOST_NAME_MAX);
            if (ret == 0)
            {
               ip_address = new_ip;
            }
            else
            {
                err_loc.err_msg = fmt::format(R"(ip_address "{}" isn't valid or can't be found from the local service file)", ip_address);
                return false;
            } 
        }
        if (!ip_address.empty() && !check_str_for_error(ip_address, err_loc)) return false;

        if (!get_val(connection, "connection_timeout", connection_timeout, err_loc, Get_Type::Optional)) return false;
        if (!check_connection_timeout(err_loc)) return false;

        // we still need a port at the moment but we may use a service.
        if (!get_val(connection, "port", port, err_loc, Get_Type::Optional)) return false;
        if (!check_port(err_loc)) return false;

        if (!get_val(connection, "service", service, err_loc, Get_Type::Optional)) return false;
        if (!service.empty()) { service_to_port(service, port); }

        // RTU stuff:
        if (!get_val(connection, "serial_device", serial_device, err_loc, Get_Type::Optional)) return false;
        if (!serial_device.empty() && !check_str_for_error(serial_device, err_loc, "")) return false;

        if (!check_ip_serial(err_loc)) return false;

        if (!get_val(connection, "baud_rate", baud_rate, err_loc, Get_Type::Optional)) return false;
        if (!check_baud_rate(err_loc)) return false;

        if (!get_val(connection, "parity", parity_str, err_loc, Get_Type::Optional)) return false;
        if (!check_parity(err_loc)) return false;

        if (!get_val(connection, "data_bits", data_bits, err_loc, Get_Type::Optional)) return false;
        if (!check_data_bits(err_loc)) return false;

        if (!get_val(connection, "stop_bits", stop_bits, err_loc, Get_Type::Optional)) return false;
        if (!check_stop_bits(err_loc)) return false;

        // max_num_conns stuff:
        if (!get_val(connection, "max_num_connections", max_num_conns, err_loc, Get_Type::Optional)) return false;
        if (!check_max_num_conns(err_loc)) return false;

        // inheritance stuff:
        if (!get_val(connection, "off_by_one", off_by_one, err_loc, Get_Type::Optional)) return false;
        if (!get_val(connection, "byte_swap", word_swap, err_loc, Get_Type::Optional)) return false;
        if (!get_val(connection, "multi_write_op_code", multi_write_op_code, err_loc, Get_Type::Optional)) return false;
        if (!get_val(connection, "frequency", frequency, err_loc, Get_Type::Optional)) return false;

        if (!get_val(connection, "device_id", device_id, err_loc, Get_Type::Optional)) return false;
        if (!check_device_id(device_id, err_loc)) return false;

        if (!get_val(connection, "debounce", debounce, err_loc, Get_Type::Optional)) return false;
        if (!check_debounce(debounce, err_loc)) return false;

        return true;
    }

    bool check_ip_serial(Error_Location& err_loc)
    {
        if (ip_address.empty() && serial_device.empty())
        {
            err_loc.err_msg = "neither ip_address (TCP) nor serial_device (RTU) were provided";
            return false;
        }
        if (!ip_address.empty() && !serial_device.empty())
        {
            err_loc.err_msg = "only one between ip_address (TCP) and serial_device (RTU) can be provided";
            return false;
        }
        if (!ip_address.empty()) conn_type = Conn_Types::TCP;
        if (!serial_device.empty()) conn_type = Conn_Types::RTU;
        return true;
    }

    bool check_port(Error_Location& err_loc)
    {
        if (port > std::numeric_limits<u16>::max())
        {
            err_loc.err_msg = fmt::format("port (currently: {}) must be between 0 and {}", port, std::numeric_limits<u16>::max());
            return false;
        }
        return true;
    }

    bool check_baud_rate(Error_Location& err_loc)
    {
        // TODO(WALKER): What is a good check for baud_rate?
        if (!baud_rate)
        {
            err_loc.err_msg = "baud_rate must be greater than 0";
            return false;
        }
        return true;
    }

    bool check_connection_timeout(Error_Location& err_loc)
    {
        // TODO(WALKER): What is a good check for baud_rate?
        if (connection_timeout< 2 || connection_timeout > 10)
        {
            err_loc.err_msg = fmt::format(R"(connection_timeout  (currently: \"{}\") incorrect reset to 2 seconds)", connection_timeout);
            connection_timeout = 2;
        }
        return true;
    }

    bool check_parity(Error_Location& err_loc)
    {
        if (parity_str == "none") { parity = 'N'; return true; }
        if (parity_str == "even") { parity = 'E'; return true; }
        if (parity_str == "odd")  { parity = 'O'; return true; }
        err_loc.err_msg = fmt::format(R"(parity (currently: \"{}\") must be one of "none", "even", or "odd")", parity_str);
        return false;
    }

    bool check_data_bits(Error_Location& err_loc)
    {
        if (data_bits < 5 || data_bits > 8)
        {
            err_loc.err_msg = fmt::format("data_bits (currently: {}) must be between 5 and 8", data_bits);
            return false;
        }
        return true;
    }

    bool check_stop_bits(Error_Location& err_loc)
    {
        if (!(stop_bits == 1 || stop_bits == 2))
        {
            err_loc.err_msg = fmt::format("stop_bits (currently: {}) must be 1 or 2", stop_bits);
            return false;
        }
        return true;
    }

    bool check_max_num_conns(Error_Location& err_loc)
    {
        if (max_num_conns == 0)
        {
            err_loc.err_msg = fmt::format("max_num_connections cannot be 0");
            return false;
        }
        if (max_num_conns > Max_Num_Conns)
        {
            err_loc.err_msg = fmt::format("max_num_connections (currently: {}, default: 255) exceeds the maximum of {}", max_num_conns, Max_Num_Conns);
            return false;
        }
        return true;
    }
};

struct Registers_Mapping
{
    std::string type;
    std::vector<Decode> map;

    // derived information:
    Register_Types reg_type;
    u8 num_registers = 0; // no more than MODBUS_MAX_READ_REGISTERS (currently 125)
    u16 start_offset = 0;
    u8 num_bit_str_arrays = 0;

    bool load(simdjson::ondemand::object& reg_mapping, Error_Location& err_loc, bool off_by_one, bool multi_write_op_code)
    {
        bool is_coil_or_discrete_input = false;

        if (!get_val(reg_mapping, "type", type, err_loc, Get_Type::Mandatory)) return false;
        if (!check_type(is_coil_or_discrete_input, err_loc)) return false;
        err_loc.register_type = type;

        simdjson::ondemand::array decode_arr;
        if (!get_val(reg_mapping, "map", decode_arr, err_loc, Get_Type::Mandatory)) return false;

        for (auto decode_obj : decode_arr)
        {
            simdjson::ondemand::object decode;
            if (!get_array_val(decode_obj, decode, err_loc)) return false;

            bool packed_register = false;
            get_val(decode, "packed_register", packed_register, err_loc, Get_Type::Optional);
            if (packed_register)
            {
                // As this is a packed register, the offset will be required at this level of configuration,
                // and additional fields can be provided as well. These fields will then be distributed to
                // each of the child registers
                Decode parent_config;
                parent_config.packed_register = true;
                get_val(decode, "offset", parent_config.offset, err_loc, Get_Type::Mandatory);
                get_val(decode, "enum", parent_config.Enum, err_loc, Get_Type::Optional);
                get_val(decode, "random_enum", parent_config.random_enum, err_loc, Get_Type::Optional);
                get_val(decode, "bit_field", parent_config.bit_field, err_loc, Get_Type::Optional);
                get_val(decode, "individual_bits", parent_config.individual_bits, err_loc, Get_Type::Optional);
                get_val(decode, "size", parent_config.size, err_loc, Get_Type::Optional);

                simdjson::ondemand::array bit_ranges;
                if (!get_val(decode, "bit_ranges", bit_ranges, err_loc, Get_Type::Mandatory)) return false;
                // Configure each of the packed_registers as a unique entry in the map with the same offset
                for (auto bit_register : bit_ranges)
                {
                    simdjson::ondemand::object decode_register;
                    if (!get_array_val(bit_register, decode_register, err_loc)) return false;

                    auto& current_decode = map.emplace_back();
                    // inheritance stuff:
                    current_decode.multi_write_op_code = multi_write_op_code;
                    if (!current_decode.load(decode_register, err_loc, is_coil_or_discrete_input, off_by_one, false, &parent_config)) return false;
                }
            }
            else
            {
                auto& current_decode = map.emplace_back();
                // inheritance stuff:
                current_decode.multi_write_op_code = multi_write_op_code;
                if (!current_decode.load(decode, err_loc, is_coil_or_discrete_input, off_by_one)) return false;
            }

            ++err_loc.decode_idx;
        }
        if (!check_map(err_loc)) return false;
        count_bit_str_arrays();

        err_loc.decode_idx = 0;
        return true;
    }

    bool check_type(bool& is_coil_or_discrete_input, Error_Location& err_loc)
    {
        if (type == "Holding" || type == "Holding Registers") { reg_type = Register_Types::Holding; return true; }
        if (type == "Input" || type == "Input Registers") { reg_type = Register_Types::Input; return true; }
        if (type == "Coil" || type == "Coils") { reg_type = Register_Types::Coil; is_coil_or_discrete_input = true; return true; }
        if (type == "Discrete Input" || type == "Discrete Inputs") { reg_type = Register_Types::Discrete_Input; is_coil_or_discrete_input = true; return true; }

        err_loc.err_msg = fmt::format(R"(type (currently: {}) must one of "Holding"/"Holding Registers", "Input"/"Input Registers", "Coil"/"Coils", "Discrete Input"/"Discrete Inputs")", type);
        return false;
    }

    // checks validity of map (no overlapping offsets) and calculates the minimum offset + number of registers to poll
    bool check_map(Error_Location& err_loc)
    {
        if (map.empty())
        {
            err_loc.err_msg = fmt::format("map array is empty");
            return false;
        }

        err_loc.key = "";
        err_loc.type = Required_Types::None;

        std::sort(map.begin(), map.end());
        int prev_offset = -1;
        int prev_size = 1;
        std::string_view prev_id = "";

        // size check:
        if (map.size() > MODBUS_MAX_READ_REGISTERS) // MODBUS_MAX_READ_REGISTERS == 125
        {
            err_loc.err_msg = fmt::format("number of decode mappings (currently: {}) exceeds the max number of mappings (currently: {})", map.size(), MODBUS_MAX_READ_REGISTERS);
            return false;
        }

        for (const auto& decode : map)
        {
            if (!decode.packed_register && (prev_offset + prev_size - 1) >= static_cast<int>(decode.offset)) // overlap check
            {
                err_loc.err_msg = fmt::format("decode object \"{}\" (offset: {}, size: {}) overlaps with decode object \"{}\" (offset: {}, size: {})", prev_id, prev_offset, prev_size, decode.id, decode.offset, decode.size);
                return false;
            }
            prev_offset = static_cast<int>(decode.offset);
            prev_size = static_cast<int>(decode.size);
            prev_id = decode.id;
        }

        const auto num_regs = (map.back().offset + map.back().size - 1UL) - map.front().offset + 1UL;
        if (num_regs > MODBUS_MAX_READ_REGISTERS) // MODBUS_MAX_READ_REGISTERS == 125
        {
            err_loc.err_msg = fmt::format("number of registers for mapping (currently: {}) exceeds the maximum number (currently: {})", num_regs, MODBUS_MAX_READ_REGISTERS);
            return false;
        }

        start_offset = static_cast<u16>(map.front().offset);
        num_registers = static_cast<u8>(num_regs);
        return true;
    }

    void count_bit_str_arrays()
    {
        for (const auto& decode : map)
        {
            if (decode.has_bit_strings) ++ num_bit_str_arrays;
        }
    }
};

struct Component
{
    static constexpr auto Max_Num_Registers = std::numeric_limits<u8>::max(); // the maximum number of threads for a single component (very generous)

    std::string id;

    // inheritance stuff:
    bool off_by_one = false; // subtracts 1 from all offsets for this component if true
    bool word_swap = false; // "byte_swap" for key (bad terminology)
    bool multi_write_op_code = false; // dumb flag this is needed because vendors are dumb
    s64 frequency = 0;
    u64 device_id = 255;
    s64 debounce = 0;

    // heartbeat stuff:
    bool heartbeat_enabled = false;
    std::string heartbeat_read_id;              // "component_heartbeat_read_uri"
    std::string heartbeat_write_id;             // "component_heartbeat_write_uri"
    // s64 heartbeat_check_frequency = 500;        // "modbus_heartbeat_timeout_ms" (might remove this -> no point in checking faster than poll frequency or heartbeat_timeout)
    s64 heartbeat_timeout = 0;                  // "component_heartbeat_timeout_ms"

    std::vector<Registers_Mapping> registers;

    // derived info (might change this in the future):
    u8 heartbeat_read_reg_idx = Thread_All_Idx;
    u8 heartbeat_read_decode_idx = Decode_All_Idx;
    u8 heartbeat_write_reg_idx = Thread_All_Idx;
    u8 heartbeat_write_decode_idx = Decode_All_Idx;

    bool load(simdjson::ondemand::object& component, Error_Location& err_loc)
    {
        std::unordered_map<std::string_view, bool> taken_map; // to determine which ids have already been taken within this component

        if (!get_val(component, "id", id, err_loc, Get_Type::Mandatory)) return false;
        if (!check_str_for_error(id, err_loc)) return false;
        err_loc.component_id = id;

        // inheritance stuff:
        if (!get_val(component, "off_by_one", off_by_one, err_loc, Get_Type::Optional)) return false;
        if (!get_val(component, "byte_swap", word_swap, err_loc, Get_Type::Optional)) return false;
        if (!get_val(component, "multi_write_op_code", multi_write_op_code, err_loc, Get_Type::Optional)) return false;

        if (!get_val(component, "frequency", frequency, err_loc, Get_Type::Optional)) return false;
        if (!check_frequency(err_loc)) return false;

        if (!get_val(component, "device_id", device_id, err_loc, Get_Type::Optional)) return false;
        if (!check_device_id(device_id, err_loc)) return false;

        if (!get_val(component, "debounce", debounce, err_loc, Get_Type::Optional)) return false;
        if (!check_debounce(debounce, err_loc)) return false;

        // heartbeat stuff:
        if (!get_val(component, "heartbeat_enabled", heartbeat_enabled, err_loc, Get_Type::Optional)) return false;

        if (heartbeat_enabled)
        {
            if (!get_val(component, "component_heartbeat_read_uri", heartbeat_read_id, err_loc, Get_Type::Mandatory)) return false;
            if (!check_heartbeat_read_id(err_loc)) return false;

            if (!get_val(component, "component_heartbeat_write_uri", heartbeat_write_id, err_loc, Get_Type::Optional)) return false;
            if (!check_heartbeat_write_id(err_loc)) return false;

            // if (!get_val(component, "modbus_heartbeat_timeout_ms", heartbeat_check_frequency, err_loc, Get_Type::Optional)) return false;
            // if (!check_heartbeat_check_frequency(err_loc)) return false;

            heartbeat_timeout = frequency * 2; // default heartbeat_timeout to be 2x frequency
            if (!get_val(component, "component_heartbeat_timeout_ms", heartbeat_timeout, err_loc, Get_Type::Optional)) return false;
            if (!check_heartbeat_timeout(err_loc)) return false;
        }

        // registers array stuff:
        simdjson::ondemand::array reg_arr;
        if (!get_val(component, "registers", reg_arr, err_loc, Get_Type::Mandatory)) return false;

        for (auto reg_map : reg_arr)
        {
            simdjson::ondemand::object reg_obj;
            if (!get_array_val(reg_map, reg_obj, err_loc)) return false;

            registers.emplace_back();
            if (!registers.back().load(reg_obj, err_loc, off_by_one, multi_write_op_code)) return false;

            ++err_loc.register_idx;
        }
        err_loc.register_idx = 0;

        if (!check_registers(err_loc)) return false;
        if (heartbeat_enabled && !check_registers_for_heartbeat(err_loc)) return false;

        return true;
    }

    bool check_frequency(Error_Location& err_loc)
    {
        if (frequency <= 0) 
        {
            err_loc.err_msg = fmt::format("frequency (currently: {}) must be greater than 0, and must be provided/overriden in each component object or inherited from the connection object", frequency);
            return false;
        }
        return true;
    }

    bool check_heartbeat_read_id(Error_Location& err_loc)
    {
        return check_str_for_error(heartbeat_read_id, err_loc);
    }

    bool check_heartbeat_write_id(Error_Location& err_loc)
    {
        if (!heartbeat_write_id.empty()) return check_str_for_error(heartbeat_write_id, err_loc);
        return true;
    }

    // bool check_heartbeat_check_frequency(Error_Location& err_loc)
    // {
    //     if (heartbeat_check_frequency <= 0)
    //     {
    //         err_loc.err_msg = fmt::format("modbus_heartbeat_timeout_ms (currently: {}, default: 500) must be greater than 0", heartbeat_check_frequency);
    //         return false;
    //     }
    //     return true;
    // }

    bool check_heartbeat_timeout(Error_Location& err_loc)
    {
        if (heartbeat_timeout <= 0)
        {
            err_loc.err_msg = fmt::format("component_heartbeat_timeout_ms (currently: {}, default: 2x frequency) must be greater than 0", heartbeat_timeout);
            return false;
        }
        // if (heartbeat_timeout < heartbeat_check_frequency)
        // {
        //     err_loc.err_msg = fmt::format("component_heartbeat_timeout_ms (currently: {}, default: 2x frequency) must be greater than or equal to modbus_heartbeat_timeout_ms (currently: {})", heartbeat_timeout, heartbeat_check_frequency);
        //     return false;
        // }
        if (heartbeat_timeout < frequency)
        {
            err_loc.err_msg = fmt::format("component_heartbeat_timeout_ms (currently: {}, default: 2x frequency) must be greater than or equal to frequency (currently: {})", heartbeat_timeout, frequency);
            return false;
        }
        return true;
    }

    bool check_registers(Error_Location& err_loc)
    {
        if (registers.empty())
        {
            err_loc.err_msg = fmt::format("registers array is empty");
            return false;
        }

        std::unordered_map<std::string_view, bool> taken_map;
        err_loc.key = "";
        err_loc.type = Required_Types::None;

        // size check:
        if (registers.size() > Max_Num_Registers) // at most 255 threads/mappings per component (that is a reasonable standard)
        {
            err_loc.err_msg = fmt::format("The number of registers mappings (currently: {}) for this component has exceeded the maximum of {}", registers.size(), Max_Num_Registers);
            return false;
        }

        err_loc.register_idx = 0;
        err_loc.decode_idx = 0;
        err_loc.bit_strings_idx = 0;

        for (const auto& reg : registers)
        {
            err_loc.register_type = reg.type;
            for (const auto& decode : reg.map)
            {
                err_loc.decode_id = decode.id;
                if (decode.individual_bits) // resolve uris based on bit_strings:
                {
                    for (const auto& bit_str : decode.bit_strings)
                    {
                        if (!bit_str.is_unknown && !bit_str.num_ignored && taken_map[bit_str.id])
                        {
                            err_loc.err_msg = fmt::format("individual_bit id \"{}\" has already been mapped within this component", bit_str.id);
                            return false;
                        }
                        taken_map[bit_str.id] = true; // claim individual_bit id
                        ++err_loc.bit_strings_idx;
                    }
                    err_loc.bit_strings_idx = 0;
                }
                else // regular register (no more _all stuff for the new one, not unless someone wants it in the future):
                {
                    if (taken_map[decode.id])
                    {
                        err_loc.err_msg = fmt::format("decode id \"{}\" has already been mapped within this component", decode.id);
                        return false;
                    }
                    taken_map[decode.id] = true; // claim id
                }
                ++err_loc.decode_idx;
            }
            err_loc.decode_idx = 0;
            ++err_loc.register_idx;
        }

        return true;
    }

    // POSSIBLE TODO(WALKER): does this need to check that the registers are only of unsigned integer type? With no special flags?
    bool check_registers_for_heartbeat(Error_Location& err_loc)
    {
        bool read_found = false;
        bool write_found = false;
        err_loc.register_idx = 0;
        err_loc.decode_idx = 0;
        err_loc.bit_strings_idx = 0;
        err_loc.key = "";
        err_loc.type = Required_Types::None;

        for (const auto& reg : registers)
        {
            err_loc.register_type = reg.type;
            for (const auto& decode : reg.map)
            {
                err_loc.decode_id = decode.id;
                if (decode.id == heartbeat_read_id)
                {
                    if (!(reg.reg_type == Register_Types::Holding || reg.reg_type == Register_Types::Input))
                    {
                        err_loc.err_msg = fmt::format("component_heartbeat_read_uri (currently: \"{}\") was found, but register is of type \"{}\" instead of \"Holding\" or \"Input\"", heartbeat_read_id, reg.type);
                        return false;
                    }
                    heartbeat_read_reg_idx = static_cast<u8>(err_loc.register_idx);
                    heartbeat_read_decode_idx = static_cast<u8>(err_loc.decode_idx);
                    read_found = true;
                }
                if (decode.id == heartbeat_write_id)
                {
                    if (reg.reg_type != Register_Types::Holding)
                    {
                        err_loc.err_msg = fmt::format("component_heartbeat_write_uri (currently: \"{}\") was found, but register is of type \"{}\" instead of \"Holding\"", heartbeat_write_id, reg.type);
                        return false;
                    }
                    heartbeat_write_reg_idx = static_cast<u8>(err_loc.register_idx);
                    heartbeat_write_decode_idx = static_cast<u8>(err_loc.decode_idx);
                    write_found = true;
                }
                ++err_loc.decode_idx;
            }
            err_loc.decode_idx = 0;
            ++err_loc.register_idx;
        }
        --err_loc.register_idx; // gets rid of the last excessive ++
        if (!read_found)
        {
            err_loc.err_msg = fmt::format("component_heartbeat_read_uri (currently: \"{}\") was provided but could not be found in this component", heartbeat_read_id);
            return false;
        }
        if (!heartbeat_write_id.empty() && !write_found)
        {
            err_loc.err_msg = fmt::format("component_heartbeat_write_uri (currently: \"{}\") was provided but could not be found in this component", heartbeat_write_id);
            return false;
        }
        return true;
    }
};

struct Client_Config
{
    Connection connection;
    std::vector<Component> components;

    // derived info:
    std::size_t total_num_threads = 0; // this is the total number of "mappings" that we have in all the components (main worker threads count only)

    bool load(simdjson::ondemand::object& config_obj, Error_Location& err_loc)
    {
        simdjson::ondemand::object conn_obj;
        if (!get_val(config_obj, "connection", conn_obj, err_loc, Get_Type::Mandatory)) return false;
        if (!connection.load(conn_obj, err_loc)) return false;

        simdjson::ondemand::array comp_arr;
        if (!get_val(config_obj, "components", comp_arr, err_loc, Get_Type::Mandatory)) return false;

        for (auto comp : comp_arr)
        {
            simdjson::ondemand::object comp_obj;
            if (!get_array_val(comp, comp_obj, err_loc)) return false;

            auto& current_comp = components.emplace_back();
            // inheritance stuff:
            current_comp.off_by_one = connection.off_by_one;
            current_comp.word_swap = connection.word_swap;
            current_comp.multi_write_op_code = connection.multi_write_op_code;
            current_comp.frequency = connection.frequency;
            current_comp.device_id = connection.device_id;
            current_comp.debounce = connection.debounce;

            if (!current_comp.load(comp_obj, err_loc)) return false;

            ++err_loc.component_idx;
        }
        err_loc.component_idx = 0;

        if (!check_component_ids(err_loc)) return false;
        if (!check_uri_lengths(err_loc)) return false;

        // add up total number of mappings (1 mapping == 1 worker thread)
        for (const auto& comp : components)
        {
            total_num_threads += comp.registers.size();
        }

        // set max_num_conns to a lower number if we have less threads than it:
        // for example (max is 10 but we only have 2 mappings in one component)
        if (total_num_threads < connection.max_num_conns)
        {
            connection.max_num_conns = total_num_threads;
        }

        return true;
    }

    bool check_component_ids(Error_Location& err_loc)
    {
        if (components.empty())
        {
            err_loc.err_msg = fmt::format("components array is empty");
            return false;
        }

        std::unordered_map<std::string_view, bool> taken_map;
        err_loc.key = "";
        err_loc.type = Required_Types::None;

        if (components.size() > Max_Num_Components)
        {
            err_loc.err_msg = fmt::format("The number of components (currently: {}) has exceeded the maximum of {}", components.size(), Max_Num_Components);
            return false;
        }
        err_loc.reset();

        for (const auto& comp : components)
        {
            err_loc.component_id = comp.id;
            if (taken_map[comp.id])
            {
                err_loc.err_msg = fmt::format("component id \"{}\" has already been mapped", comp.id);
                return false;
            }
            taken_map[comp.id] = true; // claim component id
            ++err_loc.component_idx;
        }
        err_loc.component_idx = 0;

        return true;
    }

    bool check_uri_lengths(Error_Location& err_loc)
    {
        err_loc.reset();
        std::size_t curr_uri_length = 0;

        for (const auto& comp : components)
        {
            err_loc.component_id = comp.id;
            const auto comp_uri_length = Main_Uri_Frag_Length + comp.id.size() + 1UL; // + 1 is for the second '/' before the decode/individual_bit id
            curr_uri_length = comp_uri_length;
            for (const auto& reg : comp.registers)
            {
                err_loc.register_type = reg.type;
                for (const auto& decode : reg.map)
                {
                    err_loc.decode_id = decode.id;
                    if (decode.individual_bits) // use each individual_bits id
                    {
                        for (const auto& bit_str : decode.bit_strings)
                        {
                            if (bit_str.is_unknown || bit_str.num_ignored > 0) { ++err_loc.bit_strings_idx; curr_uri_length = comp_uri_length; continue; } // skip unknown or ignored bits (null stuff)
                            curr_uri_length += bit_str.id.size();
                            if (curr_uri_length > Max_Uri_Length)
                            {
                                err_loc.err_msg = fmt::format("Total uri length (including {} characters for \"{}\" and '/') for individual_bits id \"{}\" within this component (currently: {}) exceeds the maximum of {}", Main_Uri_Frag_Length + 1UL, Main_Uri_Frag, bit_str.id, curr_uri_length, Max_Uri_Length);
                                return false;
                            }
                            ++err_loc.bit_strings_idx;
                            curr_uri_length = comp_uri_length; // reset before next bit_strings id
                        }
                        err_loc.bit_strings_idx = 0;
                    }
                    else // use regular id
                    {
                        curr_uri_length += decode.id.size();
                        if (curr_uri_length > Max_Uri_Length)
                        {
                            err_loc.err_msg = fmt::format("Total uri length (including {} characters for \"{}\" and '/') for decode id \"{}\" within this component (currently: {}) exceeds the maximum of {}", Main_Uri_Frag_Length + 1UL, Main_Uri_Frag, decode.id, curr_uri_length, Max_Uri_Length);
                            return false;
                        }
                    }
                    ++err_loc.decode_idx;
                    curr_uri_length = comp_uri_length; // reset before going to next decode or register mapping
                }
                err_loc.decode_idx = 0;
                ++err_loc.register_idx;
            }
            err_loc.register_idx = 0;
            ++err_loc.component_idx;
        }
        return true;
    }
};

struct Client_Configs_Array
{
    std::vector<Client_Config> configs;

    // derived info:
    std::size_t total_str_length = 0;

    // single object (can expand this out as needed):
    bool load(simdjson::ondemand::object& config_obj, Error_Location& err_loc)
    {
        auto& curr_config = configs.emplace_back();

        if (!curr_config.load(config_obj, err_loc)) return false;
        if (!check_configs(err_loc)) return false;

        if (!get_total_str_length(err_loc)) return false;

        return true;
    }

    // NOTE(walker): experimental, not in use for now
    // bool load(simdjson::ondemand::array& config_array, Error_Location& err_loc)
    // {
    //     for (auto config : config_array)
    //     {
    //         auto& curr_config = configs.emplace_back();
    //     }
    //     return true;
    // }

    bool check_configs(Error_Location& err_loc)
    {
        if (configs.size() > Max_Num_Clients)
        {
            err_loc.err_msg = fmt::format("The total number of configs (currently: {}) exceeds the maximum of {}", configs.size(), Max_Num_Clients);
            return false;
        }
        return true;
    }

    bool get_total_str_length(Error_Location& err_loc)
    {
        err_loc.reset();
        std::unordered_map<std::string, bool> str_map; // to help to remove duplicates when counting total length for string storage

        // TODO(WALKER): In the future provide a "config index/idx" for Error_Location to print out (and increment that)
        // maybe include the "config_name" so we know which config in the array caused the problem based on its connection name
        for (const auto& config : configs)
        {
            auto& connection = config.connection;
            auto& components = config.components;

            if (!str_map[connection.name])
            {
                str_map[connection.name] = true;
                total_str_length += connection.name.size();
                if (total_str_length > String_Storage::Max_Str_Storage_Size)
                {
                    err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                    return false;
                }
            }
            if (!connection.ip_address.empty()) 
            { 
                if (!str_map[connection.ip_address])
                {
                    str_map[connection.ip_address] = true;
                    total_str_length += connection.ip_address.size() + 1UL; // + 1 is for the '\0' character
                    if (total_str_length > String_Storage::Max_Str_Storage_Size)
                    {
                        err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                        return false;
                    }
                }
            }
            if (!connection.serial_device.empty()) 
            { 
                if (!str_map[connection.serial_device])
                {
                    str_map[connection.serial_device] = true;
                    total_str_length += connection.serial_device.size() + 1UL; // + 1 is for the '\0' character
                    if (total_str_length > String_Storage::Max_Str_Storage_Size)
                    {
                        err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                        return false;
                    }
                }
            }

            for (const auto& comp : components)
            {
                // NOTE(WALKER): For component ids, I store /components/x in string storage as opposed to the string itself (this is to prevent extra heap allocations during runtime)
                err_loc.component_id = comp.id;
                const auto comp_uri = fmt::format("{}{}", Main_Uri_Frag, comp.id);
                if (!str_map[comp_uri])
                {
                    str_map[comp_uri] = true;
                    total_str_length += comp_uri.size();
                    if (total_str_length > String_Storage::Max_Str_Storage_Size)
                    {
                        err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                        return false;
                    }
                }
                for (const auto& reg : comp.registers)
                {
                    err_loc.register_type = reg.type;
                    for (const auto& decode : reg.map)
                    {
                        err_loc.decode_id = decode.id;
                        if (!decode.individual_bits) // in the future make sure to count "individual_enums" or get rid of this if, "if" we are going to publish the whole thing as well (the "_all" idea)
                        {
                            if (!str_map[decode.id])
                            {
                                str_map[decode.id] = true;
                                total_str_length += decode.id.size();
                                if (total_str_length > String_Storage::Max_Str_Storage_Size)
                                {
                                    err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                                    return false;
                                }
                            }
                        }
                        // if we have bit_strings stuff then account for those strings:
                        if (decode.has_bit_strings)
                        {
                            for (const auto& bit_str : decode.bit_strings)
                            {
                                if (bit_str.is_unknown || bit_str.num_ignored > 0) { ++err_loc.bit_strings_idx; continue; } // skip unknown or ignored bits (null stuff)
                                if (decode.individual_bits || decode.bit_field)
                                {
                                    if (!str_map[bit_str.id])
                                    {
                                        str_map[bit_str.id] = true;
                                        total_str_length += bit_str.id.size();
                                        if (total_str_length > String_Storage::Max_Str_Storage_Size)
                                        {
                                            err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                                            return false;
                                        }
                                    }
                                }
                                else // enum (in the future make sure to check for individual_enums/enum_fields)
                                {
                                    if (!str_map[bit_str.enum_pair.enum_string])
                                    {
                                        str_map[bit_str.enum_pair.enum_string] = true;
                                        total_str_length += bit_str.enum_pair.enum_string.size();
                                        if (total_str_length > String_Storage::Max_Str_Storage_Size)
                                        {
                                            err_loc.err_msg = fmt::format("Total string size up to this point in the config (currently: {}, this includes \"{}\" per component id) exceeds the maximum of {} characters for string storage", total_str_length, Main_Uri_Frag, String_Storage::Max_Str_Storage_Size);
                                            return false;
                                        }
                                    }
                                }
                                ++err_loc.bit_strings_idx;
                            }
                            err_loc.bit_strings_idx = 0;
                        }
                        ++err_loc.decode_idx;
                    }
                    err_loc.decode_idx = 0;
                    ++err_loc.register_idx;
                }
                err_loc.register_idx = 0;
                ++err_loc.component_idx;
            }
        }
        return true;
    }
};

}

// formatters:

template<>
struct fmt::formatter<config_loader::Connection>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Connection& connection, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), R"({{
        "name": "{}",
        "ip_address": "{}",
        "port": {},
        "serial_device": "{}",
        "baud_rate": {},
        "parity": "{}",
        "data_bits": {},
        "stop_bits": {},
        "off_by_one": {},
        "byte_swap": {},
        "multi_write_op_code": {},
        "frequency": {},
        "device_id": {},
        "debounce": {},
        "max_num_connections": {}
    }})",       connection.name, 
                connection.ip_address, 
                connection.port,
                connection.serial_device, 
                connection.baud_rate, 
                connection.parity_str, 
                connection.data_bits, 
                connection.stop_bits,
                connection.off_by_one,
                connection.word_swap,
                connection.multi_write_op_code,
                connection.frequency,
                connection.device_id,
                connection.debounce,
                connection.max_num_conns);
    }
};

template<>
struct fmt::formatter<config_loader::Registers_Mapping>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Registers_Mapping& reg_map, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), R"(
                {{
                    "type": "{}",
                    "start_offset": {},
                    "num_registers": {},
                    "map": [{}
                    ]
                }})",   reg_map.type,
                        reg_map.start_offset,
                        reg_map.num_registers,
                        fmt::join(reg_map.map, ","));
    }
};

template<>
struct fmt::formatter<config_loader::Component>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Component& comp, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), R"(
        {{
            "id": "{}",
            "off_by_one": {},
            "byte_swap": {},
            "multi_write_op_code": {},
            "frequency": {},
            "device_id": {},
            "debounce": {},
            "heartbeat_enabled": {},
            "component_heatbeat_read_uri": "{}",
            "component_heartbeat_write_uri": "{}",
            "component_heartbeat_timeout_ms": {},
            "heartbeat_read_reg_idx": {},
            "heartbeat_read_decode_idx": {},
            "heartbeat_write_reg_idx": {},
            "heartbeat_write_decode_idx": {},
            "registers": [{}
            ]
        }})",   comp.id,
                comp.off_by_one,
                comp.word_swap,
                comp.multi_write_op_code,
                comp.frequency,
                comp.device_id,
                comp.debounce,
                comp.heartbeat_enabled,
                comp.heartbeat_read_id,
                comp.heartbeat_write_id,
                // comp.heartbeat_check_frequency,
                comp.heartbeat_timeout,
                comp.heartbeat_read_reg_idx,
                comp.heartbeat_read_decode_idx,
                comp.heartbeat_write_reg_idx,
                comp.heartbeat_write_decode_idx,
                fmt::join(comp.registers, ","));
    }
};

template<>
struct fmt::formatter<config_loader::Client_Config>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Client_Config& config, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), R"({{
    "connection": {},
    "components": [{}
    ]
}})",   config.connection, 
        fmt::join(config.components, ","));
    }
};

template<>
struct fmt::formatter<config_loader::Client_Configs_Array>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const config_loader::Client_Configs_Array& configs_array, FormatContext& ctx) const
    {
        if (configs_array.configs.size() == 1)
        {
            return fmt::format_to(ctx.out(), "{}", configs_array.configs[0]);
        }
        else
        {
            return fmt::format_to(ctx.out(), R"([
{}
])", fmt::join(configs_array.configs, ",\n"));
        }
    }
};
