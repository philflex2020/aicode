// given a file name
// creates a  
//      std::map<std::string, std::any>

#include <iostream>
#include <any>
#include <map>
#include <vector>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <tuple>

#include <regex>
#include <cxxabi.h>



#include "gcom_config.h"
#include "logger/logger.h"

#include "gcom_config_tmpl.h"
#include "gcom_iothread.h"

using namespace std::string_view_literals;


std::vector<std::string> split_string(const std::string& str, char delimiter);


cfg::Register_Types cfg::typeFromStr(std::string& rtype) {
    // now we get modbus specific
    if (rtype == "Holding")
        return Register_Types::Holding;
    else if (rtype == "Coil")
        return Register_Types::Coil;
    else if (rtype == "Input")
        return Register_Types::Input;
    else if (rtype == "Input Registers")
        return Register_Types::Input;
    else if (rtype == "Discrete_Input")
        return Register_Types::Discrete_Input;
    else if (rtype == "Discrete Inputs")
        return Register_Types::Discrete_Input;
    else 
        return Register_Types::Undefined;
}

std::string cfg::typeToStr(cfg::Register_Types rtype) {
    // now we get modbus specific
    if (rtype == Register_Types::Holding)
        return "Holding";
    else if (rtype == Register_Types::Coil)
        return "Coil";
    else if (rtype == Register_Types::Input)
        return "Input";
    else if (rtype == Register_Types::Discrete_Input)
        return "Discrete_Input";
    else 
        return "Undefined";
}


// namespace mycfg {  // Assuming cfg namespace based on the given extract_maps function

// struct item_struct {
//     std::string id;
//     std::string name;
//     std::string rtype;
//     int offset;
//     int size;
//     int device_id;
//     std::any value=0;
// };

// }  // namespace mycfg

bool extract_connection(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug);
bool extract_components(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug);

// skips packed registers


// Function to reset the cfg object
void resetCfg(struct cfg& cfgInstance) {
    // Create a new instance of cfg with default values
    struct cfg newCfg;

    // Assign the newCfg to the existing cfgInstance
    cfgInstance = newCfg;
}

// TODO sort out packed_register decode
// we'll have to add a packedvec to the leading packed register to capture the duplicates.
void checkMapsSizes(std::shared_ptr<cfg::reg_struct>reg, std::vector<std::shared_ptr<cfg::map_struct>> maps, bool debug =  false) {

    for (size_t i = 0; i < maps.size(); ++i) {
        if (!maps[i]->packed_register) {
            if(debug)printf( ">>>> %s setting [%d] mapix %d to map %p id %s\n",__func__, (int)i, maps[i]->offset, (void*)maps[i].get(), maps[i]->id.c_str());
            //reg.mapix[maps[i].offset]= &maps[i];
            if (maps[i]->size == 0)  maps[i]->size = 1;
        }
    }
}

int getFirstOffset(const std::vector<std::shared_ptr<cfg::map_struct>> elements) {
    if (elements.empty()) {
        // Handle the error here: throw an exception, return a sentinel, etc.
        //throw std::out_of_range("The elements vector is empty");
        // or
        return -1; // or some other sentinel value
    }
    return elements[0]->offset;
}

// int getFirstOffset(const std::vector<std::shared_ptr<cfg::map_struct>>& elements) {
//     return elements[0]->offset;
// }

// skips packed registers
// TODO need to set up the next value correctly
int checkMapsOffsets(std::vector<std::shared_ptr<cfg::map_struct>> maps) {
    if (maps.empty()) {
        std::cerr << "Error: maps vector is empty" << std::endl;
        return 0; // or handle error appropriately
    }

    int total_size = maps[0]->size;

    for (size_t i = 1; i < maps.size(); ++i) {
        auto unpacked = maps[i];
        unpacked->next.reset();

        if (!unpacked->packed_register) {
            total_size += unpacked->size;
            if (unpacked->offset != maps[i-1]->offset + maps[i-1]->size) {
                std::cout << "size gap at :"<< maps[i-1]->id <<" Offset "<< maps[i-1]->offset<< std::endl;
            }
        }
        maps[i-1]->next = unpacked; // setting the 'next' pointer to the next shared_ptr
    }
    return total_size;
}

// int checkMapsOffsets(std::vector<cfg::map_struct>& maps) {
//     int total_size = maps[0].size;
//     for (size_t i = 1; i < maps.size(); ++i) {
//         auto unpacked = &maps[i];
//         maps[i].next = nullptr;

//         if (!maps[i].packed_register) {

//             total_size += maps[i].size;
//             if (maps[i].offset != maps[i-1].offset + maps[i-1].size) {
//                 std::cout << "size gap at :"<< maps[i-1].id <<" Offset "<< maps[i-1].offset<< std::endl;
//                 //return false;  // Offsets and sizes don't match the condition
//             }
//         } 
//         {
//             maps[i-1].next = unpacked;//&elements[i];
//         }
        
//     }
//     return total_size;  // All elements satisfy the condition
// }

void sortMapsVector(std::vector<std::shared_ptr<cfg::map_struct>> elements) {
    std::sort(elements.begin(), elements.end(),
              [](const std::shared_ptr<cfg::map_struct> a, const std::shared_ptr<cfg::map_struct> b) -> bool {
                  return cfg::map_struct::compare(*a, *b);  // Dereference the shared_ptr here
              });
}

// void sortMapsVector(std::vector<std::shared_ptr<cfg::map_struct>>& elements) {
//     std::sort(elements->begin(), elements->end(), cfg::map_struct::compare);
// }


// this loads in the config file 
// extracts components and connections 

bool  gcom_load_cfg_file(std::map<std::string, std::any>& gcom_map, const char *filename, struct cfg& myCfg, bool debug) {

    bool ok = true;
    auto fok = gcom_parse_file(gcom_map, filename, debug);
    if (fok < 0)
        ok = false; 

            // pull out connection from gcom_map
    if (!ok) {
        std::cout << " Unable to parse config file :"<< filename << " quitting" << std::endl;
        return false;
    }

    ok = extract_connection(gcom_map, "connection", myCfg, debug);
    if (!ok) {
        std::cout << " Unable to extract connection from  :"<< filename << " quitting" << std::endl;
        return false;
    }

    // pull out the components into myCfg
    ok = extract_components(gcom_map, "components", myCfg, debug);
    if (!ok) {
        std::cout << " Unable to extract components from  :"<< filename << " quitting" << std::endl;
        return false;
    }

        for ( auto &myComp : myCfg.itemMap) 
        {
            printf(" >>>>>>>>>>>>> <%s> comp >> <%s> \n"
                                , __func__
                                , myComp.first.c_str()
                                );

        }

    std::cout << "connection ip: "<< myCfg.connection.ip_address  << " port : "<< myCfg.connection.port  << std::endl;

    // now load up the components we have found
    for (auto &mycomp : myCfg.components)
    {
        if (debug)
            std::cout 
                << "component id: "<< mycomp->id 
                << " device_id: "<< mycomp->device_id 
                << " freq " << mycomp->frequency 
                << " offset_time :" << mycomp->offset_time 
                << std::endl;
        //printf(" mycomp ptr %p \n", (void *)&mycomp);

        // add a subscription to this component
        myCfg.addSub("components", mycomp->id);

        // this is the key to setting up a pub.
        // the io_work framework is here
        // add a pub system for this component
        myCfg.addPub("components", mycomp->id, mycomp, &myCfg);

        // now check reg sizes and correct if needed
        for (auto &myreg : mycomp->registers)
        {
            checkMapsSizes(myreg, myreg->maps);
            // TODO add next pointers
            sortMapsVector(myreg->maps);

            int first_offset = getFirstOffset(myreg->maps);
            int total_size = checkMapsOffsets(myreg->maps);

            // maybe report config file error here
            myreg->starting_offset = first_offset;
            myreg->number_of_registers = total_size;


            if(debug)
                std::cout   << "     register type: "<< myreg->type 
                        << " offset: "<< myreg->starting_offset
                        << " num_regs: "<< myreg->number_of_registers
                        << " first_offset: "<< first_offset
                        << " total_size: "<<total_size
                                                        << std::endl;

            for (auto &mymap : myreg->maps)
            {
                if(debug)
                    std::cout   << "         map : "<< mymap->id 
                        << " offset : " << mymap->offset
                        << " size : " << mymap->size
                                                        << std::endl;
            }
        }
    }
    myCfg.client_name = "my_client"; 
    return true;
}

bool test_findMapVar(
    std::shared_ptr<cfg::map_struct>& map_result, const struct cfg &myCfg, const std::vector<std::string>& uri_keys, std::string kvar ="");

bool test_findMapVar(
    std::shared_ptr<cfg::map_struct>& map_result, const struct cfg &myCfg, const std::vector<std::string>& uri_keys, std::string kvar) {

    int key_idx = 0;
    if ((uri_keys.size() > 1) && (uri_keys[0].size() == 0)) key_idx = 1;

    std::string myvar = kvar;
    if ((int)uri_keys.size() < (key_idx + 3) ) 
    {
        myvar = kvar;
    }
    else 
    {
        myvar = uri_keys[key_idx + 2];

    }
    //return false;
    //auto myvar = uri_keys[key_idx + 2];
    std::cout   << "         func : #1 "<< __func__ << " myVAR [" << myvar << "] \n";

    try {
        // Using at() for safe access. Catch exceptions if key not found
        auto &myComp = myCfg.itemMap.at(uri_keys[key_idx]);
        std::cout   << "         func : #2 "<< __func__ << "\n";
        auto &myReg = myComp.at(uri_keys[key_idx + 1]);
        std::cout   << "         func : #3 "<< __func__ << "\n";
        map_result = myReg.at(myvar);
        return true;
    } catch (const std::out_of_range&) {
        // Key not found, return nullptr
        return false;
    }
    return false;

}

bool test_findMapMap(
    std::map<std::string, std::shared_ptr<cfg::map_struct>>& map_result, const struct cfg &myCfg, const std::vector<std::string>& uri_keys) {

    int key_idx = 0;
    if ((uri_keys.size() > 1) && (uri_keys[0].size() == 0)) key_idx = 1;

    if ((int)uri_keys.size() >= (key_idx + 2) ) {
        std::cout   << "         func : #1 "<< __func__ << "\n";

        try {
            // Using at() for safe access. Catch exceptions if key not found
            const auto &myComp = myCfg.itemMap.at(uri_keys[key_idx]);
            std::cout   << "         func : #2 "<< __func__ << "\n";
            map_result = myComp.at(uri_keys[key_idx + 1]);
            return true;
        } catch (const std::out_of_range&) {
            // If you want to log or handle the exception here, you can do it.
            throw;  // Then, rethrow the exception.
        }
    }
    // Throw an exception if there aren't enough keys provided
    throw std::out_of_range("Not enough keys provided or key not found");
    return false;
}

// const std::map<std::string,std::shared_ptr<cfg::map_struct>>& test_findMapMap(
//     const struct cfg &myCfg, const std::vector<std::string>& uri_keys) {

//     int key_idx = 0;
//     if ((uri_keys.size() > 1) && (uri_keys[0].size() == 0)) key_idx = 1;

//     if ((int)uri_keys.size() >= (key_idx + 2) ) {
//         std::cout   << "         func : #1 "<< __func__ << "\n";

//         // Using at() for safe access. Catch exceptions if key not found
//         auto &myComp = myCfg.itemMap.at(uri_keys[key_idx]);
//         std::cout   << "         func : #2 "<< __func__ << "\n";
//         return &myComp.at(uri_keys[key_idx + 1]);
//     }
//     raise std::out_of_range;
// }


// here's the message management
// we create a vector of map_struct items  and values
// we'll encode them and create set work.

bool test_uri(struct cfg& myCfg, const char* uri) {

    std::cout << "client_name #2 " << myCfg.client_name << "\n";
    for ( auto &myComp : myCfg.itemMap) 
    {
        for ( auto &myReg : myComp.second) 
        {
            for ( auto &myMap : myReg.second) 
            {
                printf(" >>>>>>>>>>>>> <%s> comp >> <%s>  reg <%s> map <%s>\n"
                            , __func__
                            , myComp.first.c_str()
                            , myReg.first.c_str()
                            , myMap.first.c_str()

                            );
            }
        }
    }

    auto uri_keys = split_string(uri, '/');
    for ( auto key : uri_keys)
    {
        std::cout << " key : ["<< key << "] "<< " size : " << key.size()<<std::endl;

    }

     // 3 keys /comp/id
     // 4 keys  /comp/id/var
    // gcom_config_any
    std::shared_ptr<cfg::map_struct> var_result;
    std::map<std::string, std::shared_ptr<cfg::map_struct>> map_result;
    bool result = false;


    // if is single we'll find it here
    if(uri_keys.size() >= 4) 
    {
        std::cout << "  looking for var  " << "\n";
        result = test_findMapVar(var_result, myCfg, uri_keys);
        if(result) {
            std::cout << " var found " << var_result->id << "\n";
        }else{
            std::cout << " var NOT found " << "\n";
        }
    }
    else { // this is a map
        try {
        //auto& rfoo = 
        std::cout << "  looking for map  " << "\n";
        result = test_findMapMap(map_result, myCfg, uri_keys);
        std::cout << " >>> map found " << "\n";
        } catch ( const std::out_of_range&) {
            std::cout << " >>> map NOT found " << "\n";

        }
    }

    return (var_result != nullptr);

    // bool ret = true;
    // if (std::holds_alternative<MapPointer>(result) && std::get<MapPointer>(result) != nullptr) {
    //     //MapPointer pointer = std::get<MapPointer>(result);
    //     std::cout << " uri : "<< uri << " found  map of items " << std::endl;
    //     // Use pointer as MapPointer
    // } else if (std::holds_alternative<MapStructPointer>(result) && std::get<MapStructPointer>(result) != nullptr) {
    //     //MapStructPointer pointer = std::get<MapStructPointer>(result);
    //     // Use pointer as MapStructPointer
    //     std::cout << " uri : "<< uri << " found  map item ptr" << std::endl;
    // } else {
    //     // Handle the case where no value is found
    //     std::cout << " uri : "<< uri << " not found " << std::endl;
    //     ret = false;

    // }
    // return ret;
}
//       from components[x].component_id or "/components if missing"
//std::map<std::string,               
//           from components[x].id   must be present  
//         std::map<std::string, 
//            from        components[x].registers[y].map[z].id must be present
//                     std::map<std::string, 
//                             cfg::item_struct*>>>
//                                   id from components[x].registers[y].map[z].id
//                                   name from components[x].registers[y].map[z].name
//                                   rtype from components[x].registers[y].type
//                                   offset from components[x].registers[y].map[z].offset
//                                   size from components[x].registers[y].map[z].size or 1
//                                   device_id from components[x].device_id or 255


// //using MapItemPtr = std::shared_ptr<mycfg::item_struct>;
// using MapItemMap = std::map<std::string, std::map<std::string, std::map<std::string, cfg::map_struct*>>>;

// // device_id type , offset , item
// using MapIdMap = std::map<int, std::map<std::string, std::map<int, cfg::map_struct*>>>;

int gcom_parse_data(std::any &gcom_any, const char* data, size_t length, bool debug);
bool extractJsonValue(const simdjson::dom::element& el, std::any& value);
void printAny(std::any& value, int indent = 0);


#include <memory>
#include <algorithm>

std::shared_ptr<IO_Work> make_work(cfg::Register_Types reg_type,  int device_id, int offset, int num_regs, uint16_t* u16bufs, uint8_t* u8bufs, WorkTypes wtype );
WorkTypes strToWorkType(std::string roper, bool);


// int merge_IO_Work_Bits(std::vector<std::shared_ptr<IO_Work>>& work_vector) {
//     auto it = work_vector.begin();
//     while (it != work_vector.end()) {
//         if (it + 1 != work_vector.end() && (*it)->offset + (*it)->num_bits == (*(it + 1))->offset) {
//             // Calculate total number of bits after merge
//             int total_bits = (*it)->num_bits + (*(it + 1))->num_bits;

//             // If buf8 is not large enough, you might need to allocate a new buffer
//             // and copy existing bits from (*it)->buf8 to the new buffer.

//             // Then, copy bits from (*(it + 1))->buf8 to the new or existing buffer at the correct position
//             for (int i = 0; i < (*(it + 1))->num_bits; ++i) {
//                 // Set each bit in (*it)->buf8 from (*(it + 1))->buf8
//                 // This example assumes each bit in buf8 represents a Modbus coil (might need to adjust based on your implementation)
//                 int position = (*it)->num_bits + i; // Calculate the correct position in (*it)->buf8
//                 // Set or clear the bit at 'position' in (*it)->buf8 based on the bit value in (*(it + 1))->buf8[i]
//                 if ((*(it + 1))->buf8[i / 8] & (1 << (i % 8))) { // If the bit in (*(it + 1))->buf8[i] is set
//                     (*it)->buf8[position / 8] |= (1 << (position % 8)); // Set the corresponding bit in (*it)->buf8
//                 } else { // If the bit in (*(it + 1))->buf8[i] is cleared
//                     (*it)->buf8[position / 8] &= ~(1 << (position % 8)); // Clear the corresponding bit in (*it)->buf8
//                 }
//             }

//             // Update the num_bits of (*it) to reflect the total number of bits after the merge
//             (*it)->num_bits = total_bits;

//             // Optionally, deallocate memory of (*(it + 1))->buf8 if necessary
//             // And remove the (it + 1) element from work_vector since it has been merged into (*it)
//             it = work_vector.erase(it + 1); // it now points to the element after the erased one
//         } else {
//             ++it;
//         }
//     }
//     return 0;
// }



// TODO test for max reg size
int merge_IO_Work_Reg(std::vector<std::shared_ptr<IO_Work>>& work_vector,
                  std::vector<std::shared_ptr<IO_Work>>& discard_vector) 
{
    auto it = work_vector.begin();
    while (it != work_vector.end()) {
        if (it + 1 != work_vector.end() && (*it)->offset + (*it)->size == (*(it + 1))->offset) {
            // TODO copy 
            for (auto i = 0 ; i < (*it)->size; i++) {
                (*it)->buf16[(*it)->num_registers+i] = (*(it + 1))->buf16[i];
            }
            (*it)->size += (*(it + 1))->size;
            (*it)->num_registers += (*(it + 1))->size;

            discard_vector.emplace_back(*(it + 1));
            it = work_vector.erase(it + 1); // it now points to the element after the erased one
        } else {
            ++it;
        }
    }
    return 0;
}


int sort_IO_Work(std::vector<std::shared_ptr<IO_Work>> &work_vector) {
    // Sorting the vector based on IO_Work->offset
    std::sort(work_vector.begin(), work_vector.end(),
        [](const std::shared_ptr<IO_Work>& a, const std::shared_ptr<IO_Work>& b) -> bool {
            return a->offset < b->offset;
        });
    return 0;
}

// this is a single set message
// if its a single then encode it into an work item. The body is the data
// if its a multi we'll  create a vector of work items.
// the first part id the id and the rest is the data
//


/// @brief 
/// @param item 
/// @param val 
/// @return 
int getanybval(std::shared_ptr<cfg::map_struct>item, std::any val)
{

    int oval = 0;
    if (val.type() == typeid(std::map<std::string, std::any>)) {
        auto mval = std::any_cast<std::map<std::string, std::any>>(val);
        if (mval.find("value")!= mval.end())
        {
            val = mval["value"];
        }
    }

    if (val.type() == typeid(bool)) {
        if (std::any_cast<bool>(val))
            oval = 1;
    }
    else if (val.type() == typeid(int64_t)) {
        if (std::any_cast<int64_t>(val)> 0)
            oval = 1;
    }
    else if (val.type() == typeid(uint64_t)) {
        if (std::any_cast<uint64_t>(val)> 0)
            oval = 1;
    }
    else if (val.type() == typeid(std::string)) {
        if (std::any_cast<std::string>(val) == "true")
            oval = 1;
    }
    else if (val.type() == typeid(double)) {
        if (std::any_cast<double>(val)> 0)
            oval = 1;
    }

    if(item->scale == -1)
        oval ^= 1;
    return oval;
}


/// @brief 
/// @param item 
/// @param val 
/// @return 
uint64_t getanyuval(std::shared_ptr<cfg::map_struct>item, std::any val, uint16_t*regs16)
{

    uint16_t u16val = 0;
    //int16_t i16val = 0;
    uint32_t u32val = 0;
    //int32_t i32val = 0;

    uint64_t uval = 0;
    int64_t ival = 0;
    double dval = 0.0;
    bool is_uint =  false;
    bool is_int =  false;

    // if (!item->is_forced)
    // {
    // // handle val being in a value field
    if (val.type() == typeid(std::map<std::string, std::any>)) {
        auto mval = std::any_cast<std::map<std::string, std::any>>(val);
        if (mval.find("value")!= mval.end())
        {
            val = mval["value"];
            std::cout << ">>>>" <<__func__<< " value found " << std::endl;
        }
    }

    // todo add other exceptions here
    if(item->is_bit_field || item->is_enum || item->packed_register) 
    {
        item->normal_set = false;
    }
    else
    {
        std::cout << __func__ << " item normal set, item size " << item->size <<  std::endl;
        item->normal_set = true;
    }

    if(item->normal_set)
    {
        std::cout << ">>>>" <<__func__<< " normal_set " << std::endl;
        if (val.type() == typeid(bool)) {
            if (std::any_cast<bool>(val))
                ival = 1;
        }
        else if ((val.type() == typeid(int32_t)) || (val.type() == typeid(int64_t))) {
            is_int = true;
            if (val.type() == typeid(int32_t)) {
                ival = static_cast<int64_t>(std::any_cast<int32_t>(val));
            } else {
                ival = std::any_cast<int64_t>(val);
            }
            std::cout << ">>>>" <<__func__<< " offset  " << item->offset<< " found int >>" << ival << std::endl;
            ival <<=item->starting_bit_pos;
            ival -= item->shift;
            dval = static_cast<double>(ival);

            if(item->scale)
            {
                dval *= item->scale;
            }
        }
        else if ((val.type() == typeid(uint32_t)) || (val.type() == typeid(uint64_t))) {
            is_uint = true;
            if (val.type() == typeid(uint32_t)) {
                uval = static_cast<uint64_t>(std::any_cast<uint32_t>(val));
            } else {
                uval = std::any_cast<int64_t>(val);
            }
            std::cout << ">>>>" <<__func__<< " offset  " << item->offset<<  " found uint >>" << uval << std::endl;
            uval <<=item->starting_bit_pos;
            uval -= item->shift;
            dval = static_cast<double>(uval);
            if(item->scale)
            {
                dval *= item->scale;
            }
        }
        else if (val.type() == typeid(double)) {
            dval = std::any_cast<double>(val);
            std::cout << ">>>>" <<__func__<< " offset  " << item->offset << " found double >>" << dval << std::endl;
            dval -= item->shift;
            if(item->scale )
            {
                dval *= item->scale;
            }
        }
        else {
            std::cout << ">>>>" <<__func__<< " value undefined  " << val.type().name()<<std::endl;
        }
        
        if(item->size == 1)
        {
            if (is_uint && !item->scale)
            {
                u16val = static_cast<uint16_t>(uval);
            }
            else if (is_int && !item->scale)
            {
                u16val = static_cast<int16_t>(ival);
            }
            else
            {
                if(!item->is_signed)
                {
                    u16val = static_cast<uint16_t>(dval);
                }
                else
                {
                    u16val = static_cast<uint16_t>(dval);
                }
            }
            u16val ^= static_cast<uint16_t>(item->invert_mask);  
            std::cout << ">>>>" <<__func__<< " offset  " << item->offset << " final uval >>" << u16val << " ival >> " << ival << std::endl;
            if(item->is_forced)
                u16val = static_cast<uint16_t>(item->forced_val);  


            regs16[0] = static_cast<uint16_t>(u16val);
            uval = static_cast<uint64_t>(u16val);

        }
        else if(item->size == 2)
        {

            if (is_uint && !item->scale)
            {
                if(!item->is_float)
                {
                    u32val = static_cast<uint32_t>(uval);
                }
                else
                {
                    const auto f32val = static_cast<float>(uval);
                     memcpy(&u32val, &f32val, sizeof(u32val));
                }
            }
            else if (is_int && !item->scale)
            {

                if(!item->is_float)
                {
                    u32val = static_cast<uint32_t>(ival);
                }
                else
                {
                    const auto f32val = static_cast<float>(ival);
                     memcpy(&u32val, &f32val, sizeof(u32val));
                }
            }
            else
            {
                if(item->is_float)
                {
                    const auto f32val = static_cast<float>(dval);
                    memcpy(&u32val, &f32val, sizeof(u32val));
                }
                else if(!item->is_signed)
                {
                    u32val = static_cast<uint32_t>(dval);
                }
                else
                {
                    u32val = static_cast<uint32_t>(static_cast<int32_t>(dval));
                }
            }
            u32val ^= static_cast<uint32_t>(item->invert_mask);  

            if(item->is_forced)
                u32val = static_cast<uint32_t>(item->forced_val);  

            if(!item->is_byte_swap)
            {
                regs16[0] = static_cast<uint16_t>(u32val >> 16);
                regs16[1] = static_cast<uint16_t>(u32val >> 0);
            }
            else
            {
                regs16[0] = static_cast<uint16_t>(u32val >> 0);
                regs16[1] = static_cast<uint16_t>(u32val >> 16);
            }
            uval = static_cast<uint64_t>(u32val);

        }
        else if(item->size == 4)
        {
            if (is_uint && !item->scale)
            {
                if(!item->is_float)
                {
                    //uval = static_cast<uint64_t>(uval);
                }
                else
                {
                    const auto f64val = static_cast<double>(uval);
                    memcpy(&uval, &f64val, sizeof(uval));
                }
            }
            else if (is_int && !item->scale)
            {

                if(!item->is_float)
                {
                    uval = static_cast<uint64_t>(ival);
                }
                else
                {
                    const auto f64val = static_cast<double>(ival);
                    memcpy(&uval, &f64val, sizeof(uval));
                }
            }
            else
            {
                if(item->is_float)
                {
                    //const auto f32val = static_cast<float>(dval);
                    //memcpy(&current_u64_val, &current_float_val, sizeof(current_float_val));
                    memcpy(&uval, &dval, sizeof(uval));
                }
                else if(!item->is_signed)
                {
                    uval = static_cast<uint64_t>(dval);
                }
                else
                {
                    uval = static_cast<uint64_t>(static_cast<int64_t>(dval));
                }
            }
            //uint64_t tval;
                                //memcpy(&current_u64_val, &current_float_val, sizeof(current_float_val));

            uval ^= static_cast<uint64_t>(item->invert_mask);  

            if(item->is_forced)
                uval = item->forced_val;  

            if(!item->is_byte_swap)
            {
                regs16[0] = static_cast<uint16_t>(uval >> 48);
                regs16[1] = static_cast<uint16_t>(uval >> 32);
                regs16[2] = static_cast<uint16_t>(uval >> 16);
                regs16[3] = static_cast<uint16_t>(uval >> 0);
                // memcpy(&tval, regs16, sizeof(tval));
                // printf( " sent regs16 #1 0x%08lx  \n", tval);
                // printf( " sent uval   #1 0x%08lx  \n", uval);
            }
            else
            {

                regs16[0] = static_cast<uint16_t>(uval >> 0);
                regs16[1] = static_cast<uint16_t>(uval >> 16);
                regs16[2] = static_cast<uint16_t>(uval >> 32);
                regs16[3] = static_cast<uint16_t>(uval >> 48);
                // memcpy(&tval, regs16, sizeof(tval));
                // printf( " sent regs16 #2 0x%08lx  \n", tval);
                // printf( " sent uval   #2 0x%08lx  \n", uval);
            }

        }    
    }    
    return uval;
}

bool decode_map_struct(std::vector<std::shared_ptr<IO_Work>>&wvec, std::shared_ptr<cfg::map_struct>item, std::any val, struct cfg& myCfg, Uri_req& uri, bool debug)
{
    std::shared_ptr<IO_Work> iop;
    iop = make_work(item->reg_type, item->device_id, item->offset, 1, item->reg16, item->reg8, strToWorkType("get", false));
    wvec.emplace_back(iop);
    return true;
}

//// TODO complete this.
/// @brief given an item , a value and a vector of IO_Work objects , add this item to the vector assuyme a "set" operationm
/// @param wvec  vector of IO_Work items
/// @param item  the item we are setting
/// @param val   the value we are using
/// @return true if all worked out
bool encode_map_struct(std::vector<std::shared_ptr<IO_Work>>&wvec, std::shared_ptr<cfg::map_struct>item, std::any val, struct cfg& myCfg, Uri_req& uri, bool debug)
{
    std::shared_ptr<IO_Work> iop;
    
    // iop->reg_type = item->reg_type;
    // iop->offset = item->offset;
    // iop->device_id = item->device_id;

    if (item->reg_type == cfg::Register_Types::Coil)
    {
        if (debug)
        {
            std::cout << ">>>>" <<__func__<< " regtype Coil " << std::endl;
        }
        iop = make_work(item->reg_type, item->device_id, item->offset, 1, item->reg16, item->reg8, strToWorkType("set", false));
        //iop->ip_address = myCfg.connection.ip_address;
        //iop->port = myCfg.connection.port;
        
        auto bval = getanybval(item,val);
        //item->reg8[0] = bval;
        //iop->u8_buff[0] = bval;
        iop->buf8[0] = bval;
        wvec.emplace_back(iop);
    }
    else if (item->reg_type == cfg::Register_Types::Holding)
    {
        if (debug)
        {
            std::cout << ">>>>" <<__func__<< " regtype Holding " << std::endl;
        }
        iop = make_work(item->reg_type, item->device_id, item->offset, item->size, item->reg16, item->reg8, strToWorkType("set", false));
        // todo fix this
        
        if (uri.is_unforce_request)
        {
            item->is_forced = false;
        }
        auto uval = getanyuval(item, val, iop->buf16);
        if (uri.is_force_request)
        {
            item->is_forced = true;
            item->forced_val = uval;
        }

        std::cout << ">>>>" <<__func__<< " uval " << uval << std::endl;

        // // its all word / bytes swapped enumed etc 
        // //item->reg8[0] = bval;
        // if(item->size == 1)
        // {
        //     uval ^= static_cast<uint16_t>(item->invert_mask);            
        //     iop->buf16[0] = uval & 0xFFFF;

        // }
        // else if(item->size == 2)
        // {
        //     uval ^= static_cast<uint32_t>(item->invert_mask);            

        //     if(!item->is_byte_swap)
        //     {
        //         iop->buf16[0] = static_cast<u16>((uval>>16) & 0xffff);
        //         iop->buf16[1] = static_cast<u16>((uval>>0) & 0xffff);
        //     }
        //     else 
        //     {
        //         iop->buf16[0] = static_cast<u16>((uval>>0) & 0xffff);
        //         iop->buf16[1] = static_cast<u16>((uval>>16) & 0xffff);
        //     }
        // }

        // else if(item->size == 4)
        // {
        //     if(!item->is_word_swap)
        //     {
        //         iop->buf16[0] = (uval>>48) & 0xffff;
        //         iop->buf16[1] = (uval>>32) & 0xffff;
        //         iop->buf16[2] = (uval>>16) & 0xffff;
        //         iop->buf16[3] = (uval) & 0xffff;
        //     }
        //     else 
        //     {
        //         iop->buf16[2] = (uval>>48) & 0xffff;
        //         iop->buf16[3] = (uval>>32) & 0xffff;
        //         iop->buf16[0] = (uval>>16) & 0xffff;
        //         iop->buf16[1] = (uval) & 0xffff;
        //     }

        // }
        wvec.emplace_back(iop);
    }
    //wvec.emplace_back(iop);
    return true;
}

bool encode_map_struct(std::vector<std::shared_ptr<IO_Work>>&wvec, std::shared_ptr<cfg::map_struct>item, std::any val, struct cfg& myCfg, Uri_req& uri, bool debug);
bool decode_map_struct(std::vector<std::shared_ptr<IO_Work>>&wvec, std::shared_ptr<cfg::map_struct>item, std::any val, struct cfg& myCfg, Uri_req& uri, bool debug);


bool uri_is_single( std::shared_ptr<cfg::map_struct>& var_result, struct cfg& myCfg, struct Uri_req& uri, bool debug)
{
    bool single = false;
    for ( auto key : uri.uriv)
    {
        std::cout << " key : ["<< key << "] "<< " size : " << key.size()<<std::endl;

    }

     // 3 keys /comp/id
     // 4 keys  /comp/id/var
    // gcom_config_any
    //std::shared_ptr<cfg::map_struct> var_result;
//    std::map<std::string, std::shared_ptr<cfg::map_struct>> map_result;


    // if is single we'll find it here
    if(uri.uriv.size() >= 4) 
    {
        std::cout << "  looking for var  " << "\n";
        single = test_findMapVar(var_result, myCfg, uri.uriv);
        if(single) {
            std::cout << " var found " << var_result->id << "\n";
        }else{
            std::cout << " var NOT found " << "\n";
        }
    }
    return single;
    // else { // this is a map
    //     try {
    //     //auto& rfoo = 
    //     std::cout << "  looking for map  " << "\n";
    //     result = test_findMapMap(map_result, myCfg, uri_keys);
    //     std::cout << " >>> map found " << "\n";
    //     } catch ( const std::out_of_range&) {
    //         std::cout << " >>> map NOT found " << "\n";

    //     }
    // }


    return single;
}

bool pollWork (std::shared_ptr<IO_Work> io_work);

ioChannel<std::shared_ptr<IO_Work>> io_respChan;      // Use Channel to send IO-Work to thread

void clearChan(bool debug) {
    std::shared_ptr<IO_Work> io_work;
    double delay = 0.1;
    if (debug) {
        std::cout << " Clearing respChan" << std::endl;
    }
    while (io_respChan.receive(io_work, delay)) {
        //runThreadWork(io_thread, io_work, debug);
        if (debug) {
            std::cout 
            << "  start "<< io_work.get()->offset
            << "  number "<< io_work.get()->num_registers
            << std::endl;
        }            
    }
    if (debug) {
        std::cout << " Cleared respChan" << std::endl;
    }

}


//         is_raw_request           = str_ends_with(uri_view, Raw_Request_Suffix);
//         is_timings_request       = str_ends_with(uri_view, Timings_Request_Suffix);
//         is_reset_timings_request = str_ends_with(uri_view, Reset_Timings_Request_Suffix);
//         is_reload_request        = str_ends_with(uri_view, Reload_Request_Suffix);
//         is_enable_request        = str_ends_with(uri_view, Enable_Suffix);
//         is_disable_request       = str_ends_with(uri_view, Disable_Suffix);
//         is_force_request         = str_ends_with(uri_view, Force_Suffix);
//         is_unforce_request       = str_ends_with(uri_view, Unforce_Suffix);
//     std::vector<std::string> uriv;


// };

bool test_uri_body(struct cfg& myCfg, const char *uri, const char* method, const char*pname,const char*uname,const char*repto, const char* body) {

 
 
    std::string_view uri_view;
    Uri_req uri_req(uri_view, uri);



    // if (std::string(method) == "set") {
    //     std::cout << myCfg.client_name <<" detected set method, uri :"<< uri << "\n" ;
    // }
    // else{
    //     std::cout << "detected other method\n" ;
    // }

    // create an any object from the data
    std::any gcom_data;
    gcom_parse_data(gcom_data, body, strlen(body), true);

    // std::cout << __func__<< "   decode body [" << body << "]"<< std::endl ;

    // printAny(gcom_data);

    //std::cout << __func__<< "   decode done" << std::endl ;
    //look for a single
    bool debug = true;
    std::shared_ptr<cfg::map_struct> var;
    std::vector<std::shared_ptr<IO_Work>>wvec;
    
    // have to look for _disable / _enable  / _force /_unforce

    if (uri_is_single(var, myCfg, uri_req, debug))
    {
        if (std::string(method) == "set") 
        {
            std::cout << __func__<< "  found Set single [" << uri << "]" << std::endl ;
            //auto ok = 
            encode_map_struct(wvec, var, gcom_data, myCfg, uri_req, debug);
            std::cout << __func__<< "  wvec size  [" << wvec.size() << "]" << std::endl ;
            auto io_work = wvec.begin();
            if (io_work != wvec.end()) 
            { 
                (*io_work)->io_repChan = &io_respChan;
                pollWork (*io_work);
                wvec.erase(io_work);
            }
            clearChan(true);
        }
        else if (std::string(method) == "get") {
            std::cout << __func__<< "  found Get single [" << uri << "]" << std::endl ;
            //auto ok = 
            decode_map_struct(wvec, var, gcom_data, myCfg, uri_req, debug);
            std::cout << __func__<< "  wvec size  [" << wvec.size() << "]" << std::endl ;
            auto io_work = wvec.begin();
            if (io_work != wvec.end()) 
            { 
                (*io_work)->io_repChan = &io_respChan;
                pollWork (*io_work);
                wvec.erase(io_work);
            }
            clearChan(true);

        }
    }
    else
    {
        if (std::string(method) == "set") 
        {
            std::cout << __func__<< "  processing set multi [" << uri << "]" << std::endl ;
            if (gcom_data.type() == typeid(std::map<std::string, std::any>)) {
                std::cout << " found map\n";
                auto baseMap = std::any_cast<std::map<std::string, std::any>>(gcom_data);
                for (const auto& [key, value] : baseMap) {
                    auto vok = test_findMapVar(var, myCfg, uri_req.uriv, key);
                    if(vok) 
                    {
                        encode_map_struct(wvec, var, value, myCfg, uri_req, debug);
                        std::cout << " found :"<< key << " offset "<< var->offset<<std::endl;
                    }
                    else
                    {
                        std::cout << " Not found :"<< key <<std::endl;
                    }

                }
                //printMap(std::any_cast<std::map<std::string, std::any>>(value), indent + 1);

            }
            std::cout << __func__<< "  wvec size  [" << wvec.size() << "]" << std::endl ;

            // TODO comperss this
            //std::vector<std::shared_ptr<IO_Work>>wvec;
            for (auto io_work : wvec)
            { 
                (io_work)->io_repChan = &io_respChan;
                pollWork (io_work);
            }
            wvec.clear();
        }
        else if (std::string(method) == "get") 
        {
            std::cout << __func__<< "  Not yet processing get multi [" << uri << "]" << std::endl ;
        }
    }
    return true;  // You might want to return a status indicating success or failure.

}



void printMapItem(const std::shared_ptr<cfg::map_struct> item) {
    if (item) {
        std::cout << "Here is the item " << std::endl;
        std::cout << "     ID: " << item->id << "\n";
        std::cout << "     Name: " << item->name << "\n";
        std::cout << "     Type: " << item->rtype << "\n";
        std::cout << "     Offset: " << item->offset << "\n";
        std::cout << "     Size: " << item->size << "\n";
        //std::cout << "     Device ID: " << item->device_id << "\n";
    } else {
        std::cout << "Item is nullptr!\n";
    }
}


// TODO put MapItemMap inside cfg
void addMapItem(MapItemMap& imap, const char* comp, const char* uri, std::shared_ptr<cfg::map_struct> item) {
    if (item) {
        if (imap.find(comp) == imap.end()) {
            imap[comp] = std::map<std::string, std::map<std::string,  std::shared_ptr<cfg::map_struct>>>();
        }
        auto& icomp = imap[comp];
        if (icomp.find(uri) == icomp.end()) {
            icomp[uri] = std::map<std::string,  std::shared_ptr<cfg::map_struct>>();
        }
        auto& iuri = icomp[uri];
        iuri[item->id] = item;
    } else {
        std::cout << "Item is nullptr!\n";
    }
}

// TODO put MapItemMap inside cfg
void addMapId(MapIdMap& imap, const int device_id, cfg::Register_Types rtype, cfg::map_struct* item) {
    if (item) {
        if (imap.find(device_id) == imap.end()) {
            imap[device_id] = std::map<cfg::Register_Types, std::map<int, cfg::map_struct*>>();
        }
        auto& itype = imap[device_id];
        if (itype.find(rtype) == itype.end()) {
            itype[rtype] = std::map<int, cfg::map_struct*>();
        }
        auto& ioffset = itype[rtype];
        ioffset[item->offset] = item;
    } else {
        std::cout << "Item is nullptr!\n";
    }
}

void printItemMap(const MapItemMap& items) {
    for (const auto& comp : items) {
        std::cout << "Component: " << comp.first << "\n";
        for (const auto& idPair : comp.second) {
            std::cout << "  Uri: " << idPair.first << "\n";
            for (const auto& itemPair : idPair.second) {
                std::cout << "    Inner ID: " << itemPair.first << "\n";
                auto& item = itemPair.second;
                printMapItem(item);
            }
        }
    }
}


using ItemSharedPtr = std::shared_ptr<cfg::map_struct>;
using ItemMap = std::map<std::string, std::map<std::string, std::map<std::string, ItemSharedPtr>>>;
// this shows the itemMap structure
void printItem(const ItemSharedPtr& item) {
    if (item) {
        std::cout << "Here is the item " << std::endl;
        std::cout << "     ID: " << item->id << "\n";
        std::cout << "     Name: " << item->name << "\n";
        std::cout << "     Type: " << item->rtype << "\n";
        std::cout << "     Offset: " << item->offset << "\n";
        std::cout << "     Size: " << item->size << "\n";
        std::cout << "     Device ID: " << item->device_id << "\n";
    } else {
        std::cout << "Item is nullptr!\n";
    }
}

// void printItemMap(const ItemMap& items) {
//     for (const auto& componentPair : items) {
//         std::cout << "Component: " << componentPair.first << "\n";
//         for (const auto& idPair : componentPair.second) {
//             std::cout << "  ID: " << idPair.first << "\n";
//             for (const auto& innerIdPair : idPair.second) {
//                 std::cout << "    Inner ID: " << innerIdPair.first << "\n";
//                 const ItemSharedPtr& item = innerIdPair.second;
//                 printItem(item);
//             }
//         }
//     }
// }

ItemSharedPtr findItem(const ItemMap& items, const std::string& component, const std::string& id, const std::string& name) {
    std::cout << " Seeking :"<<component<<"/"<<id<<"  "<<name  << std::endl;

    auto componentIt = items.find(component);
    if (componentIt != items.end()) {
        const auto& idMap = componentIt->second;
        auto idIt = idMap.find(id);
        if (idIt != idMap.end()) {
            for (const auto& innerIdPair : idIt->second) {
                const ItemSharedPtr& item = innerIdPair.second;
                if (item && item->id == name) {
                    return item;
                }
            }
        }
    }
    return nullptr;
}


bool extractJsonValue(const simdjson::dom::element& el, std::any& value) {
    bool ret =  true;
    if (el.is_bool()) {
        value = bool(el);
    } else if (el.is_int64()) {
        value = int64_t(el);
    } else if (el.is_string()) {
        value = std::string(el);
    } else if (el.is_double()) {
        value = double(el);
    } else {
        ret =  false;
    }
    // Extend this for other data types if needed
    return ret;
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
#include <variant>

using MapPointer = std::map<std::string,std::shared_ptr<cfg::map_struct>>*;
using MapStructPointer = std::shared_ptr<cfg::map_struct>;
using MapStructxPointer = cfg::map_struct*;
using RetVar = std::variant<std::monostate, MapPointer, MapStructPointer>;

RetVar findMapVar(struct cfg& myCfg, std::vector<std::string> keys) {
    // Look up outerKey in the outer map
    std::cout << "skipping key 0" << std::endl;
    if(keys.size()< 3)
    {
        std::cout << "Not enough keys found" << std::endl;
        return std::monostate{};
    }
    int key_idx = 0;
    if ((int)keys[0].size() == (int)0)
    {
        //std::cout << "skipping key 0" << std::endl;
        key_idx = 1;
    }

   //std::cout << "itemMap" << myCfg.itemMap << std::endl;
    auto outerIt = myCfg.itemMap.find(keys[key_idx]);
    if (outerIt != myCfg.itemMap.end()) {
        std::cout << "found key 0" << std::endl;
        // If outerKey is found, look up innerKey in the inner map
        auto& innerMap = outerIt->second;
        auto innerIt = innerMap.find(keys[key_idx+1]);
        if (innerIt != innerMap.end()) {
            if ((int)keys.size()==key_idx+2) {
                // If innerKey is also found, return the inner map
                std::cout << "found key 2" << std::endl;
                return &innerIt->second;
            } else {
                auto iMap = innerMap[keys[key_idx+1]];
                auto mapIt = iMap.find(keys[key_idx+2]);
                if (mapIt != iMap.end()) {
                    std::cout << "found key 1" << std::endl;
                    return mapIt->second;
                }
            }
        }
    }
    // If any key not found, return nullopt
    return std::monostate{};
}

// ReturnVariant result = findMapItem(keys);
// if (std::holds_alternative<std::monostate>(result)) {
//     // Handle the case where no value is found
// } else if (std::holds_alternative<MapPointer>(result)) {
//     MapPointer pointer = std::get<MapPointer>(result);
//     // Use pointer as MapPointer
// } else if (std::holds_alternative<MapStructPointer>(result)) {
//     MapStructPointer pointer = std::get<MapStructPointer>(result);
//     // Use pointer as MapStructPointer
// }


std::map<std::string,std::shared_ptr<cfg::map_struct>>* cfg::findMapItem(std::vector<std::string> keys) {

    // Look up outerKey in the outer map
    if(keys.size()< 3)
    {
        std::cout << "Not enough keys found" << std::endl;
        return nullptr;
    }
    int key_idx = 0;
    if ((int)keys[0].size() == (int)0)
    {
        std::cout << "skipping key 0" << std::endl;
        key_idx = 1;
    }

    auto outerIt = itemMap.find(keys[key_idx]);
    if (outerIt != itemMap.end()) {
        // If outerKey is found, look up innerKey in the inner map
        auto& innerMap = outerIt->second;
        auto innerIt = innerMap.find(keys[key_idx+1]);
        if (innerIt != innerMap.end()) {
            // If innerKey is also found, return the inner map
            return &innerIt->second;
        }
        else {
            std::cout << " inner key [" << keys[key_idx+1] << "] not  found" << std::endl;
            return nullptr;
        }
    }
    else {
        std::cout << " base key [" << keys[key_idx] << "] not  found" << std::endl;
    }
    // If any key not found, return nullopt
    return nullptr;
}

//
// this is for random sets , we are able to bin the data into CompressedResults
// for processing pubs we.ll need to get back to the register structure , if all the mapped items are included in the message then the whole register set can be 
// processed.
// this is really for the modbus server.
// the gcom_map is the items
//
bool parseFimsMessage(struct cfg& myCfg, const ItemMap& items, std::vector<std::pair<ItemSharedPtr, std::any>>& result,
                                                                    const std::string&method, const std::string& uri, const std::string& body) {
    simdjson::dom::parser parser;
    simdjson::dom::object json_obj;
    auto error = parser.parse(body).get(json_obj);
    if (error) {
        std::cout << "parse body failed [" << body << "]"<< std::endl;
        return false; // JSON parsing failed
    }
   std::cout << "parse body OK" << std::endl;

    auto uri_tokens = split_string(uri, '/');
    if (uri_tokens.size() < 2) {
        std::cout << "uri_tokens fail size :"<< uri_tokens.size() <<" uri :"<< uri << std::endl;
        return false; // Invalid URI
    }
    // auto uri_keys = split(uri_view, "/")
    // find itemMap[uri[0]][uri[1]


    auto mapItemMap = myCfg.findMapItem(uri_tokens);
    return true;

    std::cout << "uri_tokens OK size :"<< uri_tokens.size() << std::endl;
    printf (" mapItemMap %p \n", (void*)mapItemMap);
     
    const auto& first_field = uri_tokens[1];
    if (items.find(first_field) == items.end()) {
        std::cout << "components field not mapped in config :"<< first_field << std::endl;
        return false; // The first field of the URI doesn't match with the map
    }
    //std::cout << "first_field :"<< first_field << " OK" << std::endl;
    const auto& item_id = uri_tokens[2];
    //std::cout << "item_id :"<< item_id << " OK" << std::endl;

    // If the URI provides a direct identification to an item
    // ie /components/comp_sel_2440/heartbeat 1234
    // or /components/comp_sel_2440/heartbeat '{"value":1234}'
    if (uri_tokens.size() == 4) {
        if (json_obj.size() != 1) {

            std::cout << "single uri body invalid :"<< body << std::endl;
            return false; // Expected a direct value
        }

        for (auto [key, value] : json_obj) {
            if (key == item_id) {
                ItemSharedPtr foundItem = items.at(first_field).begin()->second.at(item_id);
                std::any foundValue;
                extractJsonValue(value, foundValue);
                result.emplace_back(foundItem, foundValue);
                return true; // Successfully matched a specific item
            }
        }
        return false; // No match found
    }

    // Check the body for multiple items
    for (auto [key, value] : json_obj) {

        std::string key_str = std::string(key);
        //std::cout << " seeking item " << key_str << " in config" << std::endl;

        auto it = items.at(first_field).find(item_id);
        if (it != items.at(first_field).end()) {

            auto it = items.at(first_field).at(item_id).find(key_str);
            if (it != items.at(first_field).at(item_id).end()) {
                ItemSharedPtr foundItem = items.at(first_field).at(item_id).at(key_str);

                //ItemSharedPtr foundItem = (it->second.begin())->second;
                std::any foundValue;

                if (value.is_object() && value["value"].error() == simdjson::SUCCESS) {
                    extractJsonValue(value["value"], foundValue);
                } else {
                    extractJsonValue(value, foundValue);
                }
                //std::cout << " Found item " << key_str << " in config  " << std::endl;
                result.emplace_back(foundItem, foundValue);
            }
        }
        else
        {
            std::cout << " Unable to find item " << key_str << " in config , ignoring it " << std::endl;
            return false;
        }
    }

    return true; // Parsing and matching process completed successfully (even if no matches are found)
}

// after getting in a fims message we'll get a vector of items found in the message and values 
// if this is a SET message then we'll have to compress the result vector and the encode the values into the compressed buffer.
// a SET message will also set the local values.
// the iothread can then send the compressed buffer to the device and return the result to the main thread.
// the pubs already have compressed resuts ready to send in that case the iothread will return data in the iobuffer and we'll have to decode that into the actual values.
// a GET message will only access the local data values
//
void printResultVector(const std::vector<std::pair<ItemSharedPtr, std::any>>& result) {
    for (const auto& [itemPtr, anyValue] : result) {
        // Accessing members of the item struct via the shared_ptr
        //printItem(itemPtr);

        std::cout << "Item Details:\n";
        std::cout << "         ID: " << itemPtr->id << "\n";
        std::cout << "         Name: " << itemPtr->name << "\n";
        std::cout << "         Type: " << itemPtr->rtype << "\n";
        std::cout << "         Offset: " << itemPtr->offset << "\n";
        std::cout << "         Size: " << itemPtr->size << "\n";
        std::cout << "         Device ID: " << itemPtr->device_id << "\n";

        // Extracting and printing value from std::any
        if (anyValue.type() == typeid(bool)) {
            std::cout << "             Value: " << std::boolalpha << std::any_cast<bool>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(int64_t)) {
            std::cout << "             Value: " << std::any_cast<int64_t>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(int)) {
            std::cout << "             Value: " << std::any_cast<int>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(std::string)) {
            std::cout << "             Value: " << std::any_cast<std::string>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(double)) {
            std::cout << "             Value: " << std::any_cast<double>(anyValue) << "\n";
        } else {
            std::cout << "using unhandled type :" << anyValue.type().name();
        } // Add more types as needed
// You can add more types if needed

        std::cout << "--------------------\n";
    }
}


struct CompressedItem {
    std::string pg_name; // which publish group did this come from 
    double timeId;     // this is the time from get_time_double that we submitted this item
    int itemId;        // in a list of items this is the itemid or sequence number
    int device_id;
    std::string rtype;
    int start_offset;
    int end_offset;
    int number_of_registers;
    
    int size;  // Note: this is cumulative size now.
    //std::vector<std::any> values; // Storing the related values
    std::vector<std::pair<ItemSharedPtr, std::any>> values;
    unsigned int buffer[125];
};

bool compressResults(const std::vector<std::pair<ItemSharedPtr, std::any>>& results,
                     std::vector<CompressedItem>& compressedResults) {
    
    // First, sort the results by device_id, rtype, and offset.
    auto sortedResults = results; // Make a copy to sort
    std::sort(sortedResults.begin(), sortedResults.end(),
              [](const std::pair<ItemSharedPtr, std::any>& a, const std::pair<ItemSharedPtr, std::any>& b) {
                  if (a.first->device_id != b.first->device_id) return a.first->device_id < b.first->device_id;
                  if (a.first->rtype != b.first->rtype) return a.first->rtype < b.first->rtype;
                  return a.first->offset < b.first->offset;
              });

    for (size_t i = 0; i < sortedResults.size(); ++i) {
        const auto& currentPair = sortedResults[i];
        
        // Start a new compressed item
        CompressedItem compressedItem;
        compressedItem.device_id = currentPair.first->device_id;
        compressedItem.rtype = currentPair.first->rtype;
        compressedItem.start_offset = currentPair.first->offset;
        compressedItem.end_offset = currentPair.first->offset + currentPair.first->size - 1; // Initial value
        compressedItem.values.push_back(currentPair);
        
        // Try to group following items if they match in device_id, rtype, and offset sequence
        while (i + 1 < sortedResults.size()) {
            const auto& nextPair = sortedResults[i + 1];
            if (nextPair.first->device_id == compressedItem.device_id &&
                nextPair.first->rtype == compressedItem.rtype &&
                nextPair.first->offset == compressedItem.end_offset + 1) {
                // Group together
                compressedItem.end_offset = nextPair.first->offset + nextPair.first->size - 1;
                compressedItem.values.push_back(nextPair);
                ++i; // Move to the next result
            } else {
                break; // Exit if we can't group the next item
            }
        }
        
        compressedResults.push_back(compressedItem);
    }
    
    return true; 
}

void printCompressedResults(const std::vector<CompressedItem>& compressedResult) {
    for (const auto& item : compressedResult) {
        std::cout << "    Compressed Item Details:\n";
        std::cout << "          Device ID: " << item.device_id << "\n";
        std::cout << "          Type: " << item.rtype << "\n";
        std::cout << "          Start Offset: " << item.start_offset << "\n";
        std::cout << "          End Offset: " << item.end_offset << "\n";
        
        std::cout << "          Values:\n";
        for (const auto& [itemPtr, anyValue] : item.values) {
            //printItem(itemPtr);

            // Accessing members of the original item struct via the shared_ptr
            std::cout << "          - Item ID: " << itemPtr->id << ", Name: " << itemPtr->name << ", Value: ";

            // Extracting and printing value from std::any
            if (anyValue.type() == typeid(bool)) {
                std::cout << std::boolalpha << std::any_cast<bool>(anyValue);
            } else if (anyValue.type() == typeid(int64_t)) {
                std::cout << std::any_cast<int64_t>(anyValue);
            } else if (anyValue.type() == typeid(int)) {
                std::cout << std::any_cast<int>(anyValue);
            } else if (anyValue.type() == typeid(std::string)) {
                std::cout << std::any_cast<std::string>(anyValue);
            } else if (anyValue.type() == typeid(double)) {
                std::cout << std::any_cast<double>(anyValue);
            } else {
                std::cout << "using unhandled type :" << anyValue.type().name();
            } // Add more types as needed
            std::cout << "\n";
        }

        std::cout << "--------------------\n";
    }
}

// this create the Item map 
// from the gcom_map

ItemMap extract_structure(ItemMap &structure, const std::map<std::string, std::any>& jsonData) {
    bool debug = false;
    //ItemMap structure;

    // Extract components array
    auto rawComponents = getMapValue<std::vector<std::any>>(jsonData, "components");
    if (!rawComponents.has_value()) {
        std::cout << __func__ << " error no  components in jsonData"<<std::endl;
        return structure;
        //throw std::runtime_error("Missing components in JSON data.");
    }
    std::cout << __func__ << " >>>> found components in jsonData"<<std::endl;

    for (const std::any& rawComponent : rawComponents.value()) {
        if (rawComponent.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> componentData = std::any_cast<std::map<std::string, std::any>>(rawComponent);

            std::string componentId;
            getItemFromMap(componentData, "component_id", componentId, std::string("components"), true, true, false).value();
            std::cout << __func__ << " >>>> found componentsId :"<< componentId<<" in jsonData"<<std::endl;
            std::string id; 
            getItemFromMap(componentData, "id", id, std::string(), true, false, false).value();

            int device_id; 
            getItemFromMap(componentData, "device_id", device_id, 255, true, true, false).value();

            auto rawRegisters = getMapValue<std::vector<std::any>>(componentData, "registers");
            if (!rawRegisters.has_value()) {
                throw std::runtime_error("Missing registers in component data.");
            }
            std::cout << __func__ << " >>>> found registers in jsonData"<<std::endl;

            for (const std::any& rawRegister : rawRegisters.value()) {
                if (rawRegister.type() == typeid(std::map<std::string, std::any>)) {
                    std::map<std::string, std::any> registerData = std::any_cast<std::map<std::string, std::any>>(rawRegister);

                    std::string rtype; 
                    getItemFromMap(registerData, "type", rtype, std::string(), true, false,  debug).value();

                    auto rawMaps = getMapValue<std::vector<std::any>>(registerData, "map");
                    if (!rawMaps.has_value()) {
                        continue;  // No maps to process in this register
                    }

                    for (const std::any& rawMap : rawMaps.value()) {
                        if (rawMap.type() == typeid(std::map<std::string, std::any>)) {
                            std::map<std::string, std::any> mapData = std::any_cast<std::map<std::string, std::any>>(rawMap);

                            // this needs to be struct cfg::map_struct
                            //std::vector<cfg::map_struct> extract_maps(struct cfg::reg_struct* reg,  std::any& rawMapData) {
                            // but we may need a different root object here
                            // look at extract components

                            ItemSharedPtr item = std::make_shared<cfg::map_struct>();
                            //item->id = 
                            getItemFromMap(mapData, "id", item->id, std::string("some_id"), true, false, false);
                            //item->name = 
                            getItemFromMap(mapData, "name", item->name, std::string("some_name"), true, false,  false);
                            item->rtype = rtype;
                            //item->offset = 
                            getItemFromMap(mapData, "offset", item->offset, 0, true,false,  debug);
                            //item->size = 
                            getItemFromMap(mapData, "size", item->size, 1, true, true, false);
                            item->device_id = device_id;

                            structure[componentId][id][item->id] = item;
                        }
                    }
                }
            }
        }
    }

    return structure;
}
//}  // namespace mycfg




double get_time_double();


// this may be the big unpacker
void cfg::addPub(const std::string& base, const std::string& cname, std::shared_ptr<comp_struct> comp, struct cfg* myCfg) {
        auto mypub = std::make_shared<pub_struct>();

        std::string pstr = "pub_" + base + "_" + cname;
        //std::string bstr = "tmr_" + pstr;
        //std::string lstr = "tlate_" + pstr;
        pubs[pstr]=mypub;

        mypub->id =  pstr;
        mypub->comp = comp;
        mypub->cfg = myCfg;

        mypub->count = 0;
        mypub->tRequest = 0.0;
        mypub->tAvg = 0.0;
        mypub->tTotal = 0.0;
        mypub->tMax = 0.0;
        mypub->tLate = 0.0;
        mypub->pubStats.set_label(pstr);

    }


class PublishGroup {
private:

public:
    double timeId;     // this is the time from get_time_double that we submitted this item
    int numItems;      //  this is the number of items in the vector.

    std::string name;
    int frequency;
    int offset_time;
    std::vector<CompressedItem> compressedItems;
    // Constructor
    PublishGroup(const std::string& component_id, const std::string& id, 
                 int freq, int offset) 
        : name(component_id + "_" + id), frequency(freq), offset_time(offset) {}

    // Add a CompressedItem to the list
    void addCompressedItem(const CompressedItem& item) {
        compressedItems.push_back(item);
    }

    // (Optional) Retrieve the list of CompressedItem objects
    const std::vector<CompressedItem>& getCompressedItems() const {
        return compressedItems;
    }

    // Method to handle publishing based on the timer
    void publish(std::vector<CompressedItem>& requests) {
        auto itemId = 0;
        timeId = get_time_double();
        for (auto& item : compressedItems) {
            item.pg_name = name;   // who sent this
            item.timeId = timeId;  // I'm assuming timeId is defined somewhere in your class
            item.itemId = itemId++;
            requests.push_back(item);  // Appending to the passed-in requests vector
        }
    }
};


// this is a floating object at the moment 
// each request needs to be sent to the iothread get queue
std::vector<CompressedItem> pub_requests;
//these will get sent on to the ioThreads

void publish_cb(void * timer, void *data) {
    // Cast data pointer back to publish_group
    PublishGroup* pg = static_cast<PublishGroup*>(data);
    pg->publish(pub_requests);

    // For now, let's just print a greeting
    std::cout << "Hi from publish_cb! Processing data for publish group: " << pg->name << std::endl;
}

// Dummy timer create function
void timer_create(const std::string& name, int frequency, int offset, void (*callback)(void*, void*), void* data) {
    // For now, let's just print the timer setup
    // Cast the void pointer back to PublishGroup*
    PublishGroup* pg = static_cast<PublishGroup*>(data);
    std::cout << "Timer created with name: " << name << ", frequency: " << pg->frequency << ", offset: " << offset << std::endl;
}

void register_publish_groups_with_timer(const std::vector<std::shared_ptr<PublishGroup>>& publishGroups) {
    for (auto& pgPtr : publishGroups) {
        // Construct timer name from publish group name
        std::string timerName = "timer_for_" + pgPtr->name;

        // Register with timer_create
        // We'll use get() to obtain the raw pointer from shared_ptr
        timer_create(timerName, pgPtr->frequency, pgPtr->offset_time, publish_cb, (void*)pgPtr.get());
    }
}


void show_publish_groups(std::vector<std::shared_ptr<PublishGroup>> &publishGroups) {
    for (auto& pg : publishGroups) {
        std::cout << "Publish Group name: " << pg->name
                  << ", frequency: " << pg->frequency
                  << ", offset: " << pg->offset_time << std::endl;

        // Showing Compressed Items
        std::cout << "  Compressed Items: " << std::endl;
        printCompressedResults(pg->compressedItems);

    }
}

// TODO stick this somewhere
std::map<std::string, std::shared_ptr<PublishGroup>> publishGroupMap;


// this is run once after start up 
// it will create the different publish_groups
// when connected to a timer, the callback will get a requests list generated and send the requrst to the iothread pub queue.
// as each result gets back from the iothread the main thread will collect all the results into a pub_vector 
// when they are all back we can then pub the results out.
// of course we'll have to decode the results and save the values in the local Itemmap

bool extract_publish_groups(std::vector<std::shared_ptr<PublishGroup>> &publishGroups, const ItemMap& items, const std::map<std::string, std::any>& jsonData) {
    bool debug = false;
    // Extract components array
    auto rawComponents = getMapValue<std::vector<std::any>>(jsonData, "components");
    if (!rawComponents.has_value()) {
        std::cout << __func__ << " error: no components in jsonData" << std::endl;
        return false;
    }
    std::cout << __func__ << " >>>> found components in jsonData" << std::endl;

    for (const std::any& rawComponent : rawComponents.value()) {
        if (rawComponent.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> componentData = std::any_cast<std::map<std::string, std::any>>(rawComponent);
            std::string componentId;
            getItemFromMap(componentData, "component_id", componentId, std::string("components"), true, true, false);
            std::string comp_id; 
            getItemFromMap(componentData, "id", comp_id, std::string(), true, false, false);
            int frequency;
            getItemFromMap(componentData, "frequency", frequency, 1000, true, false, false);
            int offset_time;
            getItemFromMap(componentData, "offset_time", offset_time, 0, true, false, false);
            int comp_device_id;
            getItemFromMap(componentData, "device_id", comp_device_id, 255, true, false,  debug);


            std::string groupName = componentId + "_" + comp_id;
            auto pubGroup = std::make_shared<PublishGroup>(componentId, comp_id, frequency, offset_time); // Frequency and offset are hardcoded to 0 for now.

            auto rawRegisters = getMapValue<std::vector<std::any>>(componentData, "registers");
            if (!rawRegisters.has_value()) {
                throw std::runtime_error("Missing registers in component data.");
            }

            for (const std::any& rawRegister : rawRegisters.value()) {
                if (rawRegister.type() == typeid(std::map<std::string, std::any>)) {
                    std::map<std::string, std::any> registerData = std::any_cast<std::map<std::string, std::any>>(rawRegister);

                    CompressedItem compressedItem;
                    int device_id;
                    getItemFromMap(registerData, "device_id", device_id, comp_device_id, true, true,  debug);
                    compressedItem.device_id = device_id;
                    

                    std::string rtype;
                    if (getItemFromMap(registerData, "type", rtype, std::string(), true, false,  debug).has_value()) {
                        compressedItem.rtype = rtype;
                    }

                    int start_offset;
                    if (getItemFromMap(registerData, "starting_offset", start_offset, 0, true, false,  debug).has_value()) {
                        compressedItem.start_offset = start_offset;
                    }
                    compressedItem.end_offset = compressedItem.start_offset; // Assuming end_offset is the same for now.

                    int number_of_registers;
                    if (getItemFromMap(registerData, "number_of_registers", number_of_registers, 1, true, false,  debug).has_value()) {
                        compressedItem.end_offset = compressedItem.start_offset + number_of_registers - 1;
                        compressedItem.number_of_registers = number_of_registers;
                    }
                    auto rawMaps = getMapValue<std::vector<std::any>>(registerData, "map");
                    if (!rawMaps.has_value()) {
                        continue;  // No maps to process in this register
                    }

                    for (const std::any& rawMap : rawMaps.value()) {
                        if (rawMap.type() == typeid(std::map<std::string, std::any>)) {
                            std::map<std::string, std::any> mapData = std::any_cast<std::map<std::string, std::any>>(rawMap);

                            std::string id; 
                            getItemFromMap(mapData, "id", id, std::string("some_id"), true, false, false);
                            // need to push this into the values vec structure[componentId][id][item->id] = item;
                            // 

                            std::cout << " adding comp :["<< componentId << "]  id :[" <<comp_id << "] item : "<< id << std::endl;
                            auto itemP = findItem(items, componentId, comp_id, id);
                            if (itemP)
                            {

                                std::cout << " found comp :["<< componentId << "]  id :[" <<comp_id << "] item : "<< id << std::endl;
                                std::any foundValue = itemP->value;
                                compressedItem.values.emplace_back(itemP, foundValue);
                            }
                        }
                    }

                    // Add the CompressedItem to the pubGroup
                    pubGroup->addCompressedItem(compressedItem);
                }
            }
            publishGroups.push_back(pubGroup);
        }
    }

    return true;
}



std::map<std::string, std::any> jsonToMap(simdjson::dom::object obj);

std::string mapToString(const std::map<std::string, std::any>& m);
// TODO check for duplicate or even use
bool anyTypeString(const std::any& value) {

    int status;
    char* demangled = abi::__cxa_demangle(value.type().name(), 0, 0, &status);
    if(status == 0) {
        std::cout << "Type of value: " << demangled << std::endl;
        free(demangled);
    } else {
        std::cout << "Type of value (mangled): " << value.type().name() << std::endl;
    }
    return true;
}



std::string anyToString(const std::any& value) {
    //std::cout << "Type of value: " << value.type().name() << std::endl;

    try {
        return std::to_string(std::any_cast<int>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::to_string(std::any_cast<double>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::to_string(std::any_cast<long>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::to_string(std::any_cast<unsigned long>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::any_cast<bool>(value) ? "true" : "false";
    } catch (const std::bad_any_cast&) {}
    try {
        return std::any_cast<std::string>(value);
    } catch (const std::bad_any_cast&) {}
    try {
        return mapToString(std::any_cast<std::map<std::string, std::any>>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        std::vector<std::any> vec = std::any_cast<std::vector<std::any>>(value);
        std::string result = "[";
        for (const auto& v : vec) {
            result += anyToString(v) + ", ";
        }
        if (result.length() > 1) {
            result = result.substr(0, result.length() - 2);  // remove the last ", "
        }
        result += "]";
        return result;
    } catch (const std::bad_any_cast& e) {
        std::cout << "Bad type "<< e.what() << std::endl;
    }
    return "unknown";
}


std::string mapToString(const std::map<std::string, std::any>& m) {
    std::string result = "{";
    for (const auto& [key, value] : m) {
        result += "\"" + key + "\": " + anyToString(value) + ", ";
    }
    if (result.length() > 1) {
        result = result.substr(0, result.length() - 2);  // remove the last ", "
    }
    result += "}";
    return result;
}

/*
uri:/components/comp_sel_2440/fuse_monitoring  body:false"
uri:/components/comp_sel_2440/fuse_monitoring  body:{value:false}"
uri:/components/comp_sel_2440                  body:{fuse_monitoring:false}"
uri:/components/comp_sel_2440                  body:{fuse_monitoring: {value:false}}"

uri:/components/comp_sel_2440/voltage          body:1234"
uri:/components/comp_sel_2440/voltage          body:{value:1234}"
uri:/components/comp_sel_2440                  body:{voltage:1234}"
uri:/components/comp_sel_2440                  body:{voltage: {value:1234}}"

uri:/components/comp_sel_2440/status           body:"running""
uri:/components/comp_sel_2440/status           body:{value:"running"}"
uri:/components/comp_sel_2440                  body:{status:"running"}"
uri:/components/comp_sel_2440                  body:{status: {value:"running"}}"

uri:/components/comp_sel_2440                  body:{voltage:1234, fuse_monitoring:false, status:"running" }"
uri:/components/comp_sel_2440                  body:{voltage:{value:1234}, fuse_monitoring:{value:false}, status:{value:"running" }}"
*/



// use the split thing these are deprecated
// from parseFimsMessage
// auto uri_tokens = split_string(uri, '/');
    // if (uri_tokens.size() < 2) {
    //     std::cout << "uri_tokens fail size :"<< uri_tokens.size() <<" uri :"<< uri << std::endl;
    //     return false; // Invalid URI
    // }


std::string extractCompFromURI(const std::string& uri) {
    // Finding the start of the component ID
    std::size_t start = uri.find("/");
    start += std::strlen("/");  // moving past "/"

    // Finding the end of the component ID (next slash or end of the string)
    std::size_t end = uri.find("/", start);

    // Extracting the component ID
    if (end != std::string::npos) {
        return uri.substr(start, end - start);
    } else {
        return uri.substr(start);  // Till the end of the string
    }
}

std::string extractCompIdFromURI(const std::string& uri, const std::string&comp) {
    // Finding the start of the component ID
    std::size_t start = uri.find(comp);
    if (start == std::string::npos) {
        return ""; // or throw an error if appropriate
    }
    start += std::strlen("/");  // moving past "/components/"

    // Finding the end of the component ID (next slash or end of the string)
    std::size_t end = uri.find("/", start);

    // Extracting the component ID
    if (end != std::string::npos) {
        return uri.substr(start, end - start);
    } else {
        return uri.substr(start);  // Till the end of the string
    }
}

std::string extractIdFromURI(const std::string& uri) {
    // Finding the start of the component ID
    std::size_t start = uri.find("/components/");
    if (start == std::string::npos) {
        return ""; // or throw an error if appropriate
    }
    start += std::strlen("/components/");  // moving past "/components/"

    // Finding the end of the component ID (next slash or end of the string)
    std::size_t end = uri.find("/", start);

    // Extracting the component ID
    if (end != std::string::npos) {
        return uri.substr(start, end - start);
    } else {
        return uri.substr(start);  // Till the end of the string
    }
}

std::string getKeyFromURI(const std::string& uri) {
    if (uri.back() == '/') { 
        return "";
    }

    auto pos = uri.find_last_of('/');
    if (pos == std::string::npos) {
        return uri;
    }

    return uri.substr(pos + 1);
}


std::string getElementTypeString(simdjson::dom::element_type etype) {
    switch (etype) {
        case simdjson::dom::element_type::ARRAY: return "ARRAY";
        case simdjson::dom::element_type::OBJECT: return "OBJECT";
        case simdjson::dom::element_type::INT64: return "INT64";
        case simdjson::dom::element_type::UINT64: return "UINT64";
        case simdjson::dom::element_type::DOUBLE: return "DOUBLE";
        case simdjson::dom::element_type::STRING: return "STRING";
        case simdjson::dom::element_type::BOOL: return "BOOL";
        case simdjson::dom::element_type::NULL_VALUE: return "NULL_VALUE";
        default: return "UNKNOWN";
    }
}

std::string processFieldAsString(const simdjson::dom::element& field_value) {
    simdjson::dom::element_type etype = field_value.type();

    if (field_value.is_string()) {
        return std::string(field_value);
    } 
    else if (field_value.is_int64()) {
        int64_t int_val;
        simdjson::error_code ec = field_value.get(int_val);
        if (ec == simdjson::SUCCESS) {
            return std::to_string(int_val);
        } else {
            return "Failed to extract int64 value: " + std::string(simdjson::error_message(ec));
        }
    } 
    else if (field_value.is_uint64()) {
        uint64_t uint_val;
        simdjson::error_code ec = field_value.get(uint_val);
        if (ec == simdjson::SUCCESS) {
            return std::to_string(static_cast<unsigned int>(uint_val));
        } else {
            return "Failed to extract uint64 value: " + std::string(simdjson::error_message(ec));
        }
    } 
    else if (field_value.is_bool()) {
        bool bool_val;
        if (field_value.get(bool_val) == simdjson::SUCCESS) {
            return bool_val ? "true" : "false";
        }
    }
    else if (field_value.is_object()) {
        return "{...}"; // Some generic representation for objects, or you can serialize it if you want
    }
    else {
        return "Unknown or unhandled type encountered: " + getElementTypeString(etype);
    }
    return "Unknown Error";
}

std::any processField(const simdjson::dom::element& field_value) {
        simdjson::dom::element_type etype = field_value.type();
        std::cout << " this is the type encountered: " << getElementTypeString(etype) << std::endl;

    if (field_value.is_string()) {
        return std::any(std::string(field_value));
    } 
    else if (field_value.is_int64()) {
        int64_t int_val;
        simdjson::error_code ec = field_value.get(int_val);
        if (ec == simdjson::SUCCESS) {
            std::cout << "Extract int64 value: " << int_val << std::endl;
            return std::any(int_val);
        } else {
            std::cerr << "Failed to extract int64 value: " << simdjson::error_message(ec) << std::endl;
        }
    } 
    else if (field_value.is_uint64()) {
        uint64_t uint_val;
        simdjson::error_code ec = field_value.get(uint_val);
        if (ec == simdjson::SUCCESS) {
            return std::any(static_cast<unsigned int>(uint_val));
        } else {
            std::cerr << "Failed to extract uint64 value: " << simdjson::error_message(ec) << std::endl;
        }
    } 
    else if (field_value.is_bool()) {
        bool bool_val;
        if (field_value.get(bool_val) == simdjson::SUCCESS) {
            return std::any(bool_val);
        }
    }
    else if (field_value.is_object()) {
        std::map<std::string, std::any> valueMap;
        for (auto [sub_key, sub_value] : field_value.get_object()) {
            valueMap[std::string(sub_key)] = processField(sub_value);
        }
        return valueMap;
    }
    else {
        // In case of an unknown type or an error, you might want to log it:
        simdjson::dom::element_type etype = field_value.type();
        std::cerr << "Unknown or unhandled type encountered: " << getElementTypeString(etype) << std::endl;

    }
    // // In case of an unknown type or an error, you might want to log it:
    // std::cerr << "Unknown or unhandled type encountered." << std::endl;

    return {};  // returning an empty std::any
}


void mergeSubMaps(std::map<std::string, std::any>& base, const std::map<std::string, std::any>& toMerge) {
    for (const auto& [key, value] : toMerge) {
        if (base.find(key) != base.end() && base[key].type() == typeid(std::map<std::string, std::any>) && value.type() == typeid(std::map<std::string, std::any>)) {
            // If the key exists in both maps and both values are maps, merge them recursively
            mergeSubMaps(std::any_cast<std::map<std::string, std::any>&>(base[key]), std::any_cast<const std::map<std::string, std::any>&>(value));
        } else {
            // Otherwise, simply overwrite (or add) the value from 'toMerge' into 'base'
            base[key] = value;
        }
    }
}


// std::map<std::string, std::any> mergeMap(const std::string& uri1, const std::string& body1, const std::string& uri2, const std::string& body2) {
//     std::map<std::string, std::any> baseMap = parseMessage(uri1, body1);
//     std::map<std::string, std::any> secondaryMap = parseMessage(uri2, body2);
    
//     mergeSubMaps(baseMap, secondaryMap);

//     return baseMap;
// }

std::map<std::string, std::any> parseInputMessage(std::map<std::string, std::any> sysMap, const std::string& uri, const std::string& method, const std::string& body) {

    std::map<std::string, std::any> baseMap;


    // Extract the id and key from the URI
    // /components/comp_sel_2440

    // TODO use the split thing
    std::string comp = extractCompFromURI(uri);
    std::string id = extractCompIdFromURI(uri, comp);
    std::string key = getKeyFromURI(uri);

    std::cout << __func__ << " found comp [" << comp<<"]" <<std::endl;
    std::cout << __func__ << " found id [" << id<<"]" <<std::endl;
    std::cout << __func__ << " found key [" << key<<"]" <<std::endl;
    // so the comp is our base map
    // currently its always "components" 

    // Create the initial baseMap entry for this id if it doesn't exist
    // if (!baseMap[id].has_value()) {
    //     baseMap[id] = std::map<std::string, std::any>();
    // }
    // Parsing JSON using simdjson
    simdjson::dom::parser parser;
    simdjson::dom::element obj = parser.parse(body);

    if (!obj.is_object()) {
        std::cerr << "Expected a JSON object in the message body." << std::endl;
        return baseMap;
    }

    // this id is comp_sel_2440 
    // it has to be one of our comps in sysmap if not forget it
    if (!sysMap[key].has_value()) {
        std::cerr << "no key ["<<key<<"] match in sysMap." << std::endl;
        return baseMap;
    }  
    // auto sysObj = sysMap[id];
    //the next key 
    //if (!baseMap[id].has_value()) {
    //    baseMap[id] = std::map<std::string, std::any>();
    //}

             

    // If the URI contains a key, process the body as its value

    auto& componentMap = std::any_cast<std::map<std::string, std::any>&>(sysMap[key]);

    for (auto [key, value] : obj.get_object()) {
        std::cout << " Processing key:" << key << std::endl;
        if (!componentMap[std::string(key)].has_value()) {

            std::cerr << "no key match in sysMap." << std::endl;
        }
        else
        {
            if (value.is_object()) {
                std::cout << "found key/obj ["<< key << " ] match in sysMap." << std::endl;
                for (auto [nested_key, nested_value] : value.get_object()) {
                    if (nested_key == "value") { // if you're specifically looking for the "value" key
                        auto valstr = processFieldAsString(nested_value);
                        std::cout << "  found nested key/value ["<< nested_key << " ] [" << valstr << "] in sysMap." << std::endl;
                    }
                }

            } else {

                auto foo = getElementTypeString(value.type());

                std::cout << "found key/value ["<< key << " ] ["<<value<<"] type ["<<foo<<"] in sysMap." << std::endl;
            }
            // now value can be an obg with "value" i it or some kid of raw valye

        }

    }

    return baseMap;

    for (auto [key, value] : obj.get_object()) {
        std::cout << " Processing key:" << key << std::endl;


        if (value.is_object()) {
            std::map<std::string, std::any> subMap;
            for (auto [sub_key, sub_value] : value.get_object()) {
                subMap[std::string(sub_key)] = processField(sub_value);
            }
            componentMap[std::string(key)] = subMap;
        } else {
            componentMap[std::string(key)] = processField(value);
        }
    }
    return baseMap;
}


std::map<std::string, std::any> parseMessage(const std::string& uri,const std::string method,  const std::string& body) {
    std::map<std::string, std::any> baseMap;

    // Parsing JSON using simdjson
    simdjson::dom::parser parser;
    simdjson::dom::element obj = parser.parse(body);

    // Extract the id and key from the URI
    std::string id = extractIdFromURI(uri);
    std::string key = getKeyFromURI(uri);

    // Create the initial baseMap entry for this id if it doesn't exist
    // if (!baseMap[id].has_value()) {
    //     baseMap[id] = std::map<std::string, std::any>();
    // }
    if (!obj.is_object()) {
        std::cerr << "Expected a JSON object in the message body." << std::endl;
        return baseMap;
    }

    if (!baseMap[id].has_value()) {
        baseMap[id] = std::map<std::string, std::any>();
    }

    // If the URI contains a key, process the body as its value
    auto& componentMap = std::any_cast<std::map<std::string, std::any>&>(baseMap[id]);

    for (auto [key, value] : obj.get_object()) {
        std::cout << " Processing key:" << key << std::endl;

        if (value.is_object()) {
            std::map<std::string, std::any> subMap;
            for (auto [sub_key, sub_value] : value.get_object()) {
                subMap[std::string(sub_key)] = processField(sub_value);
            }
            componentMap[std::string(key)] = subMap;
        } else {
            componentMap[std::string(key)] = processField(value);
        }
    }
    return baseMap;
}



// we have not dome the output fims message handling yet but this is all 
// stuff we may hvae to use.
// Your utility function to print nested maps
size_t computeSize(const std::map<std::string, std::any>& baseMap, int indent = 0) {
    size_t totalSize = 0;

    for (const auto& [key, value] : baseMap) {
        totalSize += key.size() + 2 * indent + 2; // Key size + indentation + ": " 

        if (value.type() == typeid(std::map<std::string, std::any>)) {
            totalSize += 1; // Newline
            totalSize += computeSize(std::any_cast<std::map<std::string, std::any>>(value), indent + 1);
        } else if (value.type() == typeid(int)) {
            totalSize += std::to_string(std::any_cast<int>(value)).size() + 1; // Value + newline
        } else if (value.type() == typeid(int64_t)) {
            totalSize += std::to_string(std::any_cast<int64_t>(value)).size() + 1;
        } else if (value.type() == typeid(double)) {
            // This is an approximation; actual size might vary due to floating-point representation
            totalSize += std::to_string(std::any_cast<double>(value)).size() + 1;
        } else if (value.type() == typeid(std::string)) {
            totalSize += std::any_cast<std::string>(value).size() + 1;
        } else if (value.type() == typeid(bool)) {
            totalSize += std::any_cast<bool>(value) ? 5 : 6; // Either "true\n" or "false\n"
        }
    }
    return totalSize;
}



int test_buffer_size() {

    std::map<std::string, std::any> testMap = {
        {"key1", std::map<std::string, std::any>{
            {"subkey1", 123},
            {"subkey2", "value2"},
            {"subkey3", std::map<std::string, std::any>{
                {"subsubkey1", 45.6}
            }}
        }},
        {"key2", "value2"},
        {"key3", 789}
    };

    size_t requiredBufferSize = computeSize(testMap);
    std::cout << "Required buffer size: " << requiredBufferSize << std::endl;

    return 0;
}



constexpr size_t bufferSize = 4096 * 128;  // Modify according to your needs

void mapToRawBuffer(const std::map<std::string, std::any>& baseMap, spdlog::memory_buf_t& buf, int indent = 0) {
    for (const auto& [key, value] : baseMap) {
        for (int i = 0; i < indent; ++i) {
            //buf.append("  ", "  " + 2);
            //fmt::format_to(buf, "  ");
            fmt::format_to(std::back_inserter(buf), "  ");
        }
        fmt::format_to(std::back_inserter(buf), "{}: ", key);
        
        if (value.type() == typeid(std::map<std::string, std::any>)) {
            fmt::format_to(std::back_inserter(buf), "\n");
            mapToRawBuffer(std::any_cast<std::map<std::string, std::any>>(value), buf, indent + 1);
        } else if (value.type() == typeid(int)) {
            fmt::format_to(std::back_inserter(buf), "{}\n", std::any_cast<int>(value));
        } else if (value.type() == typeid(int64_t)) {
            fmt::format_to(std::back_inserter(buf), "{}\n", std::any_cast<int64_t>(value));
        } else if (value.type() == typeid(double)) {
            fmt::format_to(std::back_inserter(buf), "{}\n", std::any_cast<double>(value));
        } else if (value.type() == typeid(std::string)) {
            fmt::format_to(std::back_inserter(buf), "{}\n", std::any_cast<std::string>(value));
        } else if (value.type() == typeid(bool)) {
            fmt::format_to(std::back_inserter(buf), "{}\n", std::any_cast<bool>(value));
        }
    }
}


void mapToBuffer(const std::map<std::string, std::any>& baseMap, spdlog::memory_buf_t& buf, int indent = 0) {
    fmt::format_to(std::back_inserter(buf), "{{\n");
    bool firstItem = true;
    for (const auto& [key, value] : baseMap) {
        if (!firstItem) {
            fmt::format_to(std::back_inserter(buf), ",\n");
        }
        firstItem = false;

        for (int i = 0; i < indent + 1; ++i) {
            fmt::format_to(std::back_inserter(buf), "  ");
        }

        // simdjson::dom::element_type etype = value.type();
        // std::cout << " this is the type encountered: " << getElementTypeString(etype) << std::endl;
        //std::cout << " value type >>>" <<value.type().name() << std::endl;

        if (value.type() == typeid(std::map<std::string, std::any>)) {
            fmt::format_to(std::back_inserter(buf), "\"{}\": ", key);
            mapToBuffer(std::any_cast<std::map<std::string, std::any>>(value), buf, indent + 1);
        } else {
            //buf.append(key.begin(), key.end());
            //fmt::format_to(std::back_inserter(buf), ": ");

            fmt::format_to(std::back_inserter(buf), "\"{}\": ", key);
            if (value.type() == typeid(int)) {
                fmt::format_to(std::back_inserter(buf), "{}", std::any_cast<int>(value));
            } else if (value.type() == typeid(int64_t)) {
                fmt::format_to(std::back_inserter(buf), "{}", std::any_cast<int64_t>(value));
            } else if (value.type() == typeid(double)) {
                fmt::format_to(std::back_inserter(buf), "{}", std::any_cast<double>(value));
            } else if (value.type() == typeid(const char *)) {
                fmt::format_to(std::back_inserter(buf), "\"{}\"", std::any_cast<const char*>(value));
            } else if (value.type() == typeid(std::string)) {
                const auto& strValue = std::any_cast<std::string>(value);
                //buf.append(strValue.begin(), strValue.end());
                //fmt::format_to(std::back_inserter(buf), "\"{}\"", std::any_cast<std::string>(value));
                std::cout << " found strvalue ["<<strValue<<"]"<<std::endl;
                fmt::format_to(std::back_inserter(buf), "\"{}\"", strValue);
            } else if (value.type() == typeid(bool)) {
                fmt::format_to(std::back_inserter(buf), "{}", std::any_cast<bool>(value) ? "true" : "false");
            }
        }
    }

    fmt::format_to(std::back_inserter(buf), "\n");
    for (int i = 0; i < indent; ++i) {
        fmt::format_to(std::back_inserter(buf), "  ");
    }
    fmt::format_to(std::back_inserter(buf), "}}");
}


int test_printMap() {
    // Setup spdlog
    spdlog::set_level(spdlog::level::debug);

    std::map<std::string, std::any> testMap = {
        {"key1", std::map<std::string, std::any>{
            {"subkey1", 123},
            {"subkey2", "value2"},
            {"subkey3", std::map<std::string, std::any>{
                {"subsubkey1", 45.6}
            }}
        }},
        {"key2", "value2"},
        {"key3", 789}
    };

    //spdlog::details::fixed_buffer<bufferSize> raw_buffer;
    //int raw_buffer = 4096;
    //spdlog::memory_buf_t buffer(raw_buffer);
    spdlog::memory_buf_t buffer;

    mapToBuffer(testMap, buffer);

    spdlog::info("{}", fmt::to_string(buffer));

    return 0;
}

// ... (include the parseMessage function from the previous answer here)

//



// Utility function to print nested maps
void printMap(const std::map<std::string, std::any>& baseMap, int indent = 0) {
    for (const auto& [key, value] : baseMap) {
        for (int i = 0; i < indent; ++i) {
            std::cout << "  ";
        }
        std::cout << key << ": ";
        if (value.type() == typeid(std::map<std::string, std::any>)) {
            std::cout << "\n";
            printMap(std::any_cast<std::map<std::string, std::any>>(value), indent + 1);
        } else if (value.type() == typeid(int)) {
            std::cout << std::any_cast<int>(value) << "\n";
        } else if (value.type() == typeid(int64_t)) {
            std::cout << std::any_cast<int64_t>(value) << "\n";
        } else if (value.type() == typeid(double)) {
            std::cout << std::any_cast<double>(value) << "\n";
        } else if (value.type() == typeid(std::string)) {
            std::cout << std::any_cast<std::string>(value) << "\n";
        } else if (value.type() == typeid(bool)) {
            auto val = std::any_cast<bool>(value);
            if (val)
                std::cout << "true" << "\n";
            else
                std::cout << "false" << "\n";
        }
    }
}

// Utility function to print nested maps
void printAny(std::any& value, int indent) {

    if (value.type() == typeid(std::map<std::string, std::any>)) {
        std::cout << "\n";
        printMap(std::any_cast<std::map<std::string, std::any>>(value), indent + 1);
    } else if (value.type() == typeid(int)) {
        std::cout << std::any_cast<int>(value) << "\n";
    } else if (value.type() == typeid(int64_t)) {
        std::cout << std::any_cast<int64_t>(value) << "\n";
    } else if (value.type() == typeid(double)) {
        std::cout << std::any_cast<double>(value) << "\n";
    } else if (value.type() == typeid(std::string)) {
        std::cout << std::any_cast<std::string>(value) << "\n";
    } else if (value.type() == typeid(bool)) {
        auto val = std::any_cast<bool>(value);
        if (val)
            std::cout << "true" << "\n";
        else
            std::cout << "false" << "\n";
    } else {
            std::cout << "type unknown" << "\n";
    }
}

// this may be deprecated
void test_parse_message(const char* uri, const char* method, const char* body) {
    std::vector<std::tuple<std::string, std::string, std::string>> testCases = {
        {"/components/comp_sel_2440/fuse_monitoring", "set", "false"},
        {"/components/comp_sel_2440/fuse_monitoring", "set", "{\"value\":false}"},
        {"/components/comp_sel_2440", "set", "{\"fuse_monitoring\":false}"},
        {"/components/comp_sel_2440", "set", "{\"fuse_monitoring\": {\"value\":false}}"},
        
        {"/components/comp_sel_2440/voltage", "set", "1234"},
        {"/components/comp_sel_2440/voltage", "set", "{\"value\":1234}"},
        {"/components/comp_sel_2440", "set", "{\"voltage\":1234}"},
        {"/components/comp_sel_2440", "set", "{\"voltage\": {\"value\":1234}}"},

        {"/components/comp_sel_2440/status", "set", "\"running\""},
        {"/components/comp_sel_2440/status", "set", "{\"value\":\"running\"}"},
        {"/components/comp_sel_2440", "set", "{\"status\":\"running\"}"},
        {"/components/comp_sel_2440", "set", "{\"status\": {\"value\":\"running\"}}"},

        {"/components/comp_sel_2440", "set", "{\"voltage\":1234, \"fuse_monitoring\":false, \"status\":\"running\" }"},
        {"/components/comp_sel_2440", "set", "{\"voltage\":{\"value\":1234}, \"fuse_monitoring\":{\"value\":false}, \"status\":{\"value\":\"running\"}}"},
        {"/components/comp_sel_2440", "set", "{\"voltage\":{\"value\":1234, \"size\":2}, \"fuse_monitoring\":{\"value\":false}, \"status\":{\"value\":\"running\"}}"}
    };

    for (const auto& testCase : testCases) {
        std::string uri, method, body;
        std::tie(uri, method, body) = testCase;
        std::cout << "URI: " << uri << "\n";
        std::cout << "meth: " << method << "\n";
        std::cout << "Body: " << body << "\n";
        std::map<std::string, std::any> resultMap = parseMessage(uri, method, body);
        parseInputMessage(resultMap, uri, method, body);
        printMap(resultMap);
        std::cout << "--------------------\n";
    }
    if (uri && body)
    {
        std::map<std::string, std::any> resultMap = parseMessage(uri, method, body);
        printMap(resultMap);
        std::cout << "--------------------\n";

    }
    test_printMap();
}

// this tests the concept of merging maps 
// we may not use it since we can create the results vecs from FimsInput 
void test_merge_message(const char* uri, const char* method, const char* body) {
    std::vector<std::tuple<std::string, std::string, std::string>> testCases = {
        {"/components/comp_sel_2440/fuse_monitoring", "set", "false"},
        {"/components/comp_sel_2440/hvac_status", "set", "{\"value\":false}"},
        {"/components/comp_sel_2440", "set", "{\"active_power\":23456}"},
        {"/components/comp_sel_2440", "set", "{\"fuse_monitoring\": {\"value\":false}}"},
        
        {"/components/comp_sel_2440/voltage", "set", "1234"},
        {"/components/comp_sel_2440/max_voltage", "set", "{\"value\":1234}"},
        {"/components/comp_sel_2440", "set", "{\"avg_voltage\":1234}"},
        {"/components/comp_sel_2440", "set", "{\"min_voltage\": {\"value\":1234}}"},

        {"/components/comp_sel_2440/status", "set", "\"running\""},
        {"/components/comp_sel_2440/old_status", "set", "{\"value\":\"running\"}"},
        {"/components/comp_sel_2440", "set", "{\"new_status\":\"running\"}"},
        {"/components/comp_sel_2440", "set", "{\"any_status\": {\"value\":\"running\"}}"},

        {"/components/comp_sel_2440", "set", "{\"voltage\":1234, \"fuse_monitoring\":false, \"status\":\"running\" }"},
        {"/components/comp_sel_2440", "set", "{\"avg_voltage\":{\"value\":1234}, \"old_fuse_monitoring\":{\"value\":false}, \"new_status\":{\"value\":\"running\"}}"},
        {"/components/comp_sel_2440", "set", "{\"voltage\":{\"value\":1234, \"size\":2}, \"fuse_monitoring\":{\"value\":false}, \"status\":{\"value\":\"running\"}}"}
    };

    std::map<std::string, std::any> baseMap;
    for (const auto& testCase : testCases) {
        std::string uri, method, body;
        std::tie(uri, method, body) = testCase;
        std::cout << "URI: " << uri << "\n";
        std::cout << "meth: " << method << "\n";
        std::cout << "Body: " << body << "\n";
        std::map<std::string, std::any> tmpMap = parseMessage(uri, method, body);
        mergeSubMaps(baseMap, tmpMap);

    }
    if (uri && method && body)
    {
        std::map<std::string, std::any> tmpMap = parseMessage(uri, method, body);
        mergeSubMaps(baseMap, tmpMap);
        std::cout << "--------------------\n";
    }
    printMap(baseMap);
    spdlog::memory_buf_t buffer;

    mapToBuffer(baseMap, buffer);

    spdlog::info("{}", fmt::to_string(buffer));

}


int test_extract() {
    std::map<std::string, std::any> parsedMap;

    parsedMap = parseMessage("/components/comp_sel_2440/fuse_monitoring", "set", R"(false)");
    // Add code to display the map, merge with other parsed messages, etc.

    return 0;
}



// may be deprecated 

std::any parseValue(const std::string& mystr) {
    simdjson::dom::parser parser;
    auto result = parser.parse(mystr);
    
    if (result.error()) {
        std::cerr << simdjson::error_message(result.error()) << std::endl;
        return std::any(); // Return an empty any object
    }
    
    simdjson::dom::element obj = result.value();

    // Check if it's a simple type: string, number or boolean
    if (obj.is_string()) {
        return std::any(std::string(obj));
    } else if (obj.is_int64()) {
        return std::any(int64_t(obj));
    } else if (obj.is_double()) {
        return std::any(double(obj));
    } else if (obj.is_bool()) {
        return std::any(bool(obj));
    }

    // Check if it's an object with a "value" key
    if (obj.is_object()) {
        simdjson::dom::element value_obj;
        if (!obj["value"].get(value_obj)) {
            if (value_obj.is_string()) {
                return std::any(std::string(value_obj));
            } else if (value_obj.is_int64()) {
                return std::any(int64_t(value_obj));
            } else if (value_obj.is_double()) {
                return std::any(double(value_obj));
            } else if (value_obj.is_bool()) {
                return std::any(bool(value_obj));
            }
        }
    }

    return std::any(); // Return an empty any object for unsupported types
}


int test_ParseValue() {
    std::string str1 = R"({"value": "Hello"})";
    std::string str2 = R"(42)";
    std::string str3 = R"( true)";
    std::string str4 = R"({"value": 23.5})";

    std::any value1 = parseValue(str1);
    std::any value2 = parseValue(str2);
    std::any value3 = parseValue(str3);
    std::any value4 = parseValue(str4);

    // You can use std::any_cast to extract and use the values
    if (value1.has_value()) {
        std::cout << "Value1: " << std::any_cast<std::string>(value1) << std::endl;
    }

    if (value2.has_value()) {
        std::cout << "Value2: " << std::any_cast<int64_t>(value2) << std::endl;
    }

    if (value3.has_value()) {
        std::cout << "Value3: " << std::any_cast<bool>(value3) << std::endl;
    }

    if (value4.has_value()) {
        std::cout << "Value4: " << std::any_cast<double>(value4) << std::endl;
    }

    return 0;
}


// OK to release
std::any jsonToAny(simdjson::dom::element elem) {
    switch (elem.type()) {
        case simdjson::dom::element_type::INT64: {
            int64_t val;
            if (elem.get(val) == simdjson::SUCCESS) {
                if (val <= INT32_MAX && val >= INT32_MIN) {
                    return static_cast<int32_t>(val);
                } else {
                    return val;
                }
            }
            break;
        }
        case simdjson::dom::element_type::BOOL: {
            bool val;
            if (elem.get(val) == simdjson::SUCCESS) {
                return val;
            }
            break;
        }
        case simdjson::dom::element_type::UINT64: {
            uint64_t val;
            if (elem.get(val) == simdjson::SUCCESS) {
                if (val <= UINT32_MAX) {
                    return static_cast<uint32_t>(val);
                } else {
                    return val;
                }
            }
            break;
        }
        case simdjson::dom::element_type::DOUBLE: {
            double val;
            if (elem.get(val) == simdjson::SUCCESS) {
                return val;
            }
            break;
        }
        case simdjson::dom::element_type::STRING: {
            std::string_view sv;
            if (elem.get(sv) == simdjson::SUCCESS) {
                return std::string(sv);
            }
            break;
        }
        case simdjson::dom::element_type::ARRAY: {
            simdjson::dom::array arr = elem;
            std::vector<std::any> vec;
            for (simdjson::dom::element child : arr) {
                vec.push_back(jsonToAny(child));
            }
            return vec;
        }
        case simdjson::dom::element_type::OBJECT:
            return jsonToMap(elem);
        default:
            return std::any();  // Empty std::any
    }

    // If we reach here, something went wrong.
    std::cerr << "Error processing JSON element" << std::endl;
    return std::any(); // Return empty std::any
}

// std::any xxjsonToAny(const simdjson::dom::element& elem) {
//     switch (elem.type()) {
//         case simdjson::dom::element_type::OBJECT: {
//             std::map<std::string, std::any> obj;
//             for (const auto& [key, value] : elem.get_object()) {
//                 obj[key] = xxjsonToAny(value);
//             }
//             return obj;
//         }
//         case simdjson::dom::element_type::ARRAY: {
//             std::vector<std::any> arr;
//             for (const auto& value : elem.get_array()) {
//                 arr.push_back(xxjsonToAny(value));
//             }
//             return arr;
//         }
//         case simdjson::dom::element_type::STRING:
//             return std::string(elem.get_string());
//         case simdjson::dom::element_type::INT64:
//             return int64_t(elem.get_int64());
//         case simdjson::dom::element_type::UINT64:
//             return uint64_t(elem.get_uint64());
//         case simdjson::dom::element_type::DOUBLE:
//             return double(elem.get_double());
//         case simdjson::dom::element_type::BOOL:
//             return bool(elem.get_bool());
//         default:
//             return std::any(); // or handle null and unknown types as you prefer
//     }
// }

// OK to release
std::map<std::string, std::any> jsonToMap(simdjson::dom::object obj) {
    std::map<std::string, std::any> m;

    for (auto [key, value] : obj) {
        m[std::string(key)] = jsonToAny(value);
    }

    return m;
}


// utility may not be used now
// deprecated
template <typename T>
std::optional<T> getMapFeature(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;  // Return an empty optional if the type is wrong
        }
    }
    return std::nullopt;  // Return an empty optional if the key is not found
}
//auto connectionName = getMapFeature<std::string>(data, "connection.name");
//auto maxNumConnections = getMapFeature<int32_t>(data, "connection.max_num_connections");
// deprecated
std::string getMapFeatureType(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        const std::type_info& type = it->second.type();
        return type.name();
    }
    return "Unknown"; // or any other indication for non-existing keys
}

// deprecated
std::optional<std::vector<std::any>> getMapArrayFeature(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        try {
            return std::any_cast<std::vector<std::any>>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;  // Return an empty optional if the type is wrong
        }
    }
    return std::nullopt;  // Return an empty optional if the key is not found
}
//deprecated
std::optional<std::any> getMapArrayFeatureItem(const std::map<std::string, std::any>& dataMap, const std::string& key, size_t index) {
    auto arrayOpt = getMapArrayFeature(dataMap, key);
    if (arrayOpt.has_value() && index < arrayOpt.value().size()) {
        return arrayOpt.value()[index];
    }
    return std::nullopt;  // Return an empty optional if the item doesn't exist
}

//deprecated
// auto registers = getMapArrayFeature(data, "components.registers");
size_t getMapArrayFeatureSize(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto arrayOpt = getMapArrayFeature(dataMap, key);
    if (arrayOpt.has_value()) {
        return arrayOpt.value().size();
    }
    return 0;  // Return 0 if the array feature doesn't exist
}


// #include <cstdint> // For uint8_t
// #include <any>
// #include <map>
// #include <iostream>
// #include <string>
// #include <simdjson.h>

// using u8 = uint8_t;

// // Function to convert JSON to map (assuming you have this implemented)
// std::map<std::string, std::any> jsonToMap(const simdjson::dom::element& elem);


template<typename T>
bool testAnyVal(const std::any& anyVal, const T& defaultValue) {
    return anyVal.type() == typeid(T);
}


template<typename T>
T getAnyVal(const std::any& anyVal, const T& defaultValue) {
    try {
        return std::any_cast<T>(anyVal);
    } catch (const std::bad_any_cast&) {
        return defaultValue;
    }
}


// parses the body of an incoming fims message
// this could be raw data or some kind of json object
int gcom_parse_data(std::any &gcom_map, const char* data, size_t length, bool debug) {
    simdjson::dom::parser parser;
    
    // Convert u8* data to padded_string
    simdjson::padded_string padded_content(reinterpret_cast<const char*>(data), length);

    // if(debug)
    // {
    //     std::cout << " \"parse_data\": {" << std::endl;
    //     std::cout << "                 \"Data length\": " << length << std::endl;
    //     std::cout << "} " << std::endl;
    // }

    // Parse
    auto result = parser.parse(padded_content);
    if (result.error()) {
        std::cout << "parser error ....\n";
        std::cerr << simdjson::error_message(result.error()) << std::endl;
        return -1;
    }
    std::cout << "parser result OK ....\n";

    // Convert JSON to any
    //gcom_map = jsonToMap(result.value());
    gcom_map = jsonToAny(result.value());
    // Utility function to print nested maps
    printAny(gcom_map);
    if (gcom_map.type() == typeid(std::map<std::string, std::any>)) {
        std::cout << "parser result  map ....\n";
        auto mkey = std::any_cast<std::map<std::string, std::any>>(gcom_map);
        for (auto key : mkey) {
            std::cout << " key " << key.first << "\n";
                    //TODO create the IO_Work items
        }        

    }
    
    return 0;
}


// OK to release
// reads a file into the m structure
// used in the main code to decode the config file

int gcom_parse_file(std::map<std::string, std::any> &gcom_map, const char *filename, bool debug) {
    simdjson::dom::parser parser;

    // Open file
    std::ifstream file(filename, std::ios::in | std::ios::binary); 
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return -1;
    }

    // Read content into padded_string
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    simdjson::padded_string padded_content = simdjson::padded_string(content);

    if(debug)
    {
        std::cout << " \"parse_file\": {" << std::endl;
        std::cout << "                 \"File name\": \"" << filename << "\"," << std::endl;
        std::cout << "                 \"File length\": " << padded_content.size() << std::endl;
        std::cout << "} " << std::endl;
    }

    // Parse
    auto result = parser.parse(padded_content);
    if (result.error()) {
        std::cerr << simdjson::error_message(result.error()) << std::endl;
        return -1;
    }

    // Convert JSON to map
    gcom_map = jsonToMap(result.value());
    
    return 0;
}

// debug routine 
std::string gcom_show_FirstLevel(const std::map<std::string, std::any>& m, std::string key) {
    std::ostringstream oss;

    oss << "\""<<key<<"\" :{\n";

    bool isFirstItem = true;
    for (const auto& [key, value] : m) {
        if (!isFirstItem) {
            oss << ",\n";
        }
        oss << "   \"" << key << "\": ";

        if (value.type() == typeid(int)) {
            oss << std::any_cast<int>(value);
        } else if (value.type() == typeid(int64_t)) {
            oss << std::any_cast<int64_t>(value);
        } else if (value.type() == typeid(double)) {
            oss << std::any_cast<double>(value);
        } else if (value.type() == typeid(std::string)) {
            oss << "\"" << std::any_cast<std::string>(value) << "\"";  // Quoted because it's a string in JSON
        } else {
            // Unknown types will not be added to the JSON to maintain valid format
            continue;
        }

        isFirstItem = false;
    }

    oss << "\n}\n";
    std::cout << oss.str();

    return oss.str();
}


// deprecated
std::optional<int> getMapInt1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(int)) {
        return std::any_cast<int>(*current);
    } 

    return std::nullopt;
}
// deprecated
std::optional<int> getMapInt(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(int) && !std::getline(ss, key, '.')) {
            return std::any_cast<int>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

// deprecated
std::optional<bool> getMapBool(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(bool) && !std::getline(ss, key, '.')) {
            return std::any_cast<bool>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

//deprecated
std::optional<std::map<std::string, std::any>> getMapObj1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(std::map<std::string, std::any>)) {
        return std::any_cast<std::map<std::string, std::any>>(*current);
    } 

    return std::nullopt;
}
//deprecated
std::optional<std::map<std::string,std::any>> getMapObj(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::map<std::string, std::any>) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::map<std::string,std::any>>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}
//deprecated
std::optional<std::vector<std::any>> getMapArray1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(std::vector<std::any>)) {
        return std::any_cast<std::vector<std::any>>(*current);
    } 

    return std::nullopt;
}

//deprecated
std::optional<std::vector<std::any>> getMapArray(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::vector<std::any>) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::vector<std::any>>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

//deprecated
std::optional<std::string> getMapString(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::string) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::string>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

// deprecated
bool getMapType(const std::map<std::string, std::any>& m, const std::string& query, const std::type_info& targetType) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return false;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else {
            return it->second.type() == targetType;
        }
    }

    return false;
}

int some_test (std::map<std::string, std::any> &m)
{
    // Now m contains the JSON data
    for (auto mx : m) {
    std::cout << " Key " << mx.first << std::endl;
    try {
        auto mymap = std::any_cast<std::map<std::string, std::any>>(mx.second);
        for (auto my : mymap) {
            std::cout << "     Item " << my.first << std::endl;
        }
        } catch (const std::bad_any_cast&) {
            std::cout << "     Value cannot be cast to map<string, any>" << std::endl;
        }
    }

    // for (auto mx : m) {
    //     std::cout << " Key " << mx.first << std::endl;
    //     for (auto my : mx.second) {
    //         std::cout << "     Item " << my.first << std::endl;
    //     }
    // }
    spdlog::info("Map content: {}", mapToString(m));
    
    return 0;
}

// TODO early test code see if we still use this.

// map of type names / offsets to actual map component
std::map<std::string,std::map<int,struct type_map>>types;

// map of comp names /  ids  to actual map components
std::map<std::string,std::map<std::string,struct type_map>>comps;

// #include <iostream>
// #include <typeinfo>
// #include <cxxabi.h>
// #include <memory>

// std::string demangle(const char* mangledName) {
//     int status;
//     std::unique_ptr<char, void (*)(void*)> result(
//         abi::__cxa_demangle(mangledName, nullptr, nullptr, &status),
//         std::free
//     );

//     if (status == 0) {
//         return result.get();
//     } else {
//         return mangledName; // Demangling failed, return the mangled name
//     }
// }

/// @brief 
///    extract the bistring structure into two vectors from the std::any object 
/// @param map 
/// @param rawStringData 
void extract_bitstrings(std::shared_ptr<cfg::map_struct> map, std::any& rawStringData) {
    // Ensure map is not nullptr
    if (!map) {
        throw std::runtime_error("Map pointer is null!");
    }

    map->bits_known = 0;
    map->bits_unknown = 0;

    if (rawStringData.type() == typeid(std::vector<std::any>)) {
        std::vector<std::any> rawDataList = std::any_cast<std::vector<std::any>>(rawStringData);
        auto bit_num  = 0; 
        for (const std::any& rawData : rawDataList) {
            // Handle simple string
            if (rawData.type() == typeid(std::string)) {
                std::string stringData = std::any_cast<std::string>(rawData);
                map->bit_str.push_back(stringData);
                map->bit_str_num.push_back(bit_num);
            } 
            // Handle object with "value" and "string"
            else if (rawData.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> rawMap = std::any_cast<std::map<std::string, std::any>>(rawData);
                if (rawMap.find("string") != rawMap.end() && rawMap["string"].type() == typeid(std::string)) {
                    map->bit_str.push_back(std::any_cast<std::string>(rawMap["string"]));
                }
                if (rawMap.find("value") != rawMap.end() && rawMap["value"].type() == typeid(int)) {
                    bit_num = std::any_cast<int>(rawMap["value"]);
                    map->bit_str_num.push_back(bit_num);
                }
                else
                {
                    map->bit_str_num.push_back(bit_num);
                }
            }
            else if (rawData.type() == typeid(void)) {
                    map->bit_str.push_back("");
                    map->bit_str_num.push_back(bit_num);
                    map->bits_unknown |= 1<< bit_num;
            }
            else
            {
                std::string err = map->id;
                err += ": unknown bitstring type :";                
                err += anyTypeString(rawData);                
                throw std::runtime_error(err);
            }
            map->bits_known |= 1<< bit_num;
            bit_num++;
        }
    }

    return;
}


// Example usage
bool gcom_test_bit_str() {
    auto my_map = std::make_shared<cfg::map_struct>();

    std::vector<std::any> rawStringData = {
        std::string("Stopped"),
        std::map<std::string, std::any>{{"value", 1}, {"string", std::string("Running")}},
        std::map<std::string, std::any>{{"value", 4}, {"string", std::string("Paused")}},
        std::string("Fault"),
    };

    std::any data = rawStringData;
    extract_bitstrings(my_map, data); // Ensure extract_bitstrings is modified to accept shared_ptr

    std::cout <<"Bit Strings " << std::endl;
    for (const auto& s : my_map->bit_str) { // Note the use of -> instead of . for accessing members
        std::cout << s << std::endl;
    }

    // std::cout <<"Bit Numbers " << std::endl;
    // for (const auto& s : my_map->bit_num) { // Note the use of -> instead of . for accessing members
    //     std::cout << s << std::endl;
    // }
    
    std::cout <<"Bits Known "  
                << "0x"
                << std::setfill('0') 
                << std::setw(8) 
                << std::hex << my_map->bits_known <<std::endl; // Note the use of -> instead of . for accessing members
    std::cout <<"Bits Unknown "  
                << "0x"
                << std::setfill('0') 
                << std::setw(8) 
                << std::hex << my_map->bits_unknown <<std::endl; // Note the use of -> instead of . for accessing members

    return true;
}

// bool gcom_test_bit_str() {
//     cfg::map_struct my_map;

//     std::vector<std::any> rawStringData = {
//         std::string("Stopped"),
//         std::map<std::string, std::any>{{"value", 1}, {"string", std::string("Running")}},
//         std::map<std::string, std::any>{{"value", 4}, {"string", std::string("Paused")}},
//         std::string("Fault"),
//     };

//     std::any data = rawStringData;
//     extract_bitstrings(my_map, data);

//     std::cout <<"Bit Strings " << std::endl;
//     for (const auto& s : my_map.bit_str) {
//         std::cout << s << std::endl;
//     }

//     std::cout <<"Bit Numbers " << std::endl;
//     for (const auto& s : my_map.bit_num) {
//         std::cout << s << std::endl;
//     }
    
//     std::cout <<"Bits Known "  
//                 << "0x"
//                 << std::setfill('0') 
//                 << std::setw(8) 
//                 << std::hex << my_map.bits_known <<std::endl;

//     return true;
// }





// std::vector<std::string> extract_bitstrings(struct cfg::map_struct* map,  std::any& rawStringData) {
//     std::vector<std::string> bit_str;
//     // did we get a vector of strings
//     if (rawStringData.type() == typeid(std::vector<std::any>)) {
//                 std::vector<std::string> rawStrings = std::any_cast<std::vector<std::string>>(rawStringData);
//         for (const std::any& rawString : rawStrings) {
//             if (rawString.type() == typeid(std::string)) {
//                 std::string stringData = std::any_cast<std::string>(rawString);
//                 map->bit_str.push_back(stringData);
//             }
//         }
//     }
//     return map->bit_str;
// }

// now we have myCfg , was we find each item or data point we can build up the maps for 
// /comp/uri:point ->&map ( dataPoint)
//and
// <device_id><type><offset> -> &map (dataPoint)
//  
std::vector<std::string> any_split(std::string_view str, char delimiter) {
    std::vector<std::string> result;
    size_t pos = 0;
    size_t next_pos;
    while ((next_pos = str.find(delimiter, pos)) != std::string_view::npos) {
        result.emplace_back(str.substr(pos, next_pos - pos));
        pos = next_pos + 1; // Move past the delimiter
    }
    result.emplace_back(str.substr(pos)); // Add the last token
    return result;
}

bool extract_maps(std::shared_ptr<struct cfg::reg_struct>reg, std::shared_ptr<struct cfg::map_struct>packed, std::any& rawMapData, struct cfg& myCfg, bool debug = false) {
    // did we get a vector of maps 
    if (rawMapData.type() == typeid(std::vector<std::any>)) {
        std::vector<std::any> rawMaps = std::any_cast<std::vector<std::any>>(rawMapData);
        for (const std::any& rawMap : rawMaps) {
            if (rawMap.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> mapData = std::any_cast<std::map<std::string, std::any>>(rawMap);
                //struct cfg::map_struct map;
                auto map = std::make_shared<cfg::map_struct>();
                map->reg_type = reg->reg_type;
                //map.reg_type = reg->reg_type;

        //int starting_bit_pos;
        //int shift;
        //double scale; 
        //bool normal_set;  
        //bool is_float;  
                double scale = 0.0;
                map->scale = scale;
                map->rtype = reg->type;
                auto pmap = map;
                if(packed)
                    pmap = packed;

                getItemFromMap(mapData, "id",                map->id,                 std::string("Some_id"),       true, true, debug);
                getItemFromMap(mapData, "name",              map->name,               std::string("Some_name"),     true, true, debug);
                getItemFromMap(mapData, "offset",            map->offset,             pmap->offset,                 true, true, debug);
                getItemFromMap(mapData, "size",              map->size,               pmap->size,                   true, true, debug);
                getItemFromMap(mapData, "shift",             map->shift,              pmap->shift,                  true, true, debug);
                getItemFromMap(mapData, "starting_bit_pos",  map->starting_bit_pos,   pmap->starting_bit_pos,       true, true, debug);
                getItemFromMap(mapData, "number_of_bits",    map->number_of_bits,     pmap->number_of_bits,         true, true, debug);
                getItemFromMap(mapData, "scale",             map->scale,              pmap->scale,                  true, true, debug);
                getItemFromMap(mapData, "normal_set",        map->normal_set,         pmap->normal_set,             true, true, debug);
                getItemFromMap(mapData, "signed",            map->is_signed,          pmap->is_signed,              true, true, debug);
                map->invert_mask = 0;
                map->care_mask = std::numeric_limits<u64>::max();
                std::string imask("0x0");
                std::string dimask("0x0");
                map->invert_mask = 0;
                map->invert_mask = pmap->invert_mask;

                getItemFromMap(mapData, "invert_mask",       imask,        dimask,            true, true, debug);
                if (imask != "0x0") 
                {
                    uint64_t invmask = 0;
                    const char* istr = imask.c_str();
                    if ((istr[0] == '0') && (istr[1] == 'x')) {
                        istr+=2;
                        invmask = strtoull(istr, NULL, 16);
                    }
                    else if ((imask[0] == '0') && (imask[1] == 'b')) {
                        istr+=2;
                        invmask = strtoull(istr, NULL, 2);
                    }
                    map->invert_mask = invmask;
                }

                // Extract map details (like id, offset, name, etc.) here...
                // Add to the maps vector...
                map->is_float = false;
                map->is_word_swap = false;
                map->is_byte_swap = false;
                
                getItemFromMap(mapData, "float",             map->is_float,            pmap->is_float,                         true, true, debug);
                getItemFromMap(mapData, "word_swap",         map->is_word_swap,        pmap->is_word_swap,                     true, true, debug);
                getItemFromMap(mapData, "byte_swap",         map->is_byte_swap,        pmap->is_byte_swap,                     true, true, debug);
                //getItemFromMap(mapData, "float64",           map->is_float64,          false,                         true, true, debug);

                map->is_enum = false;
                map->is_random_enum = false;
                map->is_individual_bits = false;
                map->is_bit_field = false;
                getItemFromMap(mapData, "enum",              map->is_enum,             pmap->is_enum,                         true, true, debug);
                getItemFromMap(mapData, "random_enum",       map->is_random_enum,      pmap->is_random_enum,                         true, true, debug);
                getItemFromMap(mapData, "individual_bits",   map->is_individual_bits,  pmap->is_individual_bits,                         true, true, debug);
                getItemFromMap(mapData, "bit_field",         map->is_bit_field,        map->is_bit_field,                         true, true, debug);

                double dval  = 0.0;
                map->debounce = dval;
                map->deadband = dval;
                map->use_bool = false;
                getItemFromMap(mapData, "debounce",             map->debounce,         pmap->debounce,                         true, true, debug);
                getItemFromMap(mapData, "deadband",             map->deadband,         pmap->deadband,                         true, true, debug);
                if (map->debounce > 0.0) map->use_debounce = true;
                if (map->deadband > 0.0) map->use_deadband = true;
                getItemFromMap(mapData, "use_bool",             map->use_bool,          pmap->use_bool,                         true, true, debug);


                if ((map->is_enum) 
                        || (map->is_random_enum) 
                        || (map->is_individual_bits) 
                        || (map->is_bit_field) 
                        /* maybe add more here */)
                {
                    extract_bitstrings(map, mapData["bit_strings"]);
                }

                map->offtime = 0;
                map->forced_val = 0;
                map->raw_val = 0;
                map->device_id = reg->device_id;
                map->packer = packed;
                if(!packed)
                {
                    reg->maps.push_back(map);
                    auto& mymap = reg->maps.back();
                    //auto regshr = mymap->reg.lock();
                    reg->mapix[mymap->offset] = mymap;
                } 
                else
                {
                    packed->bit_ranges.push_back(map);
                }

                // we only want the root registers in the main dict 
                //if(!packed)
                    addMapItem(myCfg.itemMap, reg->comp_id.c_str(), reg->id.c_str(), map);
                //addMapId(myCfg.idMap, device_id, map.reg_type, mymap);

                getItemFromMap(mapData, "packed_register",         map->packed_register,        false,                         true, true, debug);

                if (map->packed_register && !packed)
                {
                    extract_maps(reg, map, mapData["bit_ranges"], myCfg);
                }
                if(true||debug)printf(" >>>>>>>>>>>>> <%s> comp [%s] reg [%s] id [%s] packed [%d] \n"
                                , __func__
                                //, (void*)reg.get()
                                //, (void *) mymap.get()
                                , reg->comp_id.c_str()
                                , reg->id.c_str()
                                , map->id.c_str()
                                , (int)map->packed_register
                                // , (void *)regshr.get()
                                // , (void *)compshr.get()
                                );
                // pull out 

            }
        }
    }

    return true;
}

bool extract_registers(std::vector<std::shared_ptr<cfg::reg_struct>> &registers, const std::any& rawRegisterData,
              std::shared_ptr<struct cfg::comp_struct>item, struct cfg& myCfg, bool debug = false) {
    //std::vector<cfg::reg_struct> registers;
    
    bool Ok = true;

//    std::shared_ptr<cfg::comp_struct> itemsp(item);
//    // ... initialize item ...

    if (rawRegisterData.type() == typeid(std::vector<std::any>)) {
        std::vector<std::any> rawRegisters = std::any_cast<std::vector<std::any>>(rawRegisterData);
        for (const std::any& rawRegister : rawRegisters) {
            if (rawRegister.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> regData = std::any_cast<std::map<std::string, std::any>>(rawRegister);
                //struct cfg::reg_struct reg;
                auto reg = std::make_shared<cfg::reg_struct>();

                reg->id = item->id;
                reg->comp_id = item->comp_id;
                if(debug)printf(" >>>>>>>>>>>>> <%s> comp %p  initial reg %p \n", __func__, (void*)item.get(), (void *) reg.get());

                Ok &= getItemFromMapOk(regData, "type",                reg->type,                 std::string("type"),               true, true, debug);


                // now we get modbus specific
                reg->reg_type = myCfg.typeFromStr(reg->type);

                Ok &= getItemFromMapOk(regData, "starting_offset",     reg->starting_offset,      0,                                 true, true, debug);
                Ok &= getItemFromMapOk(regData, "number_of_registers", reg->number_of_registers,  0,                                 true, true, debug);
                Ok &= getItemFromMapOk(regData, "device_id",           reg->device_id,            item->device_id,        true, true, debug);
                Ok &= getItemFromMapOk(regData, "byte_swap",           reg->is_byte_swap,         item->is_byte_swap,                true, true, debug);
                Ok &= getItemFromMapOk(regData, "word_swap",           reg->is_word_swap,         item->is_word_swap,                true, true, debug);

                // Extract register details (like type, starting_offset, etc.) here...
                // Call extract_maps for each register to extract map details...
                if (Ok) {
                    //reg.maps = extract_maps(&reg, regData["map"], myCfg);
                    // Add register details + extracted maps to the registers vector...
                    registers.push_back(reg);
                    //registers.emplace_back(reg);
                    auto& myreg = registers.back();
                    //myreg->comp = item;
                    extract_maps(myreg, nullptr, regData["map"], myCfg);

                    if(true|| debug)
                        printf(" >>>>>>>>>>>>> <%s> comp %p  myreg %p  device_id %d \n"
                                , __func__
                                , (void*)item.get()
                                , (void *) &myreg
                                , myreg->device_id
                                //, (void *)myreg->comp.get()
                                );
                    //myreg.comp = item;
                }
            }
        }
    }

    return Ok;
}

//
// TODO debug

/// @brief 
/// @param gcom_map 
/// @param query 
/// @param myCfg 
/// @param debug 
/// @return 
bool extract_connection(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug = false) { 
    bool ok = true;

    // auto optArrayData = getMapValue<std::vector<std::any>>(gcom_map, query);

    // if (optArrayData.has_value()) {
    //     if(debug) std::cout << " Found connection " << std::endl;
    // } else {
    //     std::cout << " Could not find connection " << std::endl;
    //     return false;
    // }

    if(!getItemFromMap(gcom_map, "connection.port",                 myCfg.connection.port,       502,                      true,true,false)) ok = false;
    if(!getItemFromMap(gcom_map, "connection.ip_address",           myCfg.connection.ip_address, std::string("172.3.0.2"), true,true,false)) ok = false;
    if(!getItemFromMap(gcom_map, "connection.debug",                myCfg.connection.debug,      false,                    true,true,false)) ok = false;
    if(!getItemFromMap(gcom_map, "connection.device_id",            myCfg.connection.device_id,  1,                        true,true,false)) ok = false;
    if(!getItemFromMap(gcom_map, "connection.connection_timeout",   myCfg.connection.connection_timeout,  2,               true,true,false)) ok = false;
    if(!getItemFromMap(gcom_map, "connection.max_num_connections",  myCfg.connection.max_num_connections, 1,              true,true,false)) ok = false;
    if (ok)
    {
        if (myCfg.connection.connection_timeout < 2 || myCfg.connection.connection_timeout > 10)
        {
            // TODO error log
            myCfg.connection.connection_timeout = 2;
        }
    }
    if(true|| debug) {
            printf(" >>>>>>>>>>>>> <%s>  device_id %d\n"
                                , __func__
                                , myCfg.connection.device_id
                                // , (void*)item.get()
                                // , (void *) &myreg
                                // , myreg->device_id
                                //, (void *)myreg->comp.get()
                                ); 
    }
    return ok;
}


// this is the big unpacker
//  components
//     ->  registers (extract_registers)
//           -> maps (extract_maps)
//                -> 

/// @brief extract the componenents
/// @param gcom_map std::any interface map
/// @param query query string to find components
/// @param myCfg cfg to place result
/// @param debug debug true/false
/// @return 
bool extract_components(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug = false) { 

    bool Ok = true; 
    auto compArray = getMapValue<std::vector<std::any>>(gcom_map, query);

    if (compArray.has_value()) {
        if(debug) std::cout << " Found components " << std::endl;
    } else {
        std::cout << " Could not find components " << std::endl;
        return false;
    }
    std::vector<std::any> rawCompArray = compArray.value();

    for(const std::any& rawComp : rawCompArray) {
            if(debug) std::cout << " Processing  item"<< std::endl;
            if (rawComp.type() == typeid(std::map<std::string, std::any>)) {
                std::map<std::string, std::any> regMap = std::any_cast<std::map<std::string, std::any>>(rawComp);
                //struct cfg::comp_struct comp;
                auto comp = std::make_shared<cfg::comp_struct>();

                std::string componentId;
                getItemFromMap(regMap, "component_id", componentId, std::string("components"), true, true, false).value();
                comp->comp_id = componentId;
                Ok &= getItemFromMapOk(regMap, "id",           comp->id,        std::string("noId"),                      true, true, false);
                Ok &= getItemFromMapOk(regMap, "device_id",    comp->device_id,    myCfg.connection.device_id,            true, true, false);
                Ok &= getItemFromMapOk(regMap, "frequency",    comp->frequency,    100,                                   true, true, false);
                Ok &= getItemFromMapOk(regMap, "offset_time",  comp->offset_time,  0,                                     true, true,false);
                Ok &= getItemFromMapOk(regMap, "byte_swap",    comp->is_byte_swap,         false,                         true, true,debug);
                Ok &= getItemFromMapOk(regMap, "word_swap",    comp->is_word_swap,         false,                         true, true,debug);

                auto regIter = regMap.find("registers");
                if (regIter != regMap.end()) {
                    if(debug)std::cout << " Processing  registers"<< std::endl;
                    //item.registers =
                    Ok &= extract_registers(comp->registers, regIter->second, comp, myCfg);
                }
                else 
                { 
                    Ok = false;
                }
                if(Ok)
                    myCfg.components.push_back(comp);
            }
    }
    if(true|| debug) {
            printf(" >>>>>>>>>>>>> <%s> done \n"
                                , __func__
                                // , (void*)item.get()
                                // , (void *) &myreg
                                // , myreg->device_id
                                //, (void *)myreg->comp.get()
                                );
        for ( auto &myComp : myCfg.itemMap) 
        {
            printf(" >>>>>>>>>>>>> <%s> comp >> <%s> \n"
                                , __func__
                                , myComp.first.c_str()
                                );

        }
    }

    return Ok;
}

// map of typenames / offsets to actual map component
//std::map<std::string,std::map<int,struct type_map>>types;

/// @brief extract the different types in a config 
/// @param myCfg 
void gcom_extract_types(struct cfg& myCfg)
{
    for (auto& rawComp : myCfg.components) {
        for (auto& rawReg : rawComp->registers) {
            for (auto& rawMap : rawReg->maps) {
                struct type_map tmap;
                tmap.map = rawMap;
                tmap.reg = rawReg;
                tmap.comp = rawComp;

                types[rawReg->type][rawMap->offset] = tmap;
            }
        }
    }
}

// void gcom_extract_types(struct cfg& myCfg)
// {

//      for (const struct cfg::comp_struct& rawComp : myCfg.components) {
//         for (const struct cfg::reg_struct& rawReg : rawComp.registers) {
//             for (const struct cfg::map_struct& rawMap : rawReg.maps) {
//                 struct type_map tmap;
//                 tmap.map = &rawMap;
//                 tmap.reg = &rawReg;
//                 tmap.comp = &rawComp;

//                 types[rawReg.type][rawMap.offset] = tmap;
//             }
//         }
//      }
// }

/// @brief show the types found in a cfg
/// @param myCfg 
/// @return 
std::string gcom_show_types(struct cfg& myCfg) {
    std::ostringstream oss;

    oss << "\"Types found\": {\n";

    bool firstTypeItem = true;
    for (const auto& type_item : types) {
        if (!firstTypeItem) {
            oss << ",\n";
        }

        oss << "      \"" << type_item.first << "\": {\n";
        bool firstTypeReg = true;
        for (const auto& type_reg : type_item.second) {
            if (!firstTypeReg) {
                oss << ",\n";
            }
            oss << "         \"" << type_reg.first << "\": {";  // If the type_map has any properties, add them here
            oss << "\"map_id\": \"" << type_reg.second.map->id << "\"";  // Adding the map_id property
            oss << "}";
            firstTypeReg = false;
        }
        oss << "\n      }";
        firstTypeItem = false;
    }
    oss << "\n}";

    std::cout << oss.str() << "\n";

    return oss.str();
}

// std::string gcom_show_types(struct cfg& myCfg) {
//     std::ostringstream oss;

//     oss << "\"Types found\": {\n";

//     bool firstTypeItem = true;
//     for (const auto& type_item : types) {
//         if (!firstTypeItem) {
//             oss << ",\n";
//         }

//         oss << "      \"" << type_item.first << "\": {\n";
//         bool firstTypeReg = true;
//         for (const auto& type_reg : type_item.second) {
//             if (!firstTypeReg) {
//                 oss << ",\n";
//             }
//             oss << "         \"" << type_reg.first << "\": {";  // If the type_map has any properties, add them here
//             oss << "\"map_id\": \"" << type_reg.second.map->id << "\"";  // Adding the map_id property
//             oss << "}";
//             firstTypeReg = false;
//         }
//         oss << "\n      }";
//         firstTypeItem = false;
//     }
//     oss << "\n}";

//     std::cout << oss.str() << "\n";

//     return oss.str();
// }

///

// Given a device_id (TODO) a type and an offset find the type_map 
// we may not need this now

/// @brief Given a device_id (TODO) a type and an offset find the type_map 
/// @param type 
/// @param offset 
/// @param debug 
/// @return 
struct type_map* gcom_get_type(std::string type, int offset, bool debug=false) {
    if (types.find(type) != types.end()) {
        if (types[type].find(offset) != types[type].end()) {
            auto rtype = &types[type][offset];
            if (debug) {
                std::cout << "\"gcom_get_type\": {\n"
                          << "                   \"status\": \"success\",\n"
                          << "                   \"type\": \"" << type                   << "\",\n"
                          << "                   \"offset\": " << offset                 << ",\n "
                          << "                   \"comp_id\": \"" << rtype->comp->id     << "\",\n "
                          << "                   \"reg_type\": \"" << rtype->reg->type   << "\",\n"
                          << "                   \"map_id\": \"" << rtype->map->id       << "\",\n"
                          << "                   \"map_offset\": " << rtype->map->offset << "\n"
                          << "                   }" << std::endl;
            }
            return rtype;
        }
    }
    if (debug) {
        std::cout << "\"gcom_get_type\": {\n"
              << "                       \"status\": \"error\",\n"
              << "                       \"type\": \"" << type << "\",\n"
              << "                       \"offset\": " << offset << ",\n"
              << "                       \"message\": \"type '" << type << "' " << offset << " not found\""
              << "                       }" << std::endl;
    }

    return nullptr;
}

//std::map<std::string,std::map<std::string,struct type_map>>comps;
// no longer used
/// @brief  
/// @param myCfg 
void gcom_extract_comps(struct cfg& myCfg)
{

     for (auto& rawComp : myCfg.components) {
        for (auto rawReg : rawComp->registers) {
            for (auto rawMap : rawReg->maps) {
                struct type_map tmap;
                tmap.map = rawMap;
                tmap.reg = rawReg;
                tmap.comp = rawComp;

                comps[rawComp->id][rawMap->id] = tmap;
            }
        }
     }
}

//std::map<std::string,std::map<std::string,struct type_map>>comps;

// may need to revisit this now we have components in as a first layer
std::vector<std::string> subs;

void gcom_extract_subs(struct cfg& myCfg)
{

    for (auto& rawComp : myCfg.components) {
        subs.push_back(rawComp->id);
    }
}

std::string gcom_show_subs(struct cfg& myCfg, bool debug=false) {
    std::ostringstream oss;

    oss << "\"subs\": [";
    bool first = true;
    for (const auto& sub : subs) {
        if (!first)
            oss << ",\n         \"" << sub <<"\"";
        else 
            oss << "\n         \"" << sub <<"\"";
        first = false;
    }
    oss << "\n        ]\n";

    if(debug)std::cout << oss.str() << "\n";

    return oss.str();
}



// pub freq, offset , list of maps, each with a start and number, assume that the config has been corrected 
std::map<int,std::map<int,std::vector<std::shared_ptr<cfg::reg_struct>>>> pubs;

// may well be deprecated
void gcom_extract_pubs(struct cfg& myCfg)
{

    for (auto& rawComp : myCfg.components) {
         for (auto& rawReg : rawComp->registers) {
            if (pubs.find(rawComp->frequency) == pubs.end() || 
                pubs[rawComp->frequency].find(rawComp->offset_time) == pubs[rawComp->frequency].end()) {
                std::vector<std::shared_ptr<cfg::reg_struct>> rs;
                rawReg->device_id = rawComp->device_id;
                pubs[rawComp->frequency][rawComp->offset_time] = rs;
            }
            pubs[rawComp->frequency][rawComp->offset_time].push_back(rawReg);
        }
    }
}

template<typename Iterator>
std::string join(const std::string& separator, Iterator begin, Iterator end) {
    std::string result;
    if (begin != end) {
        result += *begin;
        ++begin;
    }
    while (begin != end) {
        result += separator + *begin;
        ++begin;
    }
    return result;
}



// may well be deprecated
std::string gcom_show_pubs(struct cfg& myCfg, bool debug = false) {
    std::ostringstream json;
    
    json << "{\n";

    std::vector<std::string> freqStrings;

    for (const auto& freqPair : pubs) {
        int freq = freqPair.first;
        std::ostringstream freqStream;

        freqStream << "\t\"" << freq << "\": {\n";

        std::vector<std::string> offsetStrings;

        for (const auto& offsetPair : freqPair.second) {
            int offset_time = offsetPair.first;
            std::ostringstream offsetStream;

            offsetStream << "\t\t\"" << offset_time << "\": {\n";

            std::vector<std::string> regStrings;

            for (auto regPtr : offsetPair.second) {
                std::ostringstream regStream;
                regStream << "\t\t\t\"" << regPtr->type << "\": [ [" << regPtr->device_id<< "," <<regPtr->starting_offset << ", " << regPtr->number_of_registers << "] ]";
                regStrings.push_back(regStream.str());
            }

            offsetStream << join(",\n", regStrings.begin(), regStrings.end());
            offsetStream << "\n\t\t}";

            offsetStrings.push_back(offsetStream.str());
        }

        freqStream << join(",\n", offsetStrings.begin(), offsetStrings.end());
        freqStream << "\n\t}";

        freqStrings.push_back(freqStream.str());
    }

    json << join(",\n", freqStrings.begin(), freqStrings.end());
    json << "\n}";

    if(debug) std::cout << "\"pubs\" : \n" << json.str() << "\n";

    return json.str();
}


// may well be deprecated
struct type_map* gcom_get_comp(struct cfg& myCfg, std::string comp, std::string id, bool debug=false) {
    std::ostringstream oss;

    // if (comps.find(comp) != comps.end()) {
    //     if (comps[comp].find(id) != comps[comp].end()) {
    //         if (debug) {
    //             oss << "\"gcom_get_comp\" :{\n"
    //                 << "                  \"status\": \"success\",\n"
    //                 << "                  \"comp\": \"" << comp << "\",\n"
    //                 << "                  \"id\": \"" << id << "\"\n"
    //                 << "                  }";
    //             std::cout << oss.str() << std::endl;
    //         }
    //         return &comps[comp][id];
    //     }
    // }

    if (debug) {
        oss.str("");  // Clear the stringstream for new data
        oss << "\"gcom_get_comp\" :{\n"
            << "                   \"status\": \"error\",\n"
            << "                   \"comp\": \"" << comp    << "\",\n"
            << "                   \"id\": \"" << id        << "\",\n"
            << "                   \"message\": \"comp '" << comp << "' with id '" << id << "' not found\"\n"
            << "                   }";
        std::cout << oss.str() << std::endl;
    }

    return nullptr;
}

// start of the encode / decode story
// encode a var from fims (any) map into a register
// must have at least 4 registers 
//struct cfg::comp_struct item;
//item.starting_bit_pos;
//item.shift;
//item.scale 
//item.normal_set  
//item.is_signed  
//item.invert_mask  
//item.is_float  
//item.is_float64  
//item.is_word_swapped  


// item.uses_masks  new
// item.invert_mask
// item.care_mask
// item.is_signed



// all todo

//item.is_bit_string_type  //todo
//item.is_individual_bits  //TODO
//item.bit_index
//item.is_bit_field
//item.bit_str std::vector<std::string>  ->num_bit_strs
//item.bit_str_known std::vector<bool>  ->num_bit_strs
//item.bit_str
//item.is_enum

// to string method for decoded_vals (for publishing/get purposes):
//template<Register_Types Reg_Type>
//static



bool gcom_config_test_uri(std::map<std::string, std::any>gcom_map, struct cfg& myCfg, const char* uri, const char* id)
{
    bool debug = false;
    extract_components(gcom_map,"components",myCfg);
    gcom_extract_comps(myCfg);
    std::cout << " seeking uri  :" << uri << " id :" << id << std::endl;
    //auto mytype = 
    gcom_get_comp(myCfg,  uri, id,  debug);
    //std::cout << " the type yields an id :" << mytype->map->id<<std::endl;

    return false;
}

// contains lots of early ( like last week) stuff may need to revisit

bool gcom_config_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg)
{
    gcom_show_FirstLevel(gcom_map, "connection");
    if (gcom_map.find("connection") != gcom_map.end())
    {
        auto& connectionAny = gcom_map["connection"];

        if (connectionAny.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> connectionMap = std::any_cast<std::map<std::string, std::any>>(connectionAny);
            //std::cout << " Connection ... "<< std::endl;
            gcom_show_FirstLevel(connectionMap, "types");
        }
    }
    
    bool ok = true; 
    bool debug = false;
    getItemFromMap(gcom_map, "connection.port",       myCfg.connection.port,       503,                      true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.ip_address", myCfg.connection.ip_address, std::string("172.3.0.2"), true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.debug",      myCfg.connection.debug,      false,                    true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.device_id",  myCfg.connection.device_id,  1,                        true,true, debug);

    extract_components(gcom_map,"components",myCfg);

    // now create a list of types and offsets
    // used when we get a register type and offset  
    gcom_extract_types(myCfg);
    gcom_show_types(myCfg);

    // test this list  
    //auto rtype = 
    gcom_get_type( "Discrete Inputs", 395,  debug);


    // now create a uri/id
    // map a type->offset to a structure with component, register, and a map
    gcom_extract_comps(myCfg);
    //auto ctype = 
    gcom_get_comp(myCfg,  "comp_sel_2440", "fuse_monitoring",  debug);

    // now create pub_lists frequency, offset , components  and pub_map
    // get subs
    gcom_extract_subs(myCfg);
    gcom_show_subs(myCfg,  debug);
    // get pubs
    gcom_extract_pubs(myCfg);
    gcom_show_pubs(myCfg,  debug);

    //
    ItemMap structure;
    extract_structure(structure, gcom_map);
    //ItemMap structure;
    //ItemMap structure = extract_structure(gcom_map);
    printItemMap(structure);

    ItemSharedPtr item = findItem(structure, "/components", "comp_sel_2440", "door_latch");
    if (item) {
        printItem(item);
        //std::cout << "Found item with ID: " << item->id << ", Name: " << item->name << std::endl;
     } else {
         std::cout << "Item not found!" << std::endl;
     }


    return ok;
}
//
// read a config
// send it a uri as from fims listen 
// see if you get a results vector
//
// now this is getting closer to the real thing.
bool gcom_msg_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg)
{
    // pull out the components into myCfg
    extract_components(gcom_map, "components", myCfg);

    // extract the ItemMap
    ItemMap gcom_cfg;
    extract_structure(gcom_cfg, gcom_map);

    std::vector<std::pair<ItemSharedPtr, std::any>> result;
    parseFimsMessage(myCfg, gcom_cfg, result, "set", "/components/comp_sel_2440",  "{\"door_latch\": false, \"fire_relay\":true,\"disconnect_switch\":true}");
    parseFimsMessage(myCfg, gcom_cfg, result, "set", "/components/comp_sel_2440",  "{\"heartbeat\":234}");

    auto res = result.size();
    std::cout << " result size :" << res << std::endl;
    std::cout << " result vector  >>>>:" << std::endl;
    printResultVector(result);

    std::vector<CompressedItem> compressedResult;
    compressResults(result, compressedResult); 

    //TODO encode results on a SET
    std::cout << " compressed vector  >>>>:" << std::endl;
    printCompressedResults(compressedResult);


    bool ok = true;
    return ok;
}

bool compareMapS(const std::shared_ptr<cfg::map_struct>& a, const std::shared_ptr<cfg::map_struct>& b) {
    // First, compare by reg_type
    if (a->reg_type < b->reg_type)
        return true;
    else if (a->reg_type > b->reg_type)
        return false;
    // ten by device_id
    if (a->device_id < b->device_id)
        return true;
    else if (a->device_id > b->device_id)
        return false;
    
    // If device_id is equal, compare by offset
    return a->offset < b->offset;
}


// TODO start with a blank result vector
void check_work_items( std::vector<std::shared_ptr<IO_Work>>& io_work_vec, std::vector<std::shared_ptr<cfg::map_struct>>& io_map_vec, struct cfg & myCfg, const char* oper,bool debug)
{
    // Sort the vector using the custom comparison function

    std::sort(io_map_vec.begin(), io_map_vec.end(), compareMapS);
    bool first = true;
    int offset = 0;
    int first_offset = 0;
    int device_id = 0;
    int num = 0;
    int isize = 0;
    auto items = io_map_vec;
    int max_item_size = 125;  // adjust for bit
    cfg::Register_Types reg_type;
    //io_work->items.clear();

    auto item = io_map_vec.at(0);
    auto io_work = make_work(item->reg_type, item->device_id
                                            , item->offset, 1, nullptr
                                            , nullptr, strToWorkType(oper, false));

    if(debug)
    {
        std::cout 
            << " After Sort .. "<< std::endl;
        for (auto item : items)
        {
            std::cout 
                << "            .. "
                << " Id " << item->id 
                << " device_id " << item->device_id 
                << " offset " << item->offset 
                << " size " << item->size 
                << std::endl;
        }    
    }
    for (auto item : items)
    {
        //std::cout << " >>>>>>>>>>>> state  #1 offset " << offset << " isize " << isize <<" item->offset " << item->offset << std::endl;
        if (first)
        {
            first = false;
            offset = item->offset;
            first_offset = item->offset;
            device_id = item->device_id;
            num = item->size;
            isize = item->size;
            reg_type = item->reg_type;

            io_work->items.emplace_back(item);

            if(debug)
                std::cout << " >>>>>>>>>>>> state  #1 offset " << offset 
                            << " isize " << isize 
                            << " item->offset " << item->offset 
                            << " item->size " << item->size 
                            << std::endl;
            //offset += item->size;   
        }
        else
        {
            std::cout << " >>>>>>>>>>>> state  #2 offset " << offset << " isize " << isize <<" item->offset " << item->offset << " item size " << item->size << std::endl;
            if(debug)
            {
                if (((offset + isize) != item->offset)) // || ((offset+item->size > max_item_size)) || (item->device_id != device_id) || (item->reg_type != reg_type))
                {
                    std::cout << " >>>>>>>>>>>> Break detected  #1 offset " << offset 
                                << " isize " << isize 
                                << " item->offset " << item->offset 
                                << std::endl;
                }
                if (((offset+ item->size - first_offset  > max_item_size))) // || (item->device_id != device_id) || (item->reg_type != reg_type))
                {
                    std::cout << " >>>>>>>>>>>> Break detected  #2 offset " << offset 
                            << " item->size " << item->size 
                            << " max_item_size" << max_item_size
                            << std::endl;
                }
                if ((item->device_id != device_id)) // || (item->reg_type != reg_type))
                {
                    std::cout << " >>>>>>>>>>>> Break detected  #3 (device_id)" 
                        << std::endl;
                }
                if ((item->reg_type != reg_type))
                {
                    std::cout << " >>>>>>>>>>>> Break detected  #4 (reg_type)" << std::endl;
                }
            }
            if (((offset + isize) != item->offset) 
                            || ((offset + item->size - first_offset > max_item_size)) 
                            || (item->device_id != device_id) 
                            || (item->reg_type != reg_type))
            {
                //std::cout << " >>>>>>>>>>>> Break detected " << std::endl;
                
                io_work->offset = first_offset;
                io_work->num_registers = num;
                io_work_vec.emplace_back(io_work);

                if(debug)std::cout << " >>>>>>>>>>>> At break; offset : "<< first_offset << " num : " << num << std::endl;
                io_work = make_work(item->reg_type, item->device_id
                                            , item->offset, 1
                                            , nullptr
                                            , nullptr
                                            , strToWorkType(oper, false)
                                            );

                offset = item->offset;
                first_offset = item->offset;
                device_id = item->device_id;
                num = item->size;
                isize = item->size;
                reg_type = item->reg_type;

                io_work->items.emplace_back(item);
            }
            else
            {
                offset += isize;
                isize = item->size;
                num += item->size;
                io_work->items.emplace_back(item);
                std::cout << " >>>>>>>>>>>> state  #3 offset " << offset << " isize " << isize <<" item->offset " << item->offset << " item size " << item->size << std::endl;

            }
        }
    }
    std::cout << " >>>>>>>>>>>> After Check; offset : "<< first_offset << " num : " << num << std::endl;
    io_work->offset = first_offset;
    io_work->num_registers = num;
    io_work_vec.emplace_back(io_work);
    //first sort the items 
    // if all the items follow one another then put io_work on the vec.
    // if not truncate io_work, create another one and put that on the vec.

    return;
}
                        
/// @brief check_item_debounce
//     filters get / set operations on debounce status
/// @param enabled 
/// @param item 
/// @param debug 
void check_item_debounce(bool &enabled, std::shared_ptr<cfg::map_struct> item, bool debug)
{
    double tNow = get_time_double();
    if(item->use_debounce)   // && (item->debounce > 0.0) && (io_work->tIo > item->debounce_time))
    {
        if(debug)
            std::cout << " item id " << item->id << " using_debounce" << std::endl;
        if(item->debounce == 0.0)   // && (item->debounce > 0.0) && (io_work->tIo > item->debounce_time))
        {
            if(debug)
                std::cout << " item id" << item->id  << " debounce is zero " << std::endl;
            item->use_debounce = false;
        }
        else if(item->debounce_time == 0.0)
        {
            item->debounce_time = ( tNow + item->debounce);
            if(debug)
                std::cout << " debounce time set up  :" << item->debounce_time << std::endl;
        }
        else
        {
            if( tNow > item->debounce_time)
            {
                if(debug)
                    std::cout << " debounce time   :" << item->debounce_time << " passed: tNow: " << tNow << std::endl;
                item->debounce_time += item->debounce;
            }
            else
            {
                if(debug)
                    std::cout << " still in debounce time :" << item->debounce_time <<  " tNow: " << tNow << std::endl;
                enabled =  false;
            }
        }
    
    }
}


void runThreadWork(std::shared_ptr<IO_Thread> io_thread, std::shared_ptr<IO_Work> io_work, bool debug);
//std::shared_ptr<IO_Thread> make_IO_Thread(i, ip, port , connection_timeout, myCfg);
std::shared_ptr<IO_Thread> make_IO_Thread(int idx, const char* ip, int port, int connection_timeout, struct cfg& myCfg);
double SetupModbusForThread(std::shared_ptr<IO_Thread> io_thread, bool debug);
bool CloseModbusForThread(std::shared_ptr<IO_Thread> io_thread, bool debug);
void buffer_work(cfg::Register_Types reg_type, int device_id, int offset, int num_regs);
void gcom_modbus_decode(std::shared_ptr<IO_Work>io_work, std::stringstream &ss, struct cfg& myCfg);


//return StartThreads(myCfg.connection.max_num_connections, myCfg.connection.ip_address.c_str(), myCfg.connection.port, myCfg.connection.connection_timeout, myCfg);


//void test_iothread(const char *ip, int port, const char *oper, int device_id, const char *reg_type, int offset, int value, bool debug);
// test_uri_body
// extract the strucure find all the reg maps and see if they work
bool gcom_points_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg, const char* decode)
{

    bool debug = false;
     // pull out connection from gcom_map
    Logging::Init("gcom_points", (const int)0, (const char **)nullptr);

    // extract_connection(gcom_map, "connection", myCfg);

    // // pull out the components into myCfg
    // extract_components(gcom_map, "components", myCfg);

    std::cout  << std::endl;
    std::cout  << "points test ..."<< std::endl;

    double runTime = 0.0;
    //int jobs  = 0;

//    std::string mygo(go);
 //   auto run = (mygo == "run") ;
    int idx  = 0;
    auto io_thread = make_IO_Thread(idx, myCfg.connection.ip_address.c_str(), myCfg.connection.port, myCfg.connection.connection_timeout, myCfg);

    std::cout  << " connection ip  ..."<< myCfg.connection.ip_address << ":"<< myCfg.connection.port << "   ->\n";
    SetupModbusForThread(io_thread, debug);
    io_thread->jobs = 0;
    for ( int runs = 0 ; runs < 3 ; ++ runs)
    {
        std::cout << " run  : " << runs << std::endl;


        for (const auto& comp : myCfg.components) {  // Assuming components is a std::vector or similar container
            if(1 || debug)
                std::cout << " comp : " << comp->id 
                    << " Time  Now:" << get_time_double()
                    << " Run Time :" << runTime 
                    << std::endl;
            for (const auto& reg : comp->registers) {  // Assuming registers is a std::vector or similar container inside the component
                if(debug)
                    std::cout << " >>>>  "
                                << std::left << std::setw(20)
                                <<reg->type << " start :" 
                                << reg->starting_offset 
                                    << std::endl;
                for (const auto& item : reg->maps) {   // Assuming maps is a std::vector or similar container inside the register
                    //auto regshr = map->reg.lock();
                    if(debug)
                        std::cout   << ">>>>>>>>\t\t\t\t"
                                << std::left << std::setw(20)
                                << item->id 
                                //<<  " dev " << regshr->device_id 
                                <<   "\toffset " << item->offset 
                                <<  "\tsize " << item->size  
                                <<  "\tdevice_id " << item->device_id  
                                << std::endl;
                    std::shared_ptr<IO_Work> io_work;
                        
                    {

                        // double tNow = get_time_double();

                        // std::cout   << ">>>>>>>> start time #1 :" << tNow  
                        //                 << " delay since start " << tNow-tStart 
                        //                 << std::endl;
                        io_work = make_work(item->reg_type, item->device_id
                                                    , item->offset, item->size, item->reg16
                                                    , item->reg8, strToWorkType("get", false));
                        // double tEnd = get_time_double();

                        // std::cout   << ">>>>>>>> done time #1 :" << tEnd 
                        //     << " elapsed: " << tEnd - tNow 
                        //     << " delay since start "  << tEnd - tStart 
                        //     << std::endl;

                    }
                    //std::cout << " start runThreadWork  device_id "<< item->device_id<< std::endl;
                    //io_work->device_id = 2;
                    //io_work->jobs = 0;
                    bool enabled = true;
                    // double tNow = get_time_double(); // to do use io_work time

                    if(!item->is_enabled)
                    {
                         enabled = false;
                    }
                    if(enabled)
                    {
                        bool mydebug = true;
                        check_item_debounce(enabled, item, mydebug);
                    }
                    // we'll have to work on decode before we can tackle the "get" deadband.
                    // if(enabled) 
                    // {
                    //     if(item->use_deadband)
                    //     {
                    //         item->last_float_val = 0.0; // TODO(float) item->last_value;
                    //         auto dbval = item->float_val - item->last_float_val;
                    //         if (dbval < 0.0 )
                    //             dbval *= -1.0;
                    //         if (dbval < item->deadband)
                    //         { 
                    //             enabled = false;
                    //         }
                    //     }
                    // }

                    if(enabled) 
                    {
                        io_work->items.emplace_back(item);
                        // double tNow = get_time_double();

                        // std::cout   << ">>>>>>>> start time #2 :" << tNow  
                        //             << " delay since start " << tNow-tStart 
                        //             << std::endl;


                        runThreadWork(io_thread, io_work, debug);

                        //TODO save last_float_val and val


                        // double tEnd = get_time_double();

                        // std::cout   << ">>>>>>>> done time #2 :" << tEnd 
                        //     << "elapsed: " << tEnd - tNow 
                        //     << " delay since start "  << tEnd - tStart 
                        //     << std::endl;
                        
                        if (io_work->errors < 0)
                        {
                            item->is_enabled = false;
                            std::cout << "\n\n>>>>>>>>>>   runThreadWork  failed for [/"<< comp->comp_id << "/"<< comp->id <<":" << item->id
                                << "] offset :"
                                << item->offset
                                << " size :"
                                << item->size
                                << "\n\n"
                                << std::endl;
                        }
                        else
                        {
                            std::string rundecode;
                            if(decode)
                            {
                                rundecode = decode;
                            }
                            if (rundecode == "decode")
                            {
                                //std::cout << "OK lets decode this stuff" << std::endl;
                                std::stringstream ss;
                                gcom_modbus_decode(io_work, ss, myCfg);
                            }
                            // since this is a get lets decode the incoming data

                        }
                        runTime += (io_work->tDone - io_work->tIo);
                        io_work->items.clear();
                    }
                    else 
                    {
                        std::cout << "\n\n>>>>>>>>>>   runThreadWork  skipping  [/"<< comp->comp_id << "/"<< comp->id <<":" << item->id
                            << "] offset :"
                            << item->offset
                            << " size :"
                            << item->size
                            << "\n\n"
                            << std::endl;

                    }
                    //jobs += io_work->jobs;
                    //std::cout << "                  done runThreadWork" << std::endl;


                                //if (run)
                                //    std::cout << " test_iothread deprecated" << std::endl;
                                //     test_iothread(myCfg.connection.ip_address.c_str(), myCfg.connection.port, "poll", map.reg->device_id, reg.type.c_str(), map.offset, 0, false);

                }
            }
            std::cout << ">>> done runThreadWork now : " << get_time_double() << " run time : " << runTime  
                << " Jobs : " << io_thread->jobs
                << std::endl;

        }
        if ( runs == 1)
        {
            std::cout << " sleeping for 500 mS to test debounce " << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    CloseModbusForThread(io_thread, debug);

    //if(debug) std::cout << "test_io <ip> <port> <set|pub|get> <device_id> <Holding|Coil> <offset> <value> <debug>" << std::endl;
    //test_iothread(myCfg.connection.ip_address, myCfg.connection.port, "get", map.reg->device_id, reg.type, map.offset, "",  debug);

    //auto item = myCfg.components[0].registers[0].maps[0];
    //std::cout << " >>>>> item name " << item.name << std::endl;


    // look at components
    // for ( auto &sect : gcom_map)
    // {
    //     std::cout  << " sect name ["<< sect.first <<"] "<< std::endl;
    //     //auto [comp,citem] = sect["components"];
    //     //auto comp = sect.second;
    //     //auto citem = sect.second["components"].second;
    //     if (sect.first == "components")
    //     {
    //         // ci [0]  type :St3mapISsSt3anySt4lessISsESaISt4pairIKSsS0_EEE
    //         //     cr component_heartbeat_timeout_ms  type :i
    //         //     cr component_id  type :Ss
    //         //     cr device_id  type :i
    //         //     cr frequency  type :i
    //         //     cr heartbeat_enabled  type :v
    //         //     cr id  type :Ss
    //         //     cr modbus_heartbeat_timeout_ms  type :i
    //         //     cr offset_time  type :i
    //         //     cr registers  type :St6vectorISt3anySaIS0_EE
    //         auto comp = std::any_cast<std::vector<std::any>>(sect.second);
    //         int idx = 0;
    //         for (const auto& ci_any : comp) {
    //             if (ci_any.type() == typeid(std::map<std::string, std::any>)) {
    //                 auto ci = std::any_cast<std::map<std::string, std::any>>(ci_any);
    //                 std::cout << " ci [" << idx << "]  type :" << ci_any.type().name() << std::endl;

    //                 for (const auto& cr : ci) {
    //                     std::cout << " cr " << cr.first << "  type :" << cr.second.type().name() << std::endl;
    //                 }

    //                 // cr["registers"] is where we want to go 
    //             }
    //             idx++;
    //         }

    //         // auto comp = std::any_cast<std::vector<std::any>>(sect.second);
    //         // //int idx  = 0;
    //         // for (auto ci : comp) {
    //         //     //ci is a  std::map<std::string, std::any>
    //         //     std::cout << " ci ["<< idx<<"]  type :" << ci.type().name()<< std::endl;
    //         //     for ( auto &cr : ci) {
    //         //         std::cout << " cr " << cr.first << "  type :" << cr.second.type().name()<< std::endl;
    //         //         //std::cout << "  reg  ["<< cr.first<<"]  type :" << cr.second.type().name()<< std::endl;

    //         //     }


    //         // }
    //     // show type
    //     // firstle
    //         //std::cout << " comp[0]  type :" << comp[0].type().name()<< std::endl;
    //     }
    //     //std::cout << " citem  type :" << citem.type().name()<< std::endl;


    //     // std::map<std::string, std::any> itemMap = std::any_cast<std::map<std::string, std::any>>(rawElement);        for ( auto &reg : comp.registers)
    //     // {
    //     //     std::cout  << "       reg name ["<< reg.id <<"] "<< std::endl;
    //     //     for ( auto &map : reg.maps)
    //     //     {
    //     //         std::cout  << "           map name ["<< map.id <<"]  offset :"<<map.offset<< std::endl;
    //     //     }
    //     // }
    // }
    // for each component
    // for 
    bool ok = true;
    return ok;
}


bool gcom_point_type_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg, const char* ptype, const char* decode)
{

    bool debug = false;
     // pull out connection from gcom_map
    Logging::Init("gcom_point_type", (const int)0, (const char **)nullptr);


    std::cout  << std::endl;
    std::cout  << "point type test ..."<< std::endl;

    double runTime = 0.0;
    int idx  = 0;
    auto io_thread = make_IO_Thread(idx, myCfg.connection.ip_address.c_str(), myCfg.connection.port, myCfg.connection.connection_timeout, myCfg);

    std::cout  << " connection ip  ..."<< myCfg.connection.ip_address << ":"<< myCfg.connection.port << "   ->\n";
    SetupModbusForThread(io_thread, debug);
    io_thread->jobs = 0;
    for ( int runs = 0 ; runs < 2 ; ++ runs)
    {
        std::cout << " run  : " << runs << std::endl;


        for (const auto& comp : myCfg.components) {  // Assuming components is a std::vector or similar container
            if(1 || debug)
                std::cout << " comp : " << comp->id 
                    << " Time  Now:" << get_time_double()
                    << " Run Time :" << runTime 
                    << std::endl;
            for (const auto& reg : comp->registers) {  // Assuming registers is a std::vector or similar container inside the component
                if(debug)
                    std::cout << " >>>>  "
                                << std::left << std::setw(20) <<reg->type 
                                << " start :" << reg->starting_offset 
                                    << std::endl;
                if(reg->type != ptype) continue;
                std::cout << " >>>>  "
                                << std::left << std::setw(20)
                                <<reg->type 
                                << " start :" << reg->starting_offset 
                                << " num :" << reg->number_of_registers
                                    << std::endl;
                //continue;
                //std::shared_ptr<IO_Work> io_work;

                std::vector<std::shared_ptr<IO_Work>> io_work_vec;
                std::vector<std::shared_ptr<cfg::map_struct>> io_map_vec;
                        
                // {
                //     io_work = make_work(reg->reg_type, reg->device_id
                //                                 , reg->starting_offset, reg->number_of_registers, nullptr
                //                                 , nullptr, strToWorkType("get", false));

                // }


                for (const auto& item : reg->maps) {   // Assuming maps is a std::vector or similar container inside the register
                    //auto regshr = map->reg.lock();
                    if(debug)
                        std::cout   << ">>>>>>>>\t\t\t\t"
                                << std::left << std::setw(20)
                                << item->id 
                                //<<  " dev " << regshr->device_id 
                                <<   "\toffset " << item->offset 
                                <<  "\tsize " << item->size  
                                <<  "\tdevice_id " << item->device_id  
                                << std::endl;

                    bool enabled = true;
                    // if(item->packer)
                    // {
                    //     enabled = false;
                    // }
                    if(!item->is_enabled)
                    {
                         enabled = false;
                    }
                    if(enabled)
                    {
                        bool mydebug = true;
                        check_item_debounce(enabled, item, mydebug);
                    }
                    if(enabled) 
                    {
                        // stack all the items 
                        //io_work->items.emplace_back(item);
                        io_map_vec.emplace_back(item);
                        //check_work_items(io_work_vec, io_work, myCfg, true);

                        // TODO now we have to split them

                        //runThreadWork(io_thread, io_work, debug);

                        //if (io_work->errors < 0)
                        // {
                        //     item->is_enabled = false;
                        //     std::cout << "\n\n>>>>>>>>>>   runThreadWork  failed for [/"<< comp->comp_id << "/"<< comp->id <<":" << item->id
                        //         << "] offset :"
                        //         << item->offset
                        //         << " size :"
                        //         << item->size
                        //         << "\n\n"
                        //         << std::endl;
                        // }
                        // else
                        // {
                        //     std::string rundecode;
                        //     if(decode)
                        //     {
                        //         rundecode = decode;
                        //     }
                        //     if (rundecode == "decode")
                        //     {
                        //         //std::cout << "OK lets decode this stuff" << std::endl;
                        //         std::stringstream ss;
                        //         gcom_modbus_decode(io_work, ss, myCfg);
                        //     }
                        //     // since this is a get lets decode the incoming data

                        // }
                        //runTime += (io_work->tDone - io_work->tIo);
                    }
                    else 
                    {
                        std::cout << "\n\n>>>>>>>>>>   runThreadWork  skipping  [/"<< comp->comp_id << "/"<< comp->id <<":" << item->id
                            << "] offset :"
                            << item->offset
                            << " size :"
                            << item->size
                            << "\n\n"
                            << std::endl;

                    }
                }
                //check_work_items
                check_work_items(io_work_vec, io_map_vec, myCfg, "get", true);
                io_map_vec.clear();
                for (auto io_work : io_work_vec)
                {
                        std::cout << ">>>>>>>>>> io_work item  start: " << io_work->offset
                                << " num "                              << io_work->num_registers 
                                <<" device_id "                         << io_work->device_id
                                << std::endl;

                        std::string rundecode;
                        if(decode)
                        {
                            rundecode = decode;
                        }
                        if (rundecode == "decode")
                        {
                            std::cout << ">>>>>>>>>> runThreadWork   start offset: " << io_work->offset
                                << " num :"<< io_work->num_registers
                                << std::endl;

                            runThreadWork(io_thread, io_work, debug);
                            std::cout << ">>>>>>>>>> runThreadWork  done errors: " << io_work->errors
                                << "\n\n" << std::endl;
                            
                            std::stringstream ss;
                            gcom_modbus_decode(io_work, ss, myCfg);
                            runTime += (io_work->tDone - io_work->tIo);
                            std::cout 
                                << "\n\n" << std::endl;

                        }

                }

            }
            std::cout << ">>> done runThreadWork now : " << get_time_double() << " run time : " << runTime  
                << " Jobs : " << io_thread->jobs
                << std::endl;

        }
        if ( runs == 1)
        {
            std::cout << " sleeping for 500 mS to test debounce " << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    CloseModbusForThread(io_thread, debug);

    bool ok = true;
    return ok;
}

void clearpollChan(bool debug);


// yes almost the real thing. 
bool gcom_pub_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg)
{

    ItemMap gcom_cfg;
    extract_structure(gcom_cfg, gcom_map);

    std::vector<std::shared_ptr<PublishGroup>> gcom_pub;

    extract_publish_groups(gcom_pub, gcom_cfg, gcom_map);
    register_publish_groups_with_timer(gcom_pub);
    show_publish_groups(gcom_pub);
    std::cout << " Clearing poll chan " << std::endl;
    clearpollChan(true);
    bool ok = true;
    return ok;
}
