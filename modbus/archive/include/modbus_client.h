#ifndef _MODBUS_CLIENT_H_
#define _MODBUS_CLIENT_H_

#include <string>
#include <pthread.h>
#include "modbus_utils.h"

#define DEFAULT_DEVICE_ID 255 
#define NUM_STIME 16


double get_time_dbl();

typedef struct stat_val_t{
    stat_val_t(std::string iname, const char* _cname=NULL) {
        name = iname;
        clear();
        scale = 1000.0;
        if(_cname) 
        {
            cname = _cname;
        }
        else
        {
            cname = "no name";
        }
        
    }
    ~stat_val_t()
    {
        FPS_ERROR_PRINT(" %s > Delete stat [%s]\n", cname.c_str(), name.c_str());
    };


    std::string name;
    std::string cname;
    double start_val;
    double max_val;
    double total_val;
    int count;
    double scale;

    double addVal()
    {
        double val = get_time_dbl() - start_val;
        if ((count == 0) || (val > max_val))
            max_val = val;
        count++;
        total_val += val;
        return val;
    };

    double addVal( double val)
    {
        if ((count == 0) || (val > max_val))
            max_val = val;
        count++;
        total_val += val;
        return val;
    };

    double addPer()
    {
        double val = get_time_dbl();
        if (count == 0)
        {
            start_val = val;
            count++;
        }
        else
        {
            addVal();
            start_val = val;
        }
        return val;
    };

    void clear()
    {
        count = 0;
        max_val= 0.0;
        total_val= 0.0;
    }
    double start()
    {
        start_val = get_time_dbl();
        return start_val;
    }

    double getAvg(void)
    {
        return (count > 0)?(total_val * scale/(double)count):0.0 ;
    }
    double getAvgPer(void)
    {
        return (count>1)?(total_val * scale/(double)(count-1)):0.0;
    }

    double getMax(void)
    {
        return (max_val * scale);        
    }

} stat_val;

typedef struct {
    double cur_val;
    double nxt_val;
    maps *reg;
    /* tenth of a millisecond precision */
    double last_write;
} modbus_var_traffic;

typedef struct {
    char* name;
    fims *p_fims;
    modbus_t *mb;
    pthread_mutex_t lock;
    //info for TCP connections
    char* ip_address;
    int port;
    //info for serial connections
    char* serial_device;
    char parity;
    int baud;
    int data;
    int stop;
    char* eth_dev;
} connection_config;

typedef struct {
    double next_time;
    double timeout;
    double interval;
    double max_time;

    unsigned short heartbeat_val;
    uint64_t last_read;
    bool component_connected;
    maps *heartbeat_read_reg; // make sure to include in registers in config
    maps *heartbeat_write_reg; // make sure to include in registers in config
} heartbeat_data;

#define MAX_STATS 16
typedef struct component_config_t{
    char *id;
    bool byte_swap;
    bool off_by_one;
    bool add_timings;
    //component loop frequency and time buffer for handle messages
    int frequency;
    int offset_time;
    double next_time;

    // changed this to a double added a debounce throttle
    double debounce;
    double last_debounce_time;
    double next_debounce_time;
    int device_id;
    //watchdog feature
    bool heartbeat_enabled;
    heartbeat_data *heartbeat;
    int modbus_heartbeat_freq_ms; // in ms; should be multiple of frequency
    int component_heartbeat_timeout_ms; // in ms; should be multiple of frequency
    char *read_name;
    char *write_name;
    //registers for this component
    datalog *reg_datalog;
    int reg_cnt;
    unsigned int overrun;
    fims *p_fims;
    // for write rate throttling
    std::map <std::string, modbus_var_traffic*> var_map;
    pthread_mutex_t pong_lock;
    cJSON* query_pong;
    cJSON* handler_pong;

    int query_stat;
    int query_read_stat;
    int write_stat;
    int read_stat;
    int write_val_stat;
    int write_item_stat;
    int per_missed_stat;
    int per_stat;
    int write_reg;
    stat_val* stats[MAX_STATS];
    int num_stats;
    int addStat(std::string name)
    {
        int ret = num_stats;
        if (num_stats < MAX_STATS)
        {
            stats[num_stats] = new stat_val_t(name, id);
            num_stats ++;
        }
        return ret;
    }

    component_config_t()
    {
        p_fims = NULL;
        heartbeat = NULL;
        query_pong = handler_pong = NULL;
        id = NULL;
        num_stats = overrun = 0;
        write_stat = addStat("write");
        write_reg = addStat("write_reg");
        write_item_stat = addStat("write_item");
        read_stat = addStat("read");
        query_stat = addStat("query");
        query_read_stat = addStat("query_read");
        per_stat = addStat("period");
        per_missed_stat = addStat("period late");
    }
} component_config;

typedef struct {
    connection_config *connection;
    component_config *component;
} thread_config;

/* Configuration */

int setup_register_map(cJSON *reg, const component_config *config, int index);
int get_conn_info(cJSON *config, connection_config *connection);
int get_components(cJSON *config, component_config *&components);

/* Thread Management */
void lock_connection(connection_config *connection, component_config *component);
void unlock_connection(connection_config *connection);
int establish_connection(connection_config *config);
void *component_loop(void *arguments);

/* FIMS Communication */
int handle_messages(fims *p_fims, char *base_uri, connection_config *connection, component_config *component);

/* Modbus Communication Functions */
int set_coil(bool temp_coil_val, maps *reg, connection_config *connection, component_config *component);
maps *get_register(char *name, Type_of_Register reg_type, component_config *config);
int set_register(double temp_reg_val, maps *reg, connection_config *connection, component_config *component);
bool query_device(cJSON *root, const connection_config *connection, const component_config *component);

/* Heartbeat Feature */
void heartbeat_init(component_config *component);
void heartbeat(connection_config *connection, component_config *component, double c_time);

#endif /* _MODBUS_CLIENT_H_ */
