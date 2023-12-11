#include <chrono>
#include <thread>
#include <iostream>
#include <any>
#include <future>
#include <sys/uio.h> // for receive timouet calls
#include <sys/socket.h> // for receive timouet calls

#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //for exit(0);
#include<errno.h> //For errno - the error number
#include "fims/libfims.h"
#include "fims/defer.hpp"

#include "gcom_config.h"
#include "gcom_iothread.h"

using namespace std::chrono_literals;
using namespace std::string_view_literals; // for sv

// fims helper functions:
bool send_pub(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"pub", sizeof("pub") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}
bool send_set(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"set", sizeof("set") - 1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}
// NOTE(WALKER): This is only for emit_event really (not used anywhere else)
bool send_post(fims& fims_gateway, std::string_view uri, std::string_view body) noexcept
{
    return fims_gateway.Send(fims::str_view{"post", sizeof("post") -1}, fims::str_view{uri.data(), uri.size()}, fims::str_view{nullptr, 0}, fims::str_view{nullptr, 0}, fims::str_view{body.data(), body.size()});
}

bool set_io_fims_data_buf(struct IO_Fims&io_fims, int data_buf_len)
{
    //int data_buf_len = 1000000;
    
    if(io_fims.fims_input_buf)
    {
        free(io_fims.fims_input_buf);

    }
    std::cout << " set buff len to " << data_buf_len << std::endl;
    io_fims.fims_input_buf = reinterpret_cast<uint8_t *>(malloc(data_buf_len));

    io_fims.fims_input_buf_len = data_buf_len; ///sys.fims_dependencies->data_buf_len;

    //myCfg.receiver_bufs.data_buf_len = data_buf_len; ///sys.fims_dependencies->data_buf_len;
    //myCfg.receiver_bufs.data_buf = myCfg.fims_input_buf;
    //sys.fims_dependencies->subs.push_back("/" + std::string(sys.id));
    io_fims.fims_data_buf_len = data_buf_len; ///sys.fims_dependencies->data_buf_len;

    // get the process name
    //std::string process = "modbus_client@";
    //process += sys.config_file_name;
    //sys.fims_dependencies->name = process;

    return true;
}



#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>


ioChannel<std::shared_ptr<IO_Fims>> io_fimsChan;             // Use Channel to store IO_Fims objects
ioChannel<std::shared_ptr<IO_Fims>> io_fimsProcessChan;      // Use Channel to send IO_Fims to the process 
ioChannel<int>io_fimsWakeChan;      // Use Channel to send IO_Fims to the process 


std::shared_ptr<IO_Fims> make_fims(struct cfg& myCfg) 
{

    std::shared_ptr<IO_Fims> io_fims;

    if (!io_fimsChan.peekpop(io_fims)) {  // Assuming receive will return false if no item is available.
        //std::cout << " create an io_work object "<< std::endl;
        io_fims = std::make_shared<IO_Fims>();
        io_fims->tId = get_time_double();
    }
    return (io_fims);
    //io_pollChan.send(std::move(io_work));
    //io_threadChan.send(1);

    //return true;  // You might want to return a status indicating success or failure.
}

bool send_fims(std::shared_ptr<IO_Fims> io_fims,struct cfg& myCfg)
{
    io_fimsProcessChan.send(std::move(io_fims));
    io_fimsWakeChan.send(1);
    return true;
}

bool save_fims(std::shared_ptr<IO_Fims> io_fims,struct cfg& myCfg)
{
    io_fimsChan.send(std::move(io_fims));
    return true;
}


bool gcom_recv_raw_message(fims &fims_gateway, std::shared_ptr<IO_Fims>io_fims) noexcept
{
    std::cout << " recv buff len set to  " << io_fims->data_buf_len << std::endl;

    int connection = fims_gateway.get_socket();
    // struct iovec bufs[] = {
    //     {(void *)&io_fims->meta_data, sizeof(Meta_Data_Info)},
    //     {(void *)io_fims->fims_input_buf, io_fims->fims_input_buf_len-1}};
    struct iovec bufs[2];
    bufs[0].iov_base  = &io_fims->meta_data;
    bufs[0].iov_len   = sizeof(Meta_Data_Info);
    bufs[1].iov_base  = (void *)io_fims->fims_input_buf;
    bufs[1].iov_len   = io_fims->fims_input_buf_len-1;
    printf( " bufs [0] iov_base %p \n", (void*)bufs[0].iov_base);
    printf( " bufs [1] iov_base %p \n", (void*)bufs[1].iov_base);
    printf( " bufs [1] fims_input_buf %p \n", (void*)io_fims->fims_input_buf);
    printf( " bufs [1] fims_input_buf (end)%p \n", (void*)&io_fims->fims_input_buf[100000]);
    printf( " bufs [1] fims_input_buf len %d \n", (int)bufs[1].iov_len);

    io_fims->bytes_read = readv(connection, bufs, 2 );//sizeof(bufs) / sizeof(*bufs));
    if (io_fims->bytes_read > 0)
    {
        //if (0)
        //    FPS_DEBUG_LOG
            printf(" base %d we read stuff %d ", (int)sizeof(Meta_Data_Info), (int)io_fims->bytes_read);
    }
    if (io_fims->bytes_read <= 0)
    {
        printf(" base %d we did not read stuff %d ", (int)sizeof(Meta_Data_Info), (int)io_fims->bytes_read);
        close(connection);
    }
    return io_fims->bytes_read > 0;
}


bool gcom_recv_raw_message(fims &fims_gateway, myMeta_Data_Info &meta_data, void *data_buf, uint32_t data_buf_len) noexcept
{
    std::cout << " recv buff len set to  " << data_buf_len << std::endl;

    int connection = fims_gateway.get_socket();
    struct iovec bufs[] = {
        {(void *)&meta_data, sizeof(Meta_Data_Info)},
        {(void *)data_buf, data_buf_len-1}};
    const auto bytes_read = readv(connection, bufs, sizeof(bufs) / sizeof(*bufs));
    if (bytes_read > 0)
    {
        //if (0)
        //    FPS_DEBUG_LOG
            printf(" base %d we read stuff %d ", (int)sizeof(Meta_Data_Info), (int)bytes_read);
    }
    if (bytes_read <= 0)
    {
        printf(" base %d we did not read stuff %d ", (int)sizeof(Meta_Data_Info), (int)bytes_read);
        close(connection);
    }
    return bytes_read > 0;
}

// this may not work 
bool gcom_recv_extra_raw_message(fims &fims_gateway, void *data_buf, uint32_t data_buf_len) noexcept
{
    std::cout << " extra buff len set to  " << data_buf_len << std::endl;

    int connection = fims_gateway.get_socket();
    int flags = fcntl(connection, F_GETFL, 0);
    fcntl(connection, F_SETFL, flags | O_NONBLOCK);
    ssize_t bytes_read = read(connection, data_buf, data_buf_len);
    fcntl(connection, F_SETFL, flags);
    if (bytes_read > 0)
    {
        //if (0)
        //    FPS_DEBUG_LOG
            printf(" base %d we read extra stuff %d ", (int)sizeof(Meta_Data_Info), (int)bytes_read);
    }
    if (bytes_read <= 0)
    {
        printf(" base %d we did not read extra stuff %d ", (int)sizeof(Meta_Data_Info), (int)bytes_read);
        //close(connection);
    }
    return bytes_read > 0;
}

//bool parseHeader(struct cfg&sys, Meta_Data_Info &meta_data, unsigned char *data_buf, u64& data_buf_len)
bool parseHeader(struct cfg& myCfg, std::shared_ptr<IO_Fims> io_fims) //io_fims->meta_data, io_fims->fims_input_buf, data_buf_len);
{
    // meta data views:
    auto ix = 0;
    //sys.fims_dependencies->
    auto data_buf = io_fims->fims_input_buf;
    auto meta_data = io_fims->meta_data;

    io_fims->method_view = std::string_view{(char *)&data_buf[ix], meta_data.method_len};
    ix += meta_data.method_len;
    //sys.fims_dependencies->
    io_fims->uri_view = std::string_view{(char *)&data_buf[ix], meta_data.uri_len};
    ix += meta_data.uri_len;
    //sys.fims_dependencies->
    io_fims->replyto_view = std::string_view{(char *)&data_buf[ix], meta_data.replyto_len};
    ix += meta_data.replyto_len;
    //sys.fims_dependencies->
    io_fims->process_name_view = std::string_view{(char *)&data_buf[ix], meta_data.process_name_len};
    ix += meta_data.process_name_len;
    //sys.fims_dependencies->
    io_fims->username_view = std::string_view{(char *)&data_buf[ix], meta_data.username_len};
    ix += meta_data.username_len;
    //sys.fims_dependencies->
    io_fims->fims_data_buf = (u8 *)&data_buf[ix];
    io_fims->fims_data_len = meta_data.data_len;

    // // request handling
    // static constexpr auto Raw_Request = "/raw"sv;                     // get // I think we have to work pretty hard to access the raw data for each register in TMW.
    //                                                                   // If we just want the raw data as a whole, we can just turn on debug mode.
    // static constexpr auto Timings_Request = "/timings"sv;             // get
    // static constexpr auto Reset_Timings_Request = "/reset_timings"sv; // set
    // static constexpr auto Reload_Request = "/reload"sv;               // set
    // static constexpr auto Config_Request = "/config"sv;               // get
    // static constexpr auto Debug_Request = "/debug"sv;                 // set
    // static constexpr auto Stats_Request = "/stats"sv;                 // get
    // // static constexpr auto Channel_Stats_Request = "/channel_stats"sv; // get
    // // static constexpr auto Session_Stats_Request = "/session_stats"sv; // get
    // // static constexpr auto Reset_Stats_Request = "/reset_stats"sv; // get
    // static constexpr auto Points_Request = "/points"sv;       // get
    // static constexpr auto Local_Forced_Request = "/forced"sv; // set

    // // /x request checks:
    // sys.fims_dependencies->is_raw_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Raw_Request);
    // sys.fims_dependencies->is_timings_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Timings_Request);
    // sys.fims_dependencies->is_reset_timings_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Reset_Timings_Request);
    // sys.fims_dependencies->is_reload_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Reload_Request);
    // sys.fims_dependencies->is_config_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Config_Request);
    // sys.fims_dependencies->is_debug_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Debug_Request);
    // sys.fims_dependencies->is_stats_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Stats_Request);
    // // sys.fims_dependencies->is_stats_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Stats_Request);
    // sys.fims_dependencies->is_points_request = contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.base_uri), Points_Request);
    // sys.fims_dependencies->is_forced_request = sys.protocol_dependencies->who == DNP3_OUTSTATION && contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.local_uri), Local_Forced_Request);
    // sys.fims_dependencies->is_forced_value_set = sys.protocol_dependencies->who == DNP3_OUTSTATION && !sys.fims_dependencies->is_forced_request && contains_request(sys.fims_dependencies->uri_view, std::string_view(sys.local_uri), ""sv);

    // sys.fims_dependencies->is_request = sys.fims_dependencies->is_raw_request ||
    //                                     sys.fims_dependencies->is_timings_request ||
    //                                     sys.fims_dependencies->is_reset_timings_request ||
    //                                     sys.fims_dependencies->is_reload_request ||
    //                                     sys.fims_dependencies->is_config_request ||
    //                                     sys.fims_dependencies->is_debug_request ||
    //                                     sys.fims_dependencies->is_stats_request ||
    //                                     sys.fims_dependencies->is_points_request ||
    //                                     sys.fims_dependencies->is_forced_request;

    // if (sys.fims_dependencies->is_forced_value_set)
    // {
    //     sys.fims_dependencies->uri_view = sys.fims_dependencies->uri_view.substr(strlen(sys.local_uri));
    // }
    // // method check:
    // sys.fims_dependencies->method = FimsMethod::Unknown;

    if (io_fims->method_view == "set")
    {
        std::cout << "We got a set method " << std::endl;
        //sys.fims_dependencies->method = FimsMethod::Set;

        // // the outstation can't do "sets" unless it's in local mode (i.e. sent to local_uri)
        // if (sys.protocol_dependencies->who == DNP3_OUTSTATION &&
        //     !sys.fims_dependencies->is_forced_value_set &&
        //     !sys.fims_dependencies->is_request)
        // {
        //     FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported by dnp3_server. Message dropped",
        //                   sys.fims_dependencies->name.c_str(),
        //                   sys.fims_dependencies->process_name_view,
        //                   sys.fims_dependencies->method_view);
        //     FPS_LOG_IT("fims_method_error");

        //     return false;
        // }
    }
    else if (io_fims->method_view == "get")
    {
        std::cout << "We got a get method " << std::endl;
        // sys.fims_dependencies->method = FimsMethod::Get;
        // if (sys.fims_dependencies->replyto_view.empty())
        // {
        //     FPS_ERROR_LOG("Listener for '%s', could not complete request (no reply-to).",
        //                   sys.fims_dependencies->name.c_str());
        //     FPS_LOG_IT("fims_no_reply");

        //     return false;
        // }
    }
    else if (io_fims->method_view == "pub")
    {
        std::cout << "We got a get method " << std::endl;
        // sys.fims_dependencies->method = FimsMethod::Pub;

        // // the client can't listen for "pubs" unless it's a watchdog uri
        // if (sys.protocol_dependencies->who == DNP3_MASTER)
        // {
        //     if (sys.debug > 0)
        //     {
        //         FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported by dnp3_server. Message dropped",
        //                       sys.fims_dependencies->name.c_str(),
        //                       sys.fims_dependencies->process_name_view,
        //                       sys.fims_dependencies->method_view);
        //         FPS_LOG_IT("fims_method_error");
        //     }
        //     return false;
        // }
    }
    else // method not supported by gcom_client (it's not set, pub, or get)
    {
        std::cout << "We got a mystery  method " << std::endl;

    //     FPS_ERROR_LOG("Listener for : %s, from sender: %s method %s is not supported. Message dropped",
    //                   sys.fims_dependencies->name.c_str(),
    //                   sys.fims_dependencies->process_name_view,
    //                   sys.fims_dependencies->method_view);
    //     FPS_LOG_IT("fims_method_error");

    //     if (!sys.fims_dependencies->replyto_view.empty())
    //     {
    //         static constexpr auto err_str = "Unsupported fims method"sv;

    //         if (!send_set(sys.fims_dependencies->fims_gateway, sys.fims_dependencies->replyto_view, err_str))
    //         {
    //             FPS_ERROR_LOG("Listener for '%s', could not send replyto fims message.",
    //                           sys.fims_dependencies->name.c_str());
    //             FPS_LOG_IT("fims_send_error");

    //             return false;
    //         }
    //     }
    //     return false;
    }

    //printf("Fims receive buffer len %d .  message len  %d\n", (int)sys.fims_data_buf_len, (int)meta_data.data_len);
    if (meta_data.data_len > static_cast<uint32_t>(io_fims->fims_data_buf_len))
    {
        //FPS_ERROR_LOG
        printf("Fims receive buffer is too small. Recommend increasing data_buf_len to at least %d\n", meta_data.data_len);
        auto orig_len =(int)io_fims->fims_data_buf_len;
        printf("Extra data offset %d required  %d\n", (int)io_fims->fims_data_buf_len, meta_data.data_len-orig_len);
        //io_fims->reset_fims_data_buf((int)(meta_data.data_len * 1.5));

        //gcom_recv_extra_raw_message(myCfg.fims_gateway, &myCfg.fims_input_buf[orig_len], meta_data.data_len-orig_len);


        //FPS_LOG_IT("fims_receive_buffer");
        return false;
    }

    return true;
}


bool fims_connect(struct cfg &myCfg, bool debug)
{
    //fims fims_gateway;
    std::string name("myname");
    const auto conn_id_str = fmt::format("modbus_client_uri@{}", name);
    const auto conn_test_str = fmt::format("modbus_client_test@{}", name);
    if (!myCfg.fims_gateway.Connect(conn_id_str.data()))
    {
        NEW_FPS_ERROR_PRINT("For client init uri \"{}\": could not connect to fims_server\n", name);
        return false;
    }
    if (!myCfg.fims_test.Connect(conn_test_str.data()))
    {
        NEW_FPS_ERROR_PRINT("For client testuri \"{}\": could not connect to fims_server\n", name);
        return false;
    }
    //for (auto &mysub : myCfg.subs)

    const auto sub_string = fmt::format("/modbus_client/{}", name);
    myCfg.subs.push_back(sub_string);

    if (!myCfg.fims_gateway.Subscribe(myCfg.subs))
    {
        NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": failed to subscribe for uri init\n", name);
        return false;
    }
    if(1||debug)
        NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": subscribed OK\n", name);
    return true;
}


/*
/// @brief 
// process the incoming fims message
/// @param io_fims 
/// @param myCfg 
/// @param debug 
/// @return 
*/
static bool runProcessFims(std::shared_ptr<IO_Fims>io_fims, struct cfg& myCfg, bool debug)
{
    std::string name("myname");

    std::cout << " Process Fims Message running" << std::endl;
    bool ok = true;
    if (ok)
    {
        //ok = parseHeader(myCfg, io_fims->meta_data, io_fims->fims_input_buf, data_buf_len);
        ok = parseHeader(myCfg, io_fims);//.meta_data, io_fims->fims_input_buf, data_buf_len);

        std::cout << " header parsed  " << io_fims->data_buf_len << std::endl;

        if(ok)
        {
            printf("client \"%s\": received a message len %d  [%.*s]\n", name.c_str(),  (int)io_fims->data_buf_len ,(int)io_fims->data_buf_len, io_fims->fims_data_buf  );
        }
        else
        {
            printf("client \"%s\": failed with a message len %d \n", name.c_str(),  (int)io_fims->data_buf_len );
        }
    }
    else
    {
        printf("client \"%s\": did not receive a message \n", name.c_str());

    }

    return true;
}

/*
/// @brief 
    fims_message processor
/// @param myCfg 
/// @return 
*/
//static 
bool process_thread(struct cfg& myCfg) noexcept 
{
    double delay = 0.1;
    int signal = -1;
    bool debug = false;
    bool run = true;
    int jobs = 0;
    bool save = false;
    while (run && myCfg.keep_running)
    {
        std::shared_ptr<IO_Fims>io_fims;
        //io_fimsProcessChan.send(std::move(io_fims));
        //io_fimsWakeChan.send(1);
        if(io_fimsWakeChan.receive(signal, delay)) {
            if(signal == 0) run = false;
            if(signal == 1) {
                jobs++;
                if (io_fimsProcessChan.receive(io_fims)) 
                {
                    save = runProcessFims(io_fims, myCfg, debug);
                }
                if (save)
                {
                    save_fims(io_fims, myCfg);
                }
            }
        }
    }
    {
        //std::lock_guard<std::mutex> lock2(io_output_mutex); 
        //FPS_INFO_LOG
        printf("process thread stopping after %d jobs",jobs) ;
        //CloseModbusForThread(io_thread, debug);
    }

    return true;
}


/*
/// @brief 
    high speed fims mesage dump
    just pass on the IO_Fims task to the process thread.
    allows us to keep up with possible fims traffic
/// @param myCfg 
/// @return 
*/
static bool listener_thread(std::vector<std::string>&subs, struct cfg& myCfg) noexcept 
{
    bool debug =  true;
    std::cout << " connect " << std::endl;
    
    std::string name("myname");
    const auto sub_string = fmt::format("/modbus_client/{}", name);
    
    //io_fims->fims_input_buf = nullptr;
    //std::cout << " set data buf " << std::endl;
    //set_io_fims_data_buf(io_fims, 100000);

    fims_connect(myCfg, debug);
    struct timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    if (setsockopt(myCfg.fims_gateway.get_socket(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) == -1)
    {
        NEW_FPS_ERROR_PRINT("listener for \"{}\": could not set socket timeout to 2 seconds. Exiting\n", myCfg.client_name);
        return false;
    }
    // we are going to use a shared pointer for our custom receivebuf
    // main loop:
    while (myCfg.keep_running)
    {
        auto io_fims = make_fims(myCfg);

        //u64 data_buf_len = 0;
        bool ok = gcom_recv_raw_message(myCfg.fims_gateway, io_fims);
        if (ok)
        {
            std::cout << " received message " << std::endl;
            send_fims(io_fims, myCfg);
        }
    }

    return true;
}

        // //ok = true;
        // if (ok)
        // {
        //     //ok = parseHeader(myCfg, io_fims.meta_data, io_fims.fims_input_buf, data_buf_len);
        //     ok = parseHeader(myCfg, io_fims);//.meta_data, io_fims.fims_input_buf, data_buf_len);

        //     std::cout << " header parsed  " << io_fims.data_buf_len << std::endl;

        // // auto config_msg = myCfg.fims_gateway.Receive_Timeout(5000000); // give them 5 seconds to respond before erroring
        // // defer { fims::free_message(config_msg); };
        // // if (!config_msg)
        // // {
        // //     NEW_FPS_ERROR_PRINT("For client with init uri \"{}\": failed to receive a message in 5 seconds\n", name);
        // //     return false;
        // // }
        // // if (!config_msg->body)
        // // {
        // //     NEW_FPS_ERROR_PRINT("For client  with init uri \"{}\": message was received, but body doesn't exist\n", name);
        // //     return false;
        // // }
        // //NEW_FPS_ERROR_PRINT
        //     if(ok)
        //     {
        //         printf("client \"%s\": received a message len %d  [%.*s]\n", name.c_str(),  (int)io_fims.data_buf_len ,(int)io_fims.data_buf_len, io_fims.fims_data_buf  );
        //     }
        //     else
        //     {
        //         printf("client \"%s\": failed with a message len %d \n", name.c_str(),  (int)io_fims.data_buf_len );
        //     }
        // }
        // else
        // {
        //     printf("client \"%s\": did not receive a message \n", name.c_str());

        // }




    // constants:
    // static constexpr auto Raw_Request_Suffix           = "/_raw"sv;
    // static constexpr auto Timings_Request_Suffix       = "/_timings"sv;
    // static constexpr auto Reset_Timings_Request_Suffix = "/_reset_timings"sv;
    // static constexpr auto Reload_Request_Suffix        = "/_reload"sv; // this will go under the /exe_name/controls uri
    // // NOTE(WALKER): an "x_request" is a "get" request that ends the get "uri" in one of the above suffixes
    // // for example:
    // // get on uri : "/components/bms_info"      -> regular
    // // raw get uri: "/components/bms_info/_raw" -> raw_request
    // // NOTE(WALKER): a "reload" request is a "set" request that ends with /_reload (this reloads the WHOLE client, NOT just that particular component)

    // // method types supported by modbus_client:
    // enum class Method_Types : u8 {
    //     Set,
    //     Get,
    //     Pub
    // };

    // // listener variables:
    // auto& my_workspace     = *main_workspace.client_workspaces[client_idx];
    // const auto client_name = main_workspace.string_storage.get_str(my_workspace.conn_workspace.conn_info.name);
    // auto& myCfg.fims_gateway     = my_workspace.myCfg.fims_gateway;
    // auto& receiver_bufs    = my_workspace.receiver_bufs;
    // auto& meta_data        = receiver_bufs.meta_data;
    // auto& parser           = my_workspace.parser;
    // auto& doc              = my_workspace.doc;
    // auto& main_work_qs     = my_workspace.main_work_qs;

    // // for telling main that there was something wrong:
    // defer {
    //     my_workspace.keep_running = false;
    //     main_work_qs.signaller.signal(); // tell main to shut everything down
    // };

    // wait until main signals everyone to start:
    // {
    //     std::unique_lock<std::mutex> lk{main_workspace.main_mutex};
    //     main_workspace.main_cond.wait(lk, [&]() {
    //         return main_workspace.start_signal.load();
    //     });
    // }

    // setup the timeout to be 2 seconds (so we can stop listener thread without it spinning infinitely on errors):

// this file


    // std::cout << " set data buf " << std::endl;
    // set_fims_data_buf(myCfg, 25);

    

    // TODO make this an optional config item
    //unsigned int mydata_size = myCfg.fims_data_buf_len;
    //unsigned char *mydata = (unsigned char *)calloc(1, mydata_size);


    //auto &meta_data = io_fims.meta_data;
    //     if (!recv_raw_message(myCfg.fims_gateway.get_socket(), myCfg.receiver_bufs))
    //     {
    //         const auto curr_errno = errno;
    //         if (curr_errno == EAGAIN || curr_errno == EWOULDBLOCK || curr_errno == EINTR) continue; // we just timed out
    //         // This is if we have a legitimate error (no timeout):
    //         NEW_FPS_ERROR_PRINT("Listener for \"{}\": could not receive message over fims. Exiting\n", myCfg.client_name);
    //         return false;
    //     }

    //     // meta data views:
    //     const auto method_view       = std::string_view{myCfg.receiver_bufs.get_method_data(), meta_data.method_len};
    //     auto uri_view                = std::string_view{myCfg.receiver_bufs.get_uri_data(), meta_data.uri_len};
    //     const auto replyto_view      = std::string_view{myCfg.receiver_bufs.get_replyto_data(), meta_data.replyto_len};
    //     const auto process_name_view = std::string_view{myCfg.receiver_bufs.get_process_name_data(), meta_data.process_name_len};
    //     // const auto user_name_view    = std::string_view{receiver_bufs.get_user_name_data(), meta_data.user_name_len};

    //     Method_Types method;

    //     // method check:
    //     if (method_view == "set")
    //     {
    //         method = Method_Types::Set;
    //     }
    //     else if (method_view == "get")
    //     {
    //         method = Method_Types::Get;
    //     }
    //     else // method not supported by modbus_client
    //     {
    //         NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": method \"{}\" is not supported by modbus_client. Message dropped\n",
    //          myCfg.client_name, process_name_view, method_view);
    //         if (!replyto_view.empty())
    //         {
    //             static constexpr auto err_str = "Modbus Client -> method not supported"sv;
    //             if (!send_set(myCfg.fims_gateway, replyto_view, err_str))
    //             {
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": could not send replyto fims message. Exiting\n"
    //                         , myCfg.client_name, process_name_view);
    //                 return false;
    //             }
    //         }
    //         continue;
    //     }

    //     // /_x request checks:
    //     const bool is_raw_request           = str_ends_with(uri_view, Raw_Request_Suffix);
    //     const bool is_timings_request       = str_ends_with(uri_view, Timings_Request_Suffix);
    //     const bool is_reset_timings_request = str_ends_with(uri_view, Reset_Timings_Request_Suffix);
    //     const bool is_reload_request        = str_ends_with(uri_view, Reload_Request_Suffix);
    //     if (is_raw_request)           uri_view.remove_suffix(Raw_Request_Suffix.size());
    //     if (is_timings_request)       uri_view.remove_suffix(Timings_Request_Suffix.size());
    //     if (is_reset_timings_request) uri_view.remove_suffix(Reset_Timings_Request_Suffix.size());
    //     if (is_reload_request)        uri_view.remove_suffix(Reload_Request_Suffix.size());

    //     // uri check:
    //     //uri_view.remove_prefix(Main_Uri_Frag_Length); // removes /components/ from the uri for processing (hashtable uri lookup)
    //     //const auto uri_it = my_workspace.uri_map.find(uri_map_hash(uri_view));
    //     // TODO accomdate the single uri
    //     // uri split 
    //     auto uri_keys = split(uri_view, "/")
    //     // find itemMap[uri[0]][uri[1]


    //     auto result = findMapVar(myCfg, uri_keys);
    //     if (std::holds_alternative<MapPointer>(result) && std::get<MapPointer>(result) != nullptr) {
    //         //MapPointer pointer = std::get<MapPointer>(result);
    //         std::cout << " uri : "<< uri << " found  map of items " << std::endl;
    //         // Use pointer as MapPointer
    //     } else if (std::holds_alternative<MapStructPointer>(result) && std::get<MapStructPointer>(result) != nullptr) {
    //         //MapStructPointer pointer = std::get<MapStructPointer>(result);
    //         // Use pointer as MapStructPointer  this is a single set 
    //         std::cout << " uri : "<< uri << " found  map item ptr" << std::endl;
    //         // get the encode from any and queue th IO_Work.

    //     } else {
    //         // Handle the case where no value is found
    //         std::cout << " uri : "<< uri << " not found " << std::endl;
    //         NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": uri \"{}\" could not be found. Message dropped\n", myCfg.client_name, process_name_view, uri_view);
    //         if (!replyto_view.empty())
    //         {
    //             static constexpr auto err_str = "Modbus Client -> uri doesn't exist"sv;
    //             if (!send_set(myCfg.fims_gateway, replyto_view, err_str))
    //             {
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": could not send replyto fims message. Exiting\n", myCfg.client_name, process_name_view, uri_view);
    //                 return false;
    //             }
    //         }
    //         continue;
    //     }

    //     // auto& msg_staging_area = my_workspace.msg_staging_areas[mapItem->component_idx];

    //     if (is_reload_request)
    //     {
    //         // POTENTIAL TODO(WALKER): do these need to respond to "replyto"? (for now no, seems kinda redundant)
    //         if (method != Method_Types::Set)
    //         {
    //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": reload request method must be \"set\"\n", myCfg.client_name, process_name_view);
    //             continue;
    //         }
    //         else
    //         {
    //             NEW_FPS_ERROR_PRINT_NO_ARGS("reload request received, reloading modbus_client\n");
    //             myCfg.reload = true;
    //             return true;
    //         }
    //     }

    //     // do logic based on uri type and multi/single uri type

    //     if (method == Method_Types::Set)
    //     {
    //         // defer replyto so we don't have to have bloated if/else logic everywhere:
    //         bool set_had_errors = false;
    //         std::string_view success_str;
    //         defer {
    //             static constexpr auto err_str = "Modbus Client -> set had errors"sv;
    //             if (!replyto_view.empty())
    //             {
    //                 bool fims_ok = true;
    //                 if (!set_had_errors && !send_set(myCfg.fims_gateway, replyto_view, success_str))
    //                 {
    //                     fims_ok = false;
    //                 }
    //                 else if (set_had_errors && !send_set(myCfg.fims_gateway, replyto_view, err_str))
    //                 {
    //                     fims_ok = false;
    //                 }
    //                 if (!fims_ok)
    //                 {
    //                     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n"
    //                         , myCfg.client_name, process_name_view, uri_view);
    //                     // no return, but next receive() call will error out at the top
    //                 }
    //             }
    //         };

    //         // no raw/timings requests for set:
    //         if (is_raw_request || is_timings_request || is_reset_timings_request)
    //         {
    //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": sets are not accepted on raw/timings request uris.\n"
    //                 , myCfg.client_name, process_name_view);
    //             set_had_errors = true;
    //             continue;
    //         }

    //         // got a more data than expected (right now this is 100,000):
    //         if (meta_data.data_len > receiver_bufs.get_max_expected_data_len())
    //         {
    //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": got a timeouta \"set\" request but got {} bytes even though the maximum expected for modbus_client is {}. Please reduce message sizes or increase the maximum expected for modbus_client. Message dropped\n"
    //                 , myCfg,client_name, process_name_view, meta_data.data_len, receiver_bufs.get_max_expected_data_len());
    //             set_had_errors = true;
    //             continue;
    //         }
    //         // now we need to decode the data into an any structure

    //         void* data = nullptr;
    //         defer { if (data && myCfg.fims_gateway.has_aes_encryption()) free(data); };
    //         data = decrypt_buf(receiver_bufs);

    //         if (!data)
    //         {
    //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": got a set request with no body. Message dropped\n"
    //                         , myCfg.client_name, process_name_view, uri_view);
    //             set_had_errors = true;
    //             continue;
    //         }

    //         // set success_str for replyto defer:
    //         success_str = std::string_view{reinterpret_cast<const char*>(data), meta_data.data_len};

    //         // if we don't have encryption set padding bytes inside buffer to '\0' so we can use a view when parsing
    //         if (!myCfg.fims_gateway.has_aes_encryption())
    //         {
    //             memset(reinterpret_cast<u8*>(data) + meta_data.data_len, '\0', Simdj_Padding);
    //         }
    //         // parse into a std::any structure
    //         std::any gcom_set;
    //         // gcom_config_any
    //         gcom_parse_data(gcom_set, data, meta_data.data_len, false);
    //         if (gcom_set.type() == typeid(std::map<std::string, std::any>)) {

    //             //MapPointer pointer = std::get<MapPointer>(result);

    //             auto mkey = std::any_cast<std::map<std::string, std::any>>(gcom_set);
    //             for (auto key : mkey) {
    //                 std::cout << " key " << key.first << "\n";
    //                 //TODO create the IO_Work items
    //             }

    //         }

    //          // gcom_set will either be a value or a map of data value pairs
    //         // well we could do this..
    //         // but we wont

    //         // // NOTE(WALKER): "parser" holds the iterator of the json string and the doc gives you "handles" back to parse the underlying strings
    //         // if (const auto err = parser.iterate(reinterpret_cast<const char*>(data), meta_data.data_len, meta_data.data_len + Simdj_Padding).get(doc); err)
    //         // {
    //         //     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": could not parse json string from set request, err = {} Message dropped\n"
    //         //         , myCfg.client_name, process_name_view, uri_view, simdjson::error_message(err));
    //         //     set_had_errors = true;
    //         //     continue;
    //         // }

    //         // // now set onto channels based on multi or single set uri:
    //         // Jval_buif to_set;
    //         // Set_Work set_work;
    //         // if (mapItem->decode_idx == Decode_All_Idx) // multi-set
    //         // {
    //         //     auto& uri_buf = my_workspace.uri_buf;

    //         //     simdjson::ondemand::object set_obj;
    //         //     if (const auto err = doc.get(set_obj); err)
    //         //     {
    //         //         NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": could not get multi-set fims message as a json object, err = {} Message dropped\n", myCfg.client_name, process_name_view, uri_view, simdjson::error_message(err));
    //         //         set_had_errors = true;
    //         //         continue;
    //         //     }

    //         //     bool inner_json_it_err = false; // if we have a "parser" error during object iteration we "break" 
    //         //     // iterate over object and extract out values (clothed or unclothed):
    //         //     for (auto pair : set_obj)
    //         //     {
    //         //         const auto key = pair.unescaped_key();
    //         //         if (const auto err = key.error(); err)
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\": parsing error on getting a multi-set key, err = {} Message dropped\n", myCfg.client_name, process_name_view, uri_view, simdjson::error_message(err));
    //         //             inner_json_it_err = true;
    //         //             break;
    //         //         }
    //         //         const auto key_view = key.value_unsafe();
    //         //         auto val = pair.value();
    //         //         if (const auto err = val.error(); err)
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with base uri \"{}\", on multi-set key \"{}\": parse error on getting value, err = {} Message dropped\n", myCfg.client_name, process_name_view, uri_view, key_view, simdjson::error_message(err));
    //         //             inner_json_it_err = true;
    //         //             break;
    //         //         }

    //         //         uri_buf.clear();
    //         //         FORMAT_TO_BUF(uri_buf, "{}/{}", uri_view, key_view);
    //         //         const auto curr_set_uri_view = std::string_view{uri_buf.data(), uri_buf.size()};

    //         //         // uri check:
    //         //         const auto inner_uri_it = my_workspace.uri_map.find(uri_map_hash(curr_set_uri_view));
    //         //         if (inner_uri_it == my_workspace.uri_map.end())
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": multi-set uri \"{}\" could not be found. Skipping that set\n", myCfg.client_name, process_name_view, curr_set_uri_view);
    //         //             continue;
    //         //         }
    //         //         const auto& inner_uri_info = inner_uri_it->second;

    //         //         // reg type check:
    //         //         if (inner_uri_info.reg_type == Register_Types::Input || inner_uri_info.reg_type == Register_Types::Discrete_Input)
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": This uri is of a register type that does not accept set requests. Skipping this uri's set\n", myCfg.client_name, process_name_view, curr_set_uri_view);
    //         //             continue;
    //         //         }

    //         //         // extract out value (clothed or unclothed):
    //         //         auto curr_val = val.value_unsafe();
    //         //         auto val_clothed = curr_val.get_object();
    //         //         if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": could not get a clothed value as an object while parsing a multi-set, err = {} Message dropped\n", myCfg.client_name, process_name_view, curr_set_uri_view, simdjson::error_message(err));
    //         //             inner_json_it_err = true;
    //         //             break;
    //         //         }
    //         //         if (!val_clothed.error())
    //         //         {
    //         //             auto inner_val = val_clothed.find_field("value");
    //         //             if (const auto err = inner_val.error(); err)
    //         //             {
    //         //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": could not get the clothed key \"value\" while parsing a multi-set, err = {} skipping this uri's set\n", myCfg.client_name, process_name_view, curr_set_uri_view, simdjson::error_message(err));
    //         //                 continue;
    //         //             }
    //         //             curr_val = std::move(inner_val.value_unsafe());
    //         //         }

    //         //         // extract out value based on type into Jval:
    //         //         if (auto val_uint = curr_val.get_uint64(); !val_uint.error())
    //         //         {
    //         //             to_set = val_uint.value_unsafe();
    //         //         }
    //         //         else if (auto val_int = curr_val.get_int64(); !val_int.error())
    //         //         {
    //         //             to_set = val_int.value_unsafe();
    //         //         }
    //         //         else if (auto val_float = curr_val.get_double(); !val_float.error())
    //         //         {
    //         //             to_set = val_float.value_unsafe();
    //         //         }
    //         //         else if (auto val_bool = curr_val.get_bool(); !val_bool.error())
    //         //         {
    //         //             to_set = static_cast<u64>(val_bool.value_unsafe()); // just set booleans equal to the whole numbers 1/0 for sets
    //         //         }
    //         //         else
    //         //         {
    //         //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": only floats, uints, ints, and bools are supported for sets, skipping this uri's set\n", myCfg.client_name, process_name_view, curr_set_uri_view);
    //         //             continue;
    //         //         }

    //         //         // check for true/false and 1/0 for coils and individual_bits:
    //         //         if (inner_uri_info.reg_type == Register_Types::Coil || inner_uri_info.flags.is_individual_bit())
    //         //         {
    //         //             if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL))
    //         //             {
    //         //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with multi-set uri \"{}\": cannot set Coil or individual_bit uri to the value: {}, only true/false and 1/0 are accepted, skipping this uri's set\n", myCfg.client_name, process_name_view, curr_set_uri_view, to_set);
    //         //                 continue;
    //         //             }
    //         //         }

    //         //         set_work.set_vals.emplace_back(Set_Info{to_set, 0, 
    //         //             inner_uri_info.component_idx, 
    //         //             inner_uri_info.thread_idx, 
    //         //             inner_uri_info.decode_idx, 
    //         //             inner_uri_info.bit_idx, 
    //         //             inner_uri_info.enum_idx});
    //         //     }
    //         //     if (inner_json_it_err)
    //         //     {
    //         //         set_had_errors = true;
    //         //         continue;
    //         //     }

    //         //     // This means that all of the sets in this set had an error (but not a parsing error):
    //         //     // for example, all of them tried to set on "Input" registers
    //         //     if (set_work.set_vals.empty())
    //         //     {
    //         //         NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": all of the sets for this message had logical errors in one way or another (for example: all the set uris were \"Input\" registers). These are NOT parsing errors. Message dropped\n", myCfg.client_name, process_name_view);
    //         //         set_had_errors = true;
    //         //         continue;
    //         //     }

    //         //     if (!main_work_qs.set_q.try_emplace(std::move(set_work)))
    //         //     {
    //         //         NEW_FPS_ERROR_PRINT_NO_ARGS("could not push a set message onto a channel, things are really backed up. This should NEVER happpen, shutting the whole thing down\n");
    //         //         return false;
    //         //     }
    //         //     main_work_qs.signaller.signal(); // tell main to wake up -> It has work to do
    //         // }


    //         else // single-set:
    //         {
    //             // reg type check:
    //             if (mapItem->reg_type == cfg::Register_Types::Input || mapItem->reg_type == cfg::Register_Types::Discrete_Input)
    //             {
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": Register type {} does not accept set requests. Message dropped\n"
    //                             , myCfg.client_name, process_name_view, uri_view, );
    //                 set_had_errors = true;
    //                 continue;
    //             }

    //             simdjson::ondemand::value curr_val;
    //             auto val_clothed = doc.get_object();
    //             if (const auto err = val_clothed.error(); !(err == simdjson::error_code::SUCCESS || err == simdjson::error_code::INCORRECT_TYPE))
    //             {
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": cannot check if set message is clothed in an object, err = {} Message dropped\n", myCfg.client_name, process_name_view, uri_view, simdjson::error_message(err));
    //                 set_had_errors = true;
    //                 continue;
    //             }
    //             if (!val_clothed.error()) // they sent a clothed value:
    //             {
    //                 auto inner_val = val_clothed.find_field("value");
    //                 if (const auto err = inner_val.error(); err)
    //                 {
    //                     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": could not get \"value\" as a key inside clothed object, err = {} Message dropped\n", myCfg.client_name, process_name_view, uri_view, simdjson::error_message(err));
    //                     set_had_errors = true;
    //                     continue;
    //                 }
    //                 curr_val = std::move(inner_val.value_unsafe());
    //             }

    //             // NOTE(WALKER): This logic has to be like this instead of like the multi set
    //             // because simdjson has a special case where if the json string itself is just a scalar
    //             // then you can't use doc.get_value() (will return an error code). So this is custom tailored 
    //             // to take that into account. Do NOT try and duplicate the multi set logic for this.
    //             if (auto val_uint = val_clothed.error() ? doc.get_uint64() : curr_val.get_uint64(); !val_uint.error()) // it is an unsigned integer
    //             {
    //                 to_set = val_uint.value_unsafe();
    //             }
    //             else if (auto val_int = val_clothed.error() ? doc.get_int64() : curr_val.get_int64(); !val_int.error()) // it is an integer
    //             {
    //                 to_set = val_int.value_unsafe();
    //             }
    //             else if (auto val_float = val_clothed.error() ? doc.get_double() : curr_val.get_double(); !val_float.error()) { // it is a float
    //                 to_set = val_float.value_unsafe();
    //             } else if (auto val_bool = val_clothed.error() ? doc.get_bool() : curr_val.get_bool(); !val_bool.error()) { // it is a bool
    //                 to_set = static_cast<u64>(val_bool.value_unsafe()); // just set booleans to the whole numbers 1/0
    //             } else { // unsupported type:
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": only floats, uints, ints, and bools are supported for sets. Message dropped\n", myCfg.client_name, process_name_view, uri_view);
    //                 set_had_errors = true;
    //                 continue;
    //             }

    //             // check for true/false and 1/0 for coils and individual_bits:
    //             if (mapItem->reg_type == Register_Types::Coil || mapItem->flags.is_individual_bit()) {
    //                 if (!to_set.holds_uint() || !(to_set.get_uint_unsafe() == 1UL || to_set.get_uint_unsafe() == 0UL)) {
    //                     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with single-set uri \"{}\": cannot set Coil or individual_bit uri to the value: {}, only true/false and 1/0 are accepted. Message dropped\n", myCfg.client_name, process_name_view, uri_view, to_set);
    //                     set_had_errors = true;
    //                     continue;
    //                 }
    //             }
    //             // push onto setChan
    //             set_work.set_vals.emplace_back(Set_Info{to_set, 0, mapItem->component_idx, mapItem->thread_idx, mapItem->decode_idx, mapItem->bit_idx, mapItem->enum_idx});
    //             if (!main_work_qs.set_q.try_emplace(std::move(set_work))) {
    //                 NEW_FPS_ERROR_PRINT_NO_ARGS("could not push a set message onto a channel, things are really backed up. This should NEVER happpen, shutting the whole thing down\n");
    //                 return false;
    //             }
    //             main_work_qs.signaller.signal(); // tell main there is work to do
    //         }
    //     } else { // "get" (replyto required)
    //         if (replyto_view.empty()) {
    //             NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with uri \"{}\": get request does not have a replyto. Message dropped\n"
    //                     , myCfg.client_name, process_name_view, uri_view);
    //             continue;
    //         }
    //         if (is_timings_request || is_reset_timings_request) { // they can only do timings requests on whole components (NOT single registers)
    //             if (mapItem->decode_idx != Decode_All_Idx) {
    //                 static constexpr auto err_str = "Modbus Client -> timings requests can NOT be given for a single register"sv;
    //                 NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\", with uri \"{}\": get requests for single register uris cannot accept timing requests. Message dropped\n",
    //                         myCfg.client_name, process_name_view, uri_view);
    //                 if (!send_set(myCfg.fims_gateway, replyto_view, err_str)) {
    //                     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n", myCfg.client_name, process_name_view, uri_view);
    //                     // fims error will cause listener to exit at the top on next receive() call
    //                 }
    //                 continue;
    //             }
    //             if (is_reset_timings_request) { // send them a message back to indicate reset 
    //                 static constexpr auto success_str = "Modbus Client -> timings reset"sv;
    //                 if (!send_set(myCfg.fims_gateway, replyto_view, success_str)) {
    //                     NEW_FPS_ERROR_PRINT("Listener for \"{}\", from sender: \"{}\": error sending replyto to uri \"{}\" on fims.\n", myCfg.client_name, process_name_view, uri_view);
    //                     // fims error will cause listener to exit at the top on next receive() call
    //                 }
    //             }
    //         }

    //         Get_Work get_work;
    //         get_work.flags.flags = 0;
    //         get_work.flags.set_raw_request(is_raw_request);
    //         get_work.flags.set_timings_request(is_timings_request);
    //         get_work.flags.set_reset_timings_request(is_reset_timings_request);
    //         get_work.comp_idx   = mapItem->component_idx;
    //         get_work.thread_idx = mapItem->thread_idx;
    //         get_work.decode_idx = mapItem->decode_idx;
    //         get_work.bit_idx    = mapItem->bit_idx;
    //         get_work.enum_idx   = mapItem->enum_idx;
    //         get_work.replyto    = replyto_view;
    //         if (!main_work_qs.get_q.try_emplace(std::move(get_work))) {
    //             NEW_FPS_ERROR_PRINT("could not push onto \"get\" channel for uri \"{}\" as it was full. From sender: \"{}\". Shutting the whole thing down\n", uri_view, process_name_view);
    //             return false;
    //         }
    //         main_work_qs.signaller.signal(); // tell main there is work to do
    //     }
    // }


bool start_fims(std::vector<std::string>&subs, struct cfg& myCfg)
{
    auto fims_thread = std::thread(listener_thread, std::ref(subs), std::ref(myCfg));

    return true;
}

bool test_fims_connect(struct cfg &myCfg, bool debug)
{
    std::string name("myname");
    const auto sub_string = fmt::format("/modbus_client/{}", name);
    //myCfg.fims_input_buf = nullptr;

    auto io_fims = make_fims(myCfg);
    
    //std::string name("myname");
    //const auto sub_string = fmt::format("/modbus_client/{}", name);
    
    // io_fims.fims_input_buf = nullptr;
    // std::cout << " set io_fims data buf " << std::endl;
    // set_io_fims_data_buf(io_fims, 100000);


    std::cout << " connect " << std::endl;
    fims_connect(myCfg, debug);
    

    // TODO make this an optional config item
    //unsigned int mydata_size = myCfg.fims_data_buf_len;
    //unsigned char *mydata = (unsigned char *)calloc(1, mydata_size);

    //auto &meta_data = io_fims.meta_data;

    bool ok;
    for (int i = 0 ; i < 3 ; ++i)
    {
        const auto body = fmt::format("hello from modbus_client {} message # {}", name, i);
        //std::cout << " send message # " << i <<  std::endl;
        if (!myCfg.fims_test.Send(
                                fims::str_view{"set", sizeof("set") - 1}, 
                                fims::str_view{sub_string.data(), sub_string.size()}, 
                                fims::str_view{nullptr, 0}, 
                                fims::str_view{nullptr, 0},
                                fims::str_view{body.data(), body.size()}
                                ))
        {
            NEW_FPS_ERROR_PRINT("For client with inti uri \"{}\": failed to send a fims set message\n", name);
            return false;
        }
         //std::cout << " message  sent" << std::endl;

    //     //unsigned char *data_buf;
        //u64 data_buf_len = 0;
        std::cout << " receive message " << std::endl;

        ok = gcom_recv_raw_message(myCfg.fims_gateway, io_fims);
        std::cout << " received message " << std::endl;
    //     //ok = true;
        if (ok)
        {
            ok = parseHeader(myCfg, io_fims); //io_fims.meta_data, io_fims.fims_input_buf, data_buf_len);

            //ok = parseHeader(myCfg, io_fims); //meta_data, myCfg.fims_input_buf, data_buf_len);

    //     //NEW_FPS_ERROR_PRINT
            if(1||ok)
            {
                    //sys.fims_data_buf;// = (char *)&data_buf[ix];
    //sys.fims_data_len;// = meta_data.data_len;

                printf("client \"%s\": received a message len %d  [%.*s]\n", name.c_str(),  (int)io_fims->data_buf_len ,(int)io_fims->data_buf_len, io_fims->fims_data_buf  );
            }
            else
            {
                printf("client \"%s\": failed with a message len %d \n", name.c_str(),  (int)io_fims->data_buf_len );
            }
        }
        else
        {
            printf("client \"%s\": did not receive a message \n", name.c_str());

        }
    }
    //delete (myCfg.fims_gateway);
    //delete (myCfg.fims_test);

    return true;
}
