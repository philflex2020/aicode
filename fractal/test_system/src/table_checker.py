# table_checker.py
def parse_table(data):
    """ Parse the table data into two dictionaries: by address and by name. """
    address_dict = {}
    name_dict = {}
    lines = data.strip().split('\n')
    for line in lines:
        parts = line.strip().split(':')
        if len(parts) < 3:
            parts.append('1')  # Default size is 1 if not specified
        name, address, size = parts[0], int(parts[1]), int(parts[2])
        address_dict[address] = {'name': name, 'size': size}
        name_dict[name] = {'address': address, 'size': size}
    return address_dict, name_dict

def compare_tables(table1_data, table2_data, name_mappings):
    """ Compare two tables based on provided name mappings using name dictionaries. """
    address_dict1, name_dict1 = parse_table(table1_data)
    address_dict2, name_dict2 = parse_table(table2_data)

    for name1, mapping in name_mappings.items():
        if name1 in name_dict1:
            entry1 = name_dict1[name1]
            if mapping in name_dict2:
                entry2 = name_dict2[mapping]
                print(f"Mapping match notice for '{name1}' to '{mapping}':")
                print(f"    Table 1 - Address: {entry1['address']}, Size: {entry1['size']}")
                print(f"    Table 2 - Address: {entry2['address']}, Size: {entry2['size']}")
            else:
                print(f"Name '{name1}' mapped to '{mapping}' which does not exist in Table 2.")
        else:
            print(f"Name '{name1}' from mapping does not exist in Table 1.")

# # Example usage
# table1_data = """
# State:0:1
# Power:4:2
# """
# table2_data = """
# status:0:1
# battery_power:4:2
# """
# name_mappings = {
#     'State': 'status',  # Mapping from Table 1 name to Table 2 name
#     'Power': 'battery_power'
# }


# def parse_table(data):
#     """ Parse the table data into a dictionary. """
#     table_dict = {}
#     lines = data.strip().split('\n')
#     for line in lines:
#         #print(line)
#         parts = line.split(':')
#         if len(parts) < 3:
#             parts.append('1')

#         name, address, size = parts[0], int(parts[1]), int(parts[2])
#         table_dict[address] = {'name': name, 'size': size}
#     return table_dict

# def compare_tables(table1, table2, name_mappings):
#     """ Compare two tables and output differences or mappings. """
#     # table1 = parse_table(table1_data)
#     # table2 = parse_table(table2_data)

#     # Iterate over both tables to find differences or mappings
#     all_addresses = set(table1.keys()) | set(table2.keys())
#     for address in sorted(all_addresses):
#         entry1 = table1.get(address, {})
#         entry2 = table2.get(address, {})
#         if address in table1 and address in table2:
#             name1 = entry1.get('name')
#             name2 = entry2.get('name')
#             if name1 in name_mappings and name_mappings[name1] == name2:
#                 print(f"Map match notice: {name1} to {name2}, Addresses and sizes: {address}, Sizes: {entry1['size']}, {entry2['size']}")
#             else:
#                 print(f"Address {address}:")
#                 print(f"    Table 1: {entry1}")
#                 print(f"    Table 2: {entry2}")
#         elif address in table1:
#             print(f"Address {address}:")
#             print(f"    Table 1: {entry1}")
#             print(f"    Table 2: None")
#         elif address in table2:
#             print(f"Address {address}:")
#             print(f"    Table 1: None")
#             print(f"    Table 2: {entry2}")


def old_compare_tables(table1, table2):
    """ Compare two dictionaries and print differences. """
    keys_table1 = set(table1.keys())
    keys_table2 = set(table2.keys())
    all_keys = keys_table1.union(keys_table2)
    
    differences = []

    for key in sorted(all_keys):
        entry1 = table1.get(key)
        entry2 = table2.get(key)

        if entry1 and entry2:
            # Both tables have an entry at this address
            if entry1 != entry2:
                differences.append((key, entry1, entry2))
        elif entry1:
            # Only table 1 has an entry at this address
            differences.append((key, entry1, None))
        elif entry2:
            # Only table 2 has an entry at this address
            differences.append((key, None, entry2))

    return differences

# Example data
table1_sbmu_rack_input = """
 State:0:1
 Max_Allowed_Charge_Power:1:1
 Max_Allowed_Discharge_Power:2:1
 Max_Charge_Voltage:3:1
 Max_Discharge_Voltage:4:1
 Max_Charge_Current:5:1
 Max_Discharge_Current:6:1
 DI1:7:1
 DI2:8:1
 DI3:9:1
 DI4:10:1
 DI5:11:1
 DI6:12:1
 DI7:13:1
 DI8:14:1
 Total_Voltage:15:1
 Total_Current:16:1
 Module_Temperature:17:1
 SOC:18:1
 SOH:19:1
 Insulation_Resistance:20:1
 Average_cell_voltage:21:1
 Average_cell_temperature:22:1
 Max_cell_voltage:23:1
 Max_cell_voltage_number:24:1
 Min_cell_voltage:25:1
 Min_cell_voltage_number:26:1
 Max_cell_temperature:27:1
 Max_cell_temperature_number:28:1
 Min_cell_temperature:29:1
 Min_cell_temperature_number:30:1
 Max_cell_SOC:31:1
 Max_cell_SOC_number:32:1
 Min_cell_SOC:33:1
 Min_cell_SOC_number:34:1
 Max_cell_SOH:35:1
 Max_cell_SOH_number:36:1
 Min_cell_SOH:37:1
 Min_cell_SOH_number:38:1
 Accumulated_charge_capacity:39:2
 Accumulated_discharge_capacity:41:2
 Accumulated_single_charge_capacity:43:2
 Accumulated_single_discharge_capacity:45:2
 Recharge_capacity:47:2
 Dischargeable_capacity:49:2
 Pack_SOC:51:40
 cell_voltage:91:700
 cell_temperature:791:700
 cell_SOC:1491:700
 cell_SOH:2191:700
"""

# rtos (rack) inputs
#[rtos:input:1:40]
table2_sbmu_rtos_input = """
  rack_voltage:0:1
  rack_current:1:1
  soc:2:1 
  soh:3:1 
  insulation_res_pos:5:1
  insulation_res_neg:6:1
  status:7:1 
  rack_contactors:8:1 
  max_temp:11:1 
  max_temp_num:13:1
  min_temp:14:1
  min_temp_num:16:1
  avg_temp:17:1
  avg_voltage:19:1
  max_voltage:20:2
  max_voltage_num:22:1
  min_voltage:23:1
  min_voltage_num:25:2
  max_soc:27:2
  max_soc_num:29
  min_soc:30:2
  min_soc_num:32:2
  max_soh:34:2
  max_soh_num:36:1
  min_soh:37:2
  min_soh_num:39:1 
dummy1:40:20
 total_charge_cap:59:2
 total_discharge_cap:61:2
 single_charge_cap:63:1
 single_discharge_cap:64:1
dummy2:65:85 
 chargeable_current:150:1
 dischargeable_current:151:1
 chargeable_power:152:1
 dischargeable_power:153:1
 daily_charge_cap:154:2
 daily_discharge_cap:156:2
 adc1:158:1
 adc2:159:1
 adc3:160:1
 adc4:161:1
"""

#  Total_Voltage:15:1
#  Total_Current:16:1
#  Module_Temperature:17:1
#  Insulation_Resistance:20:1
#  Max_cell_SOH:35:1
#  Max_cell_SOH_number:36:1
#  Min_cell_SOH:37:1
#  Min_cell_SOH_number:38:1
#  Accumulated_single_charge_capacity:43:2
#  Accumulated_single_discharge_capacity:45:2
#  Recharge_capacity:47:2
#  Dischargeable_capacity:49:2
#  Pack_SOC:51:40
#  cell_voltage:91:700
#  cell_temperature:791:700
#  cell_SOC:1491:700
#  cell_SOH:2191:700


name_mappings = {
    'State': 'status',
    'Max_Allowed_Charge_Power': 'chargeable_power',
    'Max_Allowed_Discharge_Power':'dischargeable_power',

    'Max_Charge_Voltage':'chargeable_volt',
    'Max_Discharge_Voltage':'dischargeable_volt',

    'Max_Charge_Current':'chargeable_current',
    'Max_Discharge_Current':'dischargeable_current',

    'DI1':"di1",
    'DI2':"di2",
    'DI3':"di3",
    'DI4':"di4",
    'DI5':"di5",
    'DI6':"di6",
    'DI7':"di7",
    'DI8':"di8",

    'Max_cell_voltage':'max_voltage',
    'Max_cell_voltage_number':'max_voltage_num',
    'Min_cell_voltage':'min_voltage',
    'Min_cell_voltage_number':'min_voltage_num',
    'Max_cell_temperature': 'max_temp',
    'Max_cell_temperature_number':'max_temp_num',
    'Min_cell_temperature': 'min_temp',
    'Min_cell_temperature_number':'min_temp_num',
    'Max_cell_SOC':'max_soc',
    'Max_cell_SOC_number':'max_soc_num',
    'Min_cell_SOC':'min_soc',
    'Min_cell_SOC_number':'min_soc_num',
    'SOC':'soc',
    'SOH':'soh',
    'Average_cell_voltage':'avg_voltage',
    'Average_cell_temperature':'avg_temp',

    'Accumulated_charge_capacity':'total_charge_cap',
    'Accumulated_discharge_capacity':'total_discharge_cap'

}


table1_data = table1_sbmu_rack_input
table2_data = table2_sbmu_rtos_input

table1_sbmu_1000_hold = """
 at_total_v_over:1:4
 at_total_v_under:5:4
 at_discharge_curr_over:9:4
 at_fast_charge_curr_over:13:4
 at_insulation_resistance_under:17:4
 at_single_charge_temp_over:21:4
 at_single_charge_temp_under:25:4
 at_single_voltage_over:29:4
 at_single_voltage_under:33:4
 at_single_voltage_diff:37:4
 at_single_temp_diff:41:4
 at_single_soc_under:45:4
 at_pole_temp_over_offset:49:3
 at_pole_temp_over:52:1
 at_mod_volt_over:53:4
 at_mod_volt_under:57:4
 at_single_discharge_temp_over:61:4
 at_single_discharge_temp_under:65:4
 at_single_soc_over:69:4
 value10:73:3
 value0:76:23
 ctrl_cmd_mode:99:1
 upDown_ctrl_cmd:100:1
 gpio_do_read:101:1
 at_bcm_temp_over:102:1
 at_bcm_temp_over_bcm_temp_over_diff:103:1
 g_socsohsetno:104:1
 g_socsohsetval:105:1
 insulate_enable_modbus:106:1
 clusterctrl_fanduty:107:1
 relay_ctrl_fault_recovery:108:1
 alarm_jump_instruction:109:1
 bg_system_alarm_error:110:1
 cluster_control_data_eq_ctrl:111:1
 charge_energy:112:2
 discharge_energy:114:2
 battery_type:116:1
 battery_rated_ah:117:1
 value0_1:118:3
 total_battery_num:121:1
 total_battery_temp_num:122:1
 total_bmm_num:123:1
 single_bmm_bat_num:124:48
 single_bmm_temp_num:172:48
 dummy3:220:8
 battery_self_discharge_rates:228:1
 battery_mfg_date:229:3
 power_station_operational_date:232:3
 battery_full_equ_cycles_num:235:1
 dummy:236:65
 at_cell_discharge_temp_diff_over:301:4
 at_cell_charge_temp_diff_over:305:4
 at_bmm_charge_total_volt_diff_over:309:4
 at_bmm_discharge_total_volt_diff_over:313:4
 dummy2:317:184
 Cell_Charge_End_Voltage_Value_End_Value_Start:501:1
 Cell_Charge_End_Voltage_Value_End_Value_Over:502:1
 Cell_Discharge_End_Voltage_Value_End_Value_Start:503:1
 Cell_Discharge_End_Voltage_Value_End_Value_Over:504:1
 Module_Charge_End_Voltage_Value_End_Value_Start:505:1
 Module_Charge_End_Voltage_Value_End_Value_Over:506:1
 Module_Discharge_End_Voltage_Value_End_Value_Start:507:1
 Module_Discharge_End_Voltage_Value_End_Value_Over:508:1
 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start:509:1
 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over:510:1
 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start:511:1
 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over:512:1
 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start:513:1
 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over:514:1
 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start:515:1
 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over:516:1
 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Start:517:1
 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Over:518:1
 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Start:519:1
 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Over:520:1
 Cell_Charge_end_Temperature_Value_End_Value_Start:521:1
 Cell_Charge_end_Temperature_Value_End_Value_Over:522:1
 Cell_Discharge_end_Temperature_Value_End_Value_Start:523:1
 Cell_Discharge_end_Temperature_Value_End_Value_Over:524:1
 Cluster_Charge_End_Current_Value:525:1
 Cluster_Discharge_End_Current_Value:526:1
 Cell_End_High_Temperature_Value.End_Value_Start_10;:527:1
 Cell_End_High_Temperature_Value.End_Value_Over_10;:528:1
 Cell_End_Low_Temperature_Value.End_Value_Start_10;:529:1
 Cell_End_Low_Temperature_Value.End_Value_Over_10;:530:1
 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Start_10;:531:1
 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Over_10;:532:1
 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Start_10;:533:1
 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Over_10;:534:1
"""

#[rtos:hold:0]
# dummy:0:1
table2_sbmu_rack_hold = """
 at_total_v_over:1:4
 at_total_v_under:5:4
 at_discharge_curr_over:9:4
 at_fast_charge_curr_over:13:4
 at_insulation_resistance_under:17:4
 at_single_charge_temp_over:21:4
 at_single_charge_temp_under:25:4
 at_single_voltage_over:29:4
 at_single_voltage_under:33:4
 at_single_voltage_diff:37:4
 at_single_temp_diff:41:4
 at_single_soc_under:45:4
 at_pole_temp_over_offset:49:3
 at_pole_temp_over:52:1
 at_mod_volt_over:53:4
 at_mod_volt_under:57:4
 at_single_discharge_temp_over:61:4
 at_single_discharge_temp_under:65:4
 at_single_soc_over:69:4
 value10:73:3
 value0:76:23
 ctrl_cmd_mode:99:1
 upDown_ctrl_cmd:100:1
 gpio_do_read:101:1
 at_bcm_temp_over:102:1
 at_bcm_temp_over_bcm_temp_over_diff:103:1
 g_socsohsetno:104:1
 g_socsohsetval:105:1
 insulate_enable_modbus:106:1
 clusterctrl_fanduty:107:1
 relay_ctrl_fault_recovery:108:1
 alarm_jump_instruction:109:1
 bg_system_alarm_error:110:1
 cluster_control_data_eq_ctrl:111:1
 charge_energy:112:2
 discharge_energy:114:2
 battery_type:116:1
 battery_rated_ah:117:1
 value0_1:118:3
 total_battery_num:121:1
 total_battery_temp_num:122:1
 total_bmm_num:123:1
 single_bmm_bat_num:124:48
 single_bmm_temp_num:172:48
 dummy3:220:8
 battery_self_discharge_rates:228:1
 battery_mfg_date:229:3
 power_station_operational_date:232:3
 battery_full_equ_cycles_num:235:1
 dummy:236:65
 at_cell_discharge_temp_diff_over:301:4
 at_cell_charge_temp_diff_over:305:4
 at_bmm_charge_total_volt_diff_over:309:4
 at_bmm_discharge_total_volt_diff_over:313:4
 dummy2:317:184
 Cell_Charge_End_Voltage_Value_End_Value_Start:501:1
 Cell_Charge_End_Voltage_Value_End_Value_Over:502:1
 Cell_Discharge_End_Voltage_Value_End_Value_Start:503:1
 Cell_Discharge_End_Voltage_Value_End_Value_Over:504:1
 Module_Charge_End_Voltage_Value_End_Value_Start:505:1
 Module_Charge_End_Voltage_Value_End_Value_Over:506:1
 Module_Discharge_End_Voltage_Value_End_Value_Start:507:1
 Module_Discharge_End_Voltage_Value_End_Value_Over:508:1
 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start:509:1
 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over:510:1
 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start:511:1
 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over:512:1
 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start:513:1
 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over:514:1
 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start:515:1
 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over:516:1
 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Start:517:1
 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Over:518:1
 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Start:519:1
 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Over:520:1
 Cell_Charge_end_Temperature_Value_End_Value_Start:521:1
 Cell_Charge_end_Temperature_Value_End_Value_Over:522:1
 Cell_Discharge_end_Temperature_Value_End_Value_Start:523:1
 Cell_Discharge_end_Temperature_Value_End_Value_Over:524:1
 Cluster_Charge_End_Current_Value:525:1
 Cluster_Discharge_End_Current_Value:526:1
 Cell_End_High_Temperature_Value.End_Value_Start_10;:527:1
 Cell_End_High_Temperature_Value.End_Value_Over_10;:528:1
 Cell_End_Low_Temperature_Value.End_Value_Start_10;:529:1
 Cell_End_Low_Temperature_Value.End_Value_Over_10;:530:1
 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Start_10;:531:1
 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Over_10;:532:1
 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Start_10;:533:1
 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Over_10;:534:1
"""

# table2_data = """
# at_total_v_over:1:4
# at_total_v_under:5:4
# at_discharge_curr_over:9:4
# at_fast_charge_curr_over:13:4
# at_insulation_resistance_under:17:4
# at_single_charge_temp_under:21:4
# """


compare_tables(table1_data, table2_data, name_mappings)

# # Parse the tables
# table1 = parse_table(table1_data)
# table2 = parse_table(table2_data)

# # Compare the tables
# differences = compare_tables(table1, table2, name_mappings)
# for diff in differences:
#     address, data1, data2 = diff
#     print(f'Address {address}:')
#     print(f'    Table 1: {data1}')
#     print(f'    Table 2: {data2}')
