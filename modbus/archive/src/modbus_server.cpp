/*
 * modbus_server.cpp
 *
 *  Created on: Sep 28, 2018
 *      Author: jcalcagni
 */
#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <fims/fps_utils.h>
#include <fims/libfims.h>
#include <modbus/modbus.h>
#include "modbus_server.h"

#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000

volatile bool running = true;

int hostname_to_ip(char *hostname , char *ip, int iplen)
{
   struct hostent *he;
   struct in_addr **addr_list;
   int i;

   if ( (he = gethostbyname( hostname ) ) == NULL)
   {
      // get the host info
      herror("gethostbyname");
      return 1;
   }

   addr_list = (struct in_addr **) he->h_addr_list;

   for(i = 0; addr_list[i] != NULL; i++)
   {
      //Return the first one;
      strncpy(ip , inet_ntoa(*addr_list[i]), iplen );
      return 0;
   }

   return 1;
}

int service_to_port(char *service, int *port)
{
    struct servent *serv;

   /* getservbyname() - opens the etc.services file and returns the */
   /* values for the requested service and protocol.                */

   serv = getservbyname(service, "tcp");
   if (serv == NULL) {
      FPS_ERROR_PRINT("Service [%s] not found for protocol [%s]\n",
         service, "tcp");
      return 1;
   }

   *port = ntohs(serv->s_port);
   return 0;
}


void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

/* Setup */
int establish_connection(system_config* config)
{
    struct ifreq ifr;
    struct sockaddr_in addr;
    int sock, yes;
    // modbus listen only accepts on INADDR_ANY so need to make own socket and hand it over
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        FPS_ERROR_PRINT("Failed to create socket: %s.\n", strerror(errno));
        return -1;
    }

    yes = 1;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *) &yes, sizeof(yes)))
    {
        FPS_ERROR_PRINT("Failed to set socket option reuse address and port: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }
    linger linger_s;
    linger_s.l_linger = 10; // 10 second linger
    linger_s.l_onoff = 1; // linger set to on
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&linger_s, sizeof(linger_s)))
    {
        FPS_ERROR_PRINT("Failed to set socket option linger: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    //read port out of config but allow service to override
    addr.sin_port = htons(config->port);
    //printf(" found port [%d]\n", config->port);
    if (config->service)
    {
        int ret;
        int sport; 
        //printf(" seeking service [%s]\n", config->service);
        ret = service_to_port(config->service, &sport);
        if (ret == 0) {
           FPS_ERROR_PRINT(" found service [%s] port [%d]\n", config->service, sport);
           addr.sin_port = htons(sport);
           config->port = sport;
        } else {
            FPS_ERROR_PRINT("Failed to get port for service  %s: usinf default %d.\n", 
                     config->service, config->port);
        }
    }
    if(config->eth_dev != NULL)
    {
        ifr.ifr_addr.sa_family = AF_INET;
        //read interface out of config file
        strncpy(ifr.ifr_name, config->eth_dev, IFNAMSIZ - 1);
        if(-1 == ioctl(sock, SIOCGIFADDR, &ifr))
        {
            FPS_ERROR_PRINT("Failed to get ip address for interface %s: %s.\n", ifr.ifr_name, strerror(errno));
            close(sock);
            return -1;
        }
        addr.sin_addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    }
    else if(config->ip_address != NULL)
    {
        char ip[128];
        int ret;
        ret = hostname_to_ip(config->ip_address ,ip, 128);
        if (ret == 0)
        {
	      free(config->ip_address);
              config->ip_address = strdup(ip);
        }
        addr.sin_addr.s_addr = inet_addr(config->ip_address);
    }
    else
        addr.sin_addr.s_addr = INADDR_ANY;

    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        FPS_ERROR_PRINT("Failed to bind socket: %s.\nIf trying to use a port less than 1024 run programs as sudo.\n", strerror(errno));
        close(sock);
        return -1;
    }

    if (-1 == listen(sock, 1))
    {
        FPS_ERROR_PRINT("Failed to listen on socket: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    // Values passed to modbus_new_tcp are ignored since we are listening on our own socket
    config->mb = modbus_new_tcp("127.0.0.1", MODBUS_TCP_DEFAULT_PORT);
    if(config->mb == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate context.\n");
        close(sock);
        return -1;
    }
    if(config->device_id != -1 && -1 == modbus_set_slave(config->mb, config->device_id))
    {
        FPS_ERROR_PRINT("Valid Unit identifiers (Device_Id) are between 1-247.\n");
        modbus_free(config->mb);
        config->mb = NULL;
        return -1;
    }
    FPS_ERROR_PRINT("   set Device_Id to %d\n", config->device_id);
    return sock;
}

/* Configuration */
int parse_system(cJSON *system, system_config *config)
{
    memset(config, 0, sizeof(system_config));
    cJSON *system_name = cJSON_GetObjectItem(system, "id");
    if (system_name == NULL || system_name->valuestring == NULL)
    {
        FPS_ERROR_PRINT("Failed to find system 'id' in JSON.\n");
        return -1;
    }
    config->name = strdup(system_name->valuestring);
    if(config->name == NULL)
    {
        FPS_ERROR_PRINT("Allocation failed for name");
        return -1;
    }

    cJSON *byte_swap = cJSON_GetObjectItem(system, "byte_swap");
    config->byte_swap = cJSON_IsTrue(byte_swap);

    //TODO add support for eth device instead of ip
    // get connection information from object system
    cJSON* ip_addr = cJSON_GetObjectItem(system, "ip_address");
    cJSON* serv = cJSON_GetObjectItem(system, "service");
    cJSON* serial_device = cJSON_GetObjectItem(system, "serial_device");
    config->service =  NULL;

    if (serv != NULL)
    {
        if(cJSON_IsString(serv))
        {
            config->service = strdup(serv->valuestring);
            printf(" setting up service to [%s] \n", config->service);
        }
    }
    if (ip_addr != NULL)
    {
        cJSON* port = cJSON_GetObjectItem(system, "port");
        config->port = cJSON_IsNumber(port) ? port->valueint : 502;
        if(config->port > 65535 || config->port < 0)
        {
            FPS_ERROR_PRINT("Invalid port defined.\n");
            free(config->name);
            return -1;
        }
        if(!cJSON_IsString(ip_addr))
        {
            FPS_ERROR_PRINT("Invalid ip address string.\n");
            free(config->name);
            return -1;
        }
        config->ip_address = strdup(ip_addr->valuestring);
        if(config->ip_address == NULL)
        {
            FPS_ERROR_PRINT("Allocations error ip address string.\n");
            free(config->name);
            return -1;
        }
    }
    else if (serial_device != NULL)
    {
        cJSON* baud_rate = cJSON_GetObjectItem(system, "baud_rate");
        config->baud = cJSON_IsNumber(baud_rate) ? baud_rate->valueint : 115200;
        cJSON* parity_str = cJSON_GetObjectItem(system, "parity");
        config->parity = 'N';
        // if parity field exists make sure it is valid
        if(parity_str != NULL)
        {
            if(parity_str->valuestring == NULL)
            {
                FPS_ERROR_PRINT("Invalid Parity value in json\n");
                free(config->name);
                return -1;
            }
            else if(strcmp("none", parity_str->valuestring) == 0)
                config->parity = 'N';
            else if(strcmp("even", parity_str->valuestring) == 0)
                config->parity = 'E';
            else if(strcmp("odd", parity_str->valuestring) == 0)
                config->parity = 'O';
            else
            {
                FPS_ERROR_PRINT("Invalid Parity value in json\n");
                free(config->name);
                return -1;
            }
        }

        cJSON* data_bits = cJSON_GetObjectItem(system, "data_bits");
        config->data = cJSON_IsNumber(data_bits) ? data_bits->valueint : 8;
        if(config->data > 8 || config->data < 5)
        {
            FPS_ERROR_PRINT("Invalid number of data bits.\n");
            free(config->name);
            return -1;
        }

        cJSON* stop_bits = cJSON_GetObjectItem(system, "stop_bits");
        config->stop = cJSON_IsNumber(stop_bits) ? stop_bits->valueint : 1;
        if(config->stop > 2 || config->stop < 1)
        {
            FPS_ERROR_PRINT("Invalid number of stop bits.\n");
            free(config->name);
            return -1;
        }

        if(!cJSON_IsString(serial_device))
        {
            FPS_ERROR_PRINT("Invalid string for serial device.\n");
            free(config->name);
            return -1;
        }
        config->serial_device = strdup(serial_device->valuestring);
        if(config->serial_device == NULL)
        {
            FPS_ERROR_PRINT("Allocation error string for serial device.\n");
            free(config->name);
            return -1;
        }
    }
    else
    {
        FPS_ERROR_PRINT("Failed to find either an IP_Address or Serial_Device in JSON.\n");
        free(config->name);
        return -1;
    }

    // Device ID is required for serial
    cJSON* slave_id = cJSON_GetObjectItem(system, "device_id");
    config->device_id = -1;
    // if device_id is given check that it is valid
    if(cJSON_IsNumber(slave_id))
    {
        if(slave_id->valueint < 0 || slave_id->valueint > 255)
        {
            FPS_ERROR_PRINT("Invalid Device_ID.\n");
            free(config->name);
            if(config->ip_address != NULL)
                free(config->ip_address);
            if(config->serial_device != NULL)
                free(config->serial_device);
            return -1;
        }
        config->device_id = slave_id->valueint;
    }
    // Add in debug server_delay needs compile option -DSERVER_DELAY
    cJSON* server_delay = cJSON_GetObjectItem(system, "server_delay");
    if ((server_delay) && (cJSON_IsNumber(server_delay)))
    {
        config->server_delay = server_delay->valueint;
    } else {
        config->server_delay = 0;
    }
    config->connect_delay = 0;

    // Add in bypass_init -DSERVER_DELAY
    cJSON* bypass_init = cJSON_GetObjectItem(system, "bypass_init");
    if (bypass_init)
    {
        config->bypass_init = cJSON_IsTrue(bypass_init); 
    } else {
        config->bypass_init = false;
    }
    return 0;
}

/**
 * @brief Create maps for the Modbus registers of this server.
 * 
 * @param registers Pointer to the cJSON object containing configuration data for the Modbus registers.
 * @param data Pointer to the datalog object that will be loaded with data about the different categories
 * of Modbus registers (Coil, Discrete Input, Input Register, Holding Register): starting offset for the
 * register category, number of registers belonging to that category, map of registers of that category.
 * @return server_data* Pointer to high-level maps of all registers belonging to server.
 */
server_data* create_register_map(cJSON* registers, datalog* data)
{
    // coils = read-write booleans. discrete_inputs = read-only booleans. input_registers = read-only numbers. holding_registers = read-write numbers.
    const char* reg_types[] = { "coils", "discrete_inputs", "input_registers", "holding_registers" };

    // server_data is the highest-level map in the modbus server. can access all register types from it
    server_data* srv_data = new server_data;
    if(srv_data == NULL) {
        FPS_ERROR_PRINT("failed to allocate memory for server_data");
        goto cleanup;
    }

    // iterate through each register type and parse them
    for(int i = 0; i < Num_Register_Types; i++)
    {
        // get the array of register config info of this type
        cJSON* register_array_JSON = cJSON_GetObjectItem(registers, reg_types[i]);
        if (register_array_JSON == NULL) continue;

        // integer i represents the register type
        data[i].reg_type = (Type_of_Register)i;

        // number of variables equates to number of starting registers. Note: each variable could possibly be made up of multiple registers
        int reg_cnt = data[i].map_size = cJSON_GetArraySize(register_array_JSON);
        FPS_ERROR_PRINT("Count %d variables included in %s config object.\n", reg_cnt, reg_types[i]);
        if(reg_cnt == 0)
        {
            FPS_ERROR_PRINT("No registers included in %s config object.\n", reg_types[i]);
            continue;
        }

        // allocate memory for the register map of this register type
        data[i].register_map = new maps[reg_cnt];

        // keep track of what the minimum and maximum offsets are for this register type's variables
        int min_offset = -1, max_offset = min_offset;

        // iterate through each variable in this register type's array and parse the configuration data
        for(int j = 0; j < reg_cnt; j++)
        {
            // record this variable's register type
            data[i].register_map[j].reg_type  = i;

            // extract variable configuration JSON object from array
            cJSON* current_variable_JSON = cJSON_GetArrayItem(register_array_JSON, j);
            if( !cJSON_IsObject(current_variable_JSON) ) {
                FPS_ERROR_PRINT("Failed read or in invalid format: %s variable entry %d\n", reg_types[i], i);
                goto cleanup;
            }
            
            // parse variable's ID
            cJSON* current_variable_id_json  = cJSON_GetObjectItem(current_variable_JSON, "id");
            if( !cJSON_IsString(current_variable_id_json) ) {
                FPS_ERROR_PRINT("Missing variable ID for %s variable entry %d\n", reg_types[i], i);
                goto cleanup;
            }
            data[i].register_map[j].reg_name = strdup(current_variable_id_json->valuestring);

            // parse variable's URI
            cJSON* current_variable_uri_JSON = cJSON_GetObjectItem(current_variable_JSON, "uri");
            if( !cJSON_IsString(current_variable_uri_JSON) ) {
                FPS_ERROR_PRINT("Missing URI for %s register entry %d: %s\n", reg_types[i], i, data[i].register_map[j].reg_name);
                goto cleanup;
            }
            data[i].register_map[j].uri      = strdup(current_variable_uri_JSON->valuestring);

            // parse variable's starting register offset
            cJSON* current_variable_offset_JSON = cJSON_GetObjectItem(current_variable_JSON, "offset");
            if( !cJSON_IsNumber(current_variable_offset_JSON) ) {
                FPS_ERROR_PRINT("Missing starting offset for %s register entry %d: %s\n", reg_types[i], i, data[i].register_map[j].reg_name);
                goto cleanup;
            }
            data[i].register_map[j].reg_off  = current_variable_offset_JSON->valueint;
            cJSON *byte_swap = cJSON_GetObjectItem(current_variable_JSON, "byte_swap");
            data[i].register_map[j].use_byte_swap = false;
            if(byte_swap) {
                data[i].register_map[j].byte_swap = cJSON_IsTrue(byte_swap);
                data[i].register_map[j].use_byte_swap = true;
            }

            // build a new map for this variable's URI (if map already exists for this variable's URI the insert in the next step will simply be skipped).
            // key of map entry is the URI. value of map entry is a nested map linking variable ID to further nested maps of data for this URI/ID pair.
            std::pair<const char*, body_map*> new_map_entry(data[i].register_map[j].uri, new body_map);
            if (new_map_entry.second == NULL) {
                FPS_ERROR_PRINT("Failed to allocate memory for new variable ID-data map\n");
                goto cleanup;
            }

            // nest the URI's map in the map of URIs.
            // if the URI was already found in a previous variable, the insert function will return an iterator to
            // the already-existing entry instead of creating a new one
            std::pair<uri_map::iterator, bool> rtn_pair = srv_data->uri_to_register.insert(new_map_entry);
            auto uri_map_it = rtn_pair.first;
            if(uri_map_it->second == NULL) {
                FPS_ERROR_PRINT("Failed to insert map entry for new URI\n");
                goto cleanup;
            }

            // check if there has already been a parsed variable with both the same URI and the same register ID
            auto var_id_map_it = uri_map_it->second->find(data[i].register_map[j].reg_name);
            // if there has not already been a parsed variable with both the same URI and the same register ID, create a new map at this URI/ID entry
            if(var_id_map_it == uri_map_it->second->end())
            {
                // an array of pointers to bools indicating if there is a variable for the URI/ID pair with the given register type
                // (order of bools is Coil, Discrete Input, Input Register, Holding Register)
                bool* reg_type = new bool[Num_Register_Types];
                memset(reg_type, 0, sizeof(bool) * Num_Register_Types);

                // an array of pointers to structs containing variable configuration data, one pointer for each possible register type
                maps** reg_info = new maps*[Num_Register_Types];
                memset(reg_info, 0, sizeof(maps*) * Num_Register_Types);

                // set the boolean of the currently-being-configured variable's register type to true
                reg_type[i] = true;

                // point the configuration struct pointer at the currently-being-configured register's configuration struct
                reg_info[i] = &(data[i].register_map[j]);

                // First array indicates if this URI/ID has a variable of the given type (i.e. if INPUT_REGISTER_BOOL is true then there is an Input Register with this URI/ID pair)
                // [COIL_BOOL,      DISCRETE_INPUT_BOOL,    INPUT_REGISTER_BOOL,    HOLDING_REGISTER_BOOL]
                // Second array holds pointers to structs that contain the configuration of up to 4 variables (one per register type, all sharing the same URI/ID pair)
                // [COIL_DATA_PTR,  DI_DATA_PTR,            IR_DATA_PTR,            HR_DATA_PTR]
                std::pair<bool*, maps**> reg_type_reg_config_arrays(reg_type, reg_info);

                // key of map entry is the reg ID alone (URI is key in parent map). value is the pair of arrays for the URI/ID pair
                std::pair<const char*, std::pair<bool*, maps**> > reg_id_to_arrays( data[i].register_map[j].reg_name, reg_type_reg_config_arrays);

                // insert the new map entry
                std::pair<body_map::iterator, bool> rtn_pair = uri_map_it->second->insert( reg_id_to_arrays);
                if(rtn_pair.second == false) {
                    FPS_ERROR_PRINT("Failed to insert map entry for new %s: %s\n", reg_types[i], data[i].register_map[j].reg_name);
                    goto cleanup;
                }
                FPS_RELEASE_PRINT("Inserted map entry for variable with type %s, ID %s, and URI %s.\n", reg_types[i], data[i].register_map[j].reg_name, data[i].register_map[j].uri);
            }
            else
            {
                // if the boolean for this variable's register type has already been marked true then a variable has already been configured for this URI/ID/type combination.
                // this indicates an error in the config file since there cannot be two variables with the same URI/ID/type combination.
                if(var_id_map_it->second.first[i] == true || var_id_map_it->second.second[i] != NULL)
                {
                    FPS_ERROR_PRINT("failed to insert map entry for new %s: %s. Base URI and ID already used in the map\n", reg_types[i], data[i].register_map[j].reg_name);
                    goto cleanup;
                }

                // there has already been a variable configured with the current variable's URI/ID pair, but not with the current variable's register type.
                // access the already allocated arrays. mark the current variable's register type as valid for this URI/ID pair, and point the config struct ptr at the config struct
                var_id_map_it->second.first[i] = true;
                var_id_map_it->second.second[i] = &(data[i].register_map[j]);
            }

            // fill in the configuration struct with the data found in the JSON entry

            cJSON* multi_reg = cJSON_GetObjectItem(current_variable_JSON,"size");
            data[i].register_map[j].num_regs = cJSON_IsNumber(multi_reg) ? multi_reg->valueint : 1;

            cJSON* bit_field = cJSON_GetObjectItem(current_variable_JSON, "bit_field");
            data[i].register_map[j].bit_field = cJSON_IsTrue(bit_field);

            cJSON* enum_type = cJSON_GetObjectItem(current_variable_JSON, "enum");
            data[i].register_map[j].enum_type = cJSON_IsTrue(enum_type);

            cJSON* random_enum = cJSON_GetObjectItem(current_variable_JSON, "random_enum");
            data[i].register_map[j].random_enum_type = cJSON_IsTrue(random_enum);

            cJSON* is_signed = cJSON_GetObjectItem(current_variable_JSON,"signed");
            data[i].register_map[j].sign = cJSON_IsTrue(is_signed);

            cJSON* is_bool = cJSON_GetObjectItem(current_variable_JSON, "bool");
            data[i].register_map[j].is_bool = cJSON_IsTrue(is_bool);

            cJSON* scale_value = cJSON_GetObjectItem(current_variable_JSON,"scale");
            data[i].register_map[j].scale = cJSON_IsNumber(scale_value) ? (scale_value->valuedouble) : 0.0;

            cJSON* is_float = cJSON_GetObjectItem(current_variable_JSON, "float");
            data[i].register_map[j].floating_pt = cJSON_IsTrue(is_float);
            data[i].register_map[j].sign |= data[i].register_map[j].floating_pt; // a float is signed by definition
            if (data[i].register_map[j].floating_pt && data[i].register_map[j].num_regs == 1) {
                FPS_ERROR_PRINT("%s is declared a float and has size 1 which is not allowed for floats.\n", data[i].register_map[j].reg_name);
                goto cleanup;
            }

            int offset = data[i].register_map[j].reg_off;
            int num_regs = data[i].register_map[j].num_regs;
            min_offset = (min_offset > offset || min_offset == -1) ? offset : min_offset;
            max_offset = (max_offset < (offset + num_regs)) ? (offset + num_regs) : max_offset;
        }

        // store size of register map for this register type
        data[i].start_offset = min_offset;
        data[i].num_regs = max_offset - min_offset;
        FPS_RELEASE_PRINT("Starting at an offset of %d and ending at an offset of %d, there are %d total individual registers for the %s register type.\n", min_offset, max_offset, data[i].num_regs, reg_types[i]);

        // regs_to_map contains one array per register type. each of those arrays holds pointers to all of the registers of that type
        // for the current register type, allocate memory for its array that is held by regs_to_map
        srv_data->regs_to_map[i] = new maps*[data[i].num_regs];
        memset(srv_data->regs_to_map[i], 0, sizeof(maps*) * data[i].num_regs);

        // building hash lookup for register.
        // this has to be done in a second pass because we don't know how big the register map will be
        // until we have read all the registers and got the min and max offsets.
        for(int j = 0; j < reg_cnt; j++)
        {
            // offsets will be normalized to first register of the given register type
            data[i].register_map[j].reg_off -= min_offset;
            for(unsigned int k = data[i].register_map[j].reg_off; k < data[i].register_map[j].reg_off + data[i].register_map[j].num_regs; k++)
            {
                // verify the high-level register pointer is not already pointed at an existing register
                if(srv_data->regs_to_map[i][k] != NULL) {
                    FPS_ERROR_PRINT("%s at offset %d is defined to multiple variables %s and %s\n", reg_types[i], k+min_offset, srv_data->regs_to_map[i][k]->reg_name, data[i].register_map[j].reg_name);
                    goto cleanup;
                }
                // point the high-level register pointer at this current register
                srv_data->regs_to_map[i][k] = data[i].register_map + j;
            }
        }
    }
    return srv_data;

    cleanup:
    for (auto& it : srv_data->uri_to_register) {
        for (auto body_it = it.second->begin(); body_it != it.second->end(); ++body_it) {
            delete []body_it->second.first;
            delete []body_it->second.second;
        }
        it.second->clear();
        delete(it.second);
    }
    srv_data->uri_to_register.clear();
    for (int i = 0; i < Num_Register_Types; i++)
    {
        if(data[i].register_map != NULL)     delete [] data[i].register_map;
        if(srv_data->regs_to_map[i] != NULL) delete [] srv_data->regs_to_map[i];
    }
    if (srv_data->uris != NULL) delete []srv_data->uris;
    delete srv_data;
    return NULL;
}

/* Modbus and FIMS Communication Functions */

/**
 * @brief Parses the "value" object out of the given object. If none found, assumes that
 * the passed-in pointer is the "value" object itself so returns the passed-in pointer.
 * 
 * @return cJSON* A pointer to the "value" object.
 */
cJSON* unwrap_if_clothed(cJSON* obj)
{
    cJSON* val = cJSON_GetObjectItem(obj, "value");
    if (val == NULL) val = obj;
    return val;
}

/**
 * @brief Parses the int found in the "value" object of the last object in a given cJSON array.
 * 
 * @param arr Array represented in cJSON format. Expect each object in the array to contain a nested object called "value".
 * @return int The valueint of "value" found in the last object in the given array, or 0 if there is an extraction error caused by unexpected input format.
 */
int extract_valueint_of_last_array_object(cJSON* arr)
{
    int size = cJSON_GetArraySize(arr);
    if ( size == 0 ) return 0;

    cJSON* obj = cJSON_GetArrayItem(arr, size-1);
    if ( obj == NULL ) return 0;

    cJSON* val = cJSON_GetObjectItem(obj, "value");
    if ( val == NULL ) return 0;

    return val->valueint;
}

/**
 * @brief Encode the source value, found in the cJSON object, as a 32-bit unsigned integer.
 * 
 * Booleans encoded as 0 or 1.
 * 
 * Bit fields / enumerations assumed to already be integers so no special encoding is performed other than the type-cast.
 * 
 * Two kinds of floats: direct floats and indirect floats.
 * Direct floats are transmitted directly bit-for-bit in floating-point format. Required to be size 2 and not scaled.
 * Indirect floats are scaled the truncated down to ints and transmitted as ints to be descaled on client side.
 * 
 * Both signed and unsigned integers are memcpyed directly into the unsigned integer to maintain sign bits.
 * 
 * @param settings Configuration settings indicating how to interpret/manipulate the source value.
 * @param obj cJSON object containing the source value.
 * @return uint32_t Source value with bits rawly stored in 32-bit unsigned integer variable.
 */
uint32_t json_to_uint32(maps* settings, cJSON* obj)
{
    cJSON* val = unwrap_if_clothed(obj);
    // source value may be in a variety of data types. use configuration settings to know which data type to expect
    // and parse the value into the uint32_t appropriately, including necessary type casts and/or direct memory copies.
    uint32_t encoded_val;
    if (settings->is_bool)
    {
        encoded_val = static_cast<uint32_t> (cJSON_IsTrue(val) || val->valueint == 1);
    }
    else if(settings->bit_field || settings->enum_type || settings->random_enum_type)
    {
        encoded_val = static_cast<uint32_t> (extract_valueint_of_last_array_object(obj));
    }
    // direct float: transmitted directly bit-for-bit
    else if(settings->floating_pt)
    {
        float scaled_val = val->valuedouble * (settings->scale == 0.0 ? 1.0 : settings->scale);
        memcpy(&encoded_val, &scaled_val, sizeof(encoded_val));
    }
    // indirect float: scaled and truncated to int, then transmitted as int to be descaled on client side
    else if(settings->scale != 0.0)
    {
        float scaled_val = val->valuedouble * settings->scale;
        int casted_val = static_cast<int> (scaled_val);
        memcpy(&encoded_val, &casted_val, sizeof(encoded_val));
    }
    // source value is either a signed or unsigned integer
    else
    {
        memcpy(&encoded_val, &(val->valueint), sizeof(encoded_val));
    }
    return encoded_val;
}
/**
 * @brief Encode the source value, found in the cJSON object, as a 32-bit unsigned integer.
 * 
 * Booleans encoded as 0 or 1.
 * 
 * Bit fields / enumerations assumed to already be integers so no special encoding is performed other than the type-cast.
 * 
 * Two kinds of floats: direct floats and indirect floats.
 * Direct floats are transmitted directly bit-for-bit in floating-point format. Required to be size 2 and not scaled.
 * Indirect floats are scaled the truncated down to ints and transmitted as ints to be descaled on client side.
 * 
 * Both signed and unsigned integers are memcpyed directly into the unsigned integer to maintain sign bits.
 * 
 * @param settings Configuration settings indicating how to interpret/manipulate the source value.
 * @param obj cJSON object containing the source value.
 * @return uint32_t Source value with bits rawly stored in 32-bit unsigned integer variable.
 */
uint64_t json_to_uint64(maps* settings, cJSON* obj)
{
    cJSON* val = unwrap_if_clothed(obj);
    // source value may be in a variety of data types. use configuration settings to know which data type to expect
    // and parse the value into the uint32_t appropriately, including necessary type casts and/or direct memory copies.
    uint64_t encoded_val;
    if (settings->is_bool)
    {
        encoded_val = static_cast<uint64_t> (cJSON_IsTrue(val) || val->valueint == 1);
    }
    else if(settings->bit_field || settings->enum_type || settings->random_enum_type)
    {
        encoded_val = static_cast<uint64_t> (extract_valueint_of_last_array_object(obj));
    }
    // direct float: transmitted directly bit-for-bit
    else if(settings->floating_pt)
    {
        double scaled_val = val->valuedouble * (settings->scale == 0.0 ? 1.0 : settings->scale);
        memcpy(&encoded_val, &scaled_val, sizeof(encoded_val));
    }
    // indirect float: scaled and truncated to int, then transmitted as int to be descaled on client side
    else if(settings->scale != 0.0)
    {
        double scaled_val = val->valuedouble * settings->scale;
        int64_t casted_val = static_cast<int64_t> (scaled_val);
        memcpy(&encoded_val, &casted_val, sizeof(encoded_val));
    }
    // source value is either a signed or unsigned integer
    else
    {
        memcpy(&encoded_val, &(val->valueint), sizeof(encoded_val));
    }
    return encoded_val;
}

/**
 * @brief Updates the value of a Holding Register or Input Register variable
 * with the value found in the given cJSON object.
 * 
 * @param settings Configuration settings for the targeted variable.
 * @param value cJSON object containing the value with which to update the variable.
 * @param regs Pointer to either the first Holding Register register or the first Input Register register,
 * depending on the type of variable the target is. Will be used as the base from which to index to the
 * targeted variable's register.
 */
void update_holding_or_input_register_value(maps* settings, cJSON* value, uint16_t* regs, system_config* sys_cfg)
{
    // extract the new variable value, rawly represented in 32 bits stored in a 32-bit unsigned integer
    uint32_t val = json_to_uint32(settings, value);

    // load individual Modbus registers with new value
    if(settings->num_regs == 1)
    {
        // variables declared as single-register values should be able to implicitly typecast from 32 bits to 16 bits without losing any data
        regs[settings->reg_off] = val;
    }
    else if(settings->num_regs == 2)
    {
        if (!sys_cfg->byte_swap) // regular
        {
            regs[settings->reg_off]     = static_cast<uint16_t> (val >> 16);
            regs[settings->reg_off + 1] = static_cast<uint16_t> (val & 0x0000ffff);
        }
        else // "word_swapped"
        {
            regs[settings->reg_off]     = static_cast<uint16_t> (val & 0x0000ffff);
            regs[settings->reg_off + 1] = static_cast<uint16_t> (val >> 16);
        }
    }
    else if(settings->num_regs == 4)
    {
        uint64_t val64 = json_to_uint64(settings, value);

        if (!sys_cfg->byte_swap) // regular
        {
            regs[settings->reg_off + 0] = static_cast<uint16_t> (val64 >> 48);
            regs[settings->reg_off + 1] = static_cast<uint16_t> (val64 >> 32);
            regs[settings->reg_off + 2] = static_cast<uint16_t> (val64 >> 16);
            regs[settings->reg_off + 3] = static_cast<uint16_t> (val64 & 0x0000ffff);
        }
        else // "word_swapped"
        {
            regs[settings->reg_off + 3] = static_cast<uint16_t> (val64 >> 48);
            regs[settings->reg_off + 2] = static_cast<uint16_t> (val64 >> 32);
            regs[settings->reg_off + 1] = static_cast<uint16_t> (val64 >> 16);
            regs[settings->reg_off + 0] = static_cast<uint16_t> (val64 & 0x0000ffff);
        }
    }
}

/**
 * @brief Updates the value of a variable with the value found in the given cJSON object. If there are multiple variables that all share the same URI/ID but
 * are of differing variable types, they will all be updated with the values found in the cJSON object.
 * 
 * @param map Struct containing pointers to the locations of Modbus registers in memory.
 * @param reg_type A pointer to an array of booleans, each representing a variable type (in order: Coil, Discrete Input, Input Register, Holding Register).
 * If a boolean is true, then there is a valid variable of its corresponding variable type with the targeted URI/ID. There could be zero Trues, one True, or multiple Trues.
 * @param settings A pointer to an array of configuration settings. If the bool sharing the same index as the settings pointer is False, expect the settings pointer to be
 * false since there is no valid variable for that variable type. If the bool is True, the settings pointer will point to a struct containing configuration settings for the
 * variable that has the targeted URI/ID and indexed variable type.
 * @param value cJSON object containing the value with which to update the variable.
 */
void update_variable_value(modbus_mapping_t* map, bool* reg_type, maps** settings, cJSON* value, system_config* sys_cfg)
{
    cJSON* obj = unwrap_if_clothed(value);
    for(int i = 0; i < Num_Register_Types; i++)
    {
        if(reg_type[i] == false)
            continue;
        if((i == Coil) || (i == Discrete_Input))
        {
            uint8_t* regs = (i == Coil) ? map->tab_bits : map->tab_input_bits;
            regs[settings[i]->reg_off] = cJSON_IsTrue(obj) || obj->valueint == 1;
        }
        else if((i == Input_Register) || (i == Holding_Register))
        {
            uint16_t* regs = (i == Holding_Register) ? map->tab_registers : map->tab_input_registers;
            update_holding_or_input_register_value(settings[i], value, regs, sys_cfg);
        }
    }
}

bool process_modbus_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query)
{
    uint8_t  function = query[header_length];
    uint16_t offset = MODBUS_GET_INT16_FROM_INT8(query, header_length + 1);
    uint16_t num_regs = MODBUS_GET_INT16_FROM_INT8(query, header_length + 3);
    uint16_t slave_id = MODBUS_GET_INT16_FROM_INT8(query, header_length-1);
    printf(" slave %02x offset  [%02x] [%2d]\n", MODBUS_GET_HIGH_BYTE(slave_id), offset, offset);
    if(function == MODBUS_FC_WRITE_SINGLE_COIL)
    {
        if(bytes_read != header_length + (serial ? 7 : 5))
        {
            FPS_ERROR_PRINT("Modbus body length less than expected.\n");
            return false;
        }
        uint16_t value = num_regs;
        if( offset >= data[Coil].start_offset &&
            offset <  data[Coil].start_offset + data[Coil].num_regs)
        {
            offset -= data[Coil].start_offset;
            maps* reg = server_map->regs_to_map[Coil][offset];
            if(reg == NULL)
            {
                FPS_ERROR_PRINT("Wrote to undefined coil.\n");
                return false;
            }
            cJSON* send_body = cJSON_CreateObject();
            if(send_body != NULL)
            {
                if(value == 0 || value == 0xFF00)
                {
                    char uri[512];
                    sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                    cJSON_AddBoolToObject(send_body, "value", (value == 0xFF00) ? true : false);
                    char* body_msg = cJSON_PrintUnformatted(send_body);
                    server_map->p_fims->Send("set", uri, NULL, body_msg);
                    free(body_msg);
                    cJSON_Delete(send_body);
                }
                else
                {
                    FPS_ERROR_PRINT("Invalid value sent to coil.\n");
                    cJSON_Delete(send_body);
                    return false;
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_MULTIPLE_COILS)
    {
        uint8_t data_bytes = query[header_length + 5];

        if(bytes_read != header_length + data_bytes + (serial ? 8 : 6) ||
           data_bytes != ((num_regs / 8) + ((num_regs % 8 > 0) ? 1 : 0)))
        {
            //error incorrect amount of data reported
            FPS_ERROR_PRINT("Modbus bytes of data does not match number of coils requested to write.\n");
            return false;
        }
        if( offset >= data[Coil].start_offset &&
            offset + num_regs <=  data[Coil].start_offset + data[Coil].num_regs)
        {
            offset -= data[Coil].start_offset;
            for(int i = 0 ; i < data_bytes; i++)
            {
                for(int j = 0; j < ((i != data_bytes -1) ? 8 : ((num_regs % 8 == 0) ? 8 : num_regs % 8)); j++)
                {
                    maps* reg = server_map->regs_to_map[Coil][(offset + i * 8 + j)];
                    if(reg == NULL)
                        //undefined coil
                        continue;
                    cJSON* send_body = cJSON_CreateObject();
                    if(send_body != NULL)
                    {
                        char uri[512];
                        sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                        uint8_t value = (query[header_length + 6 + i] >> j) & 0x01;
                        cJSON_AddBoolToObject(send_body, "value", (value == 1) ? true : false);
                        char* body_msg = cJSON_PrintUnformatted(send_body);
                        server_map->p_fims->Send("set", uri, NULL, body_msg);
                        free(body_msg);
                        cJSON_Delete(send_body);
                    }
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_SINGLE_REGISTER)
    {
        if(bytes_read != header_length + (serial ? 7 : 5))
        {
            FPS_ERROR_PRINT("Modbus body length less than expected, bytes read %d, header %d.\n", bytes_read, header_length);
            return false;
        }
        if( offset >= data[Holding_Register].start_offset &&
            offset <  data[Holding_Register].start_offset + data[Holding_Register].num_regs)
        {
            offset -= data[Holding_Register].start_offset;
            maps* reg = server_map->regs_to_map[Holding_Register][offset];
            if(reg == NULL)
            {
                FPS_ERROR_PRINT("Wrote to undefined register.\n");
            }
            else
            {
                if(reg->num_regs != 1)
                {
                    FPS_ERROR_PRINT("Wrote single register of Multi Register variable %s.\n", reg->reg_name);
                    //TODO either need to set the value or return error code
                }
                else
                {
                    cJSON* send_body = cJSON_CreateObject();
                    if(send_body != NULL)
                    {
                        char uri[512];
                        sprintf(uri, "%s/%s", reg->uri, reg->reg_name);

                        if(reg->is_bool)
                        {
                            bool temp_reg;
                            temp_reg = (num_regs == 1);
                            cJSON_AddBoolToObject(send_body, "value", temp_reg);
                        }
                        else
                        {
                            double temp_reg;
                            if(reg->sign == true)
                                temp_reg = static_cast<double>(static_cast<int16_t>(num_regs));
                            else
                                temp_reg = static_cast<double>(num_regs);

                            if(reg->scale != 0.0)
                                temp_reg /= reg->scale;
                            cJSON_AddNumberToObject(send_body, "value", temp_reg);
                        }
                        char* body_msg = cJSON_PrintUnformatted(send_body);
                        server_map->p_fims->Send("set", uri, NULL, body_msg);
                        free(body_msg);
                        cJSON_Delete(send_body);
                    }
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
    {
        uint8_t data_bytes =  query[header_length + 5];
        if(bytes_read != header_length + data_bytes + (serial ? 8 : 6) ||
           data_bytes != num_regs * 2)
        {
            FPS_ERROR_PRINT("Modbus bytes of data does not match number of holding registers requested to write.\n");
            return false;
        }
        if( offset >= data[Holding_Register].start_offset &&
            offset + num_regs <=  data[Holding_Register].start_offset + data[Holding_Register].num_regs)
        {
            offset -= data[Holding_Register].start_offset;
            for(unsigned int i = 0; i < num_regs;)
            {
                uint32_t byte_offset = header_length + 6 + i * 2;
                maps* reg = server_map->regs_to_map[Holding_Register][offset + i];
                if(reg == NULL)
                {
                    i++;
                    continue;
                }

                if(reg->reg_off != offset + i || reg->reg_off + reg->num_regs > offset + num_regs)
                {
                    FPS_ERROR_PRINT("Write does not include all registers of Multi Register variable %s.\n", reg->reg_name);
                    i++;
                    continue;
                }
                double temp_reg;
                if (reg->num_regs == 1)
                {
                    uint16_t temp_reg_int = MODBUS_GET_INT16_FROM_INT8(query, byte_offset);
                    // single register 16-bit short
                    if (reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int16_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else if(reg->num_regs == 2)
                {
                    //Combine 2 registers into 32-bit value
                    uint32_t temp_reg_int;
                    if (config->byte_swap == true)
                        temp_reg_int = ((static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    )      ) +
                                        (static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 16) );
                    else
                    {
                        //TODO more debug is needed but byte order appears to be swapped on system so changing query
                        temp_reg_int = ((static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    ) << 16 ) +
                                        (static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2))       ) );
                        query[byte_offset]     = static_cast<uint8_t>(temp_reg_int >> 24);
                        query[byte_offset + 1] = static_cast<uint8_t>((temp_reg_int & 0x00ff0000) >> 16);
                        query[byte_offset + 2] = static_cast<uint8_t>((temp_reg_int & 0x0000ff00) >> 8);
                        query[byte_offset + 3] = static_cast<uint8_t>(temp_reg_int & 0x000000ff);
                    }

                    if (reg->floating_pt == true)
                    {
                        float temp_reg_float;
                        memcpy(&temp_reg_float, &temp_reg_int, sizeof(float));
                        temp_reg = static_cast<double>(temp_reg_float);
                    }
                    else if (reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int32_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else if(reg->num_regs == 4)
                {
                    uint64_t temp_reg_int;
                    if ((config->byte_swap == true) || (reg->use_byte_swap && reg->byte_swap)) {
                        printf( " got byte_swap \n");
                        temp_reg_int = ((static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    )      ) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 16) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 4)) << 32) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 6)) << 48) );
                    } else {
                        temp_reg_int = ((static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    ) << 48) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 32) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 4)) << 16) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 6))      ) );
                    }
                    double treg = 0.0;
                    memcpy(&treg, &temp_reg_int, sizeof(double));
                    printf( " got temp_reg_int 0x%08lx >>%f \n",temp_reg_int, treg);


                    if (reg->floating_pt == true)
                    {
                        memcpy(&temp_reg, &temp_reg_int, sizeof(double));
                        //printf( " got                             temp_reg >>%f \n",temp_reg);
                    }
                    else if (reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int64_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                    
                    //printf( " final                             temp_reg >>%f \n",temp_reg);

                }
                else
                {
                    //not currently supported register count
                    FPS_ERROR_PRINT("Invalid Number of Registers defined for %s.\n", reg->reg_name);
                    i += reg->num_regs;
                    continue;
                }
                //Only want to scale if scale value is present and not == 0.0
                if (reg->scale != 0.0)
                    temp_reg /= reg->scale;

                cJSON* send_body = cJSON_CreateObject();
                if(send_body != NULL)
                {
                    char uri[512];
                    sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                    if(reg->is_bool)
                        cJSON_AddBoolToObject(send_body, "value", (temp_reg == 1));
                    else
                        cJSON_AddNumberToObject(send_body, "value", temp_reg);
                    char* body_msg = cJSON_PrintUnformatted(send_body);
                    server_map->p_fims->Send("set", uri, NULL, body_msg);
                    free(body_msg);
                    cJSON_Delete(send_body);
                }
                i += reg->num_regs;
            }
        }
        // Let the modbus_reply handle error message
    }
    // else no extra work to be done let standard reply handle
    return true;
}

bool process_fims_message(fims_message* msg, server_data* server_map, system_config* sys_cfg, bool* reload = nullptr)
{
    char* uri = msg->uri;
    // uri was sent to this processes URI so it needs to handle it
    FPS_ERROR_PRINT("uri %s base %s.\n", uri, server_map->base_uri);
    if(strncmp(uri, server_map->base_uri, strlen(server_map->base_uri)) == 0)
    {
        // NOTE(WALKER): code for reload logic here:
        if (reload && strcmp(msg->pfrags[msg->nfrags - 1], "_reload") == 0)
        {
            if (msg->nfrags != 3)
            {
                FPS_ERROR_PRINT("A reload uri cannot contain more fragments than 3.\n");
                return false;
            }
            *reload = true;
            return true;
        }
        if (strcmp(msg->pfrags[msg->nfrags - 1], "_bug") == 0)
        {
            FPS_ERROR_PRINT("Found a bug uri\n");
            if (msg->nfrags != 3)
            {
                FPS_ERROR_PRINT("A bug uri cannot contain more fragments than 3.\n");
                return false;
            }
            cJSON* body_obj = cJSON_Parse(msg->body);
            if (!body_obj)
                return false;
            cJSON* cur_obj = cJSON_GetObjectItem(body_obj, "delay");
            if(cur_obj != NULL)
            {
                FPS_ERROR_PRINT("Found a bug delay:%d  \n", cur_obj->valueint);
                sys_cfg->server_delay = cur_obj->valueint;
            } 
            cur_obj = cJSON_GetObjectItem(body_obj, "cdelay");
            if(cur_obj != NULL)
            {
                FPS_ERROR_PRINT("Found a bug cdelay:%d  \n", cur_obj->valueint);
                sys_cfg->connect_delay = cur_obj->valueint;
            } 
            cur_obj = cJSON_GetObjectItem(body_obj, "exception");
            if(cur_obj != NULL)
            {
                sys_cfg->exception = cur_obj->valueint;
            } 
            cur_obj = cJSON_GetObjectItem(body_obj, "mdebug");
            if(cur_obj != NULL)
            {
                sys_cfg->mdebug = cur_obj->valueint;
            } 
            cJSON_Delete(body_obj);

            return true;
        }

        if(strcmp("set", msg->method) == 0)
        {
            uri = msg->pfrags[2];
            if(msg->nfrags > 2 && strncmp("reply", uri, strlen("reply")) == 0)
            {
                uri = msg->pfrags[3] - 1;
                uri_map::iterator uri_it = server_map->uri_to_register.find(uri);
                if(uri_it == server_map->uri_to_register.end())
                {
                    // failed to find in map
                    FPS_ERROR_PRINT("Fims reply for unrequested uri: %s.\n", uri);
                    return false;
                }
                body_map* body = uri_it->second;
                cJSON* body_obj = cJSON_Parse(msg->body);
                //check for valid json
                if(body_obj == NULL)
                {
                    FPS_ERROR_PRINT("Received invalid json object for set %s. \"%s\"\n", uri, msg->body);
                    return false;
                }
                for(body_map::iterator body_it = body->begin(); body_it != body->end(); ++body_it)
                {
                    cJSON* cur_obj = cJSON_GetObjectItem(body_obj, body_it->first);
                    if(cur_obj == NULL)
                    {
                        FPS_ERROR_PRINT("Failed to find [%s] in [%s].\n", body_it->first, uri);
                    }
                    else
                    {
                        FPS_ERROR_PRINT("OK found [%s] in [%s].\n", body_it->first, uri);
                        update_variable_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, cur_obj, sys_cfg);
                    }
                }
            }
            else
            {
                if(msg->replyto != NULL)
                    server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
                return true;
            }
        }
        else if(strcmp("get", msg->method) == 0)
        {
            //don't know when we would receive a get
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
        else
        {
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
    }
    else // Check to see if this is a publish we care about
    {
        if(strcmp("pub", msg->method) == 0)
        {
            uri_map::iterator uri_it = server_map->uri_to_register.find(msg->uri);
            if(uri_it == server_map->uri_to_register.end())
            {
                //failed to find uri in devices we care about.
                FPS_ERROR_PRINT("Received pub not in uri list: %s.\n", msg->uri);
                return false;
            }
            cJSON* body_obj = cJSON_Parse(msg->body);
            if(body_obj == NULL || body_obj->child == NULL)
            {
                if(body_obj != NULL)
                    cJSON_Delete(body_obj);
                return false;
            }
            for(cJSON* id_obj = body_obj->child; id_obj != NULL; id_obj = id_obj->next)
            {
                body_map::iterator body_it = uri_it->second->find(id_obj->string);
                if(body_it == uri_it->second->end())
                {
                    // Value not in our array ignore
                    continue;
                }
                update_variable_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, id_obj, sys_cfg);
            }
            cJSON_Delete(body_obj);
        }
        else
            // Not our message so ignore
            return true;
    }
    return true;
}

bool initialize_map(server_data* server_map, system_config* sys_cfg)
{
    if(server_map->p_fims->Subscribe((const char**)server_map->uris, server_map->num_uris + 1) == false)
    {
        FPS_ERROR_PRINT("Failed to subscribe to URI.\n");
        return false;
    }

    char replyto[256];
    for(uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
    {
        sprintf(replyto, "%s/reply%s", server_map->base_uri, it->first);
        server_map->p_fims->Send("get", it->first, replyto, NULL);
        timespec current_time, timeout;
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        timeout.tv_sec += 2;
        bool found = false;

        while(server_map->p_fims->Connected() && found != true &&
             (clock_gettime(CLOCK_MONOTONIC, &current_time) == 0) && (timeout.tv_sec > current_time.tv_sec ||
             (timeout.tv_sec == current_time.tv_sec && timeout.tv_nsec > current_time.tv_nsec + 1000)))
        {
            unsigned int us_timeout_left = (timeout.tv_sec - current_time.tv_sec) * 1000000 +
                    (timeout.tv_nsec - current_time.tv_nsec) / 1000;
            fims_message* msg = server_map->p_fims->Receive_Timeout(us_timeout_left);
            if(msg == NULL)
                continue;
            bool rc = process_fims_message(msg, server_map, sys_cfg);
            if(strcmp(replyto, msg->uri) == 0)
            {
                found = true;
                if(rc == false)
                {
                    FPS_ERROR_PRINT("Error, failed to find value from config file in defined uri.\n");
                    server_map->p_fims->free_message(msg);
                    return false;
                }
            }
            server_map->p_fims->free_message(msg);
        }
        if(found == false)
        {
            FPS_ERROR_PRINT("Failed to find get response for %s.\n", it->first);
            return false;
        }
    }
    return true;
}

// credit: https://stackoverflow.com/questions/12730477/close-is-not-closing-socket-properly

int getSO_ERROR(int fd) {
   int err = 1;
   socklen_t len = sizeof err;
   if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
      perror("getSO_ERROR");
   if (err)
      errno = err;              // set errno to the socket SO_ERROR
   return err;
}

void closeSocket(int fd) {      // *not* the Windows closesocket()
   if (fd >= 0) {
      getSO_ERROR(fd); // first clear any errors, which can cause close to fail
      if (shutdown(fd, SHUT_RDWR) < 0) // secondly, terminate the 'reliable' delivery
         if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
            perror("shutdown");
      if (close(fd) < 0) // finally call close()
         perror("close");
   }
}

bool haveInput(int fd, double timeout) {
   int status;
   fd_set fds;
   struct timeval tv;
   FD_ZERO(&fds);
   FD_SET(fd, &fds);
   tv.tv_sec  = (long)timeout; // cast needed for C++
   tv.tv_usec = (long)((timeout - tv.tv_sec) * 1000000); // 'suseconds_t'

   while (1) {
      if (!(status = select(fd + 1, &fds, 0, 0, &tv)))
         return false;
      else if (status > 0 && FD_ISSET(fd, &fds))
         return true;
      else if (status > 0)
         perror("I am confused");
      else if (errno != EINTR)
         perror("select"); // tbd EBADF: man page "an error has occurred"
   }
}

// bool flushSocketBeforeClose(int fd, double timeout) {
//    const double start = time();
//    char discard[99];
//    static_assert(SHUT_WR == 1);
//    if (shutdown(fd, 1) != -1)
//       while (time() < start + timeout)
//          while (haveInput(fd, 0.01)) // can block for 0.01 secs
//             if (!read(fd, discard, sizeof discard))
//                return true; // success!
//    return false;
// }

/* Main Server */
int main(int argc, char *argv[])
{
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    bool reload;

    server_data *server_map = NULL;
    int header_length, serial_fd, fims_socket;
    int fd_max = 0;
    int reload_fd_max = 0;
    int rc = 0;
    int server_socket = -1;
    int fims_connect = 0;
    fd_set all_connections;
    fd_set rd_connections;
    fd_set wr_connections;
    fd_set er_connections;
    FD_ZERO(&all_connections);
    system_config sys_cfg;
    memset(&sys_cfg, 0, sizeof(system_config));
    datalog data[Num_Register_Types];
    memset(data, 0, sizeof(datalog)*Num_Register_Types);

    do
    {
        reload = false; // initialize and reset if we reload

        // FPS_ERROR_PRINT("Reading config file and starting setup.\n");
        cJSON* config = get_config_json(argc, argv);
        if(config == NULL)
            return 1;

        // Getting the object "System Name" from the object  "System' //
        cJSON *system_obj = cJSON_GetObjectItem(config, "system");
        if (system_obj == NULL)
        {
            FPS_ERROR_PRINT("Failed to get System object in config file.\n");
            cJSON_Delete(config);
            return 1;
        }

        cJSON *registers = cJSON_GetObjectItem(config, "registers");
        if (registers == NULL)
        {
            FPS_ERROR_PRINT("Failed to get object item 'register' in JSON.\n");
            cJSON_Delete(config);
            return 1;
        }

        // Read connection information from json and establish modbus connection
        if(-1 == parse_system(system_obj, &sys_cfg))
        {
            cJSON_Delete(config);
            return 1;
        }

        FPS_RELEASE_PRINT("System config complete: Setup register map.\n");
        server_map = create_register_map(registers, data);
        if (server_map == NULL)
        {
            FPS_ERROR_PRINT("Error creating register map for component %s\n", sys_cfg.name);
            rc = 1;
            cJSON_Delete(config);
            goto cleanup;
        }
        cJSON_Delete(config);

        // load the server map's URI array from the uri_to_register map.
        // must be done after register map creation to get a count of how many unique URIs there are.
        server_map->load_uri_array();

        // configure Modbus mapping
        server_map->mb_mapping = modbus_mapping_new_start_address(data[Coil].start_offset, data[Coil].num_regs,
                                        data[Discrete_Input].start_offset,   data[Discrete_Input].num_regs,
                                        data[Holding_Register].start_offset, data[Holding_Register].num_regs,
                                        data[Input_Register].start_offset,   data[Input_Register].num_regs);
        if (server_map->mb_mapping == NULL)
        {
            FPS_ERROR_PRINT("Failed to allocate the server map's Modbus mapping: %s", modbus_strerror(errno));
            goto cleanup;
        }

        if (server_map == NULL)
        {
            FPS_ERROR_PRINT("Failed to initialize the mapping\n");
            rc = 1;
            goto cleanup;
        }

        server_map->p_fims = new fims();
        if (server_map->p_fims == NULL)
        {
            FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
            rc = 1;
            goto cleanup;
        }
        // could alternatively fims connect using a stored name for the server
        while(fims_connect < 5 && server_map->p_fims->Connect(sys_cfg.name) == false)
        {
            fims_connect++;
            sleep(1);
        }
        if(fims_connect >= 5)
        {
            FPS_ERROR_PRINT("Failed to establish connection to FIMS server.\n");
            rc = 1;
            goto cleanup;
        }

        FPS_ERROR_PRINT("Map configured: Initializing data.\n");
        //todo this should be defined to a better length
        char uri[100];
        sprintf(uri,"/interfaces/%s", sys_cfg.name);
        server_map->base_uri = server_map->uris[0] = uri;
        if(false == initialize_map(server_map, &sys_cfg))
        {
            FPS_ERROR_PRINT("Failed to get data for defined URI.\n");
            if(!sys_cfg.bypass_init)
            {
                rc = 1;
                goto cleanup;
            }
        }
        FPS_ERROR_PRINT("Data initialized: setting up modbus connection.\n");
        if(sys_cfg.serial_device == NULL)
        {
            server_socket = establish_connection(&sys_cfg);
            if(server_socket == -1)
            {
                char message[1024];
                snprintf(message, 1024, "Modbus Server %s failed to establish open TCP port to listen for incoming connections.\n", sys_cfg.name);
                FPS_ERROR_PRINT("%s\n", message);
                emit_event(server_map->p_fims, "Modbus Server", message, 1);
                rc = 1;
                goto cleanup;
            }
            char message[1024];
            snprintf(message, 1024, "Modbus Server %s, listening for connections on %s port %d.\n", sys_cfg.name, sys_cfg.ip_address, sys_cfg.port);
            FPS_ERROR_PRINT("%s\n", message);
            emit_event(server_map->p_fims, "Modbus Server", message, 1);
                        
        }
        else
        {
            //read from configure
            sys_cfg.mb = modbus_new_rtu(sys_cfg.serial_device, sys_cfg.baud, sys_cfg.parity, sys_cfg.data, sys_cfg.stop);
            if(sys_cfg.mb == NULL)
            {
                FPS_ERROR_PRINT("Failed to allocate new modbus context.\n");
                rc = 1;
                goto cleanup;
            }
            // set device id from config
            if(-1 == modbus_set_slave(sys_cfg.mb, sys_cfg.device_id))
            {
                FPS_ERROR_PRINT("Serial devices need a valid Device_Id between 1-247.\n");
                rc = 1;
                goto cleanup;
            }
            if(-1 == modbus_connect(sys_cfg.mb))
            {
                char message[1024];
                snprintf(message, 1024, "Modbus Server %s failed to connect on interface %s: %s.\n", sys_cfg.name, sys_cfg.serial_device, modbus_strerror(errno));
                FPS_ERROR_PRINT("%s\n", message);
                emit_event(server_map->p_fims, "Modbus Server", message, 1);
                rc = 1;
                goto cleanup;
            }
            char message[1024];
            snprintf(message, 1024, "Modbus Server %s, serial connection established on interface %s.\n", sys_cfg.name, sys_cfg.serial_device);
            FPS_ERROR_PRINT("%s\n", message);
            emit_event(server_map->p_fims, "Modbus Server", message, 1);
                        
        }

        fd_set connections_with_data;
        fims_socket = server_map->p_fims->get_socket();
        serial_fd = -1;
        header_length = modbus_get_header_length(sys_cfg.mb);

        if(server_socket != -1)
        {
            FD_SET(server_socket, &all_connections);
            fd_max = server_socket;
        }
        else
        {
            serial_fd = modbus_get_socket(sys_cfg.mb);
            if(serial_fd == -1)
            {
                FPS_ERROR_PRINT("Failed to get serial file descriptor.\n");
                rc = 1;
                goto cleanup;
            }
            FD_SET(serial_fd, &all_connections);
            fd_max = serial_fd;
        }

        if(fims_socket != -1)
            FD_SET(fims_socket, &all_connections);
        else
        {
            FPS_ERROR_PRINT("Failed to get fims socket.\n");
            rc = 1;
            goto cleanup;
        }

        fd_max = (fd_max > fims_socket) ? fd_max: fims_socket;
        if (fd_max < reload_fd_max) fd_max = reload_fd_max;

        running = true;
        FPS_ERROR_PRINT("Setup complete: Entering main loop.\n");
        while(running)
        {
            connections_with_data = all_connections;
            FD_ZERO(&rd_connections);
            FD_ZERO(&wr_connections);
            FD_ZERO(&er_connections);

            // Select will block forever until one of the file descriptors has data
            if(-1 == select(fd_max+1, &connections_with_data, &wr_connections, &er_connections, NULL))
            {
                FPS_ERROR_PRINT("server select() failure: %s.\n", strerror(errno));
                break;
            }
            //Loop through file descriptors to see which have data to read
            for(int current_fd = 0; current_fd <= fd_max; current_fd++)
            {
                // if no data on this file descriptor skip to the next one
                if(!FD_ISSET(current_fd, &connections_with_data))
                    continue;
                if(FD_ISSET(current_fd, &er_connections))
                {
                    char message[1024];
                    FPS_ERROR_PRINT("server select() error on fd : %d lets close it .\n", current_fd);
                    snprintf(message, 1024, "Connection closed for fd  %d ", current_fd);
                    emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    close(current_fd);
                    FD_CLR(current_fd, &all_connections);
                    FD_CLR(current_fd, &er_connections);
                    if(current_fd == fd_max)
                        fd_max--;
                    continue;
                }
                
                if(current_fd == server_socket)
                {
                    // A new client is connecting to us
                    socklen_t address_length;
                    struct sockaddr_in client_address;
                    int new_fd;

                    address_length = sizeof(client_address);
                    memset(&client_address, 0, sizeof(address_length));
                    new_fd = accept(server_socket, (struct sockaddr *)&client_address, &address_length);
                    if(new_fd == -1)
                    {
                        FPS_ERROR_PRINT("Error accepting new connections: %s\n", strerror(errno));
                        break;
                    }
                    else
                    {
                        char message[1024];
                        FD_SET(new_fd, &all_connections);
                        fd_max = (new_fd > fd_max) ? new_fd : fd_max;
                        snprintf(message, 1024, "New connection from %s:%d on interface %s",
                        inet_ntoa(client_address.sin_addr), client_address.sin_port, sys_cfg.name);
                        FPS_ERROR_PRINT("%s\n", message);
                        emit_event(server_map->p_fims, "Modbus Server", message, 1);
                    }
                }
                else if(current_fd == fims_socket)
                {
                    // Fims message received
                    fims_message* msg = server_map->p_fims->Receive_Timeout(1*MICROSECOND_TO_MILLISECOND);
                    if(msg == NULL)
                    {
                        if(server_map->p_fims->Connected() == false)
                        {
                            // fims connection closed
                            FPS_ERROR_PRINT("Fims connection closed.\n");
                            FD_CLR(current_fd, &all_connections);
                            break;
                        }
                        else
                            FPS_ERROR_PRINT("No fims message. Select led us to a bad place.\n");
                    }
                    else
                    {
                        process_fims_message(msg, server_map, &sys_cfg, &reload);
                        server_map->p_fims->free_message(msg);
                        if (reload)
                        {
                            running = false; 
                            break;
                        }
                    }
                }
                else if(current_fd == serial_fd)
                {
                    // incoming serial modbus communication
                    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

                    rc = modbus_receive(sys_cfg.mb, query);
                    if (rc > 0)
                    {
                        process_modbus_message(rc, header_length, data, &sys_cfg, server_map, true, query);
                        modbus_reply(sys_cfg.mb, query, rc, server_map->mb_mapping);
                    }
                    else if (rc  == -1)
                    {
                        // Connection closed by the client or error, log error
                        char message[1024];
                        snprintf(message, 1024, "Modbus Server %s, communication error. Closing serial connection to recover. Error: %s\n",
                                                                                      sys_cfg.name, modbus_strerror(errno));
                        FPS_ERROR_PRINT("%s\n", message);
                        emit_event(server_map->p_fims, "Modbus Server", message, 1);
                        modbus_close(sys_cfg.mb);
                        // likely redundant close
                        close(serial_fd);
                        FD_CLR(current_fd, &all_connections);
                        if(current_fd == fd_max)
                            fd_max--;
                        running = false;
                        break;
                    }
                }
                else
                {
                    // incoming tcp modbus communication
                    modbus_set_socket(sys_cfg.mb, current_fd);
                    modbus_set_debug(sys_cfg.mb, 1);

                    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

                    rc = modbus_receive(sys_cfg.mb, query);
                    //printf(" got message from slave [%d]\n",sys_cfg.mb->slave);
                    if (rc > 0)
                    {
                        bool send_reply = true;
                        process_modbus_message(rc, header_length, data, &sys_cfg, server_map, false, query);
#if defined SERVER_DELAY
                        if(sys_cfg.connect_delay > 0) {
                            FPS_ERROR_PRINT("adding server connect delay %d ms.\n", sys_cfg.connect_delay);
                            usleep(sys_cfg.connect_delay*1000);
                            sys_cfg.connect_delay = 0;
                        }

                        else if(sys_cfg.server_delay > 0) {
                            FPS_ERROR_PRINT("adding server delay %d ms.\n", sys_cfg.server_delay);
                            usleep(sys_cfg.server_delay*1000);
                            sys_cfg.server_delay = 0;
                        }
                        else if(sys_cfg.exception > 0) {
                            FPS_ERROR_PRINT("sending server exception %d.\n", sys_cfg.exception);
                            if (sys_cfg.exception < 9 )
                            {
                                modbus_reply_exception(sys_cfg.mb, query, sys_cfg.exception);

                            } else {
                                modbus_reply_exception(sys_cfg.mb, query, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
                            }
                            sys_cfg.exception = 0;
                            send_reply = false;
                        }
                        if(sys_cfg.mdebug > -1) {
                            FPS_ERROR_PRINT("setting modbus debug  %d \n", sys_cfg.mdebug);
                            //modbus_reply_exception(sys_cfg.mb, query, MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
                            if(sys_cfg.mdebug == 1) {
                                modbus_set_debug(sys_cfg.mb, 1);
                            } else {
                                modbus_set_debug(sys_cfg.mb, 0);
                            }
                            sys_cfg.mdebug = -1;
                        }
#endif
                        if(send_reply)modbus_reply(sys_cfg.mb, query, rc, server_map->mb_mapping);
                    }
                    else if (rc  == -1)
                    {
                        // Connection closed by the client or error
                        char message[1024];
                        snprintf(message, 1024, "Modbus Server %s detected a TCP client disconnect.\n", sys_cfg.name);
                        FPS_ERROR_PRINT("%s\n", message);
                        emit_event(server_map->p_fims, "Modbus Server", message, 1);
                        close(current_fd);
                        FD_CLR(current_fd, &all_connections);
                        if(current_fd == fd_max)
                            fd_max--;
                    }
                }
            }
        }

        FPS_ERROR_PRINT("Main loop complete: Entering clean up.\n");

        cleanup:

        if(sys_cfg.eth_dev       != NULL) free(sys_cfg.eth_dev);
        if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
        if(sys_cfg.service       != NULL) free(sys_cfg.service);
        if(sys_cfg.name          != NULL) free(sys_cfg.name);
        if(sys_cfg.serial_device != NULL && !reload) free(sys_cfg.serial_device);
        if(sys_cfg.mb            != NULL && !reload) { modbus_close(sys_cfg.mb); modbus_free(sys_cfg.mb); }
        if(server_map != NULL)
        {
            for (uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
            {
                for (body_map::iterator body_it = it->second->begin(); body_it != it->second->end(); ++body_it)
                {
                    delete []body_it->second.first;
                    delete []body_it->second.second;
                }
                it->second->clear();
                delete(it->second);
            }
            server_map->uri_to_register.clear();
            for (int i = 0; i < Num_Register_Types; i++)
            {
                if(data[i].register_map != NULL)
                    delete []data[i].register_map;
                if(server_map->regs_to_map[i] != NULL) delete [] server_map->regs_to_map[i];
            }
            if (server_map->uris != NULL) delete []server_map->uris;
            if(server_map->p_fims != NULL)
            {
                if(server_map->p_fims->Connected() == true)
                {
                    FD_CLR(server_map->p_fims->get_socket(), &all_connections);
                    server_map->p_fims->Close();
                }
                delete server_map->p_fims;
            }
            if(server_map->mb_mapping != NULL)
                modbus_mapping_free(server_map->mb_mapping);
            delete server_map;
        }
        if (reload)
        {
            reload_fd_max = fd_max; // record it
        }
        for(int fd = 0; fd < fd_max; fd++) {
            if (FD_ISSET(fd, &all_connections)) {
                if (!reload)
                {
                    closeSocket(fd);
                    FD_CLR(fd, &all_connections);
                }
            }
        }
        if (!reload) FD_ZERO(&all_connections);
    } while (reload);

    return rc;
}

/**
 * @brief Copies URIs, to which we must subscribe, from the uri_to_register map to the uris array.
 * 
 */
void sdata::load_uri_array()
{
    num_uris = uri_to_register.size();
    uris = new const char*[num_uris + 1];
    int i = 1; // start with 1 because 0 is used for this processes uri
    for(auto it = uri_to_register.begin(); it != uri_to_register.end(); ++it, i++)
        uris[i] = it->first;
}
