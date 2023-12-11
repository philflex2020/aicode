#pragma once

#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <iostream>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include <typeinfo>
#include <string_view>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fims/fims.h>
#include <fims/libfims.h>
#include <fims/defer.hpp>

extern "C"{
    #include "tmwscl/utils/tmwsim.h"
}

#include "shared_utils.hpp"
#include "spdlog/fmt/fmt.h"
#include "simdjson_noexcept.hpp"
#include "Jval_buif.hpp"


using namespace std::literals::string_view_literals;

struct GcomSystem;

enum class FimsMethod : u8
{
    Set,
    Get,
    Pub,
    Unknown
};

enum class FimsFormat : u8 {
    Naked,
    Clothed,
    Full
};

enum FimsEventSeverity : uint8_t
{
    Debug = 0,
    Info = 1,
    Status = 2,
    Alarm = 3,
    Fault = 4
};


/**
 * @brief Send a fims pub to a given uri with a given message body.
 * 
 * @param fims_gateway a connected fims socket
 * @param uri string view of the uri to send the message to
 * @param body string view of the message body
 * 
 * @pre fims_gateway is connected to fims
*/
bool send_pub(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept;

/**
 * @brief Send a fims set to a given uri with a given message body.
 * 
 * @param fims_gateway a connected fims socket
 * @param uri string view of the uri to send the message to
 * @param body string view of the message body
 * 
 * @pre fims_gateway is connected to fims
*/
bool send_set(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept;

/**
 * @brief Send a fims post to a given uri with a given message body.
 * 
 * @param fims_gateway a connected fims socket
 * @param uri string view of the uri to send the message to
 * @param body string view of the message body
 * 
 * @pre fims_gateway is connected to fims
*/
bool send_post(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept;

/**
 * @brief Emits an event to /events
 * 
 * @param send_buf a memory buffer to generate the message into
 * @param fims_gateway a connected fims socket
 * @param source the process name (source) of the event
 * @param severity the event severity level (debug, info, status, alarm, or fault)
 * @param fmt_str the message we want to emit with the event
 * @param fmt_args the corresponding arguments for the fmt_str message
 * 
 * @pre fims_gateway is connected to fims
*/
template<std::size_t Size, typename... Fmt_Args>
static bool emit_event(fmt::basic_memory_buffer<char, Size>& send_buf, fims& fims_gateway, std::string_view source, FimsEventSeverity severity, fmt::format_string<Fmt_Args...> fmt_str, Fmt_Args&&... fmt_args) noexcept
{
    send_buf.clear();

    FORMAT_TO_BUF(send_buf, R"({{"source":"{}","message":")", source);
    FORMAT_TO_BUF_NO_COMPILE(send_buf, fmt_str, std::forward<Fmt_Args>(fmt_args) ...);
    FORMAT_TO_BUF(send_buf, R"(","severity":{}}})", severity);

    return send_post(fims_gateway, "/events"sv, std::string_view{send_buf.data(), send_buf.size()});
}

/**
 * @brief Initialize the receiver buffer for fims, create a subscription string for the system ID (sys.id),
 * and generate the process name based on whether we are dealing with a client or server.
 * 
 * @param sys A partially initialized GcomSystem, in which "system" information has been populated
 * but not necessarily any data points
 * 
 * @pre sys.protocol_dependencies->who, sys.fims_dependencies->data_buf_len, and sys.config_file_name have
 * been set appropriately
*/
bool init_fims(GcomSystem &sys);

/**
 * @brief Add a single uri to sys.fims_dependencies->subs
 * 
 * @param sys A partially initialized GcomSystem, in which "system" information has been populated
 * but not necessarily any data points
 * @param name The subscription to add. Must include a '/' as the first character.
*/
bool add_fims_sub(GcomSystem &sys, std::string name);

/**
 * @brief Print and log the contents of sys.fims_dependencies->subs.
 * 
 * @param sys A partially initialized GcomSystem, in which sys.fims_dependencies->subs has been populated
*/
bool show_fims_subs(GcomSystem &sys);

/**
 * @brief Connect to fims using sys.fims_dependencies->name and subscribe to all uris in sys.fims_dependencies->subs.
 * 
 * @param sys a GcomSystem with a valid sys.fims_dependencies->name (no spaces) and a valid vector of uris to
 * subscribe to
*/
bool fims_connect(GcomSystem &sys);

/**
 * @brief Parse the value in a key-value pair in a JSON object.
 * 
 * Of the format <value> or "value": <value>
 * 
 * @param val_clothed In a key-value pair, val_clothed represents the value as an object. This may or may not be valid based
 * on the particular JSON message. (Either this will be valid or curr_val will be valid.) Example: {"value": 5}
 * @param curr_val In a key-value pair, curr_val represents the value as a raw value. This may or may not be valid based
 * on the particular JSON message. (Either this will be valid or val_clothed will be valid.) Example: 5
 * @param to_set a Jval_buif that will be se to the value in the json object
 * 
 * @pre val_clothed and curr_val contain the results from parsing a simdjson doc object down to a key-value pair
*/
bool extractValueMulti(GcomSystem &sys, simdjson::simdjson_result<simdjson::fallback::ondemand::object> &val_clothed, simdjson::fallback::ondemand::value &curr_val, Jval_buif &to_set);

/**
 * @brief Parse the value in a single-item JSON message
 * 
 * Of the format <value> or {"value": <value>}
 * 
 * @param sys GcomSystem with sys.fims_dependencies->doc and pre-parsed sys.fims_dependencies->uri_view
 * @param val_clothed In a single-value message, val_clothed represents the value as an object. This may or may not be valid based
 * on the particular JSON message. (Either this will be valid or curr_val will be valid.) Example: {"value": 5}
 * @param curr_val In a key-value pair, curr_val represents the value as a raw value. This may or may not be valid based
 * on the particular JSON message. (Either this will be valid or val_clothed will be valid.) Example: 5
 * @param to_set a Jval_buif that will be se to the value in the json object
 * 
 * @pre val_clothed and curr_val contain the results from parsing a simdjson doc object down to a key-value pair
*/
bool extractValueSingle(GcomSystem &sys, simdjson::simdjson_result<simdjson::fallback::ondemand::object> &val_clothed, simdjson::fallback::ondemand::value &curr_val, Jval_buif &to_set);

/**
 * @brief Convert a Jval_buif to a double, regardless of the subtype.
 * 
 * @param to_set the Jval_buif to convert to a double
*/
double jval_to_double(Jval_buif &to_set);

/**
 * @brief Parse header data for an incoming fims message, where data is stored in data_buf.
 * 
 * Extract method, uri, reply_to, process_name, username, and message body for an incoming fims message.
 * Look for any request headers that will be processed later in processCmds. Catch any basic errors that
 * result from looking at the request header (such as a "get" with an empty reply-to or an invalid "set"
 * request to an outstation).
 * 
 * @param sys a partially initialized GcomSystem (with at least base_uri, local_uri, and "who" initialized)
 * @param meta_data a fims Meta_Data_Info object from an incoming fims message
 * @param data_buf the full data buffer received over fims
 * @param data_buf_len the full length of the received data
*/
bool parseHeader(GcomSystem &sys, Meta_Data_Info& meta_data, char* data_buf, uint32_t data_buf_len);
bool processCmds(GcomSystem &sys,  Meta_Data_Info& meta_data, void* data_buf, uint32_t data_buf_len);
bool gcom_recv_raw_message(fims &fims_gateway, Meta_Data_Info& meta_data, void* data_buf, uint32_t data_buf_len) noexcept;
bool listener_thread(GcomSystem &sys ) noexcept;
bool uriIsMultiOrSingle(GcomSystem &sys, std::string_view uri);
void replyToFullGet(GcomSystem &sys, fmt::memory_buffer &send_buf);

/**
 * @brief Format a naked TMWSIM_POINT value to a pre-initialized memory buffer.
 * 
 * Format an analog value divided by its scale using a %g flag. Format a binary value
 * based on its crob_int and crob_string values. If it's a crob_int, output the value as 0 or 1.
 * If it's a crob_string, output the value using the specified crob_true and crob_false strings.
 * If it's neither crob_int or crob_string, then output the binary value as true or false. If the
 * scale is negative for a binary value, invert the value (true becomes false and vice versa).
 * 
 * @param send_buf fmt::memory_buffer to store the output string
 * @param dbPoint TMWSIM_POINT * to a pre-initialized dbPoint
*/
void formatPointValue(fmt::memory_buffer &send_buf, TMWSIM_POINT *dbPoint);

/**
 * @brief Generate the message body for the reply to a fims-get on one of the uris
 * of the GcomSystem, depending on if it's a multi-point or single-point uri.
 * 
 * @param sys a fully-initilized GcomSystem with an active TMW session
 * @param send_buf fmt::memory_buffer to store the message body output string
*/
void replyToGet(GcomSystem &sys, fmt::memory_buffer &send_buf);