// should all be in here

#include "modbus_map.h"

int main(int argc,char *argv[])
{
    const char* dump_file =  nullptr;
    const char* echo_file = nullptr;

    set_base_time();

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    sys_t sys_main;
    sys_t* psys = &sys_main;

    //Set up configuration for entire connection
    //psys->conn_info = new connection_config;
    //component_config *components = nullptr;
    ////init_connection_config(psys->conn_info);

    //read in and parse config file to json
    cJSON* config = get_config_json(argc, argv);
    if(config == nullptr)
    {
        FPS_ERROR_PRINT("Could not get config json from file.\n");
        return 1;
    }
    if(argc > 2)
        dump_file = argv[2];
    if(argc > 3)
        echo_file = argv[3];

    //obtain connection information
    bool conn_val = get_conn_info(psys, config);//, conn_info);
    if(!conn_val)
    {
        FPS_ERROR_PRINT("Could not get connection info from config json.\n");
        return 1;
    }
    int num_components = 0;
// we either have "registers" or "components" at this time
// if we see registers it is a single component or single threaded system.

    cJSON *cjsys = cJSON_GetObjectItem(config, "system");
    cJSON *cjcomp = cJSON_GetObjectItem(config, "components");
    if(cjsys && !cjcomp)
    {
        cJSON *cjreg = cJSON_GetObjectItem(config, "registers");
        component_config *comp = new component_config();
        get_component(psys, config, comp, cjreg);
        psys->add_component(comp);
        //component_config* comp = get_component(psys, config, cjsys);
        num_components = 1;
        get_comp_registers(psys, config, cjreg, comp);
    }
    else
    {
        //obtain information for components, including register maps
        num_components = get_components(psys, config);
        FPS_ERROR_PRINT("Done with get_components num %d\n", num_components);
    }

    if(num_components <= 0)
    {
        FPS_ERROR_PRINT("Could not get components from config json.\n");
        cJSON_Delete(config);
        return 1;
    }

    FPS_ERROR_PRINT("Running fix maps >>>>>\n");
    fix_maps(psys, psys->conn_info);
    FPS_ERROR_PRINT("<<<<<Completed fix maps \n");

    if(dump_file)
    {
        psys->conn_info->dump_server = strdup(dump_file);
    }
    if(psys->conn_info->dump_server)
    {
        FPS_ERROR_PRINT("Dump Server to [%s]\n",psys->conn_info->dump_server);
        dump_server(psys, psys->conn_info);
    }
    if(echo_file)
    {
        psys->conn_info->dump_echo = strdup(echo_file);
    }

    if(psys->conn_info->dump_echo)
    {
        FPS_ERROR_PRINT("Dump Echo to [%s]\n",psys->conn_info->dump_echo);
        dump_echo(psys, psys->conn_info->dump_echo);
        FPS_ERROR_PRINT("Done with Dump Echo\n");
    }
    // now get_register looks a bit different
    get_sys_register(psys, "sbmu1_soc", Coil/*not used*/);
    //
    get_sys_uri(psys, "sbmu1_soc", "catl_smbu_warn_r");

    FPS_ERROR_PRINT("Done with Dumps\n");
    cJSON_Delete(config);

    delete psys->conn_info;
    return 0;
}
