std::vector<TransItem> sbms_trans = {
    // src                                fucn            dest                        lim         desc
	{"rtos:bits:terminal_undervoltage",  "3sum",        "sbmu:bits:summary_total_undervoltage",        "none",     "total_undervoltage"},
	{"rtos:bits:terminal_overvoltage",   "3sum",        "sbmu:bits:summary_total_overvoltage",         "none",     "total_overvoltage"},
	{"rtos:bits:terminal_undervoltage",  "3sum",        "sbmu:bits:summary_overcurrent",               "none",     "overcurrent"},
	{"rtos:bits:insulation_low",         "3sum",        "sbmu:bits:summary_low_resistance",            "none",     "low_resistance"},
	{"rtos:bits:min_temp_mod",           "3sum",        "sbmu:bits:summary_module_low_temp",          "none",      "summary_module_low_temp"},
	{"rtos:bits:max_temp_mod",           "3sum",        "sbmu:bits:summary_module_over_temp",         "none",      "summary_module_high_temp"},
	{"rtos:bits:xxx",           "3sum",                 "sbmu:bits:summary_cell_over_voltage",          "none",      "summary_cell_over_voltage"},
	{"rtos:bits:xxx",           "3sum",                 "sbmu:bits:summary_cell_under_voltage",          "none",      "summary_cell_under_voltage"},
	{"rtos:bits:xxx",           "3sum",                 "sbmu:bits:summary_cell_diff_voltage",          "none",      "summary_cell_diff_voltage"},
	{"rtos:bits:xxx",           "3sum",        "sbmu:bits:summary_module_over_temp",         "none",      "summary_module_high_temp"},
	{"rtos:bits:min_temp_cell",  "3sum",       "sbmu:bits:summary_cell_low_temp",          "none",      "summary_cell_low_temp"},
	{"rtos:bits:max_temp_cell",  "3sum",       "sbmu:bits:summary_cell_over_temp",         "none",      "summary_cell_high_temp"},
	{"rtos:bits:min_soc_cell",  "3sum",        "sbmu:bits:summary_cell_low_SOC",          "none",      "summary_cell_low_SOC"},
	{"rtos:bits:max_soc_cell",  "3sum",        "sbmu:bits:summary_cell_high_SOC",         "none",      "summary_cell_high_SOC"},
	{"rtos:bits:max_soc_cell",  "3sum",        "sbmu:bits:summary_cell_high_SOH",            "none",      "summary_cell_high_SOH"},
    {"rtos:bits:max_soc_cell",  "3sum",        "sbmu:bits:summary_terminal_over_temp",        "none",      "summary_terminal_over_temp"},

	{"internal",                      "TODO",  "sbmu:bits:esbcm_lost_comms",                  "none",    "ESBCM_lost_communication"},

    {"rtos:bits:max_soc_cell",  "3sum",         "sbmu:bits:summary_BMS_system_alarms",         "none",    "summary_BMS_system_alarms"},
    {"rtos:bits:max_soc_cell",  "3sum",         "sbmu:bits:summary_BMS_system_faults",         "none",    "summary_BMS_system_faults"},
    {"rtos:bits:max_soc_cell",  "3sum",         "sbmu:bits:summary_pack_over_voltage",         "none",    "summary_pack_over_voltage"},
    {"rtos:bits:max_soc_cell",  "3sum",         "sbmu:bits:summary_pack_under_voltage",        "none",    "summary_pack_under_voltage"},

	{"rack:input:max_voltage",      "max", 		"sbmu:input:max_battery_voltage",      "none",  "max_battery_voltage"},
	{"rack:input:max_mod_volt",     "max", 		"sbmu:input:max_pack_voltage",         "none",  "max_battery_voltage"},
	{"rack:input:max_mod_volt_num", "max", 		"sbmu:input:max_battery_temp",            "none",  "max_battery_voltage"},
	{"rack:input:max_mod_volt_num", "max", 		"sbmu:input:max_battery_temp_pack_number",      "none",  "max_battery_temp_pack"},
	{"rack:input:max_mod_volt_num", "max", 		"sbmu:input:max_battery_temp_group",      "none",  "max_battery_temp_group"},
	{"rack:input:max_module_volt",  "max", 		"sbmu:input:max_discharge_power",         "none",  "max_discharge_power"},
	{"rack:input:max_module_volt",  "max", 		"sbmu:input:max_charge_power",            "none",  "max_charge_power"},
	{"rack:input:max_module_volt",  "max", 		"sbmu:input:max_discharge_current",       "none",  "max_discharge_current"},
	{"rack:input:max_module_volt",  "max", 		"sbmu:input:max_discharge_current",       "none",  "max_charge_current"},
	{"rack:input:max_module_volt_num",   "max",   "sbmu:input:xmax_battery_voltage",       "none",  "max_battery_voltage"},
	{"rack:input:min_volt",          "min", 		"sbmu:input:min_battery_voltage",      "none",  "min_battery_voltage"},
	{"rack:input:min_volt_mod_num",  "xmin", 		"sbmu:input:min_battery_voltage_group",      "none",  "min_battery_voltage_group"},
	{"rack:input:min_volt",           "min", 		"sbmu:input:min_battery_voltage_number",      "none",  "min_battery_voltage_number"},
	{"rack:input:charge_cap",            "sum", 		"sbmu:input:max_charge_power",          "none",  "max_charge_power"},
	{"rack:input:discharge_cap",         "sum", 		"sbmu:input:max_discharge_power",       "none",   "max_discharge_power"},
	{"rack:input:chargeable_current",    "sum", 		"sbmu:input:max_charge_current",        "none",  "max_charge_current"},
	{"rack:input:dischargeable_current",  "sum", 		"sbmu:input:max_discharge_current",      "none",  "max_discharge_current"},
	{"rack:input:charge_num",             "TODO",       "sbmu:input:num_daily_charges",          "none",   "num_daily_charges"},
	{"rack:input:discharge_num",          "TODO",       "sbmu:input:num_daily_discharges",       "none",   "num_daily_discharges"}

};

// name_mappings = {
//     'State': 'status',
//     'Max_Allowed_Charge_Power': 'chargeable_power',
//     'Max_Allowed_Discharge_Power':'dischargeable_power',

//     'Max_Charge_Voltage':'chargeable_volt',
//     'Max_Discharge_Voltage':'dischargeable_volt',

//     'Max_Charge_Current':'chargeable_current',
//     'Max_Discharge_Current':'dischargeable_current',

//     'DI1':"di1",
//     'DI2':"di2",
//     'DI3':"di3",
//     'DI4':"di4",
//     'DI5':"di5",
//     'DI6':"di6",
//     'DI7':"di7",
//     'DI8':"di8",

//     'Max_cell_voltage':'max_voltage',
//     'Max_cell_voltage_number':'max_voltage_num',
//     'Min_cell_voltage':'min_voltage',
//     'Min_cell_voltage_number':'min_voltage_num',
//     'Max_cell_temperature': 'max_temp',
//     'Max_cell_temperature_number':'max_temp_num',
//     'Min_cell_temperature': 'min_temp',
//     'Min_cell_temperature_number':'min_temp_num',
//     'Max_cell_SOC':'max_soc',
//     'Max_cell_SOC_number':'max_soc_num',
//     'Min_cell_SOC':'min_soc',
//     'Min_cell_SOC_number':'min_soc_num',
//     'SOC':'soc',
//     'SOH':'soh',
//     'Average_cell_voltage':'avg_voltage',
//     'Average_cell_temperature':'avg_temp',

//     'Accumulated_charge_capacity':'total_charge_cap',
//     'Accumulated_discharge_capacity':'total_discharge_cap'

// }


std::vector<TransItem> sbms_set = {
	{"sbmu:input:100",                             "into",          "rtos:input:8",   "none",    "Status"},
	{"sbmu:input:101",                             "into",          "rtos:input:152",   "none",    "chargeable_power"},
	{"sbmu:input:102",                             "into",          "rtos:input:153",   "none",    "dischargeable_power"},
	{"sbmu:input:103",                             "into2",         "rtos:input:150",   "TODO",    "charge_volt"},
	{"sbmu:input:105",                             "into2",         "rtos:input:150",   "TODO",    "charge_current"},
	{"sbmu:input:103",                             "into2",         "rtos:input:150",   "TODO",    "discharge_volt"},
	{"sbmu:input:105",                               "into2",       "rtos:input:150",   "TODO",    "discharge_current"},
    {"sbmu:hold:1111:cluster_control_data_eq_ctrl",  "into_rack",   "rtos:hold:111:cluster_control_data_eq_ctrl","TODO", "contactor_control"},
    {"sbmu:hold:1099:ctrl_cmd_mode",                 "into_rack",  "rtos:hold:99:ctrl_cmd_mode", "TODO","contactor_mode"},
    {"sbmu:hold:1104:g_socsohsetno",                 "into_rack",  "rtos:hold:104:g_socsohsetno""TODO","set_soc_soh_no"},
    {"sbmu:hold:1105:g_socsohsetval",                 "into_rack", "rtos:hold:105:g_socsohsetval""TODO","set_soc_soh_val"}
};

void register_trans()
{
    TransItemTable sbms_entry;
    sbms_entry.name = "sbms_trans";
    sbms_entry.desc = "Activated when the sbms receives an input from the racks";
    sbms_entry.items = sbms_trans;  // Ensure `sbms_trans` is a valid vector of `TransItem`
    trans_tables["sbms_trans"] = sbms_entry;
    TransItemTable sbms_set_entry;
    sbms_set_entry.name = "sbms_set";
    sbms_set_entry.desc = "Activated when the sbms recieves a set val for a rack item";
    sbms_set_entry.items = sbms_set;  // Ensure `sbms_trans` is a valid vector of `TransItem`
    trans_tables["sbms_set"] = sbms_set_entry;

}

// Test cases
void test_decode_shmdef() {
    MemId mem_id;
    RegId reg_id;
    int rack_num;
    uint16_t offset;
    bool crash = false;
    // Test case 1: Valid input with 3 parts
    bool result = decode_shmdef(mem_id, reg_id, rack_num, offset, "sbmu:bits:100");
    myassert(result, "decode_shmdef: valid input with 3 parts", crash);
    myassert(mem_id == MEM_SBMU, "decode_shmdef: mem_id correct", crash);
    myassert(reg_id == REG_BITS, "decode_shmdef: reg_id correct", crash);
    myassert(rack_num == 0, "decode_shmdef: rack_num default correct", crash);
    myassert(offset == 100, "decode_shmdef: offset correct", crash);

    // Test case 2: Valid input with 4 parts
    result = decode_shmdef(mem_id, reg_id, rack_num, offset, "rack:input:1:300");
    myassert(result, "decode_shmdef: valid input with 4 parts", crash);
    myassert(mem_id == MEM_RACK, "decode_shmdef: mem_id correct", crash);
    myassert(reg_id == REG_INPUT, "decode_shmdef: reg_id correct", crash);
    myassert(rack_num == 1, "decode_shmdef: rack_num correct", crash);
    myassert(offset == 300, "decode_shmdef: offset correct", crash);

    // Test case 3: Invalid input (too few parts)
    result = decode_shmdef(mem_id, reg_id, rack_num, offset, "sbmu");
    myassert(!result, "decode_shmdef: invalid input (too few parts)", crash);

    // Test case 4: Special case for offset lookup
    result = decode_shmdef(mem_id, reg_id, rack_num, offset, "sbmu:bits:invalid");
    myassert(result, "decode_shmdef: special case for offset lookup", crash);
    //myassert(offset == 100, "decode_shmdef: offset lookup correct", crash);
}


void test_data_list(ConfigDataList&configData)
{
    configData.emplace_back(find_name("rtos","min_soc"), 0);
    configData.emplace_back(find_name("rtos","max_soc"), 0);
    configData.emplace_back(find_name("rtos","min_soc_num"), 0);
    configData.emplace_back(find_name("rtos","max_voltage_num"), 0);
    configData.emplace_back(find_name("rtos","min_voltage"), 0);
    configData.emplace_back(find_name("rtos","max_voltage"), 0);
    configData.emplace_back(find_name("rtos","alarms"), 0);
    ConfigDataList sorted;
    std::vector<std::string>queries;

    std::cout <<" Unsorted list :" << std::endl;
    ShowConfigDataList(configData);


    std::cout <<" Sorted list :" << std::endl;
    groupQueries(sorted, configData);
    ShowConfigDataList(sorted);

    std::cout <<" Queries :" << std::endl;
    // rack 0
    GenerateQueries("set", 2, 0, queries, sorted);

}

void test_StringWords() {
    bool crash = false;
    std::string input = "  Hello   world, this is   an example string!  ";
    std::vector<std::string> words = StringWords(input);

    // // Print each word to verify the result
    // for (const std::string& word : words) {
    //     std::cout << "'" << word << "'\n";
    // }
    myassert(words.size() == 7,"StringWords_vec_size", crash);
    myassert(words[2] == "this","StringWords_item", crash);

    return;
}

void test_str_to_offset() {
    bool crash = false;
    // Test valid decimal input
    myassert(str_to_offset("123") == 123,"str_to_offset_integer", crash);

    // Test valid hexadecimal input
    myassert(str_to_offset("0x1A3") == 419,"str_to_offset_hex_1", crash);

    // Test another valid hexadecimal input
    myassert(str_to_offset("0X1a3") == 419,"str_to_offset_hex_2", crash);

    // Test valid hexadecimal with capital letters
    myassert(str_to_offset("0XABC") == 2748,"str_to_offset_hex_3", crash);

    // Test valid negative decimal
    myassert(str_to_offset("-10") == -10,"str_to_offset_neg", crash);

    // Test valid negative hexadecimal
    myassert(str_to_offset("-0x1A") == -26,"str_to_offset_neg_hex", crash);

    // Test edge case of minimal non-zero integers
    myassert(str_to_offset("1") == 1,"str_to_offset_one", crash);
    myassert(str_to_offset("0x1") == 1,"str_to_offset_hex_one", crash);

    // Test zero
    myassert(str_to_offset("0") == 0,"str_to_offset_zero", crash);
    myassert(str_to_offset("0x0") == 0,"str_to_offset_hex_zero", crash);

    // // Test invalid inputs
    // try {
    //     str_to_offset("hello");
    //     myassert(false && "str_to_offset should have thrown an exception for 'hello'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }

    // try {
    //     str_to_offset("0x1G");
    //     myassert(false && "str_to_offset should have thrown an exception for '0x1G'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }
}

void test_decode_name() {
    std::string sm_name, item;
    int index;
    bool crash = false;

    // Test valid input with index
    decode_name(sm_name, item, index, "rtos:min_soc[2]");
    myassert(sm_name == "rtos" && item == "min_soc" && index == 2, "decode_name_with_index", crash);

    // Test valid input without index
    decode_name(sm_name, item, index, "rtos:min_soc");
    myassert(sm_name == "rtos" && item == "min_soc" && index == -1, "decode_name_without_index", crash);

    // Test valid input with improper format (missing index closing bracket)
    decode_name(sm_name, item, index, "rtos:min_soc[2");
    myassert(sm_name.empty() && item.empty() && index == -1, "decode_name_improper_format", crash);

    // Test input with no colon
    decode_name(sm_name, item, index, "rtosmin_soc");
    myassert(sm_name.empty() && item.empty() && index == -1, "decode_name_no_colon", crash);

    // Additional tests can be added as needed
}
 
void test_match_system() {
    bool crash = false; // Flag to check for test failures

    // Test case 1: Exact match without weights or tolerances
    std::shared_ptr<MatchObject> match1 = std::make_shared<MatchObject>();
    match1->vector = {100, 200, 300};
    std::vector<uint16_t> input1 = {100, 200, 300};

    double result1 = calculate_match_score(input1, match1);
    myassert(result1 == 1.0, "test_match_system|Exact match without weights or tolerances", crash);

    // Test case 2: Match with tolerances
    std::shared_ptr<MatchObject> match2 = std::make_shared<MatchObject>();
    match2->vector = {100, 205, 300};
    match2->tolerance = {0, 10, 0}; // Tolerances where only the second element has a non-zero tolerance
    std::vector<uint16_t> input2 = {100, 200, 300};

    double result2 = calculate_match_score(input2, match2);
    myassert(result2 == 1.0, "test_match_system|Match with tolerances allowing variation in the second element", crash);

    // Test case 3: Match with weights
    std::shared_ptr<MatchObject> match3 = std::make_shared<MatchObject>();
    match3->vector = {100, 200, 300};
    match3->weight = {100, 50, 25};  // Different weights for each element
    std::vector<uint16_t> input3 = {100, 200, 300};
    std::vector<uint16_t> input31 = {100, 100, 300};
    std::vector<uint16_t> input32 = {100, 200, 100};
    std::vector<uint16_t> input33 = {50, 200, 300};

    double result3 = calculate_match_score(input3, match3);
    std::cout << " result with input3 " << result3 << std::endl;
    double result31 = calculate_match_score(input31, match3);
    std::cout << " result with input31 " << result31 << std::endl;
    double result32 = calculate_match_score(input32, match3);
    std::cout << " result with input32 " << result32 << std::endl;
    double result33 = calculate_match_score(input33, match3);
    std::cout << " result with input33 " << result33 << std::endl;

    myassert(result3 > 0.58, "test_match_system|Match with varying weights", crash);

    // Test case 4: Match with mask
    std::shared_ptr<MatchObject> match4 = std::make_shared<MatchObject>();
    match4->vector = {0x10, 0x10, 0x10};
    match4->mask = {0xff, 0xff, 0xff};  // Masks for each element
    std::vector<uint16_t> input4 = {0x10, 0x110, 0x1110};

    double result4 = calculate_match_score(input4, match4);
    myassert(result4 == 1.0, "test_match_system|Match with masks", crash);

    // Test case 5: No match due to out of tolerance
    std::shared_ptr<MatchObject> match5 = std::make_shared<MatchObject>();
    match5->vector = {100, 250, 300};
    match5->tolerance = {5, 5, 5}; // Tolerance too small for second element
    std::vector<uint16_t> input5 = {100, 200, 300};

    double result5 = calculate_match_score(input5, match5);
    myassert(result5 == 0.0, "test_match_system|No match due to tolerance being too small", crash);


}

// nmap -v -sT localhost
int test_modbus(const std::string& ip, int port) {
    int mb_status = -2;
    try {
        ModbusClient client(ip.c_str(), port); // Example IP and port

        // Add a sample query in JSON format
        json query = {
            {"action", "get"},
            {"seq", 2},
            {"sm_name", "sbmu"},
            {"debug", true},
            {"reg_type", "input"},
            {"offset", 100},
            {"num", 10}
        };

        auto [status, data] = client.addQuery(query);
        mb_status = status;
        if (status == 0) {
            std::cout << "Data read successfully: ";
            for (int value : data) {
                std::cout << value << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "Error occurred during query processing." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    myassert(mb_status == 0, "test_modbus|basic read test", false);
    return 0;
}