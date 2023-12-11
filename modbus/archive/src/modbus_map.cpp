/*
  file: modbus_dump.cpp
   g++ -o md modbus_map.cpp modbus_dump.cpp -lcjson
   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
   ./md config/clou_ess_1.json  config/dump.json config/echo.sh
*/

#include <inttypes.h>
#include <modbus/modbus.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <iomanip>
#include <fims/libfims.h>
#include <fims/fps_utils.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <unistd.h>

#include <pthread.h>

#include <map>
#include <vector>

#include "modbus_map.h"


volatile bool running = true;
volatile bool modbus_reconnect = false;


long int get_time_us()
{
    long int ltime_us;
    timespec c_time;
    clock_gettime(CLOCK_MONOTONIC, &c_time);
    ltime_us = (c_time.tv_sec * 1000000) + (c_time.tv_nsec / 1000);
    return ltime_us;
}

double g_base_time = 0.0;

double get_time_dbl()
{
    return  (double) get_time_us() - g_base_time;
}
void set_base_time(void)
{
    g_base_time = get_time_dbl();

}

void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

/* Configuration */
int setup_register_map(sys_t* sys, cJSON* reg, const component_config* comp)
{
    datalog* rlog;
    int index =  comp->reg_cnt;
    FPS_DEBUG_PRINT("Setting up register map for register type, index: %d\n", index);
    // get the starting offset
    rlog =  &comp->reg_datalog[index];
    rlog->start_offset = -1;// fix later start_off->valueint;
    cJSON* start_off = cJSON_GetObjectItem(reg, "starting_offset");
    if (start_off == nullptr)
    {
        FPS_ERROR_PRINT("Failed to find starting_offset in JSON.\n");
        return -1;
    }
    else
    {
        rlog->start_offset = start_off->valueint;
    }
    
    if(comp->off_by_one )
        rlog->start_offset--;

    //check to make sure that the number of bytes to read is reasonable
    //if (rlog->start_offset > 65536)
    //{
    //    FPS_ERROR_PRINT("Valid offsets are between 0 - 65536.\n");
    //    return -1;
    //}

    //to get the start address and store it
    cJSON* cjnregs = cJSON_GetObjectItem(reg, "number_of_registers");
    if (cjnregs == nullptr)
    {
        FPS_ERROR_PRINT("Failed to find number_of_registers in JSON.\n");
        return -1;
    }
    rlog->num_regs = cjnregs->valueint; // fix later cjnregs->valueint;

    //check to make sure that the number of bytes to read is reasonable
    if (rlog->start_offset + rlog->num_regs > 65536)
    {
         FPS_ERROR_PRINT("Invalid number of registers.\n");
         return -1;
    }

    cJSON* cjtype = cJSON_GetObjectItem(reg, "type");
    if(cjtype == nullptr)
    {
        FPS_ERROR_PRINT("Register type not defined in config.\n");
        return -1;
    }
    rlog->reg_type = (Type_of_Register)decode_type(cjtype->valuestring);


    //to get the register name and its offset address
    cJSON *cjarray = cJSON_GetObjectItem(reg, "map");
    if (cjarray == nullptr)
    {
        FPS_ERROR_PRINT("Failed to find 'Map' in JSON.\n");
        return -1;
    }

    rlog->map_size  = cJSON_GetArraySize(cjarray);
    rlog->register_map = new maps [comp->reg_datalog[index].map_size];
    maps_t* rmap = rlog->register_map;

    if(rmap == nullptr)
    {
        FPS_ERROR_PRINT("Failed to allocate register map.\n");
        return -1;
    }
    int array_idx = 0;

    cJSON* arg1;
    cJSON_ArrayForEach(arg1, cjarray)
//    for (unsigned int array_idx = 0; array_idx < rlog->map_size; array_idx ++)
    {

        maps_t* ritem = &rlog->register_map[array_idx];
        cJSON* offset = cJSON_GetObjectItem(arg1,"offset");
        if (offset == nullptr)
        {
            FPS_ERROR_PRINT("Failed to get object item 'Offset' in JSON.\n");
            delete[] rmap;
            rlog->register_map = nullptr;
            return -1;
        }
        ritem->reg_off = (offset->valueint);
        if (comp->off_by_one)
            ritem->reg_off--;
        if(rlog->start_offset <= 0 )
        {
            rlog->start_offset = ritem->reg_off;

        } // fix later start_off->valueint;
        // if(rlog->start_offset < ritem->reg_off)
        // {
        //     rlog->start_offset = ritem->reg_off;
        // }
        // if(rlog->num_regs < rlog->start_offset + ritem->reg_off)
        // {
        //     rlog->num_regs = rlog->start_offset + ritem->reg_off;
        // }
        
        //Checking whether the offset is valid
        if (ritem->reg_off < rlog->start_offset ||
                ritem->reg_off > rlog->start_offset + rlog->num_regs )
        {
            FPS_ERROR_PRINT("offset %d is outside of registers read start %d -> max %d num %d.\n", ritem->reg_off, rlog->start_offset, rlog->start_offset + rlog->num_regs, rlog->num_regs);
            delete[] rmap;
            rlog->register_map = nullptr;
            return -1;
        }

        cJSON* name_reg = cJSON_GetObjectItem(arg1,"id");
        if (name_reg == nullptr)
        {
            FPS_ERROR_PRINT("Failed to get object item 'Name' in JSON.\n");
            delete[] rmap;
            rlog->register_map = nullptr;
            return -1;
        }
        ritem->reg_name = strdup(name_reg->valuestring);
        ritem->reg_type = rlog->reg_type;

        cJSON* multi_reg = cJSON_GetObjectItem(arg1,"size");
        ritem->num_regs =
            (multi_reg != nullptr && multi_reg->type == cJSON_Number) ? multi_reg->valueint : 1;

        cJSON* floating_pt = cJSON_GetObjectItem(arg1, "float");
        ritem->floating_pt = (floating_pt != nullptr && floating_pt->type == cJSON_True);

        int string_type = 0;

        cJSON* bit_field = cJSON_GetObjectItem(arg1, "bit_field");
        ritem->bit_field = (bit_field != nullptr && bit_field->type == cJSON_True && ++string_type);

        cJSON* indv_bits = cJSON_GetObjectItem(arg1, "individual_bits");
        ritem->individual_bits = (indv_bits != nullptr && indv_bits->type == cJSON_True && ++string_type);

        cJSON* enum_type = cJSON_GetObjectItem(arg1, "enum");
        ritem->enum_type = (enum_type != nullptr && enum_type->type == cJSON_True && ++string_type);

        cJSON* random_enum_type = cJSON_GetObjectItem(arg1, "random_enum");
        ritem->random_enum_type = (random_enum_type != nullptr && random_enum_type->type == cJSON_True && ++string_type);

        if(string_type > 1)
        {
            FPS_ERROR_PRINT("More than one bit_strings type defined.\n");
            delete[] rmap;
            rlog->register_map = nullptr;
            return -1;
        }

        cJSON* bit_strings = cJSON_GetObjectItem(arg1, "bit_strings");
        if (string_type == 1)
        {
            if (bit_strings == nullptr || bit_strings->type != cJSON_Array)
            {
                FPS_ERROR_PRINT("No bit_strings object to define bit_field values.\n");
                delete[] rmap;
                rlog->register_map = nullptr;
                return -1;
            }

            int array_size = cJSON_GetArraySize(bit_strings);
            if (array_size == 0)
            {
                FPS_ERROR_PRINT("No bit_strings included in bit_strings array.\n");
                delete[] rmap;
                rlog->register_map = nullptr;
                return -1;
            }

            if (ritem->enum_type || ritem->random_enum_type)
            {
                ritem->bit_strings = new char*[array_size];
                memset(ritem->bit_strings, 0, sizeof(char*) * array_size);
            }
            else
            {
                ritem->bit_strings = new char*[ritem->num_regs * 16];
                memset(ritem->bit_strings, 0, sizeof(char*) * ritem->num_regs * 16);
            }

            if(ritem->random_enum_type)
            {
                for(int i = 0; i < array_size; i++)
                {
                    cJSON* bit_string_item = cJSON_GetArrayItem(bit_strings, i);
                    if(bit_string_item != nullptr)
                    {
                        cJSON* value = cJSON_GetObjectItem(bit_string_item, "value");
                        cJSON* bit_string = cJSON_GetObjectItem(bit_string_item, "string");
                        if(value != nullptr && value->type == cJSON_Number && bit_string != nullptr && bit_string->valuestring != nullptr)
                        {
                            ritem->bit_strings[i] = strdup(bit_string->valuestring);
                            ritem->random_enum.insert(std::pair<int, char*>(value->valueint,rmap->bit_strings[i]));
                        }
                    }
                }
            }
            else
            {
                for(int i = 0; i < array_size; i++)
                {
                    cJSON* bit_string_item = cJSON_GetArrayItem(bit_strings, i);
                    if(bit_string_item != nullptr && bit_string_item->valuestring != nullptr)
                        ritem->bit_strings[i] = strdup(bit_string_item->valuestring);
                }
            }
            ritem->num_strings = array_size;
        }
        else if (string_type == 0 && bit_strings != nullptr)
        {
            FPS_ERROR_PRINT("Bit_strings object defined but no corresponding bit_field type values.\n");
            delete[] rmap;
            rlog->register_map = nullptr;
            return -1;
        }

        cJSON* is_signed = cJSON_GetObjectItem(arg1,"signed");
        ritem->sign = (is_signed != nullptr && is_signed->type == cJSON_True);

        cJSON* scale_value = cJSON_GetObjectItem(arg1,"scale");
        ritem->scale = (scale_value != nullptr) ? (scale_value->valuedouble) : 0.0;

        cJSON* shift_value = cJSON_GetObjectItem(arg1,"shift");
        ritem->shift = (shift_value != nullptr) ? (shift_value->valueint) : 0;

        cJSON* uri_reg = cJSON_GetObjectItem(arg1,"uri");
        if (uri_reg && uri_reg->valuestring)
        {
            ritem->ruri = strdup(uri_reg->valuestring);
        }

        array_idx++;

    }
    return 1;
}

bool getCJint (cJSON* cj, const char* name, int& val, int def, bool required)
{
    bool ok = !required;
    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji) {
        val = cji->valueint;
        ok = true;
    }
    else
    {
        val = def;
    }
    return ok;
}

bool getCJbool (cJSON* cj, const char* name, bool &val, bool def, bool required)
{
    bool ok = !required;
    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji) {
        val = (cji->type == cJSON_True);
        ok = true;
    }
    else
    {
        val = def;
    }
    return ok;
}

bool getCJstr (cJSON* cj, const char* name, char**val, const char* def, bool required)
{
    bool ok = !required;
    if(*val) free((void *)*val);
    *val = nullptr;

    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji)
    {
        *val = strdup(cji->valuestring);
        ok = true;
    }
    else
    {
        if(def)
            *val = strdup(def);
    }
    return ok;
}
//TODO use the sys conn_info
bool get_conn_info(sys_t *sys, cJSON *config)//, connection_config *connection)
{
    sys->conn_info = new connection_config;
    //component_config *components = nullptr;
    bool ret = true;

    cJSON* cj = cJSON_GetObjectItem(config, "connection");
    if (cj == nullptr)
    {
        cj = cJSON_GetObjectItem(config, "system");
    }
    if (cj == nullptr)
    {
        FPS_ERROR_PRINT("Invalid config object.\n");
        return -1;
    }

    if(ret) ret = getCJstr(cj, "name",        &sys->conn_info->name,       "Dummy Name",     true);
    if(ret) ret = getCJstr(cj, "ip_address",  &sys->conn_info->ip_address, "127.0.0.1",      false);
    if(ret) ret = getCJint(cj, "port",        sys->conn_info->port,        502,             false);


    FPS_DEBUG_PRINT("Connection to use TCP. IP: %s, Port: %d\n", sys->conn_info->ip_address, sys->conn_info->port);
    return -1;
    return 0;
}

int get_component(sys_t* sys, cJSON *config, component_config* comp, cJSON *component)
{
    //int rc = 0;
    bool ret = true;
    cJSON * cj = component;
    if(ret) ret = getCJstr(cj,  "id",          &comp->id,         "Dummy Name",     true);
    if(ret) ret = getCJbool(cj, "byte_swap",   comp->byte_swap,  false,          false);
    if(ret) ret = getCJbool(cj, "off_by_one",  comp->off_by_one, false,          false);
    if(ret) ret = getCJint(cj,  "frequency",   comp->frequency,  500,            false);
    if(ret) ret = getCJint(cj,  "offset_time", comp->offset_time, 100,           false);
    if(ret) ret = getCJint(cj,  "debounce",    comp->debounce, 0,                false);
    comp->debounce *= 10;
    if(ret) ret = getCJint(cj, "device_id", comp->device_id, -1,                 false);

    // if(comp->device_id < 0 || comp->device_id > 255)
    // {
    //     FPS_ERROR_PRINT("Invalid Device_ID.\n");
    //     free(&comp->id);
    //     return -1;
    // }

    if(ret) ret = getCJbool(cj, "heartbeat_enabled", comp->heartbeat_enabled, false,     false);

    if (comp->heartbeat_enabled)
    {
        if(ret) ret = getCJint(cj, "modbus_heartbeat_freq_ms",       comp->modbus_heartbeat_freq_ms,       500,     false);
        if(ret) ret = getCJint(cj, "component_heartbeat_timeout_ms", comp->component_heartbeat_timeout_ms, 500,     false);

        if(ret) ret = getCJstr(cj, "component_heartbeat_read_uri",   &comp->read_name,                      nullptr,    true);
        if (comp->read_name == nullptr)
        {
            FPS_ERROR_PRINT("No heartbeat read register provided.\n");
            delete comp;
            //free(&comp->id);
            return -1;
        }

        // heartbeat write register is optional
        if(ret) ret = getCJstr(cj, "component_heartbeat_write_uri",  &comp->write_name,                    nullptr,     false);
    }

    //parsing the configuration file to get the Type of the registers, its start and end address, and to get the value of the specified registers
    //We first get the object and then store it in the data structure
    //to get Register object
    cJSON *cjreg = cJSON_GetObjectItem(component, "registers");
    if (cjreg == nullptr)
    {
        FPS_ERROR_PRINT("Failed to get object item 'registers' in JSON.\n");
        char* tmp  = cJSON_Print (component);
        FPS_ERROR_PRINT("Component is >>%s<<\n", tmp);
         
        //cJSON_Delete(component);
        return 1;
    }
    return get_comp_registers(sys, config, cjreg, comp);
}

int get_comp_registers(sys_t* sys, cJSON *config, cJSON* cjreg, component_config* comp)
{
    int rc = 0;
    // int get_comp_registers(sys_t* sys,component_config* comp, cJSON* registers, cJSON* component)
    //to get the size of the array
    int total_regs = cJSON_GetArraySize(cjreg);
    if (total_regs == 0)
    {
        FPS_ERROR_PRINT("No registers to be mapped in the json file\n");
        //cJSON_Delete(cjcomp);
        return 1;
    }
    FPS_ERROR_PRINT("%d registers are mapped in the json file for comp_id [%s]\n", total_regs, comp->id);

    //dynamic array based on the register type entries created
    comp->reg_datalog = new datalog [total_regs];
    if( comp->reg_datalog == nullptr)
    {
        FPS_ERROR_PRINT("Memory allocation error\n");
        //cJSON_Delete(cjcomp);
        rc = 1;
    }
    comp->reg_cnt = 0;

    cJSON* reg;
    cJSON_ArrayForEach(reg, cjreg)
    {
        FPS_ERROR_PRINT(" getting reg set %d\n", comp->reg_cnt);
        rc = setup_register_map(sys, reg, comp);
        FPS_ERROR_PRINT(" setup  array item  %d rc %d\n", comp->reg_cnt, rc);
        if(rc == -1)
        {
            FPS_ERROR_PRINT("Failed to setup register map.\n");
            //cJSON_Delete(config);
            //comp->reg_cnt--;
            rc = 1;
        }
        else
        {
            comp->reg_cnt++;
        }
    }
    return rc;
}


void sys_t::add_component(component_config *comp)
{
    compvec.push_back(comp);
}

int get_components(sys_t* sys, cJSON *config)
{
    int rc = 0;
    cJSON* cji;
    cJSON *cjcomp = cJSON_GetObjectItem(config, "components");
    cJSON_ArrayForEach(cji, cjcomp)
    {
        component_config *comp = new component_config();
        get_component(sys, config, comp, cji);
        sys->add_component(comp);
        rc++;
    }
    FPS_ERROR_PRINT(" got components %d\n", rc);

    return rc;
}

// this it like the get / set from fims
maps *get_sys_uri(sys_t* sys, const char* name, const char* uri)
{
    //std::map<std::string, urimap_t*>:: iterator it;
    if (sys->urimaps.find(uri) != sys->urimaps.end())
    {
        // it->fisrt == uri
        // it->second == urimap_t*
        urimap_t* umap = sys->urimaps[uri];
        FPS_ERROR_PRINT(" seeking name [%s] in uri [%s] \n"
                        , name,  uri);

        //}urimap;
        // could use reg_tpe here
        for (int i = 0 ; i < Num_Register_Types; i++)
        {
            if(umap->urimap[i].find(name) != umap->urimap[i].end())
            {
                maps_t* ritem = umap->urimap[i][name];
                FPS_ERROR_PRINT(" found [%s] type %d in uri [%s] offset %d\n"
                                , name, 1, uri ,ritem->reg_off );
            }
        }

    }
    return nullptr;
}

// now get_register looks a bit different
maps* get_sys_register(sys_t *sys, const char* name, Type_of_Register reg_type)
{

    std::map<std::string, urimap_t*>:: iterator it;
    for (it = sys->urimaps.begin(); it != sys->urimaps.end(); ++it)
    {
        // it->fisrt == uri
        // it->second == urimap_t*
        urimap_t* umap = it->second;
        //typedef struct urimap_t {
        //  std::map<std::string, maps_t*>urimap[Num_Register_Types];
        FPS_ERROR_PRINT(" seeking name [%s] in uri [%s] \n"
                        , name,  it->first.c_str());

        //}urimap;
        // could use reg_tpe here
        for (int i = 0 ; i < Num_Register_Types; i++)
        {
            if(umap->urimap[i].find(name) != umap->urimap[i].end())
            {
                maps_t* ritem = umap->urimap[i][name];
                FPS_ERROR_PRINT(" found [%s] type %d in uri [%s] offset %d\n"
                                , name, i, it->first.c_str(),ritem->reg_off );
            }
        }

    }


    return nullptr;
}
maps* get_register (char* name, Type_of_Register reg_type, component_config* config)
{
    for ( int reg_set = 0; reg_set < config->reg_cnt; reg_set++)
    {
        if(config->reg_datalog[reg_set].reg_type == reg_type)
        {
            for (unsigned int i = 0;  i < config->reg_datalog[reg_set].map_size; i++)
            {
                if (strcmp(name, config->reg_datalog[reg_set].register_map[i].reg_name) == 0)
                    return &config->reg_datalog[reg_set].register_map[i];
                else if (config->reg_datalog[reg_set].register_map[i].individual_bits) // override for individual_bits, register name read as bit_strings
                {
                    for (size_t j = 0; j < config->reg_datalog[reg_set].register_map[i].num_strings; j++)
                    {
                        if(config->reg_datalog[reg_set].register_map[i].bit_strings[j] != nullptr)
                            if(strcmp(name, config->reg_datalog[reg_set].register_map[i].bit_strings[j]) == 0)
                                return &config->reg_datalog[reg_set].register_map[i];
                    }
                }
            }
        }
    }
    return nullptr;
}

/* Main Thread */

// TODO place all the components and reg_types in an overall system object
void add_sys_sysmap(sys_t *sys, maps_t *ritem)
{
    sys->sysmaps[ritem->reg_type][(int)ritem->reg_off] = ritem;
}

// TODO place all the components and reg_types in an overall system object
void add_sys_urimap(sys_t *sys, maps_t *ritem)
{
    char* uri = ritem->ruri? ritem->ruri:ritem->uri;
    FPS_ERROR_PRINT("adding new uri [%p].\n", uri);

    // check for uri in urimaps
    if (sys->urimaps.find(uri) == sys->urimaps.end())
    {
        FPS_ERROR_PRINT("adding new uri [%s].\n", uri);
        sys->urimaps[uri] =  new urimap[Num_Register_Types];
    }
    //std::map<char *, urimap_t*>urimapx;

    FPS_ERROR_PRINT("reg_type  [%d].\n", ritem->reg_type);
    FPS_ERROR_PRINT("reg_name  [%s].\n", ritem->reg_name);
    urimap_t*urimapx = sys->urimaps[uri];
    urimapx->urimap[ritem->reg_type][ritem->reg_name] =  ritem;
}

// got to combine all register types
void fix_maps(sys_t* sys, connection_config *cfg)
{
    component_config* pcomp;
    datalog* reg_datalog;
    datalog* regd;
    int cnum = sys->compvec.size();
    // foreach comp
    for (int i = 0 ; i < cnum; i++)
    {
        pcomp = sys->compvec[i];
        //comp->reg_cnt--;
        FPS_ERROR_PRINT("Component [%d] id [%s]  regd cnt %d\n", i, pcomp->id, pcomp->reg_cnt);
        reg_datalog = pcomp->reg_datalog;
        //foreach regtype ??
        for (int j = 0 ; j < pcomp->reg_cnt; j++)
        {
            regd = &reg_datalog[j];
            FPS_ERROR_PRINT("Component [%d] id [%s]  regd %p map_size %d\n", i, pcomp->id,  regd, regd?regd->map_size:0);
            fprintf(stdout,"   [%s] \n",get_reg_stype(regd->reg_type));


            //    regdata [%d] type [%s] start [%d] num %d map_size [%d].\n"
            //                 , i, get_reg_stype(regd->reg_type), (int)regd->start_offset
            //                 , (int)regd->num_regs
            //                 , (int)regd->map_size
            //                 );
            // for each reg
            for (int k = 0; k < (int)regd->map_size; k++)
            {
                maps_t* ritem = &regd->register_map[k];
                if(ritem->uri == nullptr)
                {
                    //ritem->uri = strdup(pcomp->id);
                    asprintf(&ritem->uri,"/components/%s", pcomp->id);
                }
                if(sys->sysmaps[regd->reg_type].find((int)ritem->reg_off)
                        != sys->sysmaps[regd->reg_type].end())
                {
                    fprintf(stdout,
                            " name [%s] already found at index %d\n"
                            , ritem->reg_name
                            , ritem->reg_off
                           );

                }
                else
                {
                    fprintf(stdout,
                            " adding name [%s]  type [%d] at index %d sysmap\n"
                            , ritem->reg_name
                            , ritem->reg_type
                            , ritem->reg_off
                           );
                    //psys->sysmaps[regd->reg_type][(int)ritem->reg_off] = ritem;
                    // check for reg in urimap
                    add_sys_sysmap(sys, ritem);
                    fprintf(stdout,
                            " adding name [%s]  type %d at index %d urimap\n"
                            , ritem->reg_name
                            , ritem->reg_type
                            , ritem->reg_off
                           );
                    add_sys_urimap(sys, ritem);
                    fprintf(stdout,
                            " adding name [%s]  at index %d done\n"
                            , ritem->reg_name
                            , ritem->reg_off
                           );

                }
            }
        }
    }
}

// got to combine all register types
void dump_server(sys_t* sys, connection_config *cfg)
{
    //component_config* pcomp;
    //datalog* reg_datalog;
    //datalog* regd;
    FILE*fp = stdout;
    //std::map<int,maps_t*>mymaps[Num_Register_Types];
    std::map<int,maps_t*>::iterator it;

    if(cfg->dump_server)
    {
        fp = fopen (cfg->dump_server, "w");
        if(!fp)
        {
            FPS_ERROR_PRINT("unable to open server_dump[%s].\n", cfg->dump_server);
            return;
        }
    }

    int port = cfg->port;
    auto mbport = getenv("MBPORT");
    if (mbport)
    {
        port = atoi(mbport);
    }
    fprintf(fp,"{\"system\":");
    fprintf(fp,"   {   \"name\":\"%s\",",cfg->name);
    fprintf(fp,"       \"protocol\": \"Modbus TCP\",");
    fprintf(fp,"       \"id\": \"%s\",",cfg->name);     //TODO
    fprintf(fp,"       \"ip_address\": \"%s\",", "0.0.0.0"/*cfg->ip_address*/);
    fprintf(fp,"       \"port\": %d ",port);
    fprintf(fp,"   },");
    fprintf(fp,"    \"registers\":     {");

    // }
    int last_reg = -1;
    for (int j = 0 ; j < Num_Register_Types ; j++ )
    {
        if(sys->sysmaps[j].size() > 0)
            last_reg = j;
    }
    for (int j = 0 ; j < Num_Register_Types ; j++ )
    {
        if(sys->sysmaps[j].size() > 0)
        {
            fprintf(fp,"      \"%s\":       [",get_reg_stype(j));
            for (it = sys->sysmaps[j].begin(); it != sys->sysmaps[j].end(); ++it)
            {
                maps_t * rmap = it->second;
                if (it != sys->sysmaps[j].begin())
                {
                    fprintf(fp,",");
                    //fprintf(fp,"\n");
                }
                fprintf(fp,
                        "          { \"name\":\"%s\",\"id\":\"%s\",\"offset\":%d,"
                        , rmap->reg_name, rmap->reg_name,(int)rmap->reg_off) ;
                if(rmap->scale != 0.0)
                {
                    fprintf(fp,
                            "\"scale\": %f, "
                            , rmap->scale) ;
                }
                if(rmap->shift != 0)
                {
                    fprintf(fp,
                            "\"shift\": %d, "
                            , rmap->shift) ;
                }
                if(rmap->num_regs != 1)
                {
                    fprintf(fp,
                            "\"size\": %d, "
                            , rmap->num_regs) ;
                }

                // signed
                if(rmap->sign)
                {
                    fprintf(fp,
                            "\"signed\": true, "
                           );
                }
                if(rmap->ruri)
                {
                    fprintf(fp,
                        "\"uri\":\"%s\" }"
                        , rmap->ruri) ;
                }
                else
                {
                    fprintf(fp,
                        "\"uri\":\"%s\" }"
                        , rmap->uri) ;
                }
            }
            fprintf(fp,"       ]");
            if(j < last_reg)
                fprintf(fp,",");
            //fprintf(fp,"\n");
        }
    }
    fprintf(fp,"    }}");
    fclose(fp);
}

void dump_echo(sys_t* sys, char * fname)//, component_config* comp, int num)
{
    //component_config *pcomp;
    //datalog *reg_datalog;
    //datalog *regd;
    FILE*fp = stdout;
    if(fname)
    {
        fp = fopen (fname, "w");
        if(!fp)
        {
            FPS_ERROR_PRINT("unable to open echo_dump[%s].\n", fname);
            return;
        }
    }
    int mcount = 0;
    {
        //FPS_ERROR_PRINT("adding new uri [%s].\n", ritem->uri);
        //sys->urimaps[ritem->uri] =  new urimap[Num_Register_Types];
        //maps_t*ritem;
        //foreach urimap
        for (std::map<std::string, urimap_t*>::iterator it = sys->urimaps.begin(); it != sys->urimaps.end(); ++it)
        {
            mcount++;
            urimap_t *uitem = it->second;
            // typedef struct urimap_t {
            //     std::map<std::string, maps_t*>urimap[Num_Register_Types];
            // }urimap;
            int phead = 1;
            bool need_comma = false;

            for (int i = 0 ; i < Num_Register_Types; i++)
            {
                // get the register map for datatype (i)
                std::map<std::string, maps_t*> *imap = &uitem->urimap[i];
                FPS_ERROR_PRINT(" uri  [%s] type %d size (%d)\n"
                                , it->first.c_str()
                                , i
                                , (int)imap->size()
                               );
                if ((int)imap->size() > 0)
                {
                    if(need_comma)
                    {
                        fprintf(fp,",");
                        //fprintf(fp,"\n");
                        need_comma = false;
                    }

                    if(phead == 1)
                    {
                        phead = 0;
                        fprintf(fp, "/usr/local/bin/fims_echo -u %s -b '{"
                                ,it->first.c_str());
                    }
                    //map for each
                    // the map should already have the min offset at the hed and the max offset at the end
                    std::map<std::string, maps_t*>::iterator xit;
                    int k = 0;
                    int min_offset;
                    int max_offset;
                    for (xit = imap->begin(); xit != imap->end(); ++xit)
                    {

                        maps_t* ritem = xit->second;
                        if(k == 0)
                        {
                            min_offset = ritem->reg_off;
                            max_offset = ritem->reg_off;
                        }
                        if ((int)max_offset < (int)ritem->reg_off + (int)ritem->num_regs)
                            max_offset = ritem->reg_off + ritem->num_regs;
                        if ((int)min_offset > (int)ritem->reg_off)
                            min_offset = ritem->reg_off;

                        FPS_ERROR_PRINT("  ....  name [%s]\n", ritem->reg_name);
                        fprintf(fp," \"%s\":{\"value\":0}"
                                , ritem->reg_name) ;
                        if(k <(int)imap->size()-1)  //TODO may need to add a "," if there are more to print
                            fprintf(fp,",");
                        //fprintf(fp,"\n");
                        k++;
                    }
                    FPS_ERROR_PRINT("  ....  name [%s] min %d max %d \n", it->first.c_str(), min_offset, max_offset);
                    need_comma = true;

                }
            }
            if(phead == 0)
            {
                fprintf(fp,"}'&");
            }
        }

    }
    FPS_ERROR_PRINT(" urimaps mcount (%d)\n", mcount);
    fclose(fp);
}

cJSON* get_config_json(int argc, char* argv[])
{
    FILE *fp = nullptr;
    if(argc <= 1)
    {
        FPS_ERROR_PRINT("Need to pass argument of name.\n");
        return nullptr;
    }

    if (argv[1] == nullptr)
    {
        FPS_ERROR_PRINT(" Failed to get the path of the config file. \n");
        return nullptr;
    }

    fp = fopen(argv[1], "r");
    if(fp == nullptr)
    {
        FPS_ERROR_PRINT("Failed to open file %s\n", argv[1]);
        return nullptr;
    }

    fseek(fp, 0, SEEK_END);
    long unsigned file_size = ftell(fp);
    rewind(fp);

    // create Configuration_file and read file in Configuration_file
    char *config_file = new char[file_size];
    if(config_file == nullptr)
    {
        FPS_ERROR_PRINT("Memory allocation error\n");
        fclose(fp);
        return nullptr;
    }

    size_t bytes_read = fread(config_file, 1, file_size, fp);
    fclose(fp);
    if(bytes_read != file_size)
    {
        FPS_ERROR_PRINT("Read size error.\n");
        delete[] config_file;
        return nullptr;
    }

    cJSON* config = cJSON_Parse(config_file);
    delete[] config_file;
    if(config == nullptr)
        FPS_ERROR_PRINT("Invalid JSON object in file\n");
    return config;
}

const char * reg_types[] = {
    "Coil", "Discrete_Input", "Input_Register", "Holding_Register"
};
const char * reg_stypes[] = {
    "coils", "discrete_inputs", "input_registers", "holding_registers"
};

int decode_type(const char* val)
{
    if(strcmp(val, "Coil") == 0)
        return Coil;
    else if(strcmp(val, "Discrete_Input") == 0)
        return Discrete_Input;
    else if(strcmp(val, "Discrete Inputs") == 0)
        return Discrete_Input;
    else if(strcmp(val, "Input_Register") == 0)
        return Input_Register;
    else if(strcmp(val, "Input Registers") == 0)
        return Input_Register;
    else if(strcmp(val, "Holding") == 0)
        return Holding_Register;
    else if(strcmp(val, "Holding Registers") == 0)
        return Holding_Register;
    else
    {
        FPS_ERROR_PRINT("Invalid register type [%s].\n", val);
        return -1;
    }
}

const char* get_reg_type(int rtype)
{
    return reg_types[rtype];

}
const char* get_reg_stype(int rtype)
{
    return reg_stypes[rtype];

}
