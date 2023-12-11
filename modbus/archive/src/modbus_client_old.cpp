/*
  file: modbus_client.cpp
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
#include <fims/libfims_special.h>
#include <fims/fps_utils.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <unistd.h>
#include <poll.h>
#include "modbus_client.h"

volatile bool running = true;
volatile bool modbus_reconnect = false;
double g_base_time = 0.0;

double  get_time_dbl()
{
    double ltime_dbl;
    timespec c_time;
    clock_gettime(CLOCK_MONOTONIC, &c_time);
    ltime_dbl = (c_time.tv_sec ) + ((double)c_time.tv_nsec / 1000000000.0); 
    return ltime_dbl - g_base_time;
}

void run_sleep_dbl(double next_time)
{
    double stime = next_time - get_time_dbl();
    if(stime > 0.0)
    {
        // poll for time in mS
        poll(NULL,0, stime * 1000);
    }
}
void set_base_time()
{
    double dtime  = get_time_dbl();
    g_base_time = dtime;
}

void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

int establish_connection (connection_config* config)
{
    if (config->ip_address != NULL)
        config->mb = modbus_new_tcp(config->ip_address, config->port);
    else if(config->serial_device != NULL)
        config->mb = modbus_new_rtu(config->serial_device, config->baud, config->parity, config->data, config->stop);
    else
        FPS_ERROR_PRINT("No modbus device defined to connect to either ip address or serial device.\n");

    if (config->mb == NULL)
    {
        FPS_ERROR_PRINT("Unable to create modbus context\n");
        return -1;
    }

    int connection_attempts = 0;
    while (connection_attempts < 5 && modbus_connect(config->mb) == -1)
    {
        connection_attempts++;
        FPS_ERROR_PRINT("Connection attempt %d of 5, Failed to connect to Modbus device: %s\n", connection_attempts, modbus_strerror(errno));
        sleep(1);
    }
    if(connection_attempts >= 5)
    {
        modbus_free(config->mb);
        config->mb = NULL;
        return -1;
    }
    modbus_set_error_recovery(config->mb, MODBUS_ERROR_RECOVERY_PROTOCOL);
    return 0;
}

/* Configuration */
int setup_register_map(cJSON *reg, const component_config *config, int index)
{
    FPS_DEBUG_PRINT("Setting up register map for register type, index: %d\n", index);
    // get the starting offset
    cJSON* start_off = cJSON_GetObjectItem(reg, "starting_offset");
    if (start_off == NULL)
    {
        FPS_ERROR_PRINT("Failed to find Start_Offset in JSON.\n");
        return -1;
    }
    config->reg_datalog[index].start_offset = start_off->valueint;
    if(config->off_by_one)
        config->reg_datalog[index].start_offset--;

    //check to make sure that the number of bytes to read is reasonable
    if (config->reg_datalog[index].start_offset > 65536)
    {
        FPS_ERROR_PRINT("Valid offsets are between 0 - 65536.\n");
        return -1;
    }

    //to get the start address and store it
    cJSON* number_of_regs = cJSON_GetObjectItem(reg, "number_of_registers");
    if (number_of_regs == NULL)
    {
        FPS_ERROR_PRINT("Failed to find Number_of_Registers in JSON.\n");
        return -1;
    }
    config->reg_datalog[index].num_regs = number_of_regs->valueint;

    //check to make sure that the number of bytes to read is reasonable
    if (config->reg_datalog[index].start_offset + config->reg_datalog[index].num_regs > 65536)
    {
        FPS_ERROR_PRINT("Invalid number of registers.\n");
        return -1;
    }

    cJSON* type = cJSON_GetObjectItem(reg, "type");
    if(type == NULL)
    {
        FPS_ERROR_PRINT("Register type not defined in config.\n");
        return -1;
    }
    if(strcmp(type->valuestring, "Coils") == 0)
        config->reg_datalog[index].reg_type = Coil;
    else if(strcmp(type->valuestring, "Discrete Inputs") == 0)
        config->reg_datalog[index].reg_type = Discrete_Input;
    else if(strcmp(type->valuestring, "Input Registers") == 0)
        config->reg_datalog[index].reg_type = Input_Register;
    else if(strcmp(type->valuestring, "Holding Registers") == 0)
        config->reg_datalog[index].reg_type = Holding_Register;
    else
    {
        FPS_ERROR_PRINT("Invalid register type.\n");
        return -1;
    }

    //to get the register name and its offset address
    cJSON *array = cJSON_GetObjectItem(reg, "map");
    if (array == NULL)
    {
        FPS_ERROR_PRINT("Failed to find 'Map' in JSON.\n");
        return -1;
    }

    config->reg_datalog[index].map_size  = cJSON_GetArraySize(array);
    config->reg_datalog[index].register_map = new maps [config->reg_datalog[index].map_size];
    if(config->reg_datalog[index].register_map == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate register map.\n");
        return -1;
    }

    for (unsigned int array_idx = 0; array_idx < config->reg_datalog[index].map_size; array_idx ++)
    {
        cJSON* arg1 = cJSON_GetArrayItem(array, array_idx);
        if (arg1 == NULL)
        {
            FPS_ERROR_PRINT("Failed to get item at %u in maps array.\n", array_idx);
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }

        cJSON* offset = cJSON_GetObjectItem(arg1,"offset");
        if (offset == NULL)
        {
            FPS_ERROR_PRINT("Failed to get object item 'Offset' in JSON.\n");
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }
        config->reg_datalog[index].register_map[array_idx].reg_off = (offset->valueint);
        if (config->off_by_one)
            config->reg_datalog[index].register_map[array_idx].reg_off--;

        //Checking whether the offset is valid
        if (config->reg_datalog[index].register_map[array_idx].reg_off < config->reg_datalog[index].start_offset ||
            config->reg_datalog[index].register_map[array_idx].reg_off > config->reg_datalog[index].start_offset + config->reg_datalog[index].num_regs )
        {
            FPS_ERROR_PRINT("offset %d is outside of registers read %d - %d.\n", config->reg_datalog[index].register_map[array_idx].reg_off, config->reg_datalog[index].start_offset, config->reg_datalog[index].start_offset + config->reg_datalog[index].num_regs);
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }

        cJSON* name_reg = cJSON_GetObjectItem(arg1,"id");
        if (name_reg == NULL)
        {
            FPS_ERROR_PRINT("Failed to get object item 'Name' in JSON.\n");
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }
        config->reg_datalog[index].register_map[array_idx].reg_name = strdup(name_reg->valuestring);

        cJSON* multi_reg = cJSON_GetObjectItem(arg1,"size");
        config->reg_datalog[index].register_map[array_idx].num_regs =
            cJSON_IsNumber(multi_reg) ? multi_reg->valueint : 1;

        cJSON* invert_mask = cJSON_GetObjectItem(arg1,"invert_mask");
        if (invert_mask != NULL) 
        {
            uint64_t invert_mask_value = cJSON_IsString(invert_mask) ? strtoull(invert_mask->valuestring, NULL, 0) : 0x0;
            if (config->reg_datalog[index].register_map[array_idx].num_regs < 4 
                && invert_mask_value >= 1ULL << (16 * config->reg_datalog[index].register_map[array_idx].num_regs))
            {
                FPS_ERROR_PRINT("Invert mask is too large for register in JSON.\n");
                delete[] config->reg_datalog[index].register_map;
                return -1;
            }
            config->reg_datalog[index].register_map[array_idx].invert_mask = invert_mask_value;
        }
        
        cJSON* floating_pt = cJSON_GetObjectItem(arg1, "float");
        config->reg_datalog[index].register_map[array_idx].floating_pt = cJSON_IsTrue(floating_pt);

        int string_type = 0;

        cJSON* bit_field = cJSON_GetObjectItem(arg1, "bit_field");
        config->reg_datalog[index].register_map[array_idx].bit_field = (cJSON_IsTrue(bit_field) && ++string_type);

        cJSON* indv_bits = cJSON_GetObjectItem(arg1, "individual_bits");
        config->reg_datalog[index].register_map[array_idx].individual_bits = (cJSON_IsTrue(indv_bits) && ++string_type);

        cJSON* enum_type = cJSON_GetObjectItem(arg1, "enum");
        config->reg_datalog[index].register_map[array_idx].enum_type = (cJSON_IsTrue(enum_type) && ++string_type);

        cJSON* random_enum_type = cJSON_GetObjectItem(arg1, "random_enum");
        config->reg_datalog[index].register_map[array_idx].random_enum_type = (cJSON_IsTrue(random_enum_type) && ++string_type);

        if(string_type > 1)
        {
            FPS_ERROR_PRINT("More than one bit_strings type defined.\n");
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }

        cJSON* bit_strings = cJSON_GetObjectItem(arg1, "bit_strings");
        if (string_type == 1)
        {
            if (!cJSON_IsArray(bit_strings))
            {
                FPS_ERROR_PRINT("No bit_strings object to define bit_field values.\n");
                delete[] config->reg_datalog[index].register_map;
                return -1;
            }

            int array_size = cJSON_GetArraySize(bit_strings);
            if (array_size == 0)
            {
                FPS_ERROR_PRINT("No bit_strings included in bit_strings array.\n");
                delete[] config->reg_datalog[index].register_map;
                return -1;
            }

            config->reg_datalog[index].register_map[array_idx].bit_strings = new char*[array_size];
            memset(config->reg_datalog[index].register_map[array_idx].bit_strings, 0, sizeof(char*) * array_size);
            config->reg_datalog[index].register_map[array_idx].num_strings = array_size;

            if(config->reg_datalog[index].register_map[array_idx].random_enum_type)
            {
                for(int i = 0; i < array_size; i++)
                {
                     cJSON* bit_string_item = cJSON_GetArrayItem(bit_strings, i);
                     if(bit_string_item != NULL)
                     {
                         cJSON* value = cJSON_GetObjectItem(bit_string_item, "value");
                         cJSON* bit_string = cJSON_GetObjectItem(bit_string_item, "string");
                         if(cJSON_IsNumber(value) && cJSON_IsString(bit_string))
                         {
                             config->reg_datalog[index].register_map[array_idx].bit_strings[i] = strdup(bit_string->valuestring);
                             config->reg_datalog[index].register_map[array_idx].random_enum.insert(std::pair<int, char*>(value->valueint,config->reg_datalog[index].register_map[array_idx].bit_strings[i]));
                         }
                     }
                }
            }
            else
            {
                for(int i = 0; i < array_size; i++)
                {
                     cJSON* bit_string_item = cJSON_GetArrayItem(bit_strings, i);
                     if(bit_string_item != NULL && bit_string_item->valuestring != NULL)
                         config->reg_datalog[index].register_map[array_idx].bit_strings[i] = strdup(bit_string_item->valuestring);
                }
            }
        }
        else if (string_type == 0 && bit_strings != NULL)
        {
            FPS_ERROR_PRINT("Bit_strings object defined but no corresponding bit_field type values.\n");
            delete[] config->reg_datalog[index].register_map;
            return -1;
        }

        cJSON* is_signed = cJSON_GetObjectItem(arg1,"signed");
        config->reg_datalog[index].register_map[array_idx].sign = cJSON_IsTrue(is_signed);

        cJSON* scale_value = cJSON_GetObjectItem(arg1,"scale");
        config->reg_datalog[index].register_map[array_idx].scale = cJSON_IsNumber(scale_value) ? (scale_value->valuedouble) : 0.0;

        cJSON* shift_value = cJSON_GetObjectItem(arg1,"shift");
        config->reg_datalog[index].register_map[array_idx].shift = cJSON_IsNumber(shift_value) ? (shift_value->valueint) : 0;
    }
    return 1;
}

bool query_device(cJSON *root, const connection_config *connection, const component_config *config)
{
    int rc;
    uint8_t tab_coil[MODBUS_MAX_READ_BITS];
    uint16_t tab_reg[MODBUS_MAX_READ_REGISTERS];

    //get the values of the registers and print them in json format//
    //Holding and Input registers are first classified on the basis of their address range.
    //Tab register is defined and modbus is read with error checking code added.
    //Every individual register is then scaled according to the scale factor and then added to the cJSON object.
    //Also, before scaling, check for 16 bit and 32 bit read is done and if it is a 32 bit, then macro is used for converting 2 16 bit int to 32 bit int

    for ( int i = 0; i < config->reg_cnt; i++ )
    {  
        double s_time;

        config->stats[config->query_read_stat]->start();

        lock_connection((connection_config*)connection, (component_config*)config);
        if (config->reg_datalog[i].reg_type == Holding_Register)
            rc = modbus_read_registers(connection->mb, config->reg_datalog[i].start_offset, config->reg_datalog[i].num_regs, tab_reg);
        else if (config->reg_datalog[i].reg_type == Input_Register)
            rc = modbus_read_input_registers(connection->mb, config->reg_datalog[i].start_offset, config->reg_datalog[i].num_regs , tab_reg);
        else if ((config->reg_datalog[i].reg_type == Coil))
            rc = modbus_read_bits(connection->mb, config->reg_datalog[i].start_offset, config->reg_datalog[i].num_regs , tab_coil);
        else if (config->reg_datalog[i].reg_type == Discrete_Input)
            rc = modbus_read_input_bits(connection->mb, config->reg_datalog[i].start_offset, config->reg_datalog[i].num_regs, tab_coil);
        else
        {
            unlock_connection((connection_config*)connection);
            FPS_ERROR_PRINT("Invalid input type.\n");
            continue;
        }
        
        if (rc == -1)
        {
            const char* mberror = strdup(modbus_strerror(errno));
            unlock_connection((connection_config*)connection);
            FPS_ERROR_PRINT("%s: modbus error: %s, type %s, start_offset %d, num_regs %d\n"
                        , config->id, mberror
                        , getRegisterType(config->reg_datalog[i].reg_type)
                        , config->reg_datalog[i].start_offset
                        , config->reg_datalog[i].num_regs);
            free((void *)mberror);
            return false;
        }

        unlock_connection((connection_config*)connection);
        s_time = config->stats[config->query_read_stat]->addVal();

        if(config->reg_datalog[i].reg_type == Holding_Register || config->reg_datalog[i].reg_type == Input_Register)
        {
            // Handle registers
            double temp_reg;
            uint64_t temp_reg_int;
            for (unsigned int x = 0; x < config->reg_datalog[i].map_size; x++)
            {
                int idx = config->reg_datalog[i].register_map[x].reg_off - config->reg_datalog[i].start_offset;
                if (config->reg_datalog[i].register_map[x].num_regs == 1)
                {
                    temp_reg_int = tab_reg[idx] ^ config->reg_datalog[i].register_map[x].invert_mask;
                    // single register 16-bit short
                    if (config->reg_datalog[i].register_map[x].sign == true)
                        temp_reg = static_cast<double>(static_cast<int16_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else if(config->reg_datalog[i].register_map[x].num_regs == 2)
                {
                    //Combine 2 registers into 32-bit value
                    if (config->byte_swap == true)
                        temp_reg_int = (tab_reg[idx] + (tab_reg[idx +1] << 16));
                    else
                        temp_reg_int = MODBUS_GET_INT32_FROM_INT16(tab_reg, idx);
                    temp_reg_int ^= config->reg_datalog[i].register_map[x].invert_mask;

                    if (config->reg_datalog[i].register_map[x].floating_pt == true)
                    {
                        float *temp_reg_ptr = reinterpret_cast<float *>((int *)&temp_reg_int);
                        temp_reg = static_cast<double>(*temp_reg_ptr);
                    }
                    else if(config->reg_datalog[i].register_map[x].sign == true)
                    {
                        temp_reg = static_cast<double>(static_cast<int32_t>(temp_reg_int));
                    }
                    else
                    {
                        temp_reg = static_cast<double>(temp_reg_int);
                    }
                }
                else if(config->reg_datalog[i].register_map[x].num_regs == 4)
                {
                    if (config->byte_swap == true)
                        temp_reg_int = (static_cast<uint64_t>(tab_reg[idx]) +
                                       (static_cast<uint64_t>(tab_reg[idx+1]) << 16) +
                                       (static_cast<uint64_t>(tab_reg[idx+2]) << 32) +
                                       (static_cast<uint64_t>(tab_reg[idx+3]) << 48));
                    else
                        temp_reg_int = ((static_cast<uint64_t>(MODBUS_GET_INT32_FROM_INT16(tab_reg, idx)) << 32) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT32_FROM_INT16(tab_reg, idx+2))));
                    temp_reg_int ^= config->reg_datalog[i].register_map[x].invert_mask;

                    if(config->reg_datalog[i].register_map[x].sign == true)
                        temp_reg = static_cast<double>(static_cast<int64_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else
                {
                    //not currently supported register count
                    FPS_ERROR_PRINT("Invalid Number of Registers defined for %s.\n", config->reg_datalog[i].register_map[x].reg_name);
                    continue;
                }

                //Only want to scale if scale value is present and not == 1
                if (config->reg_datalog[i].register_map[x].scale != 0.0)
                    temp_reg /= config->reg_datalog[i].register_map[x].scale;
                if (config->reg_datalog[i].register_map[x].shift != 0)
                    temp_reg += config->reg_datalog[i].register_map[x].shift;
                if(config->reg_datalog[i].register_map[x].bit_field == true)
                {
                    cJSON* bit_array = cJSON_CreateArray();
                    uint64_t bit_compare = 1;
                    for(unsigned int j = 0; j < (config->reg_datalog[i].register_map[x].num_regs * 16); j++)
                    {
                        if((temp_reg_int & (bit_compare<<j)) > 0)
                        {
                            cJSON* bit_obj = cJSON_CreateObject();
                            cJSON_AddNumberToObject(bit_obj, "value", j);
                            if(j >= config->reg_datalog[i].register_map[x].num_strings || config->reg_datalog[i].register_map[x].bit_strings[j] == NULL)
                                cJSON_AddStringToObject(bit_obj, "string", "Unknown");
                            else
                                cJSON_AddStringToObject(bit_obj, "string", config->reg_datalog[i].register_map[x].bit_strings[j]);
                            cJSON_AddItemToArray(bit_array, bit_obj);
                        }
                    }
                    cJSON_AddItemToObject(root, config->reg_datalog[i].register_map[x].reg_name, bit_array);
                }
                else if(config->reg_datalog[i].register_map[x].individual_bits == true)
                {
                    config->reg_datalog[i].register_map[x].data = temp_reg_int;
                    uint64_t bit_compare = 1;
                    for(unsigned int j = 0; j < config->reg_datalog[i].register_map[x].num_strings; j++)
                    {
                        if(config->reg_datalog[i].register_map[x].bit_strings[j] != NULL)
                        {
                            bool bool_reg = ((temp_reg_int & (bit_compare<<j)) > 0);
                            cJSON_AddBoolToObject(root, config->reg_datalog[i].register_map[x].bit_strings[j], bool_reg);
                        }
                    }
                }
                else if(config->reg_datalog[i].register_map[x].enum_type == true)
                {
                    cJSON* enum_array = cJSON_CreateArray();
                    cJSON* enum_obj = cJSON_CreateObject();
                    cJSON_AddNumberToObject(enum_obj, "value", temp_reg_int);
                    if(temp_reg_int < config->reg_datalog[i].register_map[x].num_strings && config->reg_datalog[i].register_map[x].bit_strings[temp_reg_int] != NULL)
                        cJSON_AddStringToObject(enum_obj, "string", config->reg_datalog[i].register_map[x].bit_strings[temp_reg_int]);
                    else
                        cJSON_AddStringToObject(enum_obj, "string", "Unknown");
                    cJSON_AddItemToArray(enum_array, enum_obj);
                    cJSON_AddItemToObject(root, config->reg_datalog[i].register_map[x].reg_name, enum_array);
                }
                else if(config->reg_datalog[i].register_map[x].random_enum_type == true)
                {
                    cJSON* enum_array = cJSON_CreateArray();
                    cJSON* enum_obj = cJSON_CreateObject();
                    cJSON_AddNumberToObject(enum_obj, "value", temp_reg_int);
                    std::map<int,char*>::iterator random_enum_itr = config->reg_datalog[i].register_map[x].random_enum.find(temp_reg_int);
                    if(random_enum_itr != config->reg_datalog[i].register_map[x].random_enum.end() && random_enum_itr->second != NULL)
                        cJSON_AddStringToObject(enum_obj, "string", random_enum_itr->second);
                    else
                        cJSON_AddStringToObject(enum_obj, "string", "Unknown");
                    cJSON_AddItemToArray(enum_array, enum_obj);
                    cJSON_AddItemToObject(root, config->reg_datalog[i].register_map[x].reg_name, enum_array);
                }
                else if(config->heartbeat_enabled && strncmp(config->read_name, config->reg_datalog[i].register_map[x].reg_name, strlen(config->read_name)) == 0)
                {
                    config->reg_datalog[i].register_map[x].data = temp_reg_int;
                    cJSON_AddNumberToObject(root, config->reg_datalog[i].register_map[x].reg_name, temp_reg);
                }
                else
                    cJSON_AddNumberToObject(root, config->reg_datalog[i].register_map[x].reg_name, temp_reg);
            }
        }
        else if(config->reg_datalog[i].reg_type == Discrete_Input || config->reg_datalog[i].reg_type == Coil)
        {
            // Handle Coils
            for (unsigned int x = 0; x < config->reg_datalog[i].map_size; x++)
            {
                bool temp_reg = static_cast<bool>(tab_coil[config->reg_datalog[i].register_map[x].reg_off -  config->reg_datalog[i].start_offset]);
                cJSON_AddBoolToObject(root, config->reg_datalog[i].register_map[x].reg_name, temp_reg);
            }
        }
        if(config->add_timings)
        {
            char nbuf[64];
            snprintf(nbuf , sizeof nbuf,"type_%02d_time_ms",  i);
            cJSON_AddNumberToObject(root, nbuf, s_time *1000.0);
        }
    }

    //Code for adding timestamp
    char buffer[64],time_str[128];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm local_tm;
    tzset();
    strftime(buffer,64,"%m-%d-%Y %T.", localtime_r(static_cast<time_t*>(&tv.tv_sec), &local_tm));
    snprintf(time_str, sizeof time_str,"%s%ld",buffer,tv.tv_usec);
    cJSON_AddStringToObject(root, "Timestamp", time_str);
    return true;
}

/* Modbus Communication Functions */
int set_coil (bool temp_coil_val, maps* reg, connection_config* connection, component_config *component)
{
    int rc;
    lock_connection(connection, component);
    component->stats[component->write_stat]->start();
    rc = modbus_write_bit(connection->mb, reg->reg_off, temp_coil_val);
    component->stats[component->write_stat]->addVal();
    const char* mberror = NULL;
    if(rc < 0) 
    {
        mberror = strdup(modbus_strerror(errno));
    }
    unlock_connection(connection);
    if(rc < 0) 
    {
        FPS_ERROR_PRINT("Set_Coil Modbus Error: %s  register: %s offset: %d value: %s \n", mberror, reg->reg_name, reg->reg_off, temp_coil_val?"true":"false");
        free((void *)mberror);
    }
    return rc;
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
                        if(config->reg_datalog[reg_set].register_map[i].bit_strings[j] != NULL)
                            if(strcmp(name, config->reg_datalog[reg_set].register_map[i].bit_strings[j]) == 0)
                                return &config->reg_datalog[reg_set].register_map[i];
                    }
                }
            }
        }
    }
    return NULL;
}

int set_register (double temp_reg_val, maps* reg, connection_config* connection, component_config* component)
{
    int rc = 0;
    char* mberror = NULL;

    if(reg->shift != 0)
        temp_reg_val -= reg->shift;
    if(reg->scale != 0.0)
        temp_reg_val *= reg->scale;

    lock_connection(connection, component);
    component->stats[component->write_reg]->start();
    if(reg->num_regs == 2)
    {
        uint16_t regs[2];
        uint32_t temp_reg;
        if(reg->sign)
            temp_reg = static_cast<int32_t>(lround(temp_reg_val));
        else
            temp_reg = static_cast<uint32_t>(lround(temp_reg_val));

        //TODO figure out why this is backwards
        if(!component->byte_swap)
        {
            regs[0] = (temp_reg >> 16) & 0xffff;
            regs[1] = temp_reg & 0xffff;
        }
        else
        {
            regs[0] = temp_reg & 0xffff;
            regs[1] = (temp_reg >> 16) & 0xffff;
        }
        rc = modbus_write_registers(connection->mb, reg->reg_off, 2, regs);
        if(rc < 0) 
        {
            asprintf(&mberror,"Set_Register Modbus Error: %s  register: %s offset: %d reg[0]: %04x reg[1]: %04x \n",
                        modbus_strerror(errno), 
                        reg->reg_name, reg->reg_off, regs[0], regs[1]);
        }

    }
    else
    {
        if(reg->sign)
            rc = modbus_write_register(connection->mb, reg->reg_off, static_cast<uint16_t>(lround(temp_reg_val)));
        else
            rc = modbus_write_register(connection->mb, reg->reg_off, static_cast<int16_t>(lround(temp_reg_val)));   
        if(rc < 0) 
        {
            asprintf(&mberror,"Set_Register Modbus Error: %s  register: %s offset: %d sign: %d reg: %04x \n",
                            modbus_strerror(errno), 
                            reg->reg_name, reg->reg_off, reg->sign,  
                            reg->sign? static_cast<uint16_t>(lround(temp_reg_val)):static_cast<int16_t>(lround(temp_reg_val))
                            );
        }
    }
    component->stats[component->write_stat]->addVal();

    unlock_connection(connection);
    if(rc < 0)
    {
        FPS_ERROR_PRINT(mberror);
        free((void *)mberror);
    }
    return rc;
}
    
/* FIMS Communication */
void *handler_loop(void *arguments)
{
    thread_config *config = (thread_config *)arguments;
    connection_config *connection = config->connection;
    component_config *component = config->component;
    component->last_debounce_time = get_time_dbl();
    component->next_debounce_time = component->last_debounce_time + component->debounce;
    FPS_DEBUG_PRINT("%s: inside handler loop\n", component->id);
    FPS_DEBUG_PRINT("%s, %s\n", connection->name, connection->ip_address);
    char uri[100];
    char* sub = uri;
    // subscribing to the URI
    sprintf(uri,"/components/%s", component->id);   
    // Connecting to the FIMS server and querying the controller after regular intervals
    int fims_connect = 0;
    component->p_fims = new fims();
    if(!component->p_fims)
    {
        FPS_ERROR_PRINT("%s: Failed to allocate Fims connection.\n", component->id);
        return (void*)-1;
    }

    while(running)
    {
        if(!component->p_fims->Connected())
        {
            if((fims_connect<5)&&(component->p_fims->Connect(component->id)== false))
            {
                run_sleep_dbl(get_time_dbl() + 0.1); // 100mS
                fims_connect++;
                if(fims_connect >= 5)
                {
                    FPS_ERROR_PRINT("%s: Failed to connect to Fims Server.\n", component->id);
                    running = false;
                }
                continue;
            }
            fims_connect = 0;
            if(component->p_fims->Subscribe((const char**)(&sub), 1) == false)
            {
                FPS_ERROR_PRINT("Component %s Failed to subscribe to URI [%s].\n", component->id, sub);
                component->p_fims->Close();
                run_sleep_dbl(get_time_dbl() + 0.1); // 100mS
                // try again in a few
                continue;
            }

            char message[1024];
            snprintf(message, 1024, "%s: Modbus Connection established to device %s", component->id, connection->name);
            emit_event(component->p_fims, "Modbus Client", message, 1);
            FPS_ERROR_PRINT("%s\n", message);
            // reset tims we dont want to add the initial connect to the timings
            component->overrun = 0;
        }

        if(component->p_fims->Connected())
        {
            handle_messages(component->p_fims, uri, connection, component);
            // perform function to go through throttle map and release all held messages over debounce time
            double now = get_time_dbl(); 
            // only do this every debounce time increment
            if( component->debounce > 0 && component->next_debounce_time  < now)
            {
                component->last_debounce_time = now;
                component->next_debounce_time = now + component->debounce;

                std::map<std::string, modbus_var_traffic*>::iterator it;
                for (it = component->var_map.begin(); it != component->var_map.end(); it++)
                {
                    // reduce next debounce
                    if(it->second->last_write + component->debounce <  component->next_debounce_time)
                        if(it->second->last_write + component->debounce > now)
                            component->next_debounce_time = it->second->last_write + component->debounce;

                    if (it->second->nxt_val != it->second->cur_val && it->second->last_write + component->debounce < now)
                    {
                        // Do not flag the buffer as "ok" if the write did not succeed
                        int rc = set_register(it->second->nxt_val, it->second->reg, connection, component);
                        it->second->last_write = now; // we still want to try to write, but wait a little
                        if(rc >= 0)
                        {
                            it->second->cur_val = it->second->nxt_val;
                        }
                    }
                }
            }
        }
    }
    return (void *)NULL;
}

int handle_messages(fims* p_fims, char* base_uri,  connection_config* connection, component_config* component)
{
    cJSON *mydata = NULL;
    unsigned int us_timeout = 10000; // convert to us

    // Waiting for the message to be received
    fims_message* msg = p_fims->Receive_Timeout(us_timeout);
    if(msg != NULL)
    {
        if(strncmp(base_uri, msg->uri, strlen(base_uri)) !=0 )
        {
            //This message is not for us so ignore it
            p_fims->free_message(msg);
            return 0;
        }
        //Adding condition for get and set//
        if (strcmp(msg->method, "get" ) == 0)
        {
            pthread_mutex_lock(&component->pong_lock);
            if (component->query_pong != NULL)
            {
                cJSON_Delete(component->handler_pong);
                component->handler_pong = component->query_pong;
                component->query_pong = NULL;
            }
            mydata = component->handler_pong;
            pthread_mutex_unlock(&component->pong_lock);

            if(mydata != NULL && msg->replyto != NULL)
            {
                if(msg->nfrags == 2)
                {
                    if(msg->replyto)
                    {
                        char* body = cJSON_PrintUnformatted(mydata);
                        p_fims->Send("set", msg->replyto, NULL, body);
                        free(body);
                    }
                }
                else if(msg->nfrags == 3)
                {
                    if(msg->replyto)
                    {
                        cJSON *answer= cJSON_GetObjectItem(mydata, msg->pfrags[2]);
                        if (answer != NULL)
                        {
                            char *body = cJSON_PrintUnformatted(answer);
                            p_fims->Send("set", msg->replyto, NULL, body);
                            free(body);
                        }
                        else
                        {
                            if (strcmp(msg->pfrags[2],"_stats")==0 || strcmp(msg->pfrags[2],"_rstats")== 0)
                            {
                                cJSON*stats = cJSON_CreateObject();

                                cJSON_AddNumberToObject(stats,"freqency(ms)", component->frequency);
                                cJSON_AddNumberToObject(stats,"overrun", component->overrun);
                                cJSON_AddNumberToObject(stats,"max_per_time(ms)",
                                                (double)(component->stats[component->per_stat]->getMax()));
                                cJSON_AddNumberToObject(stats,"avg_per_time(ms)", 
                                                (double)(component->stats[component->per_stat]->getAvg()));
                                cJSON_AddNumberToObject(stats,"max_missed_time(ms)",
                                                (double)(component->stats[component->per_missed_stat]->getMax()));
                                cJSON_AddNumberToObject(stats,"avg_missed_time(ms)", 
                                                (double)(component->stats[component->per_missed_stat]->getAvg()));
                                cJSON_AddNumberToObject(stats,"avg_write(ms)", 
                                            (double)(component->stats[component->write_stat]->getAvg()));
                                cJSON_AddNumberToObject(stats,"max_write(ms)", 
                                            (double)(component->stats[component->write_stat]->getMax()));

                                cJSON_AddNumberToObject(stats,"avg_write_reg(ms)", 
                                            (double)(component->stats[component->write_reg]->getAvg()));
                                cJSON_AddNumberToObject(stats,"max_write_reg(ms)", 
                                            (double)(component->stats[component->write_reg]->getMax()));
                                    cJSON_AddNumberToObject(stats,"avg_query(ms)", 
                                            (double)(component->stats[component->query_stat]->getAvg()));
                                cJSON_AddNumberToObject(stats,"max_query(ms)", 
                                            (double)(component->stats[component->query_stat]->getMax()));
                                cJSON_AddNumberToObject(stats,"avg_query_read(ms)", 
                                            (double)(component->stats[component->query_read_stat]->getAvg()));
                                cJSON_AddNumberToObject(stats,"max_query_read(ms)", 
                                            (double)(component->stats[component->query_read_stat]->getMax()));
                                // reset stats
                                if (strcmp(msg->pfrags[2],"_rstats")== 0)
                                {
                                    component->overrun = 0;
                                    component->stats[component->per_stat]->clear();
                                    component->stats[component->per_missed_stat]->clear();
                                    component->stats[component->write_stat]->clear();
                                    component->stats[component->query_stat]->clear();
                                    component->stats[component->query_read_stat]->clear();
                                }
                                char *body = cJSON_PrintUnformatted(stats);
                                p_fims->Send("set", msg->replyto, NULL, body);
                                free(body);
                                cJSON_Delete(stats);
                            }
                            else
                                p_fims->Send ("set", msg->replyto, NULL, "Error: requested register not found.");
                        }
                    }
                }
                else
                    if(msg->replyto)
                        p_fims->Send ("set", msg->replyto, NULL, "Error: URI not valid.");
            }
        }
        else if (strcmp(msg->method, "set") == 0)
        {
            if(msg->body != NULL)
            {
                cJSON* body = cJSON_Parse(msg->body);
                if(body == NULL || body->type != cJSON_Object)
                {
                    if(body != NULL)
                        cJSON_Delete(body);
                    if(msg->replyto != NULL)
                        p_fims->Send("set", msg->replyto, NULL, "Error: Invalid JSON in message body");
                    p_fims->free_message(msg);
                    return 0;
                }
                bool all_passed = true;
                double now = get_time_dbl();

                if (msg->nfrags == 3)
                {
                    cJSON* temp = cJSON_CreateObject();
                    cJSON_AddItemToObject(temp, msg->pfrags[2], body);
                    body = temp;
                }

                for(cJSON* cur = body->child; cur != NULL; cur = cur->next)
                {
                    if(!cJSON_IsObject(cur) || cur->string == NULL)
                    {
                        FPS_ERROR_PRINT("Malformed body of child object for reg set.\n");
                        all_passed = false;
                        continue;
                    }

                    cJSON* value = cJSON_GetObjectItem(cur, "value");
                    if( value == NULL)
                    {
                        FPS_ERROR_PRINT("Got child object \"%s\" with no \"value\" object in it for set command.\n", cur->string);
                        all_passed = false;
                        continue;
                    }
// Question why not set up the va_traffic map at component config time
// Question : a duplicate name in the Holding Regs / Coils may give us the wrong register. 
                    maps* reg;
                    std::map<std::string, modbus_var_traffic*>::iterator it = component->var_map.find(cur->string);
                    bool reg_cached = (it != component->var_map.end());
                    if (reg_cached)
                        reg = it->second->reg;
                    else
                        reg = get_register(cur->string, Holding_Register, component);

                    int rc = 0;
                    if(reg == NULL) // if register not found in holding registers check coils
                    {
                        //TODO add support for debounce of coils
                        maps* coil = get_register(cur->string, Coil, component);
                        if(coil == NULL)
                        {
                            FPS_ERROR_PRINT("Invalid URI %s for reg set.\n", cur->string);
                            all_passed = false;
                            continue;
                        }
                        if(cJSON_IsBool(value))
                            rc = set_coil(cJSON_IsTrue(value), coil, connection, component);
                        else if(cJSON_IsNumber(value) && (value->valueint == 0 || value->valueint == 1))
                            rc = set_coil(value->valueint, coil, connection, component);
                        else
                        {
                            FPS_ERROR_PRINT("Error: Invalid value type for coil %s", cur->string);
                            all_passed = false;
                            continue;
                        }
                    }
                    else if(reg->individual_bits == true) // override for individual_bits write
                    {
                        uint64_t write_val = 0;
                        if(cJSON_IsBool(value))
                            write_val = (cJSON_IsTrue(value));
                        else if (cJSON_IsNumber(value) && (value->valueint == 0 || value->valueint == 1))
                            write_val = value->valueint;
                        else
                        {
                            FPS_ERROR_PRINT("Error: Invalid value type for individual bits.");
                            all_passed = false;
                            continue;
                        }

                        size_t index = 0;
                        for(index = 0; index < reg->num_strings; index++)
                        {
                            if(reg->bit_strings[index] != NULL)
                            {
                                if(strcmp(cur->string, reg->bit_strings[index]) == 0)
                                {
                                    uint64_t reg_val = (reg->data & (0xfffffffffffffff ^ (1<<index))) | (write_val<<index);
                                    rc = set_register(reg_val, reg, connection, component);
                                }
                            }
                        }
                    }
                    else if(cJSON_IsNumber(value))
                    {
                        // throttle message and maintain throttling map here.
                        if (reg_cached)
                        {
                            if (it->second->last_write + component->debounce < now)
                            {
                                rc = set_register(value->valuedouble, it->second->reg, connection, component);

                                it->second->last_write = now;
                                // only set values if the write passed
                                if(rc >= 0)
                                {
                                    it->second->cur_val = value->valuedouble;
                                    it->second->nxt_val = value->valuedouble;
                                }
                            }
                            else
                            {
                                rc = 0;
                                it->second->nxt_val = value->valuedouble;
                            }
                        }
                        else // add to cache
                        {
                            // allocate new modbus_var_traffic
                            rc = set_register(value->valuedouble, reg, connection, component);
                            modbus_var_traffic *tmp = new modbus_var_traffic;
                            if (tmp == NULL)
                            {
                                FPS_ERROR_PRINT("Error: Memory Allocation Failure for write rate throttling map.\n");
                                continue;
                            }
                            tmp->last_write = now;
                            tmp->cur_val = value->valuedouble;
                            tmp->nxt_val = value->valuedouble;
                            tmp->reg = reg;
                            component->var_map.insert(std::pair<std::string, modbus_var_traffic*>(cur->string, tmp));
                        }
                    }
                    else
                    {
                        FPS_ERROR_PRINT("Error: Invalid value type for register %s\n", cur->string);
                        all_passed = false;
                        continue;
                    }
                    if(rc == -1)
                        all_passed = false;
                }
                cJSON_Delete(body);
                //To display the changed register value on fims_listen
                if(msg->replyto != NULL)
                {
                    if(all_passed)
                        p_fims->Send("set", msg->replyto, NULL, msg->body);
                    else
                    {
                        char error_msg[256];
                        sprintf(error_msg, "Error: failed to set one or more of the registers requested");
                        p_fims->Send("set", msg->replyto, NULL, error_msg);
                    }
                }
            }
            else if(msg->replyto != NULL)
                p_fims->Send("set", msg->replyto, NULL, "Error: set did not contain body");
        }
        p_fims->free_message(msg);
    }
    return 1;
}

int get_conn_info(cJSON *config, connection_config *connection)
{
    cJSON* connection_json = cJSON_GetObjectItem(config, "connection");
    cJSON* name = cJSON_GetObjectItem(connection_json, "name");
    if(name == NULL)
    {
        FPS_ERROR_PRINT("No Process Name Defined.\n");
        return -1;
    }
    connection->name = strdup(name->valuestring);
    cJSON* ip_addr = cJSON_GetObjectItem(connection_json, "ip_address");
    cJSON* serial_device = cJSON_GetObjectItem(connection_json, "serial_device");
    if (ip_addr != NULL)
    {
        cJSON* port = cJSON_GetObjectItem(connection_json, "port");
        connection->port = (port == NULL) ? 502 : port->valueint;
        if(connection->port > 65535 || connection->port < 0)
        {
            FPS_ERROR_PRINT("Invalid port defined.\n");
            free(connection->name);
            return -1;
        }
        if(ip_addr->valuestring == NULL)
        {
            FPS_ERROR_PRINT("Invalid ip address string.\n");
            free(connection->name);
            return -1;
        }
        connection->ip_address = strdup(ip_addr->valuestring);
        if(connection->ip_address == NULL)
        {
            FPS_ERROR_PRINT("Allocations error ip address string.\n");
            free(connection->name);
            return -1;
        }
        FPS_DEBUG_PRINT("Connection to use TCP. IP: %s, Port: %d\n", connection->ip_address, connection->port);
    }
    else if (serial_device != NULL)
    {
        FPS_DEBUG_PRINT("Connection to use Serial device\n");
        cJSON* baud_rate = cJSON_GetObjectItem(connection_json, "baud_rate");
        connection->baud = (baud_rate == NULL) ? 115200 : baud_rate->valueint;
        cJSON* parity_str = cJSON_GetObjectItem(connection_json, "parity");
        connection->parity = 'N';
        // if parity field exists make sure it is valid
        if(parity_str != NULL)
        {
            if(parity_str->valuestring == NULL)
            {
                FPS_ERROR_PRINT("Invalid Parity value in json\n");
                free(connection->name);
                return -1;
            }
            else if(strcmp("none", parity_str->valuestring) == 0)
                connection->parity = 'N';
            else if(strcmp("even", parity_str->valuestring) == 0)
                connection->parity = 'E';
            else if(strcmp("odd", parity_str->valuestring) == 0)
                connection->parity = 'O';
            else
            {
                FPS_ERROR_PRINT("Invalid Parity value in json\n");
                free(connection->name);
                return -1;
            }
        }

        cJSON* data_bits = cJSON_GetObjectItem(connection_json, "data_bits");
        connection->data = (data_bits == NULL) ? 8 : data_bits->valueint;
        if(connection->data > 8 || connection->data < 5)
        {
            FPS_ERROR_PRINT("Invalid number of data bits.\n");
            free(connection->name);
            return -1;
        }

        cJSON* stop_bits = cJSON_GetObjectItem(connection_json, "stop_bits");
        connection->stop = (stop_bits == NULL) ? 1 : stop_bits->valueint;
        if(connection->stop > 2 || connection->stop < 1)
        {
            FPS_ERROR_PRINT("Invalid number of stop bits.\n");
            free(connection->name);
            return -1;
        }

        if(serial_device->valuestring == NULL)
        {
            FPS_ERROR_PRINT("Invalid string for serial device.\n");
            free(connection->name);
            return -1;
        }
        connection->serial_device = strdup(serial_device->valuestring);
        if(connection->serial_device == NULL)
        {
            FPS_ERROR_PRINT("Allocation error string for serial device.\n");
            free(connection->name);
            return -1;
        }
    }
    else
    {
        FPS_ERROR_PRINT("Failed to find either an IP_Address or Serial_Device in JSON.\n");
        free(connection->name);
        return -1;
    }
    return 0;
}

int get_components(cJSON *config, component_config *&component_addr)
{
    int components_size = cJSON_GetArraySize(cJSON_GetObjectItem(config, "components"));
    component_addr = new component_config[components_size];
    component_config *components = component_addr;

    for (int i = 0; i < components_size; i++)
    {
        cJSON *component = cJSON_GetArrayItem(cJSON_GetObjectItem(config, "components"), i);

        cJSON *id_name = cJSON_GetObjectItem(component, "id");
        components[i].id = (cJSON_IsString(id_name)) ? strdup(id_name->valuestring) : NULL;
        if (components[i].id == NULL)
        {
            FPS_ERROR_PRINT("No component id provided.\n");
            return -1;
        }

        cJSON *byte_swap = cJSON_GetObjectItem(component, "byte_swap");
        components[i].byte_swap = cJSON_IsTrue(byte_swap);

        cJSON *off_by_one = cJSON_GetObjectItem(component, "off_by_one");
        components[i].off_by_one = cJSON_IsTrue(off_by_one);

        cJSON *add_timings = cJSON_GetObjectItem(component, "add_timings");
        components[i].add_timings = cJSON_IsTrue(add_timings);

        cJSON *freq = cJSON_GetObjectItem(component, "frequency");
        components[i].frequency = cJSON_IsNumber(freq) ? freq->valueint : 500;

        cJSON *offset = cJSON_GetObjectItem(component, "offset_time");
        components[i].offset_time = cJSON_IsNumber(offset) ? offset->valueint : 0;

        // added check for too small turned debounce into a double to match time.
        cJSON *dbnc = cJSON_GetObjectItem(component, "debounce");
        components[i].debounce = (double) (cJSON_IsNumber(dbnc) ? dbnc->valueint : 0)/1000.0;  // put time in ms
        if (components[i].debounce > 0 && components[i].debounce < 0.050)
        {
            FPS_ERROR_PRINT(" Correcting debounce time from %f ms to 50 ms", components[i].debounce * 1000.0);
            components[i].debounce = 0.050;
        }

        // Device ID is required for serial
        cJSON* slave_id = cJSON_GetObjectItem(component, "device_id");
        components[i].device_id = -1;
        // if device_id is given check that it is valid
        if(slave_id != NULL)
        {
            if(slave_id->valueint < 0 || slave_id->valueint > 255)
            {
                 FPS_ERROR_PRINT("Invalid Device_ID.\n");
                 free(&components[i].id);
                 return -1;
            }
            components[i].device_id = slave_id->valueint;
        }

        cJSON *h_enabled = cJSON_GetObjectItem(component, "heartbeat_enabled");
        components[i].heartbeat_enabled = cJSON_IsTrue(h_enabled);
        if (components[i].heartbeat_enabled)
        {
            cJSON *h_freq = cJSON_GetObjectItem(component, "modbus_heartbeat_timeout_ms");
            components[i].modbus_heartbeat_freq_ms = cJSON_IsNumber(h_freq) ? h_freq->valueint : 500;

            cJSON *c_h_freq = cJSON_GetObjectItem(component, "component_heartbeat_timeout_ms");
            components[i].component_heartbeat_timeout_ms = cJSON_IsNumber(c_h_freq) ? c_h_freq->valueint : 500;

            cJSON *r_name = cJSON_GetObjectItem(component, "component_heartbeat_read_uri");
            components[i].read_name = cJSON_IsString(r_name) ? strdup(r_name->valuestring) : NULL;
            if (components[i].read_name == NULL)
            {
                FPS_ERROR_PRINT("No heartbeat read register provided.\n");
                free(&components[i].id);
                return -1;
            }

            // heartbeat write register is optional
            cJSON *w_name = cJSON_GetObjectItem(component, "component_heartbeat_write_uri");
            components[i].write_name = cJSON_IsString(w_name) ? strdup(w_name->valuestring) : NULL;
        }

        //parsing the configuration file to get the Type of the registers, its start and end address, and to get the value of the specified registers
        //We first get the object and then store it in the data structure
        //to get Register object
        cJSON *registers = cJSON_GetObjectItem(component, "registers");
        if (registers == NULL)
        {
            FPS_ERROR_PRINT("Failed to get object item 'register' in JSON.\n");
            return -1;
        }
        //to get the size of the array
        int total_regs = cJSON_GetArraySize(registers);
        if (total_regs == 0)
        {
            FPS_ERROR_PRINT("No registers to be mapped in the json file\n");
            return -1;
        }
        //dynamic array based on the register type entries created
        components[i].reg_datalog = new datalog [total_regs];
        if( components[i].reg_datalog == NULL)
        {
            FPS_ERROR_PRINT("Memory allocation error\n");
            return -1;
        }
        //set up the register map for each register in the component; used to make json for pub
        for(components[i].reg_cnt = 0; components[i].reg_cnt < total_regs; components[i].reg_cnt++)
        {
            FPS_DEBUG_PRINT("%d\n", components[i].reg_cnt);
            cJSON* reg = cJSON_GetArrayItem(registers, components[i].reg_cnt);
            if(reg == NULL)
            {
                FPS_ERROR_PRINT("Got NULL object from register map.\n");
                return -1;
            }
            if(-1 == setup_register_map(reg, &components[i], components[i].reg_cnt))
            {
                FPS_ERROR_PRINT("Failed to setup register map.\n");
                return -1;
            }
        }
    }
    return components_size;
}

/* Thread Management */
void lock_connection(connection_config *connection, component_config *component)
{
    pthread_mutex_lock(&connection->lock);
    if(component != NULL && component->device_id >= 0)
        modbus_set_slave(connection->mb, component->device_id);
    else
        modbus_set_slave(connection->mb, -1);  //DEFAULT_DEVICE_ID);
}

void unlock_connection(connection_config *connection)
{
    pthread_mutex_unlock(&connection->lock);
}

void *component_loop(void *arguments)
{
    thread_config *config = (thread_config *)arguments;
    connection_config *connection = config->connection;
    component_config *component = config->component;
    FPS_DEBUG_PRINT("%s: inside component loop\n", component->id);
    FPS_DEBUG_PRINT("%s, %s\n", connection->name, connection->ip_address);

    cJSON *ping, *pong;
    ping = pong = NULL;
    char uri[100];
    int *rc = new int;
    int failed_read = 0;
    double c_time;
    bool runQuery = false;

    // subscribing to the URI
    sprintf(uri,"/components/%s", component->id);

    // Connecting to the FIMS server and querying the controller after regular intervals
    while (running && component->p_fims == NULL)
    {
       fprintf(stderr,"Component %s Waiting for connection to FIMS server.\n", component->id);
       sleep(1);
    }

    // connect to heartbeat registers
    if (component->heartbeat_enabled)
        heartbeat_init(component);
  
    component->stats[component->per_missed_stat]->clear();

    // handler_loop will turn off running
    while(running)
    {
        runQuery = false;
        c_time = get_time_dbl();
        while (c_time > component->next_time)
        {
            // only log on the first loop
            if(!runQuery)
            {
                component->stats[component->per_stat]->addPer();
                component->stats[component->per_missed_stat]->addVal((c_time - component->next_time)*1000.0);
            }
            // this will catch up a skipped period
            component->next_time +=((component->frequency)/1000.0);

            runQuery = true;
            if (c_time > component->next_time) component->overrun++;
        }

        if(runQuery)
        {
            ping = cJSON_CreateObject();
            if (ping == NULL)
            {
                FPS_ERROR_PRINT("Memory allocation failed.\n");
                running = false;
                *rc = -1;
                break;
            }
            component->stats[component->query_stat]->start();

            if(query_device(ping, connection, component) == true)
            {
                component->stats[component->query_stat]->addVal();

                // only updates if receives good response from server
                pthread_mutex_lock(&component->pong_lock);
                if (component->query_pong  != NULL)
                {
                    cJSON_Delete(component->query_pong);
                }
                component->query_pong = pong = ping;
                ping = NULL;

                if (component->heartbeat_enabled)
                {
                    // add modbus_heartbeat to pong
                    cJSON_AddNumberToObject(pong, "modbus_heartbeat", component->heartbeat->heartbeat_val);
                    cJSON_AddBoolToObject(pong, "component_connected", component->heartbeat->component_connected);
                }
                pthread_mutex_unlock(&component->pong_lock);
                // heartbeat can take a short time due to register writes
                if (component->heartbeat_enabled && component->heartbeat != NULL)
                {
                    heartbeat(connection, component, c_time);
                }
                char *out = cJSON_PrintUnformatted(pong);
                FPS_DEBUG_PRINT("%s\n", out);
                component->p_fims->Send("pub", uri, NULL, out);
                free(out);
                failed_read = 0;
            }
            else
            {
                cJSON_Delete(ping);
                ping = NULL;
                failed_read++;
                if(failed_read > 5)
                {
                    modbus_reconnect = true;
                    // emit event about component disconnect
                    char message[1024];
                    snprintf(message, 1024, "Component %s disconnected; query_device failure.", component->id);
                    emit_event(component->p_fims, "Modbus Client", message, 1);
                    FPS_ERROR_PRINT("%s\n", message);
                    // Instead of exiting, continue executing without connection to modbus device
                    if(component->heartbeat)
                        component->heartbeat->component_connected = false;
                }
            }
        }
        // need to sleep till next
        run_sleep_dbl(component->next_time);
    }

    //:cleanup
    if(pong != NULL)
        delete pong;

    if(component->p_fims != NULL)
    {
        component->p_fims->Close();
        delete component->p_fims;
        component->p_fims = NULL;
    }
    delete component->heartbeat;
    for ( int q = 0; q < component->reg_cnt; q++)
        delete[] component->reg_datalog[q].register_map;

    if(component->reg_datalog != NULL) delete[] component->reg_datalog;
    for ( int q = 0; q < component->num_stats; q++)
        delete component->stats[q];
    return (void *)rc;
}

/* Heartbeat Feature */
// PSW reworked this logic.
// we have a max time,  no heartbeat activty after then then we are in error
// but we keep trying to reconnect.
// test every heartbeat interval
//  if the system is configured badly we fix it up.
void heartbeat_init(component_config *component)
{
    double c_time =  get_time_dbl();
    FPS_DEBUG_PRINT("%s: heartbeat enabled\n", component->id);
    component->heartbeat = new heartbeat_data;
    component->heartbeat->heartbeat_val = 0;
    component->heartbeat->last_read = 1;
    component->heartbeat->component_connected = true;
    component->heartbeat->max_time = (double)component->component_heartbeat_timeout_ms/1000.0;
    component->heartbeat->timeout = c_time + component->heartbeat->max_time;
    component->heartbeat->next_time = c_time; 
    component->heartbeat->interval = (double)component->modbus_heartbeat_freq_ms/1000.0; 
    if (component->heartbeat->interval < 0.5)
        component->heartbeat->interval = 0.5;
    if(component->heartbeat->max_time < component->heartbeat->interval * 3.0)
        component->heartbeat->max_time = component->heartbeat->interval * 3.0;

    // note this could be a Holding register
    if (component->read_name != NULL)
    {
        component->heartbeat->heartbeat_read_reg = get_register(component->read_name, Input_Register, component);
        if (!component->heartbeat->heartbeat_read_reg)
           component->heartbeat->heartbeat_read_reg = get_register(component->read_name, Holding_Register, component);
    }
    else
        component->heartbeat->heartbeat_read_reg = NULL;

    if (component->write_name != NULL)
        component->heartbeat->heartbeat_write_reg = get_register(component->write_name, Holding_Register, component);
    else
        component->heartbeat->heartbeat_write_reg = NULL;
    FPS_ERROR_PRINT("heartbeat confg , check every %f mS, Timeout after %f mS\n"
        , component->heartbeat->interval, component->heartbeat->max_time);
    FPS_ERROR_PRINT("heartbeat confg , read register [%s->%p], write register [%s->%p]\n"
        , component->read_name ? component->read_name:"not defined"
        , (void*)component->heartbeat->heartbeat_read_reg
        , component->write_name ? component->write_name:"not defined"
        , (void*)component->heartbeat->heartbeat_write_reg);
}

void heartbeat(connection_config *connection, component_config *component, double c_time)
{

    heartbeat_data *heartbeat = component->heartbeat;
    if( heartbeat->next_time > c_time)
        return;

    while (heartbeat->next_time < c_time)
        heartbeat->next_time += heartbeat->interval;

    if (heartbeat->heartbeat_read_reg)
    {
        // reset our fault trap if the component's heartbeat value has changed
        if (heartbeat->last_read != heartbeat->heartbeat_read_reg->data)
        {
            heartbeat->timeout = c_time + heartbeat->max_time;

            if (!heartbeat->component_connected)
            {
                heartbeat->component_connected = true;
                char message[1024];
                snprintf(message, 1024, "Component %s reconnected.", connection->name);
                emit_event(component->p_fims, "Modbus Client", message, 1);
                FPS_ERROR_PRINT("%s\n", message);
            }
            heartbeat->last_read = heartbeat->heartbeat_read_reg->data;
        }
        // keep trying
        if (heartbeat->heartbeat_write_reg != NULL)
        {
            set_register(heartbeat->last_read + 1, heartbeat->heartbeat_write_reg, connection, component); // Sets write register to new value
            heartbeat->heartbeat_write_reg->data = heartbeat->last_read + 1;					 // Set data field of register, which is a cache
        }
        
        if( heartbeat->timeout > c_time)
            return ;
    
        // emit event about component disconnect
        char message[1024];
        snprintf(message, 1024, "Component %s disconnected; heartbeat no longer detected.", connection->name);
        emit_event(connection->p_fims, "Modbus Client", message, 1);
        FPS_ERROR_PRINT("%s\n", message);
        // Instead of exiting, continue executing without connection to modbus device
        heartbeat->component_connected = false;
    }
}

/* Main Thread */
int main(int argc,char *argv[])
{
    set_base_time();
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    int fims_connect = 0;
    //Set up configuration for entire connection
    connection_config *conn_info = new connection_config;
    component_config *components = NULL;

    //read in and parse config file to json
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        FPS_ERROR_PRINT("Could not get config json from file.\n");
        return 1;
    }

    //obtain connection information
    int conn_val = get_conn_info(config, conn_info);
    if(conn_val == -1)
    {
        FPS_ERROR_PRINT("Could not get connection info from config json.\n");
        return 1;
    }

    if(establish_connection(conn_info) == -1)
    {
        FPS_ERROR_PRINT("Could not establish modbus connection.\n");
        cJSON_Delete(config);
        return 1;
    }
    pthread_mutex_init(&conn_info->lock, NULL);
    // Connecting to the FIMS server and querying the controller after regular intervals
    conn_info->p_fims = new fims();
    if (conn_info->p_fims == NULL)
    {
        fprintf(stderr,"Main failed to allocate connection to FIMS serve.\n");
        return 1;
    }

    while(!conn_info->p_fims->Connected())
    {
        // could alternatively fims connect using a stored name for the client if the client
        // stores state.
        if((fims_connect < 5) && (conn_info->p_fims->Connect(conn_info->name) == false))
        {
            // If cannot connect wait for a second and then retry
            sleep(1);
            fims_connect++;
            continue;
        }
        else if(fims_connect >= 5)
        {
            FPS_ERROR_PRINT("%s: Failed to connect to Fims Server.\n", conn_info->name);
            return 1;
        }
    }
    //obtain information for components, including register maps
    int num_components = get_components(config, components);
    if(num_components <= 0 || components == NULL)
    {
        FPS_ERROR_PRINT("Could not get components from config json.\n");
        return 1; 
    }

    FPS_DEBUG_PRINT("Decoded %d components\n", num_components);

    pthread_t *component_threads = new pthread_t[num_components];
    pthread_t *handler_threads = new pthread_t[num_components];
    
    double cur_time = get_time_dbl();

    for (int i = 0; i < num_components; i++)
    {
        //start each component
        thread_config *config = new thread_config;
        config->connection = conn_info;
        config->component = &components[i];
        config->component->next_time = cur_time + ((config->component->frequency + config->component->offset_time) / 1000.0);
        pthread_mutex_init(&config->component->pong_lock, NULL);

        int rc = pthread_create(&component_threads[i], NULL, component_loop, (void *)config);
        if (rc == -1)
            FPS_ERROR_PRINT("failed to start component pthread\n");
        rc = pthread_create(&handler_threads[i], NULL, handler_loop, (void *)config);
        if (rc == -1)
            FPS_ERROR_PRINT("failed to start handler pthread\n");
    }
    
    while (running)
    {
        // Need to lock while testing and connecting
        if (modbus_reconnect)
        {
            char message[1024];
            snprintf(message, 1024, "Modbus Connection %s not responding. Attempting to reconnect.", conn_info->name);
            emit_event(conn_info->p_fims, "Modbus Client", message, 1);
            FPS_ERROR_PRINT("%s\n", message);
            lock_connection(conn_info, NULL);
            modbus_close(conn_info->mb);
            modbus_free(conn_info->mb);
            if(conn_info->serial_device != NULL)
            {
                close(modbus_get_socket(conn_info->mb));
                running = false;
            }
            if(establish_connection(conn_info) == -1 || !running)
            {
                snprintf(message, 1024, "Modbus Connection %s not able to reconnect.", conn_info->name);
                emit_event(conn_info->p_fims, "Modbus Client", message, 1);
                FPS_ERROR_PRINT("%s\n", message);
                running = false;  //JC 09042020
            }
            else
            {
                snprintf(message, 1024, "Modbus Connection to %s reestablished.", conn_info->name);
                emit_event(conn_info->p_fims, "Modbus Client", message, 1);
                FPS_ERROR_PRINT("%s\n", message);
                modbus_reconnect = false;
            }
            unlock_connection(conn_info);
        }
        else
            sleep(1);
    }

    for (int i = 0; i < num_components; i++)
    {
        //wait for each component and handler thread to finish
        pthread_join(component_threads[i], NULL);
        pthread_join(handler_threads[i], NULL);
    }

    free(conn_info->name);
    if(conn_info->ip_address != NULL)  free(conn_info->ip_address);
    if(conn_info->serial_device != NULL)  free(conn_info->serial_device);
    if(conn_info->mb != NULL)
    {
        modbus_close(conn_info->mb);
        modbus_free(conn_info->mb);
    }
    // close p_fims
    conn_info->p_fims->Close();
    delete conn_info->p_fims;
    conn_info->p_fims = NULL;
}
