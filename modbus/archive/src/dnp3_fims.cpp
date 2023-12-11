// Run using: g++ -g -std=c++17 -o gcom_fims -I ../include -I /usr/local/include -I ../TMW -I ../TMW/tmwscl/tmwtarg/LinIoTarg -I ../TMW/tmwscl/tmwtarg gcom_fims.cpp -lfims -lsimdjson -lIoTarg -ldnp -lutils
#include "dnp3_fims.h"
#include <map>
#include <chrono>
#include <fstream>
#include "system_structs.h"
#include "dnp3_stats.h"
#include "Jval_buif.hpp"
#include "logger/logger.h"
#include "dnp3_flags.h"
#include "shared_utils.hpp"
#include "dnp3_utils.h"

using namespace std::string_view_literals; // gives us sv

bool send_pub(fims &fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"pub", sizeof("pub") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}

bool send_set(fims &fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"set", sizeof("set") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}

// used for emit_event
bool send_post(fims &fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"post", sizeof("post") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}

bool init_fims(GcomSystem &sys)
{
    sys.fims_dependencies->receiver_bufs.data_buf_len = sys.fims_dependencies->data_buf_len;
    sys.fims_dependencies->receiver_bufs.data_buf = reinterpret_cast<uint8_t *>(malloc(sys.fims_dependencies->receiver_bufs.data_buf_len));
    sys.fims_dependencies->subs.push_back("/" + std::string(sys.id));

    // get the process name
    std::string process = "dnp3_client@";
    if (sys.protocol_dependencies->who == DNP3_OUTSTATION)
    {
        process = "dnp3_server@";
    }
    process += sys.config_file_name;
    sys.fims_dependencies->name = process;

    return true;
}

bool add_fims_sub(GcomSystem &sys, std::string name)
{
    sys.fims_dependencies->subs.push_back(name);
    return true;
}

bool show_fims_subs(GcomSystem &sys)
{
    std::string subs = "Subscribed to:\n";
    for (auto s : sys.fims_dependencies->subs)
    {
        subs += "\t" + s + ", \n";
    }
    subs = subs.substr(0, subs.length() - 3);
    FPS_INFO_LOG(subs);
    return true;
}

bool fims_connect(GcomSystem &sys)
{
    if (!sys.fims_dependencies->fims_gateway.Connect(sys.fims_dependencies->name.c_str()))
    {
        FPS_ERROR_LOG("For client with init uri '%s': could not connect to fims_server", sys.fims_dependencies->name.c_str());
        return false;
    }
    if (!sys.fims_dependencies->fims_gateway.Subscribe(sys.fims_dependencies->subs, false))
    {
        FPS_ERROR_LOG("For client with init uri '%s': failed to subscribe for uri init", sys.fims_dependencies->name.c_str());
        return false;
    }
    return true;
}

// Helper function straight from C++20 so we can use it here in C++17:
// for checking for suffix's over fims (like a raw get request)
static constexpr bool contains_request(std::string_view uri, std::string_view process_name, std::string_view request_component) noexcept
{
    const auto uri_len = uri.size();
    const auto request_len = request_component.size();
    const auto process_name_len = process_name.size();
    return uri_len >= (request_len + process_name_len) && std::string_view::traits_type::compare(uri.begin(), process_name.data(), process_name_len) == 0 && std::string_view::traits_type::compare(uri.begin() + process_name_len, request_component.data(), request_len) == 0;
}

bool parseHeader(GcomSystem &sys, Meta_Data_Info &meta_data, char *data_buf, uint32_t data_buf_len)
{
    // meta data views:
    auto ix = 0;
    sys.fims_dependencies->method_view = std::string_view{(char *)&data_buf[ix], meta_data.method_len};
    ix += meta_data.method_len;
    sys.fims_dependencies->uri_view = std::string_view{(char *)&data_buf[ix], meta_data.uri_len};
    ix += meta_data.uri_len;
    sys.fims_dependencies->replyto_view = std::string_view{(char *)&data_buf[ix], meta_data.replyto_len};
    ix += meta_data.replyto_len;
    sys.fims_dependencies->process_name_view = std::string_view{(char *)&data_buf[ix], meta_data.process_name_len};
    ix += meta_data.process_name_len;
    sys.fims_dependencies->username_view = std::string_view{(char *)&data_buf[ix], meta_data.username_len};
    ix += meta_data.username_len;
    sys.fims_dependencies->data_buf = (char *)&data_buf[ix];

    // request handling
    static constexpr auto Raw_Request = "/raw"sv;                     // get // I think we have to work pretty hard to access the raw data for each register in TMW.
                                                                      // If we just want the raw data as a whole, we can just turn on debug mode.
    static constexpr auto Timings_Request = "/timings"sv;             // get
    static constexpr auto Reset_Timings_Request = "/reset_timings"sv; // set
    static constexpr auto Reload_Request = "/reload"sv;               // set
    static constexpr auto Config_Request = "/config"sv;               // get
    static constexpr auto Debug_Request = "/debug"sv;                 // set
    static constexpr auto Stats_Request = "/stats"sv;                 // get
    // static constexpr auto Channel_Stats_Request = "/channel_stats"sv; // get
    // static constexpr auto Session_Stats_Request = "/session_stats"sv; // get
    // static constexpr auto Reset_Stats_Request = "/reset_stats"sv; // get
    static constexpr auto Points_Request = "/points"sv;       // get
    static constexpr auto Local_Forced_Request = "/forced"sv; // set

    // /x request checks:
    sys.fims_dependencies->is_raw_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Raw_Request);
    sys.fims_dependencies->is_timings_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Timings_Request);
    sys.fims_dependencies->is_reset_timings_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Reset_Timings_Request);
    sys.fims_dependencies->is_reload_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Reload_Request);
    sys.fims_dependencies->is_config_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Config_Request);
    sys.fims_dependencies->is_debug_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Debug_Request);
    sys.fims_dependencies->is_stats_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Stats_Request);
    // sys.fims_dependencies->is_stats_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Stats_Request);
    sys.fims_dependencies->is_points_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Points_Request);
    sys.fims_dependencies->is_forced_request = sys.protocol_dependencies->who == DNP3_OUTSTATION && contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.local_uri), Local_Forced_Request);
    sys.fims_dependencies->is_forced_value_set = sys.protocol_dependencies->who == DNP3_OUTSTATION && !sys.fims_dependencies->is_forced_request && contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.local_uri), ""sv);

    sys.fims_dependencies->is_request = sys.fims_dependencies->is_raw_request ||
                                        sys.fims_dependencies->is_timings_request ||
                                        sys.fims_dependencies->is_reset_timings_request ||
                                        sys.fims_dependencies->is_reload_request ||
                                        sys.fims_dependencies->is_config_request ||
                                        sys.fims_dependencies->is_debug_request ||
                                        sys.fims_dependencies->is_stats_request ||
                                        sys.fims_dependencies->is_points_request ||
                                        sys.fims_dependencies->is_forced_request;

    if (sys.fims_dependencies->is_forced_value_set)
    {
        sys.fims_dependencies->uri_view = sys.fims_dependencies->uri_view.substr(strlen(sys.local_uri));
    }
    // method check:
    sys.fims_dependencies->method = FimsMethod::Unknown;

    if (sys.fims_dependencies->method_view == "set")
    {
        sys.fims_dependencies->method = FimsMethod::Set;

        // the outstation can't do "sets" unless it's in local mode (i.e. sent to local_uri)
        if (sys.protocol_dependencies->who == DNP3_OUTSTATION &&
            !sys.fims_dependencies->is_forced_value_set &&
            !sys.fims_dependencies->is_request)
        {
            FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported by dnp3_server. Message dropped",
                          sys.fims_dependencies->name.c_str(),
                          sys.fims_dependencies->process_name_view,
                          sys.fims_dependencies->method_view);
            FPS_LOG_IT("fims_method_error");

            return false;
        }
    }
    else if (sys.fims_dependencies->method_view == "get")
    {
        sys.fims_dependencies->method = FimsMethod::Get;
        if (sys.fims_dependencies->replyto_view.empty())
        {
            FPS_ERROR_LOG("Listener for '%s', could not complete request (no reply-to).",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_no_reply");

            return false;
        }
    }
    else if (sys.fims_dependencies->method_view == "pub")
    {
        sys.fims_dependencies->method = FimsMethod::Pub;

        // the client can't listen for "pubs" unless it's a watchdog uri
        if (sys.protocol_dependencies->who == DNP3_MASTER)
        {
            if (sys.debug > 0)
            {
                FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported by dnp3_server. Message dropped",
                              sys.fims_dependencies->name.c_str(),
                              sys.fims_dependencies->process_name_view,
                              sys.fims_dependencies->method_view);
                FPS_LOG_IT("fims_method_error");
            }
            return false;
        }
    }
    else // method not supported by gcom_client (it's not set, pub, or get)
    {
        FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported. Message dropped",
                      sys.fims_dependencies->name.c_str(),
                      sys.fims_dependencies->process_name_view,
                      sys.fims_dependencies->method_view);
        FPS_LOG_IT("fims_method_error");

        if (!sys.fims_dependencies->replyto_view.empty())
        {
            static constexpr auto err_str = "Unsupported fims method"sv;

            if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, err_str))
            {
                FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message.",
                              sys.fims_dependencies->name.c_str());
                FPS_LOG_IT("fims_send_error");

                return false;
            }
        }
        return false;
    }

    if (meta_data.data_len > static_cast<uint32_t>(sys.fims_dependencies->data_buf_len))
    {
        FPS_ERROR_LOG("Fims receive buffer is too small. Recommend increasing data_buf_len to at least %d", meta_data.data_len);
        FPS_LOG_IT("fims_receive_buffer");
        return false;
    }

    return true;
}

bool uriIsMultiOrSingle(GcomSystem &sys, std::string_view uri)
{
    std::string suri = {uri.begin(), uri.end()};
    std::map<std::string, varList *>::iterator it = sys.dburiMap.find(suri);
    if (it != sys.dburiMap.end())
    {
        const auto multi = it->second->multi;
        if (multi)
            return true;
    }
    return false;
}

bool processCmds(GcomSystem &sys, Meta_Data_Info &meta_data)
{
    // handle config request
    if (sys.fims_dependencies->is_config_request && sys.fims_dependencies->method == FimsMethod::Get)
    {
        fmt::memory_buffer send_buf;
        send_buf.clear();
        char *config_str = cJSON_PrintUnformatted(sys.config);
        FORMAT_TO_BUF(send_buf, R"({})", config_str);

        if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_send_error");
            return false;
        }
        return true;
    }

    // handle local_forced request
    if (sys.fims_dependencies->is_forced_request && sys.fims_dependencies->method == FimsMethod::Set)
    {
        const auto uri_len = sys.fims_dependencies->uri_view.size();
        const auto request_len = std::string_view{"/forced"}.size();
        const auto process_name_len = std::string_view(sys.local_uri).size();
        std::string uri = std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)};
        std::map<std::string, varList *>::iterator uri_item = sys.dburiMap.find(uri);
        if (uri_len > (request_len + process_name_len) &&
            uri_item != sys.dburiMap.end())
        {
            auto &parser = sys.fims_dependencies->parser;
            auto &doc = sys.fims_dependencies->doc;
            int forced_request = -1;

            // if the message contains a value, parse the value to determine what to do
            memset(reinterpret_cast<u8 *>(sys.fims_dependencies->data_buf) + meta_data.data_len, '\0', Simdj_Padding);
            if (const auto err = parser.iterate(reinterpret_cast<const char *>(sys.fims_dependencies->data_buf),
                                                meta_data.data_len,
                                                meta_data.data_len + Simdj_Padding)
                                     .get(doc);
                !err)
            {
                Jval_buif to_set;
                simdjson::ondemand::value curr_val;
                auto val_clothed = doc.get_object();

                auto success = extractValueSingle(sys, val_clothed, curr_val, to_set);
                if (success)
                {
                    forced_request = static_cast<int>(jval_to_double(to_set));
                }
            }

            if (forced_request == 0)
            {
                for (std::pair<const std::string, TMWSIM_POINT *> pair : uri_item->second->dbmap)
                {
                    TMWSIM_POINT *dbPoint = pair.second;
                    dbPoint->flags &= ~DNPDEFS_DBAS_FLAG_LOCAL_FORCED;
                    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
                    {
                        dbPoint->data.analog.value = ((FlexPoint *)dbPoint->flexPointHandle)->standby_value;
                    }
                    else if (dbPoint->type == TMWSIM_TYPE_BINARY)
                    {
                        dbPoint->data.binary.value = static_cast<bool>(((FlexPoint *)dbPoint->flexPointHandle)->standby_value);
                    }
                }
            }
            else if (forced_request > 0)
            {
                for (std::pair<const std::string, TMWSIM_POINT *> pair : uri_item->second->dbmap)
                {
                    TMWSIM_POINT *dbPoint = pair.second;
                    dbPoint->flags |= DNPDEFS_DBAS_FLAG_LOCAL_FORCED;
                    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
                    {
                        ((FlexPoint *)dbPoint->flexPointHandle)->standby_value = dbPoint->data.analog.value;
                    }
                    else if (dbPoint->type == TMWSIM_TYPE_BINARY)
                    {
                        ((FlexPoint *)dbPoint->flexPointHandle)->standby_value = static_cast<double>(dbPoint->data.binary.value);
                    }
                }
            }
        }
        else
        {
            for (std::pair<const std::string, TMWSIM_POINT *> pair : uri_item->second->dbmap)
            {
                TMWSIM_POINT *dbPoint = pair.second;
                dbPoint->flags ^= DNPDEFS_DBAS_FLAG_LOCAL_FORCED;
                if ((dbPoint->flags & DNPDEFS_DBAS_FLAG_LOCAL_FORCED) == 0)
                {
                    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
                    {
                        dbPoint->data.analog.value = ((FlexPoint *)dbPoint->flexPointHandle)->standby_value;
                    }
                    else if (dbPoint->type == TMWSIM_TYPE_BINARY)
                    {
                        dbPoint->data.binary.value = static_cast<bool>(((FlexPoint *)dbPoint->flexPointHandle)->standby_value);
                    }
                }
                else
                {
                    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
                    {
                        ((FlexPoint *)dbPoint->flexPointHandle)->standby_value = dbPoint->data.analog.value;
                    }
                    else if (dbPoint->type == TMWSIM_TYPE_BINARY)
                    {
                        ((FlexPoint *)dbPoint->flexPointHandle)->standby_value = static_cast<double>(dbPoint->data.binary.value);
                    }
                }
            }
        }
        return true;
    }

    // handle debug request
    if (sys.fims_dependencies->is_debug_request)
    {
        if (sys.fims_dependencies->method == FimsMethod::Set)
        {
            FPS_INFO_LOG("Debug Request for  : %s", sys.fims_dependencies->name);
            auto &parser = sys.fims_dependencies->parser;
            auto &doc = sys.fims_dependencies->doc;
            int debug_request;

            // by default, just "flip the switch" on debug
            if (sys.debug == 0)
            {
                debug_request = 1;
            }
            else
            {
                debug_request = 0;
            }

            // if the message contains a value, parse the value to determine what to do
            memset(reinterpret_cast<u8 *>(sys.fims_dependencies->data_buf) + meta_data.data_len, '\0', Simdj_Padding);
            if (const auto err = parser.iterate(reinterpret_cast<const char *>(sys.fims_dependencies->data_buf),
                                                meta_data.data_len,
                                                meta_data.data_len + Simdj_Padding)
                                     .get(doc);
                !err)
            {
                Jval_buif to_set;
                simdjson::ondemand::value curr_val;
                auto val_clothed = doc.get_object();

                auto success = extractValueSingle(sys, val_clothed, curr_val, to_set);
                if (success)
                {
                    debug_request = static_cast<int>(jval_to_double(to_set));
                }
            }

            sys.debug = debug_request;

            if (sys.debug >= 1)
            {
                sys.protocol_dependencies->dnp3.channelConfig.chnlDiagMask = (TMWDIAG_ID_PHYS | TMWDIAG_ID_LINK | TMWDIAG_ID_TPRT | TMWDIAG_ID_APPL |
                                                                              TMWDIAG_ID_USER | TMWDIAG_ID_MMI | TMWDIAG_ID_STATIC_DATA |
                                                                              TMWDIAG_ID_STATIC_HDRS | TMWDIAG_ID_EVENT_DATA | TMWDIAG_ID_EVENT_HDRS |
                                                                              TMWDIAG_ID_CYCLIC_DATA | TMWDIAG_ID_CYCLIC_HDRS | TMWDIAG_ID_SECURITY_DATA |
                                                                              TMWDIAG_ID_SECURITY_HDRS | TMWDIAG_ID_TX | TMWDIAG_ID_RX |
                                                                              TMWDIAG_ID_TIMESTAMP | TMWDIAG_ID_ERROR | TMWDIAG_ID_TARGET);
                sys.protocol_dependencies->dnp3.clientSesnConfig.sesnDiagMask = (TMWDIAG_ID_PHYS | TMWDIAG_ID_LINK | TMWDIAG_ID_TPRT | TMWDIAG_ID_APPL |
                                                                                 TMWDIAG_ID_USER | TMWDIAG_ID_MMI | TMWDIAG_ID_STATIC_DATA |
                                                                                 TMWDIAG_ID_STATIC_HDRS | TMWDIAG_ID_EVENT_DATA | TMWDIAG_ID_EVENT_HDRS |
                                                                                 TMWDIAG_ID_CYCLIC_DATA | TMWDIAG_ID_CYCLIC_HDRS | TMWDIAG_ID_SECURITY_DATA |
                                                                                 TMWDIAG_ID_SECURITY_HDRS | TMWDIAG_ID_TX | TMWDIAG_ID_RX |
                                                                                 TMWDIAG_ID_TIMESTAMP | TMWDIAG_ID_ERROR | TMWDIAG_ID_TARGET);
                tmwtargp_registerPutDiagStringFunc(TMWDEFS_NULL);
            }
            else
            {
                sys.protocol_dependencies->dnp3.channelConfig.chnlDiagMask = TMWDIAG_ID_ERROR;
                sys.protocol_dependencies->dnp3.clientSesnConfig.sesnDiagMask = TMWDIAG_ID_ERROR;
                tmwtargp_registerPutDiagStringFunc(Logging::log_TMW_message);
            }
            dnpchnl_setChannelConfig(sys.protocol_dependencies->dnp3.pChannel, &(sys.protocol_dependencies->dnp3.channelConfig), &(sys.protocol_dependencies->dnp3.tprtConfig), &(sys.protocol_dependencies->dnp3.linkConfig), &(sys.protocol_dependencies->dnp3.physConfig));
            mdnpsesn_setSessionConfig(sys.protocol_dependencies->dnp3.pSession, &sys.protocol_dependencies->dnp3.clientSesnConfig);

            return true;
        }
        else if (sys.fims_dependencies->method == FimsMethod::Get)
        {
            fmt::memory_buffer send_buf;
            send_buf.clear();
            FORMAT_TO_BUF(send_buf, R"({{"debug":{}}})", sys.debug);

            if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
            {
                FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                              sys.fims_dependencies->name.c_str());
                FPS_LOG_IT("fims_send_error");
                return false;
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    if (sys.fims_dependencies->is_points_request && sys.fims_dependencies->method == FimsMethod::Get)
    {
        const auto uri_len = sys.fims_dependencies->uri_view.size();
        const auto request_len = std::string_view{"/points"}.size();
        const auto process_name_len = std::string_view(sys.base_uri).size();
        fmt::memory_buffer send_buf;
        send_buf.clear();
        if (uri_len > (request_len + process_name_len) &&
            sys.protocol_dependencies->dnp3.point_group_map.find(std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}) != sys.protocol_dependencies->dnp3.point_group_map.end())
        {
            if (sys.protocol_dependencies->who == DNP3_MASTER)
            {
                updatePointStatus(sys);
            }
            FORMAT_TO_BUF(send_buf, R"({})", sys.protocol_dependencies->dnp3.point_group_map[std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}]);
        }
        else
        {
            if (sys.protocol_dependencies->who == DNP3_MASTER)
            {
                updatePointStatus(sys);
            }
            send_buf.push_back('{');
            FORMAT_TO_BUF(send_buf, R"({})", *sys.protocol_dependencies->dnp3.point_status_info);
            send_buf.push_back('}');
        }
        if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_send_error");

            return false;
        }
        return true;
    }

    if (sys.fims_dependencies->is_stats_request && sys.fims_dependencies->method == FimsMethod::Get)
    {
        const auto uri_len = sys.fims_dependencies->uri_view.size();
        const auto request_len = std::string_view{"/stats"}.size();
        const auto process_name_len = std::string_view(sys.base_uri).size();
        fmt::memory_buffer send_buf;
        send_buf.clear();
        if (uri_len > (request_len + process_name_len) &&
            sys.protocol_dependencies->dnp3.channel_stats->channel_stats_map.find(std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}) != sys.protocol_dependencies->dnp3.channel_stats->channel_stats_map.end())
        {
            sys.protocol_dependencies->dnp3.channel_stats->channel_stats_mutex.lock();
            FORMAT_TO_BUF(send_buf, R"({})", *sys.protocol_dependencies->dnp3.channel_stats->channel_stats_map[std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}]);
            sys.protocol_dependencies->dnp3.channel_stats->channel_stats_mutex.unlock();
        }
        else if (uri_len > (request_len + process_name_len) &&
                 sys.protocol_dependencies->dnp3.session_stats->session_stats_map.find(std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}) != sys.protocol_dependencies->dnp3.session_stats->session_stats_map.end())
        {
            sys.protocol_dependencies->dnp3.session_stats->session_stats_mutex.lock();
            FORMAT_TO_BUF(send_buf, R"({})", *sys.protocol_dependencies->dnp3.session_stats->session_stats_map[std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}]);
            sys.protocol_dependencies->dnp3.session_stats->session_stats_mutex.unlock();
        }
        else
        {
            FORMAT_TO_BUF(send_buf, R"({{"channel_stats":{},"session_stats":{}}})", *sys.protocol_dependencies->dnp3.channel_stats, *sys.protocol_dependencies->dnp3.session_stats);
        }

        if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_send_error");

            return false;
        }
        return true;
    }

    if (sys.fims_dependencies->is_timings_request && sys.fims_dependencies->method == FimsMethod::Get)
    {
        const auto uri_len = sys.fims_dependencies->uri_view.size();
        const auto request_len = std::string_view{"/timings"}.size();
        const auto process_name_len = std::string_view(sys.base_uri).size();
        fmt::memory_buffer send_buf;
        send_buf.clear();
        if (uri_len > (request_len + process_name_len) &&
            sys.protocol_dependencies->dnp3.timings->timings_map.find(std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}) != sys.protocol_dependencies->dnp3.timings->timings_map.end())
        {
            FORMAT_TO_BUF(send_buf, R"({})", *sys.protocol_dependencies->dnp3.timings->timings_map[std::string{sys.fims_dependencies->uri_view.substr(request_len + process_name_len)}]);
        }
        else
        {
            FORMAT_TO_BUF(send_buf, R"({})", *sys.protocol_dependencies->dnp3.timings);
        }

        if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_send_error");

            return false;
        }
        return true;
    }

    if (sys.fims_dependencies->is_reset_timings_request && sys.fims_dependencies->method == FimsMethod::Set)
    {
        dnp3stats_resetTimings();
        return true;
    }

    return true;
}

bool extractValueMulti(GcomSystem &sys, simdjson::simdjson_result<simdjson::fallback::ondemand::object> &val_clothed, simdjson::fallback::ondemand::value &curr_val, Jval_buif &to_set)
{
    if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
    {
        FPS_ERROR_LOG("Could not get a clothed value as an object while parsing a multi-value message, err = %s Message dropped", simdjson::error_message(err));
        FPS_LOG_IT("parsing_error");
        return false;
    }
    if (!val_clothed.error())
    {
        auto inner_val = val_clothed.find_field("value");
        if (const auto err = inner_val.error(); err)
        {
            FPS_ERROR_LOG("Could not get the clothed key 'value' while parsing a multi-value message, err = %s skipping this uri: %s", simdjson::error_message(err), sys.fims_dependencies->uri_view);
            FPS_LOG_IT("parsing_error");
            return false;
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
        to_set = static_cast<uint64_t>(val_bool.value_unsafe()); // just set booleans equal to the whole numbers 1/0 for sets
    }
    else
    {
        FPS_ERROR_LOG("only floats, uints, ints, and bools are supported, skipping this uri: %s", sys.fims_dependencies->uri_view);
        FPS_LOG_IT("parsing_error");
        return false;
    }
    return true;
}

bool extractValueSingle(GcomSystem &sys, simdjson::simdjson_result<simdjson::fallback::ondemand::object> &val_clothed, simdjson::fallback::ondemand::value &curr_val, Jval_buif &to_set)
{
    auto &doc = sys.fims_dependencies->doc;
    if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
    {
        FPS_ERROR_LOG("with single-value uri '%s': could not get a clothed value as an object while parsing a single-value message, err = %s Message dropped", sys.fims_dependencies->uri_view, simdjson::error_message(err));
        FPS_LOG_IT("parsing_error");
        return false;
    }
    if (!val_clothed.error()) // they sent a clothed value:
    {
        auto inner_val = val_clothed.find_field("value");
        if (const auto err = inner_val.error(); err)
        {
            FPS_ERROR_LOG("with single-value uri '%s': could not get the clothed key 'value' while parsing a single-value message, err = %s skipping this value", sys.fims_dependencies->uri_view, simdjson::error_message(err));
            FPS_LOG_IT("parsing_error");
            return false;
        }
        curr_val = std::move(inner_val.value_unsafe());
    }

    if (auto val_uint = val_clothed.error() ? doc.get_uint64() : curr_val.get_uint64(); !val_uint.error()) // it is an unsigned integer
    {
        to_set = val_uint.value_unsafe();
    }
    else if (auto val_int = val_clothed.error() ? doc.get_int64() : curr_val.get_int64(); !val_int.error()) // it is an integer
    {
        to_set = val_int.value_unsafe();
    }
    else if (auto val_float = val_clothed.error() ? doc.get_double() : curr_val.get_double(); !val_float.error()) // it is a float
    {
        to_set = val_float.value_unsafe();
    }
    else if (auto val_bool = val_clothed.error() ? doc.get_bool() : curr_val.get_bool(); !val_bool.error()) // it is a bool
    {
        to_set = static_cast<uint64_t>(val_bool.value_unsafe()); // just set booleans to the whole numbers 1/0
    }
    else
    {
        FPS_ERROR_LOG("only floats, uints, ints, and bools are supported, skipping this uri: %s", sys.fims_dependencies->uri_view);
        FPS_LOG_IT("parsing_error");
        return false;
    }
    return true;
}

double jval_to_double(Jval_buif &to_set)
{
    double value;
    if (to_set.holds_int())
    {
        value = static_cast<double>(to_set.get_int());
    }
    else if (to_set.holds_uint())
    {
        value = static_cast<double>(to_set.get_uint());
    }
    else if (to_set.holds_bool())
    {
        value = static_cast<double>(to_set.get_bool());
    }
    else
    {
        value = to_set.get_float();
    }
    return value;
}

bool gcom_recv_raw_message(fims &fims_gateway, Meta_Data_Info &meta_data, void *data_buf, uint32_t data_buf_len) noexcept
{
    int connection = fims_gateway.get_socket();
    struct iovec bufs[] = {
        {(void *)&meta_data, sizeof(Meta_Data_Info)},
        {(void *)data_buf, data_buf_len}};
    const auto bytes_read = readv(connection, bufs, sizeof(bufs) / sizeof(*bufs));
    if (bytes_read > 0)
    {
        if (0)
            FPS_DEBUG_LOG(" base %d we read stuff %d ", (int)sizeof(Meta_Data_Info), (int)bytes_read);
    }
    if (bytes_read == 0)
    {
        close(connection);
    }
    return bytes_read > 0;
}

void replyToFullGet(GcomSystem &sys, fmt::memory_buffer &send_buf)
{
    const auto uri_len = sys.fims_dependencies->uri_view.size();
    const auto request_len = std::string_view{"/full"}.size();
    const auto process_name_len = std::string_view(sys.base_uri).size();
    if (uri_len >= (request_len + process_name_len))
    {
        TMWSIM_POINT *dbPoint = getDbVar(sys, sys.fims_dependencies->uri_view.substr(request_len + process_name_len), {});
        if (dbPoint)
        {
            send_buf.clear();
            FORMAT_TO_BUF(send_buf, R"({})", *dbPoint);
            if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
            {
                FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                              sys.fims_dependencies->name.c_str());
                FPS_LOG_IT("fims_send_error");
            }
        }
    }
}

void formatPointValue(fmt::memory_buffer &send_buf, TMWSIM_POINT *dbPoint)
{
    if (dbPoint->type == TMWSIM_TYPE_ANALOG)
    {
        FORMAT_TO_BUF(send_buf, R"({:g})", dbPoint->data.analog.value / (((FlexPoint *)(dbPoint->flexPointHandle))->scale));
    }
    else
    {
        if (((FlexPoint *)(dbPoint->flexPointHandle))->scale < 0)
        {
            if (((FlexPoint *)(dbPoint->flexPointHandle))->crob_string)
            {
                FORMAT_TO_BUF(send_buf, R"("{}")", static_cast<bool>(dbPoint->data.binary.value) ? (((FlexPoint *)(dbPoint->flexPointHandle))->crob_false) : (((FlexPoint *)(dbPoint->flexPointHandle))->crob_true));
            }
            else if (((FlexPoint *)(dbPoint->flexPointHandle))->crob_int)
            {
                FORMAT_TO_BUF(send_buf, R"({})", static_cast<bool>(dbPoint->data.binary.value) ? 0 : 1);
            }
            else
            {
                FORMAT_TO_BUF(send_buf, R"({})", static_cast<bool>(dbPoint->data.binary.value) ? "false" : "true");
            }
        }
        else
        {
            if (((FlexPoint *)(dbPoint->flexPointHandle))->crob_string)
            {
                FORMAT_TO_BUF(send_buf, R"("{}")", static_cast<bool>(dbPoint->data.binary.value) ? (((FlexPoint *)(dbPoint->flexPointHandle))->crob_true) : (((FlexPoint *)(dbPoint->flexPointHandle))->crob_false));
            }
            else if (((FlexPoint *)(dbPoint->flexPointHandle))->crob_int)
            {
                FORMAT_TO_BUF(send_buf, R"({})", static_cast<bool>(dbPoint->data.binary.value) ? 1 : 0);
            }
            else
            {
                FORMAT_TO_BUF(send_buf, R"({})", static_cast<bool>(dbPoint->data.binary.value) ? "true" : "false");
            }
        }
    }
}

void replyToGet(GcomSystem &sys, fmt::memory_buffer &send_buf)
{
    TMWSIM_POINT *dbPoint = nullptr;
    bool has_one_point = false;
    if (contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), {}))
    {
        if (uriIsMultiOrSingle(sys, sys.fims_dependencies->uri_view.substr(strlen(sys.base_uri))))
        {
            send_buf.clear();
            FORMAT_TO_BUF(send_buf, R"({{)");
            for (auto &dbVar : sys.dburiMap[std::string{sys.fims_dependencies->uri_view.substr(strlen(sys.base_uri))}]->dbmap)
            {
                dbPoint = dbVar.second;
                if (dbPoint)
                {
                    FORMAT_TO_BUF(send_buf, R"("{}": )", dbVar.first);
                    formatPointValue(send_buf, dbVar.second);
                    FORMAT_TO_BUF(send_buf, R"(, )");
                    has_one_point = true;
                }
            }
            send_buf.resize(send_buf.size() - (2 * has_one_point)); // get rid of the last comma and space if we have them
            FORMAT_TO_BUF(send_buf, R"(}})");
        }
        else
        {
            dbPoint = getDbVar(sys, sys.fims_dependencies->uri_view.substr(strlen(sys.base_uri)), {});
            if (dbPoint)
            {
                send_buf.clear();
                formatPointValue(send_buf, dbPoint);
                has_one_point = true;
            }
        }
    }
    else if (sys.protocol_dependencies->who == DNP3_MASTER && uriIsMultiOrSingle(sys, sys.fims_dependencies->uri_view))
    {
        send_buf.clear();
        FORMAT_TO_BUF(send_buf, R"({{)");
        for (auto &dbVar : sys.dburiMap[std::string{sys.fims_dependencies->uri_view}]->dbmap)
        {
            dbPoint = dbVar.second;
            if (dbPoint)
            {
                FORMAT_TO_BUF(send_buf, R"("{}": )", dbVar.first);
                formatPointValue(send_buf, dbVar.second);
                FORMAT_TO_BUF(send_buf, R"(, )");
                has_one_point = true;
            }
        }
        send_buf.resize(send_buf.size() - (2 * has_one_point)); // get rid of the last comma and space if we have them
        FORMAT_TO_BUF(send_buf, R"(}})");
    }
    else if (sys.protocol_dependencies->who == DNP3_MASTER)
    {
        dbPoint = getDbVar(sys, sys.fims_dependencies->uri_view, {});
        if (dbPoint)
        {
            send_buf.clear();
            formatPointValue(send_buf, dbPoint);
            has_one_point = true;
        }
    }
    if (has_one_point)
    {
        if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{send_buf.data(), send_buf.size()}))
        {
            FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message. Exiting",
                          sys.fims_dependencies->name.c_str());
            FPS_LOG_IT("fims_send_error");
        }
    }
}

bool listener_thread(GcomSystem &sys) noexcept
{
    if (sys.fims_dependencies->parseBody == nullptr)
    {
        FPS_ERROR_LOG("Failed to initialize fims properly. Missing declaration of parseBody function.");
        return false;
    }

    const auto client_name = sys.fims_dependencies->name;
    auto &fims_gateway = sys.fims_dependencies->fims_gateway;
    auto &receiver_bufs = sys.fims_dependencies->receiver_bufs;
    fmt::memory_buffer send_buf;

    auto &meta_data = receiver_bufs.meta_data;

    // TODO make this an optional config item
    unsigned int mydata_size = sys.fims_dependencies->data_buf_len;
    unsigned char *mydata = (unsigned char *)calloc(1, mydata_size);

    defer
    {
        if (mydata)
            free(mydata);
        mydata = nullptr;
        sys.keep_running = false;
        sys.start_signal = false;
    };

    {
        std::unique_lock<std::mutex> lk{sys.main_mutex};
        sys.main_cond.wait(lk, [&]()
                           { return sys.start_signal; });
    }

    // setup the timeout to be 2 seconds (so we can stop listener thread without it spinning infinitely on errors):
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(fims_gateway.get_socket(), SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) == -1)
    {
        FPS_ERROR_LOG("listener for '%s': could not set socket timeout to 2 seconds. Exiting", sys.fims_dependencies->name.c_str());
        return false;
    }

    // main loop:
    while (sys.keep_running)
    {
        if (!gcom_recv_raw_message(fims_gateway, meta_data, mydata, mydata_size))
        {
            const auto curr_errno = errno;
            if (curr_errno == EAGAIN || curr_errno == EWOULDBLOCK)
                continue; // we just timed out
            // This is if we have a legitimate error (no timeout):
            FPS_ERROR_LOG("Listener for '%s': could not receive message over fims. Exiting", sys.fims_dependencies->name.c_str());
            return false;
        }

        auto parseOK = parseHeader(sys, meta_data, (char *)mydata, mydata_size);
        if (!parseOK)
            continue;

        if (sys.fims_dependencies->is_request)
        {
            processCmds(sys, meta_data);
            continue;
        }

        // this will run in CONFIG mode or DATA  mode <-- maybe handle config mode in processCmds?
        // after config mode we'll have to re evaluate subs and reconnect fims with a new subs list.

        if (sys.fims_dependencies->method == FimsMethod::Set)
        {
            sys.fims_dependencies->parseBody(sys, meta_data);
            if (!sys.fims_dependencies->replyto_view.empty())
            {
                if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, std::string_view{reinterpret_cast<char *>(sys.fims_dependencies->data_buf)}))
                {
                    FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message.",
                                  sys.fims_dependencies->name.c_str());
                    FPS_LOG_IT("fims_send_error");
                }
            }
        }
        else if ((sys.fims_dependencies->method == FimsMethod::Pub && sys.protocol_dependencies->who == DNP3_OUTSTATION))
        {
            sys.fims_dependencies->parseBody(sys, meta_data);
        }
        else if (sys.fims_dependencies->method == FimsMethod::Get)
        {
            if (contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), "/full"sv))
            {
                replyToFullGet(sys, send_buf);
            }
            else
            {
                replyToGet(sys, send_buf);
            }
        }
    }
    return true;
}