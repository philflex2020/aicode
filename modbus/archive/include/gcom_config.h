//#pragma once
#ifndef GCOM_CONFIG_H
#define GCOM_CONFIG_H

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <any>
#include <map>
#include <vector>
#include <optional>

#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/compile.h>
#include <spdlog/fmt/bundled/format.h>

#include <string>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm> // for std::find

#include "modbus/modbus.h"
#include "fims/libfims.h"

#include "gcom_perf.h"


using namespace std::string_view_literals;

//#include "gcom_iothread.h"



typedef uint64_t u64;
typedef int64_t s64;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint8_t u8;


// Mock-up definitions
enum class RegisterTypes {
    Holding,
    Coil,
    Input,
    Discrete_Input,
    TYPE_A,
    TYPE_B
    // ... Add other types as needed
};

enum class WorkTypes {
    Noop,
    Set,
    SetMulti,
    BitSet,
    BitSetMulti,
    Get,
    GetMulti,
    BitGet,
    BitGetMulti,
    // ... Add other types as needed
};


struct cfg {

    enum class Register_Types : uint8_t
    {
        Holding,
        Input,
        Coil,
        Discrete_Input,
        Undefined
    };


    struct reg_struct;
    struct comp_struct;

    struct connect_struct {
        std::string device_name;
        std::string name;
        std::string ip_address;
        int port;
        int max_num_connections;
        int device_id;
        bool debug;
        int connection_timeout;

    } connection;
    
    struct map_struct {
        std::string id;
        std::string name;
        std::string rtype;
        std::any value;
        std::any last_value;

        double set_time;

        int offset;
        int size;

        int starting_bit_pos;
        int number_of_bits;
        int shift;
        double scale; 
        bool normal_set;

        u64 invert_mask;  
        u64 care_mask;  // new
        bool uses_masks;//  new

        bool is_float;
        bool is_signed;
        bool is_word_swap;
        bool is_byte_swap;
        bool is_float64;

        // TODO
        bool is_bit_string_type;
        bool is_individual_bits;
        bool is_bit_field;
        
        bool packed_register;
        std::shared_ptr<map_struct> packer;

        u8 bit_index;
        //item.bit_str 
        u64 bits_known;
        u64 bits_unknown;
        std::vector<std::string>bit_str;
        std::vector<bool>bit_str_known;
        std::vector<int>bit_str_num; 
        std::vector<std::shared_ptr<map_struct>> bit_ranges;

        //std::vector<int>bit_num;   // maybe
// todo read in the array
        bool is_enum = false;
        bool is_random_enum = false;
        bool is_forced = false;
        bool is_enabled = true;

        // offtime == 0 use reg
        // offtime < 2 ues forced
        // offtime < 1 disable
        // offtime > tNow temp disable
        // forced val 

        double offtime;

        u64 forced_val;
        u64 raw_val;

//input.bit_str_known std::vector<bool>  ->num_bit_strs

        //std::weak_ptr<reg_struct>reg; // done
        u8 reg8[4];        // holds the raw data
        u16 reg16[4];
        Register_Types reg_type;
        std::weak_ptr<map_struct> next;
        std::weak_ptr<map_struct> base;
        //map_struct *next;   // maybe
        //map_struct *base;   // base for packed bits.
        int device_id;

        double  debounce_time = 0.0;  // time when debounce is turned off
        double  debounce = 0.0;   // time to debounce
        double  deadband = 0.0;  // time when debounce is turned off
        bool use_debounce;
        bool use_deadband;

        double float_val;  // used in deadband
        double last_float_val;  // used in deadband
        u64 last_raw_val; // used in deadband

        // todo fix these up
        bool use_bool = false;
        bool use_hex = false;
        bool use_raw = false;
        bool is_bit = false;

        static bool compare(const map_struct & a , const map_struct &b) {
            return a.offset < b.offset;

        };
    };

    struct reg_struct {
        std::string type;
        Register_Types reg_type;
        int starting_offset;
        int number_of_registers;
        int device_id;
        bool enabled = true;
        bool is_word_swap = false;
        bool is_byte_swap = false;

        std::vector<int>bad_regs;
        std::vector<std::shared_ptr<map_struct>>maps;
        std::map<int, std::shared_ptr<map_struct>>mapix; // faster lookup for maps
        std::string id;
        std::string comp_id;
        //std::weak_ptr<comp_struct>comp; // done
        
    };

    //struct reg_struct;
    //struct map_struct;

    struct comp_struct {
        std::string id;
        std::string comp_id;

        int frequency;
        int offset_time;
        int device_id;
        bool is_word_swap = false;
        bool is_byte_swap = false;

        std::vector<std::shared_ptr<reg_struct>>registers;
    };

    std::vector<std::shared_ptr<comp_struct>>components;

    std::vector<std::string>subs;


    struct pub_struct {
        std::string id;
        Stats pubStats;
        double tRequest; // when did we request a pub
        double tLate;    // when is it late
        double tAvg;     
        double tMax;     
        double tTotal;     
        int count;
        int frequency;
        int offset_time;
        int device_id;
        std::weak_ptr<struct comp_struct>comp;
        struct cfg *cfg;

        std::vector<std::shared_ptr<reg_struct>>disabled_regs;

    };

    std::map<std::string, std::shared_ptr<struct pub_struct>>pubs;

    // config_any
    void addPub(const std::string& base, const std::string& cname, std::shared_ptr<comp_struct> comp, struct cfg* myCfg);

    //void addPub(const std::string& base, const std::string& cname, comp_struct* comp, cfg *myCfg);
    //  {
    //     auto mypub = std::make_shared<pub_struct>();

    //     std::string bstr = "tmr_" + base + "_" + cname;
    //     std::string lstr = "tlate_" + base + "_" + cname;
    //     mypub->id =  bstr;
    //     mypub->comp = comp;

    //     mypub->count = 0;
    //     mypub->tRequest = 0.0;
    //     mypub->tAvg = 0.0;
    //     mypub->tTotal = 0.0;
    //     mypub->tMax = 0.0;
    //     mypub->tLate = 0.0;

    //     pubs[bstr]=mypub;
    // };


    void addSub(const std::string& base, const std::string& comp) {
        std::string bstr = "/" + base + "/" + comp;
        if (std::find(subs.begin(), subs.end(), bstr) == subs.end()) {
            subs.emplace_back(bstr);
        }
    };

    std::vector<std::string>* getSubs() {
        return &subs;
    };

    Register_Types typeFromStr(std::string& rtype);
    std::string typeToStr(Register_Types rtype);
    /// @brief [componnts][uri][item] 
    std::map<std::string, std::map<std::string, std::map<std::string, std::shared_ptr<map_struct>>>> itemMap;
    std::map<int, std::map<Register_Types, std::map<int, std::shared_ptr<cfg::map_struct>>>>idMap;
    //std::map<std::string, std::map<std::string, std::map<std::string, cfg::map_struct*>>>::iterator findItem (std::string_view uri);

    std::map<std::string,std::shared_ptr<cfg::map_struct>>* findMapItem(std::vector<std::string> keys);
//    std::map<std::string,map_struct*>* findMapItem(std::vector<std::string> keys) {

//         // Look up outerKey in the outer map
//         if(keys.size()< 2)
//             // If any key not found, return nullopt
//             return nullptr;
//         auto outerIt = itemMap.find(keys[0]);
//         if (outerIt != itemMap.end()) {
//             // If outerKey is found, look up innerKey in the inner map
//             auto& innerMap = outerIt->second;
//             auto innerIt = innerMap.find(keys[1]);
//             if (innerIt != innerMap.end()) {
//                 // If innerKey is also found, return the inner map
//                 return &innerIt->second;
//             }
//         }
//         // If any key not found, return nullopt
//         return nullptr;
//     }

    
    fims fims_gateway;
    fims fims_test;
    std::string client_name;
    bool reload =  false;


};

struct type_map {
    struct std::shared_ptr<cfg::map_struct> map;
    struct std::shared_ptr<cfg::reg_struct> reg;
    struct std::shared_ptr<cfg::comp_struct> comp;

};


//using MapItemPtr = std::shared_ptr<mycfg::item_struct>;
using MapItemMap = std::map<std::string, std::map<std::string, std::map<std::string, std::shared_ptr<cfg::map_struct>>>>;

// device_id type , offset , item
using MapIdMap = std::map<int, std::map<cfg::Register_Types, std::map<int, cfg::map_struct*>>>;



// map of typenames / offsets to actual map component
//std::map<std::string,std::map<int,struct type_map>>types;


#include "shared_utils.hpp"

struct Uri_req {
    Uri_req (std::string_view& uri_view, const char* uri)
    {
        static constexpr auto Raw_Request_Suffix           = "/_raw"sv;
        static constexpr auto Timings_Request_Suffix       = "/_timings"sv;
        static constexpr auto Reset_Timings_Request_Suffix = "/_reset_timings"sv;
        static constexpr auto Reload_Request_Suffix        = "/_reload"sv;
        static constexpr auto Disable_Suffix               = "/_disable"sv;
        static constexpr auto Enable_Suffix                = "/_enable"sv;
        static constexpr auto Force_Suffix                 = "/_force"sv;
        static constexpr auto Unforce_Suffix               = "/_unforce"sv;
        uri_view                 = std::string_view{uri, strlen(uri)};
        is_raw_request           = str_ends_with(uri_view, Raw_Request_Suffix);
        is_timings_request       = str_ends_with(uri_view, Timings_Request_Suffix);
        is_reset_timings_request = str_ends_with(uri_view, Reset_Timings_Request_Suffix);
        is_reload_request        = str_ends_with(uri_view, Reload_Request_Suffix);
        is_enable_request        = str_ends_with(uri_view, Enable_Suffix);
        is_disable_request       = str_ends_with(uri_view, Disable_Suffix);
        is_force_request         = str_ends_with(uri_view, Force_Suffix);
        is_unforce_request       = str_ends_with(uri_view, Unforce_Suffix);
        if (is_raw_request)           uri_view.remove_suffix(Raw_Request_Suffix.size());
        if (is_timings_request)       uri_view.remove_suffix(Timings_Request_Suffix.size());
        if (is_reset_timings_request) uri_view.remove_suffix(Reset_Timings_Request_Suffix.size());
        if (is_reload_request)        uri_view.remove_suffix(Reload_Request_Suffix.size());
        if (is_enable_request)        uri_view.remove_suffix(Enable_Suffix.size());
        if (is_disable_request)       uri_view.remove_suffix(Disable_Suffix.size());
        if (is_force_request)         uri_view.remove_suffix(Force_Suffix.size());
        if (is_unforce_request)       uri_view.remove_suffix(Unforce_Suffix.size());
        splitUri(uri_view);

    }

    bool str_ends_with(const std::string_view& str, const std::string_view& suffix)
    {
	    return str.size() >= suffix.size() 
                && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
                //&& std::string_view::traits_type::compare(str.end() - suffix_len, suffix.data(), suffix_len) == 0;
    }

    // Function to split the URI path and store parts into a vector
    void splitUri(const std::string_view &uri_view) {
        size_t start = 0;
        size_t end = uri_view.find('/');
        while (end != std::string_view::npos) {
            uriv.emplace_back(uri_view.substr(start, end - start));
            start = end + 1;
            end = uri_view.find('/', start);
        }
        uriv.emplace_back(uri_view.substr(start, end));
    }
    bool is_raw_request;
    bool is_timings_request;
    bool is_reset_timings_request;
    bool is_reload_request;
    bool is_enable_request;
    bool is_disable_request;
    bool is_force_request;
    bool is_unforce_request;
    std::vector<std::string> uriv;


};



#define NEW_FPS_ERROR_PRINT(fmt_str, ...)            fmt::print(stderr, FMT_COMPILE(fmt_str), ##__VA_ARGS__)
#define NEW_FPS_ERROR_PRINT_NO_COMPILE(fmt_str, ...) fmt::print(stderr, fmt_str, ##__VA_ARGS__) 
#define NEW_FPS_ERROR_PRINT_NO_ARGS(fmt_str)         fmt::print(stderr, FMT_COMPILE(fmt_str))
#define FORMAT_TO_BUF(fmt_buf, fmt_str, ...) fmt::format_to(std::back_inserter(fmt_buf), FMT_COMPILE(fmt_str), ##__VA_ARGS__)


bool getMapType(const std::map<std::string, std::any>& m, const std::string& query, const std::type_info& targetType);
int gcom_parse_file(std::map<std::string, std::any> &m,const char *filename, bool debug=false);
//void gcom_printFirstLevel(const std::map<std::string, std::any>& m);
std::optional<std::string> getMapString(const std::map<std::string, std::any>& m, const std::string& query);
bool getMapType(const std::map<std::string, std::any>& m, const std::string& query, const std::type_info& targetType);
//template<typename T>
//std::optional<T> getMapValue(const std::map<std::string, std::any>& m, const std::string& query);
//template <typename T>
//bool getItemFromMap(const std::map<std::string, std::any>& m, const std::string& query, T& target, const T& defaultValue, bool def, bool required);

//bool getItemFromMap(const std::map<std::string, std::any>& m, const std::string& query, int& target, const int& defaultValue, bool def, bool required);

bool gcon_config_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg);

// std::shared_ptr<IO_Work> make_work(RegisterTypes reg_type, int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype );
// bool pollWork (std::shared_ptr<IO_Work> io_work);
// bool setWork (std::shared_ptr<IO_Work> io_work);


#endif

