
// this is a minimal pub management system.
// we need to create a list of IO_work objects that we can send to the io_threads
// they will attempt to  communicate wiht the server and then send the completed io_work object to the response ( aggregator) thread.
// A "pub" is a collection of io_work items identified by the time of creation we collect the whole list before sending it to the io thread 
// so we can mark each io_work object with a serial number (7 of 9)  
// so the aggregator will send all the pub io_work objects into something that will collect all the individually completed io_work responses
// and then decode the response data and encode the fims message.
// we'll be smart and drop or ingnore pub collections that have been superceded by another pub request for the same pub type.
// all io_work requests will return within a finite response time with good or bad data.
//
// next we'll have to work out how to handle the bad responses 
// invalid data is handled using a modbus_flush 
// invalid point means we'll have to disable points and reconstruct the io_work items to skip those points. 
// perhaps we just include a "skipped points" list in the io_work.
// so the thread will have to handle the skipped points
//  hmmm that may work 

#include <mutex>
#include <iostream>
#include <vector>
#include <any>
#include <map>


#include "gcom_config.h"
#include "gcom_timer.h"
#include "gcom_iothread.h"




            // // we want to do these things now
            // // 1. extract subscriptions -> into myCfg
            // // getSubs(myCfg)  done
            // std::cout <<"  Subs found :" <<std::endl;

            // for (auto &mysub : myCfg.subs)
            // {
            //     std::cout << "sub: "<< mysub << std::endl;
            // }
            // now we can look for the pub objects 
            //  myCfg.pubs
            // each one will need a timer object and a callback.
            // the callback will construct the pub IO_Work objects
            // and send then off to the pollChan
            // if the threads are running they will translate those intoreplies in the responseChan
            // the responseChan  collected by the responseThread will batch up the IO_Work respinses for each poll object and then 
            // run them through a decode
            // and then push out the fims pubs.


            // TODO today 09_29_2023...
            // 1..  put enough data in the IO_work to allow the response thread to collate the responses.
            // we need to know the number of iowork packets get the size of the maps vector for that.
            // then we need to annotate the io_work object with the pub name and then id each packet.
            //      done 
            //
            //  work_time 5.40045 work_name pub_components_comp_alt_2440 work_id 1 in  2

            // 2.. route the  poll chan now through the io_simulator to populate the responseChannel 
            // run the response thread and we should get pubs.
            // 3.. spit out the basic pubs



            // std::cout <<"  Pubs found :" <<std::endl;
            // // 2. extract poll groups   -> into myCfg
            // //  getPolls(myCfg) 
            // for (auto &mypub : myCfg.pubs)
            // {
            //     auto pubs = mypub.second;

            //     printf(" pubs %p\n", (void*)pubs.get());
            //     std::cout << "pub: "<< mypub.first 
            //                 << " freq: "<< pubs->comp->frequency/1000.0
            //                 << " offset: "<< mypub.second->comp->offset_time/1000.0
            //                     << std::endl;
            //     // we need to make these sync objects
            //     // the only fire once and wait for a sync
            //      std::shared_ptr<TimeObject> obj1 = createTimeObject(mypub.first, 
            //                                               5.0, 
            //                                               0, 
            //                                               mypub.second->comp->frequency/1000.0, 
            //                                               0,
            //                                               pubCallback, 
            //                                               (void *)pubs.get());
            //     //TimeObject obj1(mypub.first, 5.0,   0,    mypub.second->frequency/1000.0,  mypub.second->offset_time/1000.0,   pubCallback, (void *)mypub.second);
            //     addTimeObject(obj1, mypub.second->comp->offset_time/1000.0, true);

            // }    // getSubs(myCfg)


double get_time_double(void);
std::shared_ptr<IO_Work> make_work(cfg::Register_Types reg_type,  int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype );
bool pollWork (std::shared_ptr<IO_Work> io_work);
//bool setWork (std::shared_ptr<IO_Work> io_work) {
cfg::Register_Types strToRegType(std::string& rtype);
WorkTypes strToWorkType(std::string roper, bool debug);
void clearpollChan(void);


std::mutex cb_output_mutex;

// // these are the test bad registers
// std::vector<int>bad_regs;

// bool set_bad_reg(int reg) {
//     // Check if reg is not already in bad_regs
//     auto it = std::find(bad_regs.begin(), bad_regs.end(), reg);
//     if(it == bad_regs.end()) { // reg not found in bad_regs
//         bad_regs.push_back(reg); // Add reg to bad_regs
//         return true; // Successfully added
//     }
//     return false; // reg was already in bad_regs
// }

// void clear_bad_regs() {
//     bad_regs.clear(); // Remove all elements from bad_regs
// }

// void clear_bad_reg(int reg) {
//     // Find and remove reg from bad_regs if it exists
//     auto it = std::remove(bad_regs.begin(), bad_regs.end(), reg);
//     bad_regs.erase(it, bad_regs.end()); // Necessary to actually remove the elements
// }



// // this enables a search for bad ioregs
// // simulates finding a bad reg
// bool process_io_range(const IO_Work& work, int start_register, int num_registers) {
//     for (int reg = start_register; reg < start_register + num_registers; ++reg) {
    
//         if (std::binary_search(bad_regs.begin(), bad_regs.end(), reg)) {
//             std::cout << "Found bad reg " << reg << std::endl;
//             return false;
//         }
//     }    // Replace this function with the actual logic for processing IO requests for a range of registers
//     std::cout << "Processed IO request for registers from " << start_register << " to " << start_register + num_registers - 1 << std::endl;
//     return true;
// }


// void process_io_registers(const IO_Work& work, int start, int end, std::vector<int>& bad_registers) {
//     std::cout<< " trying start  " << start << " => end " << end << std::endl;
//     if (start > end) return;

//     // If the range consists of a single register, it must be the bad one
//     // Single register in the range, and it's bad
//     if (start == end && !process_io_range(work, start, 1)) {
//         bad_registers.push_back(start);
//         return;
//     }

//     if (process_io_range(work, start, end - start + 1)) return;
//     // Divide the range into two sub-ranges and check each
//     int mid = start + (end - start) / 2;
//     process_io_registers(work, start, mid, bad_registers); // Check the first half
//     process_io_registers(work, mid + 1, end, bad_registers); // Check the second half
// }

// void process_io(const IO_Work& work, std::vector<int>& bad_registers) {
//     int current_start = work.start_register;
//     int num_disabled = 0;
//     for (int reg = work.start_register; reg < work.start_register + work.num_registers; ++reg) {
//         if (std::binary_search(work.disabled_registers.begin(), work.disabled_registers.end(), reg)) {
//             num_disabled++;

//             // Call process_io_for_range function for registers in the range of [current_start, reg)
//             if (!process_io_range(work, current_start, reg - current_start)) {
//                 // If the range is invalid, find the bad registers within this range
//                 process_io_registers(work, current_start, reg - 1, bad_registers);
//             }

//             // Set the new current_start to be the next register after the disabled one
//             current_start = reg + 1;
//         }
//     }
//     std::cout << " Found num_disabled " << num_disabled << std::endl;

//     // Process the remaining registers if there are any
//     if (current_start < work.start_register + work.num_registers) {
//         if (!process_io_range(work, current_start, work.start_register + work.num_registers - current_start)) {
//             int current_end = work.start_register + work.num_registers - current_start;
//             std::cout << " process_io_range failed current_start " << current_start << " current_end "<< current_end << std::endl;

//             // If the remaining range is invalid, find the bad registers within this range
//             process_io_registers(work, current_start, current_end, bad_registers);
//         }
//     }
// }


// int test_find_bad_regs() {

//     std::vector<int> bad_registers;

//     set_bad_reg(25);
//     set_bad_reg(32);

//     std::cout << "Setup Bad registers: "<<std::endl;

//     for (int reg : bad_regs) {
//         std::cout << reg << " ";
//     }
//     std::cout <<std::endl;

//     IO_Work io_work;
//     io_work.start_register = 10;
//     io_work.num_registers = 50;

//     process_io(io_work, bad_registers);

//     std::cout << "Bad registers at end: "<<std::endl;

//     for (int reg : bad_registers) {
//         std::cout << reg << " ";
//     }
//     std::cout <<std::endl;

//     io_work.disabled_registers.push_back(25);
//     io_work.disabled_registers.push_back(32);
    
//     bad_registers.clear();

//     process_io(io_work, bad_registers);

//     std::cout << "Bad registers after pass 2: "<<std::endl;


//     for (int reg : bad_registers) {
//         std::cout << reg << " ";
//     }
//     std::cout <<std::endl;

//     std::cout <<std::endl;

//     return 0;
// }


void pubCallback(TimeObject* t, void* p) {
    std::lock_guard<std::mutex> lock2(cb_output_mutex); 
    cfg::pub_struct *mypub = static_cast<cfg::pub_struct*>(p);
    
    double rtime = get_time_double();
    //std::string pstr = "pub_" + base + "_" + cname;

    std::cout << "Callback for :" << t->name 
                <<" executed at : " << rtime 
                << " with sync: " << t->sync 
                << " and run: " << t->run 
                << " and psid: " << mypub->id 
                << std::endl;


    // set up the pup id == rtime
    // TODO set up the timeout callback
    // we have the coms so find the registers
    //we have to create an io_work structure.
    // if the register is enabled.
    // register is permanantly if offtime < 0 , register is forced if offtime < 2
    // register is in use if offtime is zero
    // register is temp disabled if offtime > tnow
    // forced value is in forcedbytes

    
    //Create and IO_Work object ( or get one) 
    //bool queue_work(RegisterTypes reg_type, int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype )




    double tNow = get_time_double();
    int work_id = 1;
    auto compshr = mypub->comp.lock();

    for (auto &myreg : compshr->registers) {
        auto rtype = strToRegType(myreg->type);
        //auto cfgtype = strToRegType(myreg->type);
        auto wtype = strToWorkType("poll", false);

        auto io_work = make_work(rtype, myreg->device_id, myreg->starting_offset ,myreg->number_of_registers, nullptr, nullptr, wtype);
        // set up collection id
        io_work->tNow = tNow;
        io_work->work_group = compshr->registers.size();
        io_work->work_id = work_id++;
        io_work->work_name = t->name;
        io_work->reg_map = myreg;
        io_work->comp_map = compshr;

        pollWork (io_work);
        // the response collator used this lot to 
        //work_name pub_components_comp_alt_2440 work_id 2 in  2

        std::cout 
            << " comp " << compshr->id 
            << " registers " << myreg->type
            << " start_offset " << myreg->starting_offset 
            << " number_of_registers " << myreg->number_of_registers 
            << " work_time " << io_work->tNow
            << " work_name " << io_work->work_name
            << " work_id " << io_work->work_id 
            << " in  " << compshr->registers.size() 
            << std::endl;
            for ( auto &mymap : myreg->maps) {
            std::cout 
                << "      id " << mymap->id 
                << " offset " << mymap->offset 
                << std::endl;
 
            // set up some other defaults here
                // // offtime == 0 use reg
                // offtime < 2 ues forced
                // offtime < 1 disable
                // offtime > tNow temp disable
                // forced val 
            }
    }
}


int  gcom_setup_pubs(std::map<std::string, std::any>& gcom_map, struct cfg& myCfg, double init_time, bool debug) {
    
    // we want to do these things now
    // 1. extract subscriptions -> into myCfg
    if(debug)
        std::cout <<"  Pubs found :" <<std::endl;
    // 2. extract poll groups   -> into myCfg
    //  getPolls(myCfg) 

    for (auto &mypub : myCfg.pubs)
    {
        auto pubs = mypub.second;
        auto compshr = pubs->comp.lock();

        //printf(" pubs %p\n", (void*)pubs.get());
        if(debug)
            std::cout << "pub: "<< mypub.first 
                    << " freq: "<< compshr->frequency/1000.0
                    << " offset: "<< compshr->offset_time/1000.0
                        << std::endl;
        // we need to make these sync objects
        // the only fire once and wait for a sync
        // create a timer object for these pubs
        std::shared_ptr<TimeObject> obj1 = createTimeObject(mypub.first, 
                                                    init_time, // initial offset 
                                                    0, 
                                                    compshr->frequency/1000.0, 
                                                    0,
                                                    pubCallback, 
                                                    (void *)pubs.get());
        //TimeObject obj1(mypub.first, 5.0,   0,    mypub.second->frequency/1000.0,  mypub.second->offset_time/1000.0,   pubCallback, (void *)mypub.second);
        addTimeObject(obj1, compshr->offset_time/1000.0, true);

    }
    return 0;
}