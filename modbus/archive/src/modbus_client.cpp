#include <chrono>
#include <thread>
#include <sys/uio.h> // for receive timouet calls
#include <sys/socket.h> // for receive timouet calls

#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //for exit(0);
#include<sys/socket.h>
#include<errno.h> //For errno - the error number
#include<netdb.h>	//hostent
#include<arpa/inet.h>

#include "spdlog/details/fmt_helper.h" // for microseconds formattting
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/bundled/ranges.h"
#include "spdlog/fmt/chrono.h"

#include "simdjson_noexcept.hpp"

#include "rigtorp/SPSCQueue.h"
#include "rigtorp/MPMCQueue.h"
#include "modbus/modbus.h"

#include "fims/defer.hpp"
#include "Jval_buif.hpp"
#include "tscns.h"

#include "fims/libfims.h"

#include "config_loaders/client_config_loader.hpp"
#include "client/client_structs.hpp"

using namespace std::chrono_literals;
using namespace std::string_view_literals;

// constants:
// TODO(WALKER): "broken pipe?" -> larger integer? (will have to investigate)
// static constexpr auto Modbus_Errno_Disconnect   = 104; // "Connection reset by peer" -> disconnect
// static constexpr auto Modbus_Errno_Cant_Connect = 115; // "Operation now in progress" -> no connection
static constexpr auto Modbus_Server_Busy = 10126; // "Resource temporarily unavailable" -> server busy

// helper macros:
#define FORMAT_TO_BUF(fmt_buf, fmt_str, ...)            fmt::format_to(std::back_inserter(fmt_buf), FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define FORMAT_TO_BUF_NO_COMPILE(fmt_buf, fmt_str, ...) fmt::format_to(std::back_inserter(fmt_buf), fmt_str, ##__VA_ARGS__)

// NOTE(WALKER): this is taken from spdlog's library credit goes to them
// return fraction of a second of the given time_point.
// e.g.
// fraction<std::milliseconds>(tp) -> will return the millis part of the second
template<typename ToDuration>
static inline ToDuration time_fraction(std::chrono::system_clock::time_point tp)
{
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    auto duration = tp.time_since_epoch();
    auto secs = duration_cast<seconds>(duration);
    return duration_cast<ToDuration>(duration) - duration_cast<ToDuration>(secs);
}

// Helper function straight from C++20 so we can use it here in C++17:
// for checking for suffix's over fims (like a raw get request)
static constexpr bool str_ends_with(std::string_view str, std::string_view suffix) noexcept
{
	const auto str_len = str.size();
	const auto suffix_len = suffix.size();
	return str_len >= suffix_len 
                && std::string_view::traits_type::compare(str.end() - suffix_len, suffix.data(), suffix_len) == 0;
}

// fims helper functions:
static bool send_pub(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"pub", sizeof("pub") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}
static bool send_set(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"set", sizeof("set") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}
// NOTE(WALKER): This is only for emit_event really (not used anywhere else)
static bool send_post(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"post", sizeof("post") -1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}

// NOTE(WALKER): use these in combination with send_event for proper severity levels
enum Event_Severity : uint8_t
{
    Debug = 0,
    Info = 1,
    Status = 2,
    Alarm = 3,
    Fault = 4
};
// [DEBUG, INFO, STATUS, ALARM, FAULT] 0..4 (from Kyle on slack thread)

// emits an event to /events
template<std::size_t Size, typename... Fmt_Args>
static bool emit_event(fmt::basic_memory_buffer<char, Size>& send_buf, fims& fims_gateway, std::string_view source, Event_Severity severity, fmt::format_string<Fmt_Args...> fmt_str, Fmt_Args&&... fmt_args) noexcept
{
    send_buf.clear();

    FORMAT_TO_BUF(send_buf, R"({{"source":"{}","message":")", source);
    FORMAT_TO_BUF_NO_COMPILE(send_buf, fmt_str, std::forward<Fmt_Args>(fmt_args) ...);
    FORMAT_TO_BUF(send_buf, R"(","severity":{}}})", severity);

    return send_post(fims_gateway, "/events"sv, std::string_view{send_buf.data(), send_buf.size()});
}

// Helper function for acquiring a locked connection (tries every single connection in the pool)
// will stop at the original connection if it can't acquire another slot
// static Mod_Conn& acquire_locked_conn(Ctx_Pool& ctx_pool, const std::size_t client_thread_idx)
// {
//     for (u8 i = 0; i < ctx_pool.num_conns; ++i)
//     {
//         auto& the_conn = ctx_pool.pool[(client_thread_idx + i) % ctx_pool.num_conns];
//         if (the_conn.lock.try_lock()) return the_conn; // we have acquired a lock (could be beyond the original assignment), use that one
//     }
//     auto& the_conn = ctx_pool.pool[client_thread_idx % ctx_pool.num_conns];
//     the_conn.lock.lock(); // we have exhausted all options, return the original one you were assigned
//     return the_conn;
// }

// Client event source constant:
static constexpr auto Client_Event_Source = "Modbus Client"sv;

// Global workspace (all functions will use this for client):
Main_Workspace main_workspace;

enum class Arg_Types : u8
{
    Error,
    Help,
    File,
    Uri, // for a "get" over fims
    Expand
};

static std::pair<Arg_Types, std::string> parse_command_line_arguments(const int argc, const char* argv[]) noexcept
{
    static constexpr std::size_t Max_Arg_Size = std::numeric_limits<u8>::max(); // at most 255 characters for argument paths
    static constexpr auto Modbus_Client_Help_String = 
R"(
Modbus Client Usage:

-h, --help:   print out this help string
-f, --file:   provide a json file to run Modbus Client with directly (default flag)
-u, --uri:    provide a json file uri over fims (without extension) to run Modbus Client with using a fims get command
-e, --expand: provide a json file for Modbus Client to parse through and produce another file expanded (without ranges and templating)

NOTE: no flag is the same as -f/--file (simply provide the json file to parse)

examples:

help:
/path/to/modbus_client -h
or
/path/to/modbus_client --help

file:
/path/to/modbus_client /path/to/some_json.json
or
/path/to/modbus_client -f /path/to/some_json.json
or
/path/to/modbus_client --file /path/to/some_json.json

uri:
/path/to/modbus_client -u /fims/uri/to/get
or
/path/to/modbus_client --uri /fims/uri/to/get

expand:
/path/to/modbus_client -e /path/to/json_file_to_expand.json
or
/path/to/modbus_client --expand /path/to/json_file_to_expand.json
)"sv;

    std::pair<Arg_Types, std::string> args; // error by default

    if (argc <= 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        args.first = Arg_Types::Help;
        NEW_FPS_ERROR_PRINT_NO_ARGS(Modbus_Client_Help_String);
    }
    else if (strcmp(argv[1], "-f") == 0 || strcmp(argv[1], "--file") == 0)
    {
        if (argc >= 2 && argv[2] && strlen(argv[2]) <= Max_Arg_Size)
        {
            args.first = Arg_Types::File;
            args.second = argv[2];
        }
        else
        {
            NEW_FPS_ERROR_PRINT("error in --file: json file not provided, or json file path was more than {} characters\n", Max_Arg_Size);
        }
    }
    else if (strcmp(argv[1], "-u") == 0 || strcmp(argv[1], "--uri") == 0)
    {
        if (argc >= 2 && argv[2] && strlen(argv[2]) <= Max_Arg_Size)
        {
            args.first = Arg_Types::Uri;
            args.second = argv[2];
        }
        else
        {
            NEW_FPS_ERROR_PRINT("error in --uri: fims json path not provided, or fims json path was more than {} characters\n", Max_Arg_Size);
        }
    }
    else if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--expand") == 0)
    {
        if (argc >= 2 && argv[2] && strlen(argv[2]) <= Max_Arg_Size)
        {
            args.first = Arg_Types::Expand;
            args.second = argv[2];
        }
        else
        {
            NEW_FPS_ERROR_PRINT("error in --expand: json file path not provided, or json file path was more than {} characters\n", Max_Arg_Size);
        }
    }
    else // normal file (this might change to dbi in the future by default who knows):
    {
        if (strlen(argv[1]) <= Max_Arg_Size)
        {
            args.first = Arg_Types::File;
            args.second = argv[1];
        }
        else
        {
            NEW_FPS_ERROR_PRINT("error in --file: json file path was more than {} characters\n", Max_Arg_Size);
        }
    }

    // prune the extensions of the input file then append .json
    // this way input extension doesn't matter (it can be optional or messed up by accident)
    if (args.first == Arg_Types::File || args.first == Arg_Types::Expand)
    {
        const auto first_extension_index = args.second.find_first_of('.');
        if (first_extension_index != args.second.npos) args.second.resize(first_extension_index);
        args.second.append(".json");
    }

    return args;
}

static bool load_config(const std::pair<Arg_Types, std::string>& args) noexcept
{
    if (args.first == Arg_Types::Error || args.first == Arg_Types::Help) return false; // shouldn't reach this point

    config_loader::Client_Configs_Array configs_array;
    config_loader::Error_Location err_loc;

    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc;

    simdjson::simdjson_result<simdjson::padded_string> json;
    simdjson::ondemand::object config_data_obj;
    simdjson::ondemand::array config_data_array; // this is unused (but might be in the future, who knows)

    if (args.first == Arg_Types::File || args.first == Arg_Types::Expand)
    {
        json = simdjson::padded_string::load(args.second);
    }
    else if (args.first == Arg_Types::Uri)
    {
        if (args.second.front() != '/')
        {
            NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": the uri does not begin with `/`\n", args.second);
            return false;
        }
        fims fims_gateway;
        const auto conn_id_str = fmt::format("modbus_client_uri_init@{}", args.second);
        if (!fims_gateway.Connect(conn_id_str.data()))
        {
            NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": could not connect to fims_server\n", args.second);
            return false;
        }
        const auto sub_string = fmt::format("/modbus_client_uri_init{}", args.second);
        if (!fims_gateway.Subscribe(std::vector<std::string>{sub_string}))
        {
            NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": failed to subscribe for uri init\n", args.second);
            return false;
        }
        if (!fims_gateway.Send(fims::str_view{"get", sizeof("get") - 1}, fims::str_view{args.second.data(), args.second.size()}, fims::str_view{sub_string.data(), sub_string.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}))
        {
            NEW_FPS_ERROR_PRINT("For client with inti uri \"{}\": failed to send a fims get message\n", args.second);
            return false;
        }
        auto config_msg = fims_gateway.Receive_Timeout(5000000); // give them 5 seconds to respond before erroring
        defer { fims::free_message(config_msg); };
        if (!config_msg)
        {
            NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": failed to receive a message in 5 seconds\n", args.second);
            return false;
        }
        if (!config_msg->body)
        {
            NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": message was received, but body doesn't exist\n", args.second);
            return false;
        }
        json = simdjson::padded_string{std::string{config_msg->body}};
    }

    if (const auto err = parser.iterate(json).get(doc); err)
    {
        NEW_FPS_ERROR_PRINT("error parsing config json \"{}\", err = {}\n", args.second, simdjson::error_message(err));
        return false;
    }

    if (const auto err = doc.get(config_data_obj); err)
    {
        NEW_FPS_ERROR_PRINT("error parsing config json \"{}\" as an object, err = {}\n", args.second, simdjson::error_message(err));
        return false;
    }

    if(!configs_array.load(config_data_obj, err_loc))
    {
        NEW_FPS_ERROR_PRINT("{}\n", err_loc);
        return false;
    }

    if (args.first == Arg_Types::Expand)
    {
        fmt::print("{}\n", configs_array);
        return true;
    }

    if (!initialize_main_workspace_from_config(configs_array, main_workspace)) return false;

    // finish setup for each client (fims, modbus, heartbeat_timeout check, etc.):
    bool has_single_connection = false;
    fmt::memory_buffer str_buf;
    for (u8 client_idx = 0; client_idx < main_workspace.num_clients; ++client_idx)
    {
        str_buf.clear();
        auto& client_workspace = *main_workspace.client_workspaces[client_idx];
        const auto client_name = main_workspace.string_storage.get_str(client_workspace.conn_workspace.conn_info.name);
        FORMAT_TO_BUF(str_buf, "modbus_client@{}\0", client_name);
        if (!client_workspace.fims_gateway.Connect(str_buf.data()))
        {
            NEW_FPS_ERROR_PRINT("For client \"{}\": could not connect to fims_server\n", client_name);
            return false;
        }
        std::vector<std::string> sub_array; // sub array for this client
        for (const auto& comp : configs_array.configs[client_idx].components)
        {
            str_buf.clear();
            FORMAT_TO_BUF(str_buf, "{}{}", Main_Uri_Frag, comp.id);
            sub_array.emplace_back(std::string{str_buf.data(), str_buf.size()});
        }
        if (!client_workspace.fims_gateway.Subscribe(sub_array))
        {
            NEW_FPS_ERROR_PRINT("For client \"{}\": could not subscribe to component's uris\n", client_name);
            return false;
        }

        // connect modbus contexts (no retries, don't bother, systemctl will restart it):
        const auto has_connection = client_workspace.startup_modbus_conns(main_workspace.string_storage);
        has_single_connection = has_single_connection || has_connection;
        if (has_connection)
        {
            NEW_FPS_ERROR_PRINT("client \"{}\": Connected to server with {} connection(s)\n", client_name, client_workspace.conn_workspace.IO_thread_pool.num_conns);
            // emit_event about that client connecting:
            if (!emit_event(str_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Info, R"(client \"{}\": Modbus connection established with {} total connections)", client_name, client_workspace.conn_workspace.IO_thread_pool.num_conns))
            {
                NEW_FPS_ERROR_PRINT("client \"{}\": Cannot send an event out on fims. Exiting\n", client_name);
                return false;
            }
        }
    }

    // Must have at least one client that could connect successfully before continuing the program:
    if (!has_single_connection)
    {
        NEW_FPS_ERROR_PRINT_NO_ARGS("Could not establish a single connection for any client, exiting\n");
        return false;
    }

    // after connection checks:
    for (u8 client_idx = 0; client_idx < main_workspace.num_clients; ++client_idx)
    {
        auto& client_workspace = *main_workspace.client_workspaces[client_idx];
        const auto client_name = main_workspace.string_storage.get_str(client_workspace.conn_workspace.conn_info.name);
        for (const auto& comp : configs_array.configs[client_idx].components)
        {
            // check each component_heartbeat_timeout_ms to make sure it's at least 2x frequency, no exit on error (just an event and print out warning):
            if (comp.heartbeat_enabled && comp.heartbeat_timeout < comp.frequency * 2)
            {
                NEW_FPS_ERROR_PRINT("client \"{}\", component \"{}\": \"modbus_heartbeat_timeout_ms\" (currently: {}) is less than 2x frequency. This could cause false timeout issues, consider changing it.\n", client_name, comp.id, comp.heartbeat_timeout);
                if (!emit_event(str_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Alarm, R"(client \"{}\", component \"{}\": \"modbus_heartbeat_timeout_ms\" (currently: {}) is less than 2x frequency. This could cause false timeout issues, consider changing it.)", client_name, comp.id, comp.heartbeat_timeout))
                {
                    NEW_FPS_ERROR_PRINT("client \"{}\": Cannot send an event out on fims. Exiting\n", client_name);
                    return false;
                }
            }
        }
    }
    return true;
}

static bool 
listener_thread(const u8 client_idx) noexcept {
    // constants:
    static constexpr auto Raw_Request_Suffix           = "/_raw"sv;
    static constexpr auto Timings_Request_Suffix       = "/_timings"sv;
    static constexpr auto Reset_Timings_Request_Suffix = "/_reset_timings"sv;
    static constexpr auto Reload_Request_Suffix        = "/_reload"sv; // this will go under the /exe_name/controls uri
    // NOTE(WALKER): an "x_request" is a "get" request that ends the get "uri" in one of the above suffixes
    // for example:
    // get on uri : "/components/bms_info"      -> regular
    // raw get uri: "/components/bms_info/_raw" -> raw_request
    // NOTE(WALKER): a "reload" request is a "set" request that ends with /_reload (this reloads the WHOLE client, NOT just that particular component)

    // method types supported by modbus_client:
    enum class Method_Types : u8 {
        Set,
        Get
    };

    // listener variables:
    auto& my_workspace     = *main_workspace.client_workspaces[client_idx];
    const auto client_name = main_workspace.string_storage.get_str(my_workspace.conn_workspace.conn_info.name);
    auto& fims_gateway     = my_workspace.fims_gateway;
    auto& receiver_bufs    = my_workspace.receiver_bufs;
    auto& meta_data        = receiver_bufs.meta_data;
    auto& parser           = my_workspace.parser;
    auto& doc              = my_workspace.doc;
    auto& main_work_qs     = my_workspace.main_work_qs;

    // for telling main that there was something wrong:
    defer {
        my_workspace.keep_running = false;
        main_work_qs.signaller.signal(); // tell main to shut everything down
    };

    // wait until main signals everyone to start:
    {
        std::unique_lock<std::mutex> lk{main_workspace.main_mutex};
        main_workspace.main_cond.wait(lk, [&]() {
            return main_workspace.start_signal.load();
        });
    }

    // setup the timeout to be 2 seconds (so we can stop listener thread without it spinning infinitely on errors):
    struct timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    if (setsockopt(fims_gateway.get_socket(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) == -1)
    {
        NEW_FPS_ERROR_PRINT("listener for \"{}\": could not set socket timeout to 2 seconds. Exiting\n", client_name);
        return false;
    }

    // main loop:
    while (my_workspace.keep_running)
    {
        if (!recv_raw_message(fims_gateway.get_socket(), receiver_bufs))
        {
            const auto curr_errno = errno;
            if (curr_errno == EAGAIN || curr_errno == EWOULDBLOCK || curr_errno == EINTR) continue; // we just timed out
            // This is if we have a legitimate error (no timeout):
            NEW_FPS_ERROR_PRINT("Listener for \"{}\": could not receive message over fims. Exiting\n", client_name);
            return false;
        }

        // meta data views:
        const auto method_view       = std::string_view{receiver_bufs.get_method_data(), meta_data.method_len};
        auto uri_view                = std::string_view{receiver_bufs.get_uri_data(), meta_data.uri_len};
        const auto replyto_view      = std::string_view{receiver_bufs.get_replyto_data(), meta_data.replyto_len};
        const auto process_name_view = std::string_view{receiver_bufs.get_process_name_data(), meta_data.process_name_len};
        // const auto user_name_view    = std::string_view{receiver_bufs.get_user_name_data(), meta_data.user_name_len};

        Method_Types method;

        // method check:
        if (method_view == "set")
        {
            method = Method_Types::Set;
        }
        else if (method_view == "get")
        {
            method = Method_Types::Get;
        }
        else // method not supported by modbus_client
        {
            NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": method \"{}\" is not supported by modbus_client. Message dropped\n", client_name, process_name_view, method_view);
            if (!replyto_view.empty())
            {
                static constexpr auto err_str = "Modbus Client -> method not supported"sv;
                if (!send_set(fims_gateway, replyto_view, err_str))
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": could not send replyto fims message. Exiting\n", client_name, process_name_view);
                    return false;
                }
            }
            continue;
        }

        // /_x request checks:
        const bool is_raw_request           = str_ends_with(uri_view, Raw_Request_Suffix);
        const bool is_timings_request       = str_ends_with(uri_view, Timings_Request_Suffix);
        const bool is_reset_timings_request = str_ends_with(uri_view, Reset_Timings_Request_Suffix);
        const bool is_reload_request        = str_ends_with(uri_view, Reload_Request_Suffix);
        if (is_raw_request)           uri_view.remove_suffix(Raw_Request_Suffix.size());
        if (is_timings_request)       uri_view.remove_suffix(Timings_Request_Suffix.size());
        if (is_reset_timings_request) uri_view.remove_suffix(Reset_Timings_Request_Suffix.size());
        if (is_reload_request)        uri_view.remove_suffix(Reload_Request_Suffix.size());

        // uri check:
        uri_view.remove_prefix(Main_Uri_Frag_Length); // removes /components/ from the uri for processing (hashtable uri lookup)
        const auto uri_it = my_workspace.uri_map.find(uri_map_hash(uri_view));
        if (uri_it == my_workspace.uri_map.end())
        {
            NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": uri \"{}\" could not be found. Message dropped\n", client_name, process_name_view, uri_view);
            if (!replyto_view.empty())
            {
                static constexpr auto err_str = "Modbus Client -> uri doesn't exist"sv;
                if (!send_set(fims_gateway, replyto_view, err_str))
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": could not send replyto fims message. Exiting\n", client_name, process_name_view, uri_view);
                    return false;
                }
            }
            continue;
        }
        const auto& base_uri_info = uri_it->second;
        // auto& msg_staging_area = my_workspace.msg_staging_areas[base_uri_info.component_idx];

        if (is_reload_request)
        {
            // POTENTIAL TODO(WALKER): do these need to respond to "replyto"? (for now no, seems kinda redundant)
            if (method != Method_Types::Set)
            {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": reload request method must be \"set\"\n", client_name, process_name_view);
                continue;
            }
            else
            {
                NEW_FPS_ERROR_PRINT_NO_ARGS("reload request received, reloading modbus_client\n");
                main_workspace.reload = true;
                return true;
            }
        }

        // do logic based on uri type and multi/single uri type
        if (method == Method_Types::Set)
        {
            // defer replyto so we don't have to have bloated if/else logic everywhere:
            bool set_had_errors = false;
            std::string_view success_str;
            defer {
                static constexpr auto err_str = "Modbus Client -> set had errors"sv;
                if (!replyto_view.empty())
                {
                    bool fims_ok = true;
                    if (!set_had_errors && !send_set(fims_gateway, replyto_view, success_str))
                    {
                        fims_ok = false;
                    }
                    else if (set_had_errors && !send_set(fims_gateway, replyto_view, err_str))
                    {
                        fims_ok = false;
                    }
                    if (!fims_ok)
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n", client_name, process_name_view, uri_view);
                        // no return, but next receive() call will error out at the top
                    }
                }
            };

            // no raw/timings requests for set:
            if (is_raw_request || is_timings_request || is_reset_timings_request)
            {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": sets are not accepted on raw/timings request uris.\n", client_name, process_name_view);
                set_had_errors = true;
                continue;
            }

            // got a more data than expected (right now this is 100,000):
            if (meta_data.data_len > receiver_bufs.get_max_expected_data_len())
            {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": got a timeouta \"set\" request but got {} bytes even though the maximum expected for modbus_client is {}. Please reduce message sizes or increase the maximum expected for modbus_client. Message dropped\n", client_name, process_name_view, meta_data.data_len, receiver_bufs.get_max_expected_data_len());
                set_had_errors = true;
                continue;
            }

            // "data" must exist (decrypt buff and free with defer)
            void* data = nullptr;
            defer { if (data && fims_gateway.has_aes_encryption()) free(data); };
            data = decrypt_buf(receiver_bufs);

            if (!data)
            {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": got a set request with no body. Message dropped\n", client_name, process_name_view, uri_view);
                set_had_errors = true;
                continue;
            }

            // set success_str for replyto defer:
            success_str = std::string_view{reinterpret_cast<const char*>(data), meta_data.data_len};

            // if we don't have encryption set padding bytes inside buffer to '\0' so we can use a view when parsing
            if (!fims_gateway.has_aes_encryption())
            {
                memset(reinterpret_cast<u8*>(data) + meta_data.data_len, '\0', Simdj_Padding);
            }
            // NOTE(WALKER): "parser" holds the iterator of the json string and the doc gives you "handles" back to parse the underlying strings
            if (const auto err = parser.iterate(reinterpret_cast<const char*>(data), meta_data.data_len, meta_data.data_len + Simdj_Padding).get(doc); err)
            {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": could not parse json string from set request, err = {} Message dropped\n", client_name, process_name_view, uri_view, simdjson::error_message(err));
                set_had_errors = true;
                continue;
            }

            // now set onto channels based on multi or single set uri:
            Jval_buif to_set;
            Set_Work set_work;
            if (base_uri_info.decode_idx == Decode_All_Idx) // multi-set
            {
                auto& uri_buf = my_workspace.uri_buf;

                simdjson::ondemand::object set_obj;
                if (const auto err = doc.get(set_obj); err)
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": could not get multi-set fims message as a json object, err = {} Message dropped\n", client_name, process_name_view, uri_view, simdjson::error_message(err));
                    set_had_errors = true;
                    continue;
                }

                bool inner_json_it_err = false; // if we have a "parser" error during object iteration we "break" 
                // iterate over object and extract out values (clothed or unclothed):
                for (auto pair : set_obj)
                {
                    const auto key = pair.unescaped_key();
                    if (const auto err = key.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": parsing error on getting a multi-set key, err = {} Message dropped\n", client_name, process_name_view, uri_view, simdjson::error_message(err));
                        inner_json_it_err = true;
                        break;
                    }
                    const auto key_view = key.value_unsafe();
                    auto val = pair.value();
                    if (const auto err = val.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\", on multi-set key \"{}\": parse error on getting value, err = {} Message dropped\n", client_name, process_name_view, uri_view, key_view, simdjson::error_message(err));
                        inner_json_it_err = true;
                        break;
                    }

                    uri_buf.clear();
                    FORMAT_TO_BUF(uri_buf, "{}/{}", uri_view, key_view);
                    const auto curr_set_uri_view = std::string_view{uri_buf.data(), uri_buf.size()};

                    // uri check:
                    const auto inner_uri_it = my_workspace.uri_map.find(uri_map_hash(curr_set_uri_view));
                    if (inner_uri_it == my_workspace.uri_map.end())
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": multi-set uri \"{}\" could not be found. Skipping that set\n", client_name, process_name_view, curr_set_uri_view);
                        continue;
                    }
                    const auto& inner_uri_info = inner_uri_it->second;

                    // reg type check:
                    if (inner_uri_info.reg_type == Register_Types::Input || inner_uri_info.reg_type == Register_Types::Discrete_Input)
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": This uri is of a register type that does not accept set requests. Skipping this uri's set\n", client_name, process_name_view, curr_set_uri_view);
                        continue;
                    }

                    // extract out value (clothed or unclothed):
                    auto curr_val = val.value_unsafe();
                    auto val_clothed = curr_val.get_object();
                    if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": could not get a clothed value as an object while parsing a multi-set, err = {} Message dropped\n", client_name, process_name_view, curr_set_uri_view, simdjson::error_message(err));
                        inner_json_it_err = true;
                        break;
                    }
                    if (!val_clothed.error())
                    {
                        auto inner_val = val_clothed.find_field("value");
                        if (const auto err = inner_val.error(); err)
                        {
                            NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": could not get the clothed key \"value\" while parsing a multi-set, err = {} skipping this uri's set\n", client_name, process_name_view, curr_set_uri_view, simdjson::error_message(err));
                            continue;
                        }
                        curr_val = std::move(inner_val.value_unsafe());
                    }

                    // extract out value based on type into Jval:
                    if (auto val_uint = curr_val.get_uint64(); !val_uint.error())
                    {
                        to_set = val_uint.value_unsafe();
                    }
                    else if (auto val_int = curr_val.get_int64(); !val_int.error())
                    {
                        to_set = val_int.value_unsafe();
                    }
                    else if (auto val_float = curr_val.get_double(); !val_float.error())
                    {
                        to_set = val_float.value_unsafe();
                    }
                    else if (auto val_bool = curr_val.get_bool(); !val_bool.error())
                    {
                        to_set = static_cast<u64>(val_bool.value_unsafe()); // just set booleans equal to the whole numbers 1/0 for sets
                    }
                    else
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": only floats, uints, ints, and bools are supported for sets, skipping this uri's set\n", client_name, process_name_view, curr_set_uri_view);
                        continue;
                    }

                    // check for true/false and 1/0 for coils and individual_bits:
                    if (inner_uri_info.reg_type == Register_Types::Coil || inner_uri_info.flags.is_individual_bit())
                    {
                        if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL))
                        {
                            NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": cannot set Coil or individual_bit uri to the value: {}, only true/false and 1/0 are accepted, skipping this uri's set\n", client_name, process_name_view, curr_set_uri_view, to_set);
                            continue;
                        }
                    }

                    set_work.set_vals.emplace_back(Set_Info{to_set, 0, inner_uri_info.component_idx, inner_uri_info.thread_idx, inner_uri_info.decode_idx, inner_uri_info.bit_idx, inner_uri_info.enum_idx});
                }
                if (inner_json_it_err)
                {
                    set_had_errors = true;
                    continue;
                }

                // This means that all of the sets in this set had an error (but not a parsing error):
                // for example, all of them tried to set on "Input" registers
                if (set_work.set_vals.empty())
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": all of the sets for this message had logical errors in one way or another (for example: all the set uris were \"Input\" registers). These are NOT parsing errors. Message dropped\n", client_name, process_name_view);
                    set_had_errors = true;
                    continue;
                }

                if (!main_work_qs.set_q.try_emplace(std::move(set_work)))
                {
                    NEW_FPS_ERROR_PRINT_NO_ARGS("could not push a set message onto a channel, things are really backed up. This should NEVER happpen, shutting the whole thing down\n");
                    return false;
                }
                main_work_qs.signaller.signal(); // tell main to wake up -> It has work to do
            }
            else // single-set:
            {
                // reg type check:
                if (base_uri_info.reg_type == Register_Types::Input || base_uri_info.reg_type == Register_Types::Discrete_Input)
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": This uri is of a register type that does not accept set requests. Message dropped\n", client_name, process_name_view, uri_view);
                    set_had_errors = true;
                    continue;
                }

                simdjson::ondemand::value curr_val;
                auto val_clothed = doc.get_object();
                if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
                {
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": cannot check if set message is clothed in an object, err = {} Message dropped\n", client_name, process_name_view, uri_view, simdjson::error_message(err));
                    set_had_errors = true;
                    continue;
                }
                if (!val_clothed.error()) // they sent a clothed value:
                {
                    auto inner_val = val_clothed.find_field("value");
                    if (const auto err = inner_val.error(); err)
                    {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": could not get \"value\" as a key inside clothed object, err = {} Message dropped\n", client_name, process_name_view, uri_view, simdjson::error_message(err));
                        set_had_errors = true;
                        continue;
                    }
                    curr_val = std::move(inner_val.value_unsafe());
                }

                // NOTE(WALKER): This logic has to be like this instead of like the multi set
                // because simdjson has a special case where if the json string itself is just a scalar
                // then you can't use doc.get_value() (will return an error code). So this is custom tailored 
                // to take that into account. Do NOT try and duplicate the multi set logic for this.
                if (auto val_uint = val_clothed.error() ? doc.get_uint64() : curr_val.get_uint64(); !val_uint.error()) // it is an unsigned integer
                {
                    to_set = val_uint.value_unsafe();
                }
                else if (auto val_int = val_clothed.error() ? doc.get_int64() : curr_val.get_int64(); !val_int.error()) // it is an integer
                {
                    to_set = val_int.value_unsafe();
                }
                else if (auto val_float = val_clothed.error() ? doc.get_double() : curr_val.get_double(); !val_float.error()) { // it is a float
                    to_set = val_float.value_unsafe();
                } else if (auto val_bool = val_clothed.error() ? doc.get_bool() : curr_val.get_bool(); !val_bool.error()) { // it is a bool
                    to_set = static_cast<u64>(val_bool.value_unsafe()); // just set booleans to the whole numbers 1/0
                } else { // unsupported type:
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": only floats, uints, ints, and bools are supported for sets. Message dropped\n", client_name, process_name_view, uri_view);
                    set_had_errors = true;
                    continue;
                }

                // check for true/false and 1/0 for coils and individual_bits:
                if (base_uri_info.reg_type == Register_Types::Coil || base_uri_info.flags.is_individual_bit()) {
                    if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL)) {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": cannot set Coil or individual_bit uri to the value: {}, only true/false and 1/0 are accepted. Message dropped\n", client_name, process_name_view, uri_view, to_set);
                        set_had_errors = true;
                        continue;
                    }
                }

                set_work.set_vals.emplace_back(Set_Info{to_set, 0, base_uri_info.component_idx, base_uri_info.thread_idx, base_uri_info.decode_idx, base_uri_info.bit_idx, base_uri_info.enum_idx});
                if (!main_work_qs.set_q.try_emplace(std::move(set_work))) {
                    NEW_FPS_ERROR_PRINT_NO_ARGS("could not push a set message onto a channel, things are really backed up. This should NEVER happpen, shutting the whole thing down\n");
                    return false;
                }
                main_work_qs.signaller.signal(); // tell main there is work to do
            }
        } else { // "get" (replyto required)
            if (replyto_view.empty()) {
                NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with uri \"{}\": get request does not have a replyto. Message dropped\n", client_name, process_name_view, uri_view);
                continue;
            }
            if (is_timings_request || is_reset_timings_request) { // they can only do timings requests on whole components (NOT single registers)
                if (base_uri_info.decode_idx != Decode_All_Idx) {
                    static constexpr auto err_str = "Modbus Client -> timings requests can NOT be given for a single register"sv;
                    NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with uri \"{}\": get requests for single register uris cannot accept timing requests. Message dropped\n", client_name, process_name_view, uri_view);
                    if (!send_set(fims_gateway, replyto_view, err_str)) {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n", client_name, process_name_view, uri_view);
                        // fims error will cause listener to exit at the top on next receive() call
                    }
                    continue;
                }
                if (is_reset_timings_request) { // send them a message back to indicate reset 
                    static constexpr auto success_str = "Modbus Client -> timings reset"sv;
                    if (!send_set(fims_gateway, replyto_view, success_str)) {
                        NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n", client_name, process_name_view, uri_view);
                        // fims error will cause listener to exit at the top on next receive() call
                    }
                }
            }

            Get_Work get_work;
            get_work.flags.flags = 0;
            get_work.flags.set_raw_request(is_raw_request);
            get_work.flags.set_timings_request(is_timings_request);
            get_work.flags.set_reset_timings_request(is_reset_timings_request);
            get_work.comp_idx   = base_uri_info.component_idx;
            get_work.thread_idx = base_uri_info.thread_idx;
            get_work.decode_idx = base_uri_info.decode_idx;
            get_work.bit_idx    = base_uri_info.bit_idx;
            get_work.enum_idx   = base_uri_info.enum_idx;
            get_work.replyto    = replyto_view;
            if (!main_work_qs.get_q.try_emplace(std::move(get_work))) {
                NEW_FPS_ERROR_PRINT("could not push onto \"get\" channel for uri \"{}\" as it was full. From sender: \"{}\". Shutting the whole thing down\n", uri_view, process_name_view);
                return false;
            }
            main_work_qs.signaller.signal(); // tell main there is work to do
        }
    }
    return true;
}

static bool
IO_thread(const u8 thread_id) noexcept {
    auto& mono_clock       = main_workspace.mono_clock;
    // auto& str_storage      = main_workspace.string_storage;
    auto& client_workspace = *main_workspace.client_workspaces[0];
    auto& my_workspace     = client_workspace.conn_workspace.IO_thread_pool.io_thread_workspaces[thread_id];
    auto& work_qs          = client_workspace.conn_workspace.IO_thread_pool.io_work_qs;
    auto& main_work_qs     = client_workspace.main_work_qs;

    // If IO thread exits main needs to shut everything down and everything needs to stop anyway::
    defer {
        client_workspace.keep_running = false; // both main and listener share this (tell them something is wrong as things are backed up and this should never happen)
        main_work_qs.signaller.signal();
    };

    Set_Work  set_work;  // move into this from set_q
    Poll_Work poll_work; // move into this from poll_q (increment your priority here then)
    s64 begin_poll_time;
    s64 end_poll_time;
    // buffers to encode sets into and receive polls into:
    u16 set_buffer[4];
    u16 poll_buffer_u16[MODBUS_MAX_READ_REGISTERS];  // receive into this (holding and input)
    u8  poll_buffer_u8[MODBUS_MAX_READ_REGISTERS];   // receive into this (coil and discrete input)

    while (my_workspace.keep_running) {
        work_qs.signaller.wait(); // wait for work to do

        if (!my_workspace.keep_running) break; // If main tells IO_threads to stop then stop

        // do sets before polls:
        if (work_qs.set_q.try_pop(set_work)) {
            const auto& comp_workspace = *client_workspace.msg_staging_areas[set_work.set_vals[0].component_idx].comp_workspace;
            modbus_set_slave(my_workspace.ctx, comp_workspace.decoded_caches[0].thread_workspace->slave_address);
            for (auto& val : set_work.set_vals) {
                bool set_had_errors = false;
                int set_errors = 0;
                const auto& map_workspace = *comp_workspace.decoded_caches[val.thread_idx].thread_workspace;

                switch(map_workspace.reg_type) {
                    case Register_Types::Holding: {
                        const auto& curr_decode = map_workspace.decode_array[val.decode_idx];
                        encode(
                            set_buffer, 
                            curr_decode, 
                            val.set_val, 
                            val.bit_idx, 
                            curr_decode.flags.is_individual_enums() ? 0UL : 1UL, // TODO(WALKER): in the future, this will go into bit_str_encoding_array and use the "care_masks" there (for now it is unused)
                            curr_decode.bit_str_array_idx != Bit_Str_Array_All_Idx ? &val.prev_unsigned_val : nullptr);
                        if (curr_decode.flags.is_size_one() && !curr_decode.flags.is_multi_write_op_code()) { // special case for size 1 registers (apparently it doesn't like the multi-set if the size is 1, lame)
                            set_errors = modbus_write_register(my_workspace.ctx, curr_decode.offset, set_buffer[0]);
                        } else { // write 2 or 4 registers (or single register with multi_write_op_code set to true)
                            set_errors = modbus_write_registers(my_workspace.ctx, curr_decode.offset, curr_decode.flags.get_size(), set_buffer);
                        }
                        break;
                    }
                    case Register_Types::Coil: {
                        const auto offset = map_workspace.bool_offset_array[val.decode_idx];
                        set_errors = modbus_write_bit(my_workspace.ctx, offset, val.set_val.u == 1UL);
                        break;
                    }
                    default : {
                        set_errors = -2;
                    }
                    set_work.errno_code = errno;
                    set_had_errors = (set_errors < 0);
                    if (set_had_errors) {
                        // TODO(WALKER): make this better, for now just cowabunga print it (prevent spamming in the future)
                        NEW_FPS_ERROR_PRINT("IO thread #{}, set had errors it = {} -> \"{}\" \n", thread_id, set_work.errno_code, modbus_strerror(set_work.errno_code));
                        continue;
                    }
                }
            }
            continue; // comment this out if you want a poll to be serviced before doing another set?
        }
        // do polls after sets are clear:
        if (work_qs.poll_q.try_pop(poll_work)) {
            Pub_Work pub_work; // main reads off of its pub_q;
            pub_work.comp_idx   = poll_work.comp_idx;
            pub_work.thread_idx = poll_work.thread_idx;
            bool poll_had_errors = false;
            int poll_errors = 0;
            const auto& map_workspace = *client_workspace.msg_staging_areas[poll_work.comp_idx].comp_workspace->decoded_caches[poll_work.thread_idx].thread_workspace;

            // Do a poll into the appropriate register:
            modbus_set_slave(my_workspace.ctx, map_workspace.slave_address);
            //modbus_set_debug(my_workspace.ctx, 1);

            begin_poll_time = mono_clock.rdns();
            pub_work.errno_code = 0;

            switch(map_workspace.reg_type) {
                case Register_Types::Holding: {
                    poll_errors = modbus_read_registers(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u16);
                    break;
                }
                case Register_Types::Input: {
                    poll_errors = modbus_read_input_registers(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u16);
                    break;
                }
                case Register_Types::Coil: {
                    poll_errors = modbus_read_bits(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u8);
                    break;
                }
                case Register_Types::Discrete_Input: {
                    poll_errors = modbus_read_input_bits(my_workspace.ctx, map_workspace.start_offset, map_workspace.num_regs, poll_buffer_u8);
                    break;
                }
                default: { // Discrete_Input
                    poll_errors =  -2;  // TODO warn about unknown register type
                }
            }

            if (poll_errors < 0)
            {
                poll_had_errors =  true;
                pub_work.errno_code = errno;
            }

            end_poll_time = mono_clock.rdns();
            pub_work.response_time = std::chrono::nanoseconds{end_poll_time - begin_poll_time};

            // If poll had errors then report it here: 
            // we need to react correctly to different errors 
            if (poll_had_errors) {
                 if (pub_work.errno_code == 110)
                 {
                    modbus_close(my_workspace.ctx);
                    modbus_connect(my_workspace.ctx);
                 }
                 NEW_FPS_ERROR_PRINT("IO thread #{}, poll had errors it = {} -> \"{}\" \n", thread_id, pub_work.errno_code, modbus_strerror(pub_work.errno_code));
                 if (pub_work.errno_code == 115)
                 {
                    continue;
//                    modbus_close(my_workspace.ctx);
//                    modbus_connect(my_workspace.ctx);

                 }
                 if (
                        (pub_work.errno_code == 112345684)  //112345686 -> "Memory parity error"
                    || (pub_work.errno_code == 112345685)   //112345685 -> "Negative acknowledge"
                    || (pub_work.errno_code == 112345684)   //112345684 -> "Slave device or server is busy"
                    || (pub_work.errno_code == 112345683)   //112345683 -> "Acknowledge"
                 ) {
                    continue;
                 }
                 if (
                        (pub_work.errno_code == 112345682)  //112345682 -> "Slave device or server failure"
                    || (pub_work.errno_code == 112345681)   //112345681 -> "Illegal data value"
                    || (pub_work.errno_code == 112345680)   //112345680 -> "Illegal data address"
                    || (pub_work.errno_code == 112345679)   //112345679 -> "Illegal function"

                 ) {
                    return false;
                 }



                if (!main_work_qs.pub_q.try_emplace(std::move(pub_work))) {
                    NEW_FPS_ERROR_PRINT("IO thread #{} could not push onto the publish queue. This should NEVER happen. Exiting\n", thread_id);
                    return false;
                }
                main_work_qs.signaller.signal(); // signal main there is pub work to do
                continue;
            }
            // otherwise decode it based on the register type if we didn't have an error:
            Jval_buif decoded_val;
            auto& decoded_vals = pub_work.pub_vals;
            decoded_vals.reserve(map_workspace.num_decode);

            switch (map_workspace.reg_type) {
                // regular register stuff:
                case Register_Types::Holding: // fallthrough
                case Register_Types::Input: {
                    for (u8 decode_idx = 0; decode_idx < map_workspace.num_decode; ++decode_idx) {
                        const auto& decode_info = map_workspace.decode_array[decode_idx];
                        const auto raw = decode(poll_buffer_u16 + (decode_info.offset - map_workspace.start_offset), decode_info, decoded_val);
                        decoded_vals.emplace_back(Decoded_Val{decoded_val, raw});
                    }
                    break;
                }
                // binary stuff:
                case Register_Types::Coil: // fallthrough
                default: { // Discrete_Input
                    for (u8 decode_idx = 0; decode_idx < map_workspace.num_decode; ++decode_idx) {
                        const auto offset = map_workspace.bool_offset_array[decode_idx];
                        const auto raw = poll_buffer_u8[offset - map_workspace.start_offset];
                        decoded_val = raw;
                        decoded_vals.emplace_back(Decoded_Val{decoded_val, raw});
                    }
                }
            }
            if (!main_work_qs.pub_q.try_emplace(std::move(pub_work))) {
                NEW_FPS_ERROR_PRINT("IO thread #{} could not push onto the publish queue. This should NEVER happen. Exiting\n", thread_id);
                return false;
            }
            main_work_qs.signaller.signal();
            continue;
        }
    }
    return true;
}

static bool
main_loop() noexcept {
    auto& mono_clock       = main_workspace.mono_clock;
    auto& str_storage      = main_workspace.string_storage;
    auto& client_workspace = *main_workspace.client_workspaces[0];
    auto& fims_gateway     = client_workspace.fims_gateway;
    auto& work_qs          = client_workspace.main_work_qs;
    auto& io_work_qs       = client_workspace.conn_workspace.IO_thread_pool.io_work_qs;

    // work types:
    // bool time_to_debounce  = false;
    // bool time_to_heartbeat = false;
    bool time_to_poll      = false;
    Set_Work set_work; // from listener
    Pub_Work pub_work; // from IO_threads
    Get_Work get_work; // from listener
    // other helpful stuff:
    u8 current_min_poll_comp_idx = 0;
    // u8 current_min_heartbeat_comp_idx = 0;
    fmt::memory_buffer send_buf;

    auto calc_sleep_time = [&]() {
        std::chrono::nanoseconds min_poll_time_left{std::numeric_limits<s64>::max()};
        const auto current_time = mono_clock.rdns();
        for (u8 comp_idx = 0; comp_idx < client_workspace.num_comps; ++comp_idx) {
            auto& comp_workspace = *client_workspace.msg_staging_areas[comp_idx].comp_workspace;
            const auto current_frequency = comp_workspace.decoded_caches[0].thread_workspace[0].frequency;
            const auto poll_time_left = current_frequency - std::chrono::nanoseconds{current_time - comp_workspace.last_poll_time}; // last_poll_time is mono_clock.rdns() after publishing it

            // skip components that are already in the middle of polling (only queue/wait for components that need to queue their polls)
            if (!comp_workspace.is_polling && min_poll_time_left > poll_time_left) {
                min_poll_time_left = poll_time_left;
                current_min_poll_comp_idx = comp_idx; // note which component has the least amount of time left for the timeout
            }
        }
        time_to_poll = min_poll_time_left <= 0ns;
        return min_poll_time_left; // for now this is how often we sleep, in the future get debounce in here
    };

    /**
     * Priority of work for main:
     * 1. sets
     * 2. polls
     * 3. pubs (does heartbeat stuff too)
     * 4. gets
    */
    // TODO(WALKER): Figure out how IO_threads can alert main that they had to exit so main can make sure everyone shuts down properly
    while (client_workspace.keep_running) {
        work_qs.signaller.wait_for(calc_sleep_time());

        // do work in priority that it comes when woken up:
        if (!client_workspace.keep_running) return true; // something is wrong and either the listener or an IO thread stopped
        if (main_workspace.reload) return true; // time to reload the client (stop it and the threads)
        // sets take top priority:
        if (try_pop(work_qs.set_q, set_work)) {
            for (auto& val : set_work.set_vals) {
                if (val.bit_idx != Bit_All_Idx) {
                    const auto& comp_workspace = *client_workspace.msg_staging_areas[val.component_idx].comp_workspace;
                    const auto& decoded_cache  = comp_workspace.decoded_caches[val.thread_idx];
                    val.prev_unsigned_val = decoded_cache.decoded_vals[val.decode_idx].decoded_val.u;
                }
            }
            if (!io_work_qs.set_q.try_emplace(std::move(set_work))) {
                // TODO(WALKER): Error print here
                NEW_FPS_ERROR_PRINT_NO_ARGS("sets are backed up and this should NEVER happen. Exiting\n");
                return false;
            }
            io_work_qs.signaller.signal(); // tell IO threads there is work to do
            continue;
        }
        // then polls:
        if (time_to_poll) {
            auto& comp_workspace = *client_workspace.msg_staging_areas[current_min_poll_comp_idx].comp_workspace;
            for (u8 map_idx = 0; map_idx < comp_workspace.num_threads; ++map_idx) {
                if (!io_work_qs.poll_q.try_emplace(Poll_Work{current_min_poll_comp_idx, map_idx})) {
                    NEW_FPS_ERROR_PRINT_NO_ARGS("Ran out of poll request queue space. This should NEVER happen. Exiting\n");
                    return false;
                }
            }
            for (u8 map_idx = 0; map_idx < comp_workspace.num_threads; ++map_idx) {
                io_work_qs.signaller.signal();
            }
            // set this component to a polling state for publishing later:
            comp_workspace.is_polling = true;
            comp_workspace.num_polls_in_q_left = comp_workspace.num_threads;
            continue;
        }
        // then pubs:
        if (work_qs.pub_q.try_pop(pub_work)) { // IO_threads produce onto this for main to consume
            // TODO(WALKER): Do pub work here (jsonify, etc.)
            auto& comp_workspace = *client_workspace.msg_staging_areas[pub_work.comp_idx].comp_workspace;
            auto& decoded_cache = comp_workspace.decoded_caches[pub_work.thread_idx];
            bool is_heartbeat_mapping = comp_workspace.heartbeat_enabled && comp_workspace.heartbeat_read_thread_idx == pub_work.thread_idx;

            // TODO(WALKER): Make sure that heartbeat is taken into account in the case of an errno for this?
            // probably just want to move the heartbeat code into this really?
            auto print_poll_if_finished = [&]() {
                if(pub_work.errno_code != 0) 
                    NEW_FPS_ERROR_PRINT("Component error while polling [] -> {} \n", pub_work.errno_code, modbus_strerror(pub_work.errno_code));
                if (--comp_workspace.num_polls_in_q_left == 0) {
                    // reset our error checking variables real quick:
                    if (!comp_workspace.has_error_checked_already) comp_workspace.num_poll_errors = 0; // only reset if we had no errors this cycle
                    comp_workspace.has_error_checked_already = false;
                    // time to publish:
                    bool has_one_change = false;
                    send_buf.clear();
                    if( pub_work.errno_code == 0)
                    {
                    send_buf.push_back('{'); // begin object
                    for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
                    {
                        auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];

                        // if (curr_decoded_cache.changed_mask.none()) continue; // skip stringifying caches that have no changes
                        has_one_change = true;

                        if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                // if (!curr_decoded_cache.changed_mask[decoded_idx]) continue; // skip values that have no changes

                                auto& curr_decoded_info = curr_decoded_cache.decoded_vals[decoded_idx];
                                if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
                                {
                                    FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_info.decoded_val);
                                }
                                else // format using "bit_strings":
                                {
                                    const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
                                    const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

                                    if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
                                    {
                                        for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
                                        {
                                            const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
                                            // if (((curr_bit_str_info.changed_mask >> bit_str.begin_bit) & 1UL) != 1UL) continue; // skip bits that have not changed since last time
                                            FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(bit_str.str), ((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
                                        }
                                    }
                                    else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
                                    {
                                        FORMAT_TO_BUF(send_buf, R"("{}":[)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]));
                                        bool has_one_bit_high = false;
                                        u8 last_idx = 0;
                                        for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
                                        {
                                            const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
                                            last_idx = bit_str.end_bit + 1;
                                            if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
                                            {
                                                if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                                {
                                                    has_one_bit_high = true;
                                                    FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
                                                                        bit_str.begin_bit, 
                                                                        str_storage.get_str(bit_str.str));
                                                }
                                            }
                                            else // "unknown" bits that are "inbetween" bits (and not ignored):
                                            {
                                                for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
                                                {
                                                    if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
                                                    {
                                                        has_one_bit_high = true;
                                                        FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
                                                    }
                                                }
                                            }
                                        }
                                        // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                                        const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
                                        for (; last_idx < num_bits; ++last_idx)
                                        {
                                            if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
                                            {
                                                has_one_bit_high = true;
                                                FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
                                            }
                                        }
                                        send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
                                        send_buf.append("],"sv); // finish the array
                                    }
                                    else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                                    {
                                        bool enum_found = false;
                                        for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
                                        {
                                            const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
                                            if (curr_unsigned_val == enum_str.enum_val)
                                            {
                                                enum_found = true;
                                                FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"{}"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val, str_storage.get_str(enum_str.str));
                                                break;
                                            }
                                        }
                                        if (!enum_found) // config did not account for this value:
                                        {
                                            FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"Unknown"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val);
                                        }
                                    }
                                    // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
                                }
                            }
                        }
                        else // Coil and Discrete Input:
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                // if (!curr_decoded_cache.changed_mask[decoded_idx]) continue; // skip values that have no changes
                                FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_cache.bool_vals[decoded_idx]);
                            }
                        }
                    }
                    // do heartbeat stuff here if we have it:
                    // TODO(WALKER): Finish this up when you do heartbeat stuff
                    if (comp_workspace.heartbeat_enabled)
                    {
                        has_one_change = true;
                        const auto curr_decoded_val = comp_workspace.decoded_caches[comp_workspace.heartbeat_read_thread_idx].decoded_vals[comp_workspace.heartbeat_read_decode_idx].decoded_val;
                        FORMAT_TO_BUF(send_buf, R"("modbus_heartbeat":{},"component_connected":{},)", curr_decoded_val, comp_workspace.component_connected);
                    }

                    // pub out the strings if we have changes:
                    if (has_one_change)
                    {
                        // "Timestamp" stuff:
                        const auto timestamp       = std::chrono::system_clock::now();
                        const auto timestamp_micro = time_fraction<std::chrono::microseconds>(timestamp);
                        FORMAT_TO_BUF_NO_COMPILE(send_buf, R"("Timestamp":"{:%m-%d-%Y %T}.{}"}})", timestamp, timestamp_micro.count());

                        // send_buf.resize(send_buf.size() - 1); // This gets rid of the last excessive ',' character
                        // send_buf.push_back('}');
                        if (!send_pub(fims_gateway, str_storage.get_str(comp_workspace.comp_uri), std::string_view{send_buf.data(), send_buf.size()}))
                        {
                            NEW_FPS_ERROR_PRINT_NO_ARGS("Main, cannot publish onto fims, exiting\n");
                            return false;
                        }
                    }
                    }
                    // reset variables for polling:
                    comp_workspace.is_polling = false;
                    comp_workspace.poll_time_left = decoded_cache.thread_workspace->frequency;
                    comp_workspace.last_poll_time = mono_clock.rdns();
                }
                return true;
            };

            // record response times:
            if (comp_workspace.min_response_time > pub_work.response_time) { comp_workspace.min_response_time = pub_work.response_time; }
            if (comp_workspace.max_response_time < pub_work.response_time) { comp_workspace.max_response_time = pub_work.response_time; }
            const auto prev_avg = comp_workspace.avg_response_time * comp_workspace.num_timings_recorded;
            ++comp_workspace.num_timings_recorded;
            comp_workspace.avg_response_time = (prev_avg + pub_work.response_time) / comp_workspace.num_timings_recorded;
            // process pub:
            if (pub_work.errno_code) {
                //104 -> "Connection reset by peer"
                //32 -> "Broken pipe"
                //110 -> "Connection Timed Out"
                //115 -> "Operation now in pogress"
                NEW_FPS_ERROR_PRINT("Component \"{}\", map array #{} had an error while polling, it is {} -> \"{}\"\n",
                      str_storage.get_str(comp_workspace.comp_uri), pub_work.thread_idx, pub_work.errno_code, modbus_strerror(pub_work.errno_code));
                if ((pub_work.errno_code == 104) || (pub_work.errno_code == 32))
                {
                    // no point in trying again
                    return false;
                }

                if ((pub_work.errno_code != 115))
                {
                    if (!comp_workspace.has_error_checked_already) {
                        comp_workspace.has_error_checked_already = true;
                        ++comp_workspace.num_poll_errors;
                        if (comp_workspace.num_poll_errors >= 5) {
                            NEW_FPS_ERROR_PRINT("Component \"{}\" had 5 or more poll error cycles in a row. Exiting\n", str_storage.get_str(comp_workspace.comp_uri));
                            return false;
                        }
                    }
                }
                if (!print_poll_if_finished()) return false;
                continue;
            }
            switch(decoded_cache.reg_type) {
                case Register_Types::Holding: // fallthrough
                case Register_Types::Input: {
                    for (std::size_t i = 0; i < pub_work.pub_vals.size(); ++i) {
                        auto& prev_val = decoded_cache.decoded_vals[i];
                        auto& new_val = pub_work.pub_vals[i];
                        prev_val.raw_data = new_val.raw_data;
                        if (prev_val.bit_str_array_idx == Bit_Str_All_Idx) { // regular if changed check:
                            decoded_cache.changed_mask[i] = prev_val.decoded_val.u != new_val.decoded_val.u;
                        } else { // masking required:
                        // PHIL TO CHECK this with Jack S
                        //auto curr_unsigned_val = val.decoded_val.get_uint_unsafe(); val.decoded_val = curr_unsigned_val;
                            auto& bit_str_info = decoded_cache.bit_strings_arrays[prev_val.bit_str_array_idx];
                            const auto temp_val = new_val.decoded_val.u & bit_str_info.care_mask;
                            decoded_cache.changed_mask[i] = (prev_val.decoded_val.u & bit_str_info.care_mask) != temp_val;
                        }
                        // heartbeat stuff:
                        if (is_heartbeat_mapping && comp_workspace.heartbeat_read_decode_idx == i) {
                            if (decoded_cache.changed_mask[i]) {
                                if (comp_workspace.component_connected == false) {
                                    if (!emit_event(send_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Alarm,
                                            R"(Heartbeat for component "{}" reconnected.)", str_storage.get_str(comp_workspace.comp_uri)))
                                    {
                                        NEW_FPS_ERROR_PRINT_NO_ARGS("Cannot send an event out on fims. Exiting\n");
                                        return false;
                                    }
                                }
                                comp_workspace.component_connected = true;
                                comp_workspace.heartbeat_last_change_time = mono_clock.rdns();
                            }
                            // now send out the heartbeat_val + 1 to be set (if we have a write idx):
                            if (comp_workspace.heartbeat_write_thread_idx != Thread_All_Idx) {
                                Set_Work heartbeat_set;
                                auto heartbeat_val = new_val.decoded_val;
                                ++heartbeat_val.u;
                                heartbeat_set.set_vals.emplace_back(Set_Info{heartbeat_val, heartbeat_val.u, pub_work.comp_idx, comp_workspace.heartbeat_write_thread_idx, comp_workspace.heartbeat_write_decode_idx}); // NOTE(WALKER): heartbeat registers can't have bit_strings stuff anyway, so bit_idx and enum_idx don't matter
                                if (!io_work_qs.set_q.try_emplace(std::move(heartbeat_set))) {
                                    // TODO(WALKER): Error print here
                                    NEW_FPS_ERROR_PRINT_NO_ARGS("sets are backed up and this should NEVER happen. Exiting\n");
                                    return false;
                                }
                                io_work_qs.signaller.signal(); // tell IO threads there is work to do
                            }
                        }
                        if (prev_val.bit_str_array_idx != Bit_Str_All_Idx) { // to tell which bits changed since last time:
                            auto& bit_str_info = decoded_cache.bit_strings_arrays[prev_val.bit_str_array_idx];
                            bit_str_info.changed_mask = (prev_val.decoded_val.u ^ new_val.decoded_val.u) & bit_str_info.care_mask;
                        }
                        prev_val.decoded_val = new_val.decoded_val;
                    }
                    if (is_heartbeat_mapping) {
                        if (comp_workspace.heartbeat_timeout > std::chrono::nanoseconds{mono_clock.rdns() - comp_workspace.heartbeat_last_change_time}) {
                            // TODO(WALKER): get the "rising edge" print in here on heartbeat disconnect
                            if (comp_workspace.component_connected == true) {
                                if (!emit_event(send_buf, client_workspace.fims_gateway, Client_Event_Source, Event_Severity::Alarm,
                                        R"(Heartbeat for component "{}" disconnected.)", str_storage.get_str(comp_workspace.comp_uri)))
                                {
                                    NEW_FPS_ERROR_PRINT_NO_ARGS("Cannot send an event out on fims. Exiting\n");
                                    return false;
                                }
                            }
                            comp_workspace.component_connected = false;
                        }
                    }
                    break;
                }
                case Register_Types::Coil: // fallthrough
                default: { // Discrete_Input
                    for (std::size_t i = 0; i < pub_work.pub_vals.size(); ++i) {
                        const auto prev_val = decoded_cache.bool_vals[i];
                        const auto& new_val = pub_work.pub_vals[i];
                        decoded_cache.changed_mask[i] = prev_val != new_val.decoded_val.u;
                    }
                }
            }
            // then if we are on the last pub to come back publish it then set the current time as the last_poll_time:
            if (!print_poll_if_finished()) return false;
            continue;
        }
        // then gets:
        if (try_pop(work_qs.get_q, get_work)) {
            auto& comp_workspace = *client_workspace.msg_staging_areas[get_work.comp_idx].comp_workspace;
            send_buf.clear();
            if (get_work.thread_idx == Thread_All_Idx) // They want everything:
            {
                send_buf.push_back('{');
                if (!get_work.flags.flags) // normal get (no flags set):
                {
                    for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
                    {
                        auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];
                        if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                auto& curr_decoded_info = curr_decoded_cache.decoded_vals[decoded_idx];
                                if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
                                {
                                    FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_info.decoded_val);
                                }
                                else // format using "bit_strings":
                                {
                                    const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
                                    const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

                                    if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
                                    {
                                        for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
                                        {
                                            const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
                                            FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(bit_str.str), ((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL);
                                        }
                                    }
                                    else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
                                    {
                                        FORMAT_TO_BUF(send_buf, R"("{}":[)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]));
                                        bool has_one_bit_high = false;
                                        u8 last_idx = 0;
                                        for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
                                        {
                                            const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
                                            last_idx = bit_str.end_bit + 1;
                                            if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
                                            {
                                                if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                                {
                                                    has_one_bit_high = true;
                                                    FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
                                                                        bit_str.begin_bit, 
                                                                        str_storage.get_str(bit_str.str));
                                                }
                                            }
                                            else // "unknown" bits that are "inbetween" bits (and not ignored):
                                            {
                                                for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
                                                {
                                                    if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
                                                    {
                                                        has_one_bit_high = true;
                                                        FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
                                                    }
                                                }
                                            }
                                        }
                                        // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                                        const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
                                        for (; last_idx < num_bits; ++last_idx)
                                        {
                                            if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
                                            {
                                                has_one_bit_high = true;
                                                FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
                                            }
                                        }
                                        send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
                                        send_buf.append("],"sv); // finish the array
                                    }
                                    else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                                    {
                                        bool enum_found = false;
                                        for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
                                        {
                                            const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
                                            if (curr_unsigned_val == enum_str.enum_val)
                                            {
                                                enum_found = true;
                                                FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"{}"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val, str_storage.get_str(enum_str.str));
                                                break;
                                            }
                                        }
                                        if (!enum_found) // config did not account for this value:
                                        {
                                            FORMAT_TO_BUF(send_buf, R"("{}":[{{"value":{},"string":"Unknown"}}],)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_unsigned_val);
                                        }
                                    }
                                    // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
                                }
                            }
                        }
                        else // Coil and Discrete Input:
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                FORMAT_TO_BUF(send_buf, R"("{}":{},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_cache.bool_vals[decoded_idx]);
                            }
                        }
                    }
                }
                else if (get_work.flags.is_raw_request()) // raw get: 
                {
                    for (u8 thread_idx = 0; thread_idx < comp_workspace.num_threads; ++thread_idx)
                    {
                        const auto& curr_decoded_cache = comp_workspace.decoded_caches[thread_idx];
                        if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                const auto& curr_decode_info = curr_decoded_cache.decoded_vals[decoded_idx];
                                FORMAT_TO_BUF(send_buf, R"("{0}":{{"value":{1},"binary":"{1:0>{2}b}","hex":"{1:0>{3}X}"}},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decode_info.raw_data, curr_decode_info.flags.get_size() * 16, curr_decode_info.flags.get_size() * 4);
                            }
                        }
                        else // Coil and Discrete Input:
                        {
                            for (u8 decoded_idx = 0; decoded_idx < curr_decoded_cache.num_decode; ++decoded_idx)
                            {
                                FORMAT_TO_BUF(send_buf, R"("{0}":{{"value":{1},"binary":"{1:b}","hex":"{1:X}"}},)", str_storage.get_str(curr_decoded_cache.decode_ids[decoded_idx]), curr_decoded_cache.bool_vals[decoded_idx]);
                            }
                        }
                    }
                }
                else // timings requests (could also be a reset):
                {
                    if (get_work.flags.is_reset_timings_request())
                    {
                        comp_workspace.min_response_time = timings_duration_type{std::numeric_limits<f64>::max()};
                        comp_workspace.max_response_time = timings_duration_type{0.0};
                        comp_workspace.avg_response_time = timings_duration_type{0.0};
                        comp_workspace.num_timings_recorded = 0.0;
                        continue; // don't send anything out
                    }
                    FORMAT_TO_BUF(send_buf, R"("min_response_time":"{}","max_response_time":"{}","avg_response_time":"{}","num_timings_recorded":{},)", 
                        comp_workspace.min_response_time, comp_workspace.max_response_time, comp_workspace.avg_response_time, comp_workspace.num_timings_recorded);
                }
                send_buf.resize(send_buf.size() - 1); // gets rid of the last excessive ',' character
                send_buf.push_back('}');
            }
            else // They want a single variable (no timings requests, no key -> POSSIBLE TODO(WALKER): do they want it "clothed", I assume unclothed for now, maybe have a config option for this?):
            {
                // NOTE(WALKER): the check for single get timings request is done in listener thread.
                auto& curr_decoded_cache = comp_workspace.decoded_caches[get_work.thread_idx];
                if (curr_decoded_cache.reg_type == Register_Types::Holding || curr_decoded_cache.reg_type == Register_Types::Input)
                {
                    auto& curr_decoded_info = curr_decoded_cache.decoded_vals[get_work.decode_idx];
                    if (!get_work.flags.flags) // normal get (no flags):
                    {
                        if (!curr_decoded_info.flags.is_bit_string_type()) // normal formatting:
                        {
                            FORMAT_TO_BUF(send_buf, R"({})", curr_decoded_info.decoded_val);
                        }
                        else // format using "bit_strings":
                        {
                            const auto curr_unsigned_val = curr_decoded_info.decoded_val.get_uint_unsafe();
                            const auto& curr_bit_str_info = curr_decoded_cache.bit_strings_arrays[curr_decoded_info.bit_str_array_idx];

                            if (curr_decoded_info.flags.is_individual_bits()) // resolve to multiple uris (true/false per bit):
                            {
                                FORMAT_TO_BUF(send_buf, R"({})", ((curr_unsigned_val >> get_work.bit_idx) & 1UL) == 1UL);
                            }
                            else if (curr_decoded_info.flags.is_bit_field()) // resolve to array of strings for each bit/bits that is/are == the expected value (1UL for individual bits):
                            {
                                send_buf.push_back('[');
                                bool has_one_bit_high = false;
                                u8 last_idx = 0;
                                for (u8 bit_str_idx = 0; bit_str_idx < curr_bit_str_info.num_bit_strs; ++bit_str_idx)
                                {
                                    const auto& bit_str = curr_bit_str_info.bit_strs[bit_str_idx];
                                    last_idx = bit_str.end_bit + 1;
                                    if (!bit_str.flags.is_unknown()) // "known" bits that are not ignored:
                                    {
                                        if (((curr_unsigned_val >> bit_str.begin_bit) & 1UL) == 1UL) // only format bits that are high:
                                        {
                                            has_one_bit_high = true;
                                            FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"{}"}},)",
                                                                bit_str.begin_bit, 
                                                                str_storage.get_str(bit_str.str));
                                        }
                                    }
                                    else // "unknown" bits that are "inbetween" bits (and not ignored):
                                    {
                                        for (u8 unknown_idx = bit_str.begin_bit; unknown_idx <= bit_str.end_bit; ++unknown_idx)
                                        {
                                            if (((curr_unsigned_val >> unknown_idx) & 1UL) == 1UL) // only format bits that are high:
                                            {
                                                has_one_bit_high = true;
                                                FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", unknown_idx);
                                            }
                                        }
                                    }
                                }
                                // all the other bits that we haven't masked out during the initial decode step that are high we print out as "Unknown" (all at the end of the array):
                                const u8 num_bits = curr_decoded_info.flags.get_size() * 16;
                                for (; last_idx < num_bits; ++last_idx)
                                {
                                    if (((curr_unsigned_val >> last_idx) & 1UL) == 1UL) // only format bits that are high:
                                    {
                                        has_one_bit_high = true;
                                        FORMAT_TO_BUF(send_buf, R"({{"value":{},"string":"Unknown"}},)", last_idx);
                                    }
                                }
                                send_buf.resize(send_buf.size() - (1 * has_one_bit_high)); // this gets rid of the last excessive ',' character
                                send_buf.push_back(']'); // finish the array
                            }
                            else if (curr_decoded_info.flags.is_enum()) // resolve to single string based on current input value (unsigned whole numbers):
                            {
                                bool enum_found = false;
                                for (u8 enum_str_idx = 0; enum_str_idx < curr_bit_str_info.num_bit_strs; ++enum_str_idx)
                                {
                                    const auto& enum_str = curr_bit_str_info.bit_strs[enum_str_idx];
                                    if (curr_unsigned_val == enum_str.enum_val)
                                    {
                                        enum_found = true;
                                        FORMAT_TO_BUF(send_buf, R"([{{"value":{},"string":"{}"}}])", curr_unsigned_val, str_storage.get_str(enum_str.str));
                                        break;
                                    }
                                }
                                if (!enum_found) // config did not account for this value:
                                {
                                    FORMAT_TO_BUF(send_buf, R"([{{"value":{},"string":"Unknown"}}])", curr_unsigned_val);
                                }
                            }
                            // TODO(WALKER): In the future make sure to do "enum_field" and "individual_enums"
                        }
                    }
                    else if (get_work.flags.is_raw_request()) // they want raw data (format it):
                    {
                        FORMAT_TO_BUF(send_buf, R"({{"value":{0},"binary":"{0:0>{1}b}","hex":"{0:0>{2}X}"}})", curr_decoded_info.raw_data, curr_decoded_info.flags.get_size() * 16, curr_decoded_info.flags.get_size() * 4);
                    }
                }
                else // Coil and Discrete Input:
                {
                    const auto curr_bool_val = curr_decoded_cache.bool_vals[get_work.decode_idx];
                    if (!get_work.flags.flags) // normal get (no flags):
                    {
                        FORMAT_TO_BUF(send_buf, R"({})", curr_bool_val);
                    }
                    else if (get_work.flags.is_raw_request()) // they want raw data (format it):
                    {
                        FORMAT_TO_BUF(send_buf, R"({{"value":{0},"binary":"{0:b}","hex":"{0:X}"}})", curr_bool_val);
                    }
                }
            }
            // Send out using replyto here:
            if (!send_set(fims_gateway, get_work.replyto, std::string_view{send_buf.data(), send_buf.size()}))
            {
                NEW_FPS_ERROR_PRINT("could not send reply to uri \"{}\" over fims, exiting\n", get_work.replyto);
                return false;
            }
            continue;
        }
    }
    return true;
}

int main(const int argc, const char* argv[]) noexcept {
    bool has_done_mono_clock_calibrate = false;
    main_workspace.mono_clock.init();
    const auto args = parse_command_line_arguments(argc, argv);
    if (args.first == Arg_Types::Error) return EXIT_FAILURE;
    if (args.first == Arg_Types::Help)  return EXIT_SUCCESS;

    do
    {
        if (main_workspace.reload)
        {
            // cleanup stuff here if we reload (all in main_workspace):
            main_workspace.cleanup();
            free(main_workspace.arena.data);
            main_workspace.arena.data = nullptr;
            main_workspace.arena.allocated = 0;
            main_workspace.arena.current_idx = 0;
            main_workspace.client_workspaces = nullptr;
            main_workspace.string_storage.data = nullptr;
        }
        main_workspace.reload = false; // reset reload here if we had one

        if (!load_config(args)) return EXIT_FAILURE;
        if (args.first == Arg_Types::Expand) return EXIT_SUCCESS;

        for (u8 client_idx = 0; client_idx < main_workspace.num_clients; ++client_idx)
        {
            // launch client listener thread:
            auto& client_workspace = *main_workspace.client_workspaces[client_idx];
            if (!client_workspace.conn_workspace.connected) continue; // skip clients that aren't connected (TODO(WALKER): will do restart logic later)

            client_workspace.keep_running = true;
            client_workspace.listener_future = std::async(std::launch::async, listener_thread, client_idx);

            // launch IO threads:
            for (u8 io_thread_idx = 0; io_thread_idx < client_workspace.conn_workspace.IO_thread_pool.num_conns; ++io_thread_idx) {
                auto& io_thread = client_workspace.conn_workspace.IO_thread_pool.io_thread_workspaces[io_thread_idx];
                io_thread.keep_running = true;
                io_thread.thread_future = std::async(std::launch::async, IO_thread, io_thread_idx);
            }
        }
        if (!has_done_mono_clock_calibrate)
        {
            has_done_mono_clock_calibrate = true;
            // wait a little bit before sending the "start" signal for all the threads (wait 2 seconds and synchronize mono_clock, then tell threads to start):
            NEW_FPS_ERROR_PRINT_NO_ARGS("Waiting 2 seconds to initialize the Mono clock before beginning the main program\n");
            std::this_thread::sleep_for(2s);
            NEW_FPS_ERROR_PRINT("Mono clock finished synchronizing after a 2 second wait, this hardware's frequency = {} GHz. Beginning main program\n", main_workspace.mono_clock.calibrate());
        }

        main_workspace.start_signal = true;
        main_workspace.main_cond.notify_all(); // tell listener to start doing work

        // If main_loop() has an error then we fully stop the client
        const auto main_had_errors = !main_loop();
        main_workspace.client_workspaces[0]->stop_client();
        if (main_had_errors) {
            // TODO(WALKER): Figure out what to do here? (probably nothing)
            // FPS_ERROR_PRINT();
        }
    } while (main_workspace.reload);
}
