// this is an attempt to use the original threadiung model.
// it was brittle and flaky
// now supplanted by test_iothread.cpp

#include <chrono>
#include <thread>
#include <iostream>
#include <future>

#include <sys/uio.h> // for receive timouet calls
#include <sys/socket.h> // for receive timeout calls

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

#include "gcom_timer.h"



//#include "fims/libfims.h"

#include "modbus/config_loaders/client_config_loader.hpp"
//  we dont need  this lot now #include "modbus/client/client_structs.hpp"
#include "semaphore.hpp" // Linux semaphore wrapper with helper functions (use PCQ_Semaphore)
#include "gcom_iothread.h"

using namespace std::chrono_literals;
using namespace std::string_view_literals;
std::mutex output_mutex;

// this thread's sole job is to send or get data from teh modbus client.
// It knows nothing about workspaces, config or any of that stuff
// It only knows a direction (set or get) modbus context , a device id , a register type , a starting offset, a number of registers and a data area or decode buffer.
// it gets all of this through a "work" message that arrives in a "set" or a "get" queue.
// 
// if it is a set then it will need the decode buffer to contain the set data.
// if it is a get then it will fill out the returned data in the decode buffer.
// the modbus server may have errors or the attempted transaction may cause an error.
// it is very important to diffrentiate between a complete or a partial failure. A timeout is a partial failure. We have to reconnect and try again.

// should the message fail, the return code will contain the error, the return code will also contain the time the transaction took and 
// it looks like we'll have to have some sort of unique transaction id.
 
// in any case the message will be returned to the main thread to be handled there.

// All the encode / decode stuff is handled outside of this thread.
// there is no need for any config area to be provided we'll have it all contained in the input message  to the thread queue.


// constants:
// TODO(WALKER): "broken pipe?" -> larger integer? (will have to investigate)
// static constexpr auto Modbus_Errno_Disconnect   = 104; // "Connection reset by peer" -> disconnect
// static constexpr auto Modbus_Errno_Cant_Connect = 115; // "Operation now in progress" -> no connection
//static constexpr auto Modbus_Server_Busy = 10126; // "Resource temporarily unavailable" -> server busy

// helper macros:
#define FORMAT_TO_BUF(fmt_buf, fmt_str, ...)            fmt::format_to(std::back_inserter(fmt_buf), FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define FORMAT_TO_BUF_NO_COMPILE(fmt_buf, fmt_str, ...) fmt::format_to(std::back_inserter(fmt_buf), fmt_str, ##__VA_ARGS__)


//extern Main_Workspace *main_workspace_ptr;

// I think we need the following control logic for this
// main 
//      set io q
//      get io q
//      keep running
//      io q
//      signaller
// thread
//      keep running
//      mb context
//       qs signaller
// perf system 
//      mono_clock



Channel<int> wakeChan;
Channel<IO_Work*> setChan;
Channel<IO_Work*> pollChan;
Channel<IO_Work*> respChan;
Channel<IO_Work*> storeChan;

bool GCOM_IO_thread(IO_Thread* thread, bool *debug) noexcept {
    {
        std::lock_guard<std::mutex> lock2(output_mutex); 
        if(*debug)std::cout << " thread "<< thread->id <<" launched  " << std::endl; 
    }
    //auto& mono_clock       = main_workspace_ptr->mono_clock;                       //we may need this 
    // auto& str_storage      = main_workspace_ptr->string_storage;
   //auto& client_workspace = *main_workspace_ptr->client_workspaces[0];
    //auto& my_workspace     = client_workspace.conn_workspace.IO_thread_pool.io_thread_workspaces[thread_id];
//    auto& work_qs          = client_workspace.conn_workspace.IO_thread_pool.io_work_qs;
    auto& main_work_qs     = thread->main_work->main_work_qs;
    auto& io_work_qs       = thread->main_work->io_work_qs;
  
    // If IO thread exits main needs to shut everything down and everything needs to stop anyway::
    defer {
        // looks like we'll need some sort of thread control structure maybe get that from the threadid
        thread->main_work->client_keep_running = false; // both main and listener share this (tell them something is wrong as things are backed up and this should never happen)
        thread->main_work->signaller.signal();
        {
            std::lock_guard<std::mutex> lock2(output_mutex); 
            if(*debug)std::cout << " thread "<< thread->id <<" died "  << std::endl; 
        }

    };
    {
        std::lock_guard<std::mutex> lock2(output_mutex); 
        if(*debug)std::cout << " thread "<< thread->id <<" waiting for main signal " << std::endl; 
    }
    //thread->main_work->signaller.wait(); // wait for start

    std::unique_lock<std::mutex> lock(*thread->mutex);
    thread->cv->wait(lock, [&thread]{ return *(thread->ready); });
    lock.unlock();
    {
        std::lock_guard<std::mutex> lock2(output_mutex); 
        if(*debug)std::cout << " thread "<< thread->id <<" got main run  signal " << std::endl; 
    }

    IO_Work  *set_work = nullptr;  // move into this from set_q
    IO_Work  *poll_work= nullptr;

    


    //Poll_Work poll_work; // move into this from poll_q (increment your priority here then)
    //Set_Work old_set_work; // move into this from poll_q (increment your priority here then)


    // we may use a perf structure to do this
    //s64 begin_poll_time;
    //s64 end_poll_time;

    // buffers to encode sets into and receive polls into:
    // these may be in the set_get work messages

    //u16 set_buffer[4];
    //u16 poll_buffer_u16[MODBUS_MAX_READ_REGISTERS];  // receive into this (holding and input)
    //u8  poll_buffer_u8[MODBUS_MAX_READ_REGISTERS];   // receive into this (coil and discrete input)

    while (thread->keep_running.load()) {
        {
            std::lock_guard<std::mutex> lock2(output_mutex); 
            //if(*debug)
            std::cout << " thread "<< thread->id <<" waiting for  io signal " << std::endl; 
        }
        io_work_qs.signaller.wait(); // wait for work to do
        {
            std::lock_guard<std::mutex> lock2(output_mutex); 
            //if(*debug)
            std::cout << "        thread "<< thread->id <<" got io signal " << std::endl; 
        }
        if (!thread->keep_running.load()) {
            {
                std::lock_guard<std::mutex> lock2(output_mutex); 
                //if(*debug)
                std::cout << " thread "<< thread->id <<" dying " << std::endl; 
            }
            break; // If main tells IO_threads to stop then stop
        }
        {
            std::lock_guard<std::mutex> lock2(output_mutex); 
            //if(*debug)
            std::cout << " thread "<< thread->id <<" running " << std::endl; 
        }
        bool gotSetWork = setChan.receive(set_work, std::chrono::microseconds(0));
        // do sets before polls:
        if (gotSetWork) {
//        if (io_work_qs.set_q.try_pop(set_work)) {
            {
                std::lock_guard<std::mutex> lock2(output_mutex); 
                if(*debug)std::cout << " thread "<< thread->id <<" got a set command " << std::endl; 
            }
            // we need ctx,device_id, type, offset,num_regs and data from set_work
            //  NEW STUFF
            auto err = modbus_set_slave(thread->ctx, set_work->device_id);
            {
                std::lock_guard<std::mutex> lock2(output_mutex); 
                if(*debug)std::cout << " thread  "<< thread->id <<" set slave err " << err << std::endl; 
            }
            //const auto& comp_workspace = *client_workspace.msg_staging_areas[set_work.set_vals[0].component_idx].comp_workspace;
            //modbus_set_slave(my_workspace.ctx, comp_workspace.decoded_caches[0].thread_workspace->slave_address);
            // for (auto& val : set_work.set_vals) {
            //     bool set_had_errors = false;
            //     int set_errors = 0;
            //     const auto& map_workspace = *comp_workspace.decoded_caches[val.thread_idx].thread_workspace;
                auto start = std::chrono::high_resolution_clock::now();

                set_work->errno_code = 0;
                switch(set_work->reg_type) {
                //switch(map_workspace.reg_type) {
                    case RegisterTypes::Holding: {
                        if (set_work->num_registers == 1)
                        {
                            set_work->errors = modbus_write_register(thread->ctx, set_work->offset, (int)set_work->u16_buff[0]);
                        }
                        else
                        {
                            set_work->errors = modbus_write_registers(thread->ctx, set_work->offset, set_work->size, set_work->u16_buff);
                        }
                        // const auto& curr_decode = map_workspace.decode_array[val.decode_idx];
                        // encode(
                        //     set_buffer, 
                        //     curr_decode, 
                        //     val.set_val, 
                        //     val.bit_idx, 
                        //     curr_decode.flags.is_individual_enums() ? 0UL : 1UL, // TODO(WALKER): in the future, this will go into bit_str_encoding_array and use the "care_masks" there (for now it is unused)
                        //     curr_decode.bit_str_array_idx != Bit_Str_Array_All_Idx ? &val.prev_unsigned_val : nullptr);
                        // if (curr_decode.flags.is_size_one() && !curr_decode.flags.is_multi_write_op_code()) { // special case for size 1 registers (apparently it doesn't like the multi-set if the size is 1, lame)
                        //     set_errors = modbus_write_register(my_workspace.ctx, curr_decode.offset, set_buffer[0]);
                        // } else { // write 2 or 4 registers (or single register with multi_write_op_code set to true)
                        //     set_errors = modbus_write_registers(my_workspace.ctx, curr_decode.offset, curr_decode.flags.get_size(), set_buffer);
                        // }
                        if (set_work->errors < 0 )
                            set_work->errno_code = errno;
                        else
                            set_work->errno_code = 0;

                        {
                            std::lock_guard<std::mutex> lock2(output_mutex); 
                            if(*debug)std::cout << " thread " << thread->id << " HOLDING set_work->errors " << set_work->errors<<" set_work->code " << modbus_strerror(set_work->errno_code)<< std::endl;
                        }
                        break;
                    }
                    case RegisterTypes::Coil: {
                        set_work->errors = modbus_write_bit(thread->ctx, set_work->offset, set_work->bit);
                        // const auto offset = map_workspace.bool_offset_array[val.decode_idx];
                        // set_errors = modbus_write_bit(my_workspace.ctx, offset, val.set_val.u == 1UL);
                        break;
                    }
                    default : {
                        set_work->errors = -2;
                    }
                    // set_had_errors = (set_errors < 0);
                    // if (set_had_errors) {
                    //     // TODO(WALKER): make this better, for now just cowabunga print it (prevent spamming in the future)
                    //     NEW_FPS_ERROR_PRINT("IO thread #{}, set had errors it = {} -> \"{}\" \n", thread_id, set_work.errno_code, modbus_strerror(set_work.errno_code));
                    //     continue;
                    // }
                }

                auto finish = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::micro> elapsed = finish - start;

                {
                    std::lock_guard<std::mutex> lock2(output_mutex); 
                    if(*debug)std::cout << " thread " << thread->id <<  " modbus_write_registers took " << elapsed.count() << " us\n";
                    set_work->response = elapsed.count();
                }
                if (!main_work_qs.pub_q.try_push(set_work)) {
                    NEW_FPS_ERROR_PRINT("IO thread #{} could not push onto the publish queue. This should NEVER happen. Exiting\n", thread->id);
                    return false;
                }
                main_work_qs.signaller.signal();

           //}
            //continue; // comment this out if you want a poll to be serviced before doing another set?
        }
        else
        {
    
            std::lock_guard<std::mutex> lock2(output_mutex); 
            //if(*debug)
            std::cout << " thread " << thread->id <<  " found no sets\n"; 
        }

        // repeat all of the set_work stuff for the read operations
        // but we can use the poll_work readbuffer
        // do polls after sets are clear:
        //signalChannel.send("External Signal 1");

        bool gotPollWork = pollChan.receive(poll_work, std::chrono::microseconds(0));
        // do sets before polls:
        int  got_poll = 0;
        if (gotPollWork) {
        //while (io_work_qs.poll_q.try_pop(poll_work)) {
        //if (io_work_qs.poll_q.try_pop(poll_work)) {
            {
                std::lock_guard<std::mutex> lock2(output_mutex);
                //if(*debug) {}
                std::cout << " thread "<< thread->id <<" got a poll command  reg_type " 
                        << (int)poll_work->reg_type <<" Discrete : " <<(int)Register_Types::Discrete_Input<< std::endl; 
                got_poll++;
            }
        }
        if(got_poll > 0) {
            //Pub_Work pub_work; // main reads off of its pub_q;
            //pub_work.comp_idx   = poll_work.comp_idx;
            //pub_work.thread_idx = poll_work.thread_idx;
            bool poll_had_errors = false;
            //int poll_errors = 0;
            //const auto& map_workspace = *client_workspace.msg_staging_areas[poll_work.comp_idx].comp_workspace->decoded_caches[poll_work.thread_idx].thread_workspace;

            // Do a poll into the appropriate register:
            modbus_set_slave(thread->ctx, poll_work->device_id);
            // TODO
            modbus_set_slave(thread->ctx, 255);
            //modbus_set_slave(my_workspace.ctx, map_workspace.slave_address);
            //modbus_set_debug(my_workspace.ctx, 1);

            //begin_poll_time = mono_clock.rdns();
            poll_work->errno_code = 0;
            auto start = std::chrono::high_resolution_clock::now();

            switch(poll_work->reg_type) {
                case RegisterTypes::Holding: {
                    poll_work->errors = modbus_read_registers(thread->ctx, poll_work->offset, poll_work->num_registers, poll_work->u16_buff);
                    break;
                }
                case RegisterTypes::Input: {
                    //std::cout << " thread Input "<< thread->id <<" poll offset  " << poll_work->offset; 
                    poll_work->errors = modbus_read_input_registers(thread->ctx, poll_work->offset, poll_work->num_registers, poll_work->u16_buff);
                    //std::cout << thread->id <<" poll errors  " << poll_work->errors <<  " value [" << (int)poll_work->u16_buff[0]<<"]"<< std::endl; 
                    break;
                }
                case RegisterTypes::Coil: {
                    poll_work->errors = modbus_read_bits(thread->ctx, poll_work->offset, poll_work->num_registers, poll_work->u8_buff);
                    break;
                }
                case RegisterTypes::Discrete_Input: {
                    if(*debug)
                        std::cout << " thread Discrete_Input "<< thread->id <<" poll offset  " << poll_work->offset; 

                    poll_work->errors = modbus_read_input_bits(thread->ctx, poll_work->offset, poll_work->num_registers, poll_work->u8_buff);
                    //std::cout << thread->id <<" poll errors  " << poll_work->errors <<  " value [" << (int)poll_work->u8_buff[0]<<"]"<< std::endl; 
                    // TODO 
                    poll_work->u16_buff[0] = poll_work->u8_buff[0]; 
                    break;
                }
                default: { // Discrete_Input
                    poll_work->errors =  -2;  // TODO warn about unknown register type
                }
            }
            auto finish = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> elapsed = finish - start;
            {
                std::lock_guard<std::mutex> lock2(output_mutex); 
                if(*debug)std::cout << " thread " << thread->id <<  " modbus_read_registers took " << elapsed.count() << " us\n";
                poll_work->response = elapsed.count();
            }
            if (poll_work->errors < 0)
            {
                poll_had_errors =  true;
                poll_work->errno_code = errno;
            }

            //end_poll_time = mono_clock.rdns();
            //pub_work.response_time = std::chrono::nanoseconds{end_poll_time - begin_poll_time};
            // here we handle the errors
            // If poll had errors then report it here: 
            // we need to react correctly to different errors 
            if (poll_had_errors) {
                 if (poll_work->errno_code == 110)
                 {
                    NEW_FPS_ERROR_PRINT("IO thread #{}, poll had errors it = {} -> \"{}\" \n", thread->id, poll_work->errno_code, modbus_strerror(poll_work->errno_code));
                    modbus_close(thread->ctx);
                    modbus_connect(thread->ctx);
                 }
                 if (poll_work->errno_code == 115)
                 {
                    //continue;
                    NEW_FPS_ERROR_PRINT("IO thread #{}, poll had errors it = {} -> \"{}\" \n", thread->id, poll_work->errno_code, modbus_strerror(poll_work->errno_code));
//                    modbus_close(my_workspace.ctx);
//                    modbus_connect(my_workspace.ctx);

                 }
                 if (
                        (poll_work->errno_code == 112345684)  //112345686 -> "Memory parity error"
                    || (poll_work->errno_code == 112345685)   //112345685 -> "Negative acknowledge"
                    || (poll_work->errno_code == 112345684)   //112345684 -> "Slave device or server is busy"
                    || (poll_work->errno_code == 112345683)   //112345683 -> "Acknowledge"
                 ) {
                    NEW_FPS_ERROR_PRINT("IO thread #{}, poll had errors it = {} -> \"{}\" \n", thread->id, poll_work->errno_code, modbus_strerror(poll_work->errno_code));
                    //continue;
                 }
                 if (
                        (poll_work->errno_code == 112345682)  //112345682 -> "Slave device or server failure"
                    || (poll_work->errno_code == 112345681)   //112345681 -> "Illegal data value"
                    || (poll_work->errno_code == 112345680)   //112345680 -> "Illegal data address"
                    || (poll_work->errno_code == 112345679)   //112345679 -> "Illegal function"

                 ) {
                    NEW_FPS_ERROR_PRINT("IO thread #{}, poll had errors it = {} -> \"{}\" \n", thread->id, poll_work->errno_code, modbus_strerror(poll_work->errno_code));

                    //return false;
                 }


               
                if (!main_work_qs.pub_q.try_push(poll_work)) {
                    NEW_FPS_ERROR_PRINT("IO thread #{} could not push onto the publish queue. This should NEVER happen. Exiting\n", thread->id);
                    return false;
                }
                main_work_qs.signaller.signal(); // signal main there is pub work to do
                //continue;
            }

            // no need to decode this lot here
            // just return the set or poll work object back to the main thread , its that simple
            // otherwise decode it based on the register type if we didn't have an error:
            // Jval_buif decoded_val;
            // auto& decoded_vals = pub_work.pub_vals;
            // decoded_vals.reserve(map_workspace.num_decode);

            // switch (map_workspace.reg_type) {
            //     // regular register stuff:
            //     case Register_Types::Holding: // fallthrough
            //     case Register_Types::Input: {
            //         for (u8 decode_idx = 0; decode_idx < map_workspace.num_decode; ++decode_idx) {
            //             const auto& decode_info = map_workspace.decode_array[decode_idx];
            //             const auto raw = decode(poll_buffer_u16 + (decode_info.offset - map_workspace.start_offset), decode_info, decoded_val);
            //             decoded_vals.emplace_back(Decoded_Val{decoded_val, raw});
            //         }
            //         break;
            //     }
            //     // binary stuff:
            //     case Register_Types::Coil: // fallthrough
            //     default: { // Discrete_Input
            //         for (u8 decode_idx = 0; decode_idx < map_workspace.num_decode; ++decode_idx) {
            //             const auto offset = map_workspace.bool_offset_array[decode_idx];
            //             const auto raw = poll_buffer_u8[offset - map_workspace.start_offset];
            //             decoded_val = raw;
            //             decoded_vals.emplace_back(Decoded_Val{decoded_val, raw});
            //         }
            //     }
            // }
            if (!main_work_qs.pub_q.try_push(poll_work)) {
                NEW_FPS_ERROR_PRINT("IO thread #{} could not push onto the publish queue. This should NEVER happen. Exiting\n", thread->id);
                return false;
            }
            main_work_qs.signaller.signal();
            //continue;
        }
        else
        {
            std::lock_guard<std::mutex> lock2(output_mutex); 
            if(*debug)std::cout << " thread " << thread->id <<  " found no polls\n"; 
        }

    }
    return true;
}

// TODO rework this
// put the test code right here 
bool thread_debug = false;

void test_iothread(const char* ip , int port, const char *oper, int device_id, const char *reg_type, int offset, int value, bool debug)
{
    thread_debug = debug;
    int num_threads = 4;
    // create a context
    // modbus_t* ctx = modbus_new_tcp(ip, port);
    // if (!ctx)
    // {
    //     std::cout<< " Unable to create modbus ctx to "<<ip<< "  port "<< port <<std::endl;
    //     return;
    // }
    // std::cout<< " Create modbus ctx to "<<ip<< "  port "<< port <<std::endl;
    // auto mberr = modbus_connect(ctx);
    // std::cout<< " Connect modbus "<<ip<< "  port "<< port << " Connected " << mberr<<std::endl;

    //mberr = modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_PROTOCOL);
    //std::cout<< " Err recovery modbus "<<ip<< "  port "<< port << " Connected " << mberr<<std::endl;
    // Set response timeout
    // if (modbus_set_response_timeout(ctx, to_sec, to_usec) == -1) {
    //     fprintf(stderr, "Failed to set response timeout: %s\n", modbus_strerror(errno));
    //     modbus_free(ctx);
    //     return;
    // }
    // std::cout<< " set response timeout  "<<ip<< "  port "<< port <<std::endl;

    // create request
    IO_Work* set_work = new IO_Work;

    // TODO fix these
    std::string rtype(reg_type);
    std::string roper(oper);
    //std::cout << " Register Type ["<< rtype <<"]"<<std::endl;
    if (rtype == "Holding")
        set_work->reg_type = RegisterTypes::Holding;
    else if (rtype == "Coil")
        set_work->reg_type = RegisterTypes::Coil;
    else if (rtype == "Input")
        set_work->reg_type = RegisterTypes::Input;
    else if (rtype == "Input Registers")
        set_work->reg_type = RegisterTypes::Input;
    else if (rtype == "Discrete_Input")
        set_work->reg_type = RegisterTypes::Discrete_Input;
    else if (rtype == "Discrete Inputs")
        set_work->reg_type = RegisterTypes::Discrete_Input;
    else if (rtype == "Discrete")
        set_work->reg_type = RegisterTypes::Discrete_Input;

    set_work->offset= offset;
    set_work->device_id = 255; //device_id;
    set_work->num_registers = 1;
    set_work->u16_buff[0] = value;

    xMain_Thread  main_work;
    std::vector<IO_Thread*> threadvec;

    uint32_t to_sec = 2; // 5 seconds timeout
    uint32_t to_usec = 0; // 0 microsecond
    auto start = std::chrono::high_resolution_clock::now();

    u8 i;
    // create thread
    for ( i = 0 ; i < num_threads  ; i++)
    {

        IO_Thread* io_thread = new IO_Thread;
        modbus_t* ctx = modbus_new_tcp(ip, port);
        if (!ctx)
        {
            std::cout<< "{ \"status\":\"error\" , \"message\":\" Unable to create modbus ctx to "<<ip<< "  port "<< port << "\"}"<<std::endl;
            return;
        }
        if(debug)std::cout<< " Create modbus ctx to "<<ip<< "  port "<< port <<std::endl;
        auto mberr = modbus_connect(ctx);
        if(debug || (mberr != 0)) std::cout<< std::dec << " Connect modbus "<<ip<< "  port "<< port << " Connected :" << mberr<<std::endl;
        if (mberr != 0)
        {
            std::cout<< "{ \"status\":\"error\" , \"message\":\" Unable to create modbus ctx to "<<ip<< "  port "<< port << "\"}"<<std::endl;
            modbus_free(ctx);
            return;
        }

        if (modbus_set_response_timeout(ctx, to_sec, to_usec) == -1) {
//            fprintf(stderr, "Failed to set response timeout: %s\n", modbus_strerror(errno));
            std::cout<< "{ \"status\":\"error\" , \"message\":\" Unable to set timeout  to "<<ip<< "  port "<< port << "\"}"<<std::endl;
            modbus_free(ctx);
            return;
        }
        if(debug)std::cout<< " set response timeout  "<<ip<< "  port "<< port <<std::endl;


        io_thread->ctx = ctx;
        io_thread->main_work = &main_work;
        io_thread->id = i;
        io_thread->keep_running.store(true);
        if(debug)std::cout<< " Creating thread id "<< (int)i <<std::endl;
        io_thread->thread_future = std::async(std::launch::async, GCOM_IO_thread, io_thread, &thread_debug);
        io_thread->mutex = &main_work.mutex;
        io_thread->cv = &main_work.cv;
        io_thread->ready = &main_work.ready;
        threadvec.push_back(io_thread);
    }

    std::this_thread::sleep_for(100ms);

    if(debug)std::cout<< " Sending main work cv "<<std::endl;

    {
        std::unique_lock<std::mutex> lock(main_work.mutex);
        main_work.ready = true;
        main_work.cv.notify_all();
    }
    // // launch thread(s)
    // for ( i = 0 ; i < num_threads  ; i++)
    // {
    //     main_work.signaller.signal();
    // }
    //set a set request
    if (roper == "set")
    {
        if(debug)std::cout<< " pushing set_work  "<<std::endl;
        main_work.io_work_qs.set_q.try_push(set_work);
    } 
    else if (roper == "poll")
    {
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));
        pollChan.send(std::move(set_work));

        //if(debug)
        std::cout<< " pushed set_work " <<std::endl;
    } 
    else 
    {
        std::cout << " operation " << roper << " not yet supported just use set or poll << std::endl"; 
    }
    for (auto threadp : threadvec)
    {

        // tell the threads(s)
        if(debug)std::cout << " telling  thread " << threadp->id << " To run "<< std::endl;
        main_work.io_work_qs.signaller.signal();
    }

    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = finish - start;
    {
        std::lock_guard<std::mutex> lock2(output_mutex); 
        //if(*debug)
        std::cout << " system setup  took " << elapsed.count() << " mS\n";
    }

    // wait for response
    bool done = false;
    int count = 0;
    main_work.main_work_qs.signaller.wait_for(std::chrono::nanoseconds(5000*1000));
    while (!done && count < 500)
    {
        IO_Work* pub_work = nullptr;
        
        if (main_work.main_work_qs.pub_q.try_pop(pub_work)) {
            if (debug)std::cout << " thread responded  count " << count<<  std::endl;
            std::cout << "\"test_io\": {"                                          <<  std::endl;

            if (pub_work->errno_code == 0) {
                std::cout << "           \"status\": \"success\""                                    << ","<<  std::endl;
                std::cout << "           \"value\":" << pub_work->u16_buff[0]                        << ","<<  std::endl;
                std::cout << "           \"response_time_us\":" << pub_work->response                    << ","<< std::endl;
            } else {
                std::cout << "           \"status\": \"error\""                                      << ","<<  std::endl;
            
                std::cout << "           \"response_time_us\":" << pub_work->response                    << ","<< std::endl;
                std::cout << "           \"error_code\":: \"0X" << std::hex<<pub_work->errno_code        <<"\","<<  std::endl;
                std::cout << "           \"error_string\": \" "<< modbus_strerror(pub_work->errno_code)  <<"\""<<  std::endl;
            }
            std::cout << "           }"                                          <<  std::endl;
            //done = true;
        }
        std::this_thread::sleep_for(1ms);

        count++;
    }

    //io_thread.keep_running = false;
    // tell the threads(s)
    //main_work.io_work_qs.signaller.signal();

    // stop thread(s)
    //main_work.signaller.signal();

    for (auto threadp : threadvec)
    {
        if(debug)std::cout << " telling  thread " << threadp->id << " To close "<< std::endl;
        {
            std::unique_lock<std::mutex> lock(main_work.mutex);
            threadp->keep_running.store(false);
        }
    // tell the threads(s)
        main_work.io_work_qs.signaller.signal();
        main_work.signaller.signal();
    }

    for (auto threadp : threadvec)
    {
        
        if(debug)std::cout << " waiting for thread " << threadp->id << " To close "<< std::endl;
        threadp->thread_future.wait();
        if(debug) std::cout << " thread " << threadp->id << " closing "<< std::endl;
        modbus_close(threadp->ctx);
        modbus_free(threadp->ctx);
        delete threadp;  
    }

    //io_thread.thread_future.wait();

    if(debug) std::cout << " all done " << std::endl;
    delete set_work;



    // send some requests
    // stop the thread
    // done

}