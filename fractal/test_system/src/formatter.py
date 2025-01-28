# Sample data as a multi-line string (you might be reading this from a file)
data = """
0,1 dummy
1,4 at_total_v_over 
5,4 at_total_v_under
9,4 at_discharge_curr_over
13,4 at_fast_charge_curr_over
17,4 at_insulation_resistance_under
21,4 at_single_charge_temp_over
25,4 at_single_charge_temp_under
29,4 at_single_voltage_over
33,4 at_single_voltage_under
37,4 at_single_voltage_diff
41,4 at_single_temp_diff
45,4 at_single_soc_under
49,3 at_pole_temp_over_offset
52,1 at_pole_temp_over
53,4 at_mod_volt_over
57,4 at_mod_volt_under
61,4 at_single_discharge_temp_over
65,4 at_single_discharge_temp_under
69,4 at_single_soc_over
73,3 value10
76,23 value0
99,1 ctrl_cmd_mode
100,1 upDown_ctrl_cmd
101,1 gpio_do_read 
102, 1 at_bcm_temp_over
103, 1 at_bcm_temp_over_bcm_temp_over_diff
104,1 g_socsohsetno 
105,1 g_socsohsetval
106,1 insulate_enable_modbus
107,1 clusterctrl_fanduty
108,1 relay_ctrl_fault_recovery
109,1 alarm_jump_instruction
110,1 bg_system_alarm_error
111,1 cluster_control_data_eq_ctrl
112,2 charge_energy
114,2 discharge_energy
116,1 battery_type
117,1 battery_rated_ah
118,3 value0_1
121,1 total_battery_num
122,1 total_battery_temp_num
123,1 total_bmm_num
124, 48 single_bmm_bat_num
172, 48 single_bmm_temp_num
220,8 dummy3
228,1 battery_self_discharge_rates
229,3 battery_mfg_date
232,3 power_station_operational_date
235,1 battery_full_equ_cycles_num
236,65 dummy
301,4 at_cell_discharge_temp_diff_over
305,4 at_cell_charge_temp_diff_over
309,4 at_bmm_charge_total_volt_diff_over
313,4 at_bmm_discharge_total_volt_diff_over
317,184 dummy2
501,1 Cell_Charge_End_Voltage_Value_End_Value_Start
502,1 Cell_Charge_End_Voltage_Value_End_Value_Over
503,1 Cell_Discharge_End_Voltage_Value_End_Value_Start
504,1 Cell_Discharge_End_Voltage_Value_End_Value_Over
505,1 Module_Charge_End_Voltage_Value_End_Value_Start
506,1 Module_Charge_End_Voltage_Value_End_Value_Over
507,1 Module_Discharge_End_Voltage_Value_End_Value_Start
508,1 Module_Discharge_End_Voltage_Value_End_Value_Over
509,1 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start
510,1 Module_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over
511,1 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start
512,1 Module_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over
513,1 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Start
514,1 Cluster_Voltage_Difference_Charge_End_Voltage_Value_End_Value_Over
515,1 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Start
516,1 Cluster_Voltage_Difference_Discharge_End_Voltage_Value_End_Value_Over
517,1 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Start
518,1 Cluster_Charge_Cell_Range_end_Voltage_Value_End_Value_Over
519,1 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Start
520,1 Cluster_Discharge_Cell_Range_end_Voltage_Value_End_Value_Over
521,1 Cell_Charge_end_Temperature_Value_End_Value_Start
522,1 Cell_Charge_end_Temperature_Value_End_Value_Over
523,1 Cell_Discharge_end_Temperature_Value_End_Value_Start
524,1 Cell_Discharge_end_Temperature_Value_End_Value_Over
525, 1 Cluster_Charge_End_Current_Value
526,1 Cluster_Discharge_End_Current_Value
527,1 Cell_End_High_Temperature_Value.End_Value_Start_10;
528,1 Cell_End_High_Temperature_Value.End_Value_Over_10;
529,1 Cell_End_Low_Temperature_Value.End_Value_Start_10;
530,1 Cell_End_Low_Temperature_Value.End_Value_Over_10;
531, 1 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Start_10;
532, 1 Cluster_Charge_Cell_Temperature_Difference_Value.End_Value_Over_10;
533, 1 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Start_10;
534,1 Cluster_Discharge_Cell_Temperature_Difference_Value.End_Value_Over_10;
"""
xxxxdata = """
500, 1 rack_num
501, 1 system_fault_reset
502, 1 main_circuit_breaker
503, 1 system_power_control
504, 20 maintenance_mode
524, 1 year
525, 1 month
526, 1 day
527, 1 hour
528, 1 minute
529, 1 second
530, 1 heartbeat
   """
sbmu_data = """   
1, 3 summary_total_undervoltage
4, 3 summary_total_overvoltage
7, 3 summary_overcurrent
10, 3 summary_low_resistance
13, 3 summary_module_low_temp
16, 3 summary_module_over_temp
19, 3 summary_cell_over_voltage     
22, 3 summary_cell_under_voltage   
25, 3 summary_cell_diff_voltage
27, 3 summary_cell_low_temp
31, 3 summary_cell_over_temp
34, 3 summary_cell_diff_temp
37, 3 summary_cell_low_SOC
40, 3 summary_cell_high_SOC
43, 3 summary_cell_low_SOH
46, 3 summary_cell_high_SOH
49, 1 ESBCM_lost_communication
50, 1 ESBMM_lost_communication
51, 1 Abnormal_total_voltage_diff
52, 1 Abnormal_contactor_open
53, 1 Abnormal_contactor_close
54, 1 Charge_prohibited
55, 1 Discharge_prohibited
56, 1 summary_BMS_system_alarms
57, 1 summary_BMS_system_faults
58, 1 input_IN0
59, 1 input_IN1
60, 1 input_IN2
61, 1 input_IN3
62, 9 gap
70, 3 summary_terminal_over_temp
73, 3 summary_pack_over_voltage
76, 3 summary_pack_under_voltage
79, 1 cell_voltage_acquisition_fault
80, 1 cell_temp_acquisition_fault

"""
sbmu_bits_data = """
200, 1 esbcm_lost_comms  
201, 3 total_undervoltage
204, 3 total_overvoltage
207, 3 overcurrent  
210, 3 cell_under_voltage 
213, 3 cell_over_voltage 
216, 3 cell_low_temp    
219, 3 cell_over_temp   
222, 3 cell_low_SOC  
225, 3 cell_high_SOC  
228, 3 cell_low_SOH
231, 3 cell_voltage_diff 
234, 3 cell_temp_diff 
270, 6 bmm_lost_comms 
277, 3 terminal_high_temp 
280, 3 pack_over_voltage 
283, 3 pack_under_voltage 
286, 1 cell_voltage_acquisition_fault
287, 1 cell_temp_acquisition_fault
"""
rack_di_data = """
1,3 terminal_overvoltage
4,3 terminal_undervoltage
7,3 discharge_overcurrent
10,3 fastcharge_overcurrent
13,3 insulation_low
16,3 single_charge_overtemp
19,3 single_charge_lowtemp
22,3 single_overvoltage
25,3 single_undervoltage
28,3 single_voltage_diff
31,3 single_temp_diff
34,3 soc_low
37,3 terminal_overtemp
40,3 module_overvoltage
43,3 module_undervoltage
46,1 di1mcerror
47,1 di2mcerror
48,1 di3mcerror
49,1 di4mcerror
50,1 di5mcerror
51,1 di6mcerror
52,1 di7mcerror
53,1 di8mcerror
54,1 can0error
55,1 vol_lost
56,1 temp_lost
57,1 bg_system_alarm_error
58,1 group_voltdiff
59,1 jump_instruction
60,1 battery_extreme_error
61,1 project_version_diff
62,1 pcs_comms_lost
63,1 pcs_debug_mode
64,1 can_hall_sensor_error
65,1 can_hall_comm_lost
66,1 pcb_check_error
67,1 svol_harness_error
68,1 eq_error
69,1 ems_comm_error
70,1 bms_comm_error
71,3 single_discharge_overtemp 
74,3 single_discharge_lowtemp 
76,3 soc_over 
79,47 gap
126,1 zero1
127,3 bmm_charge_vol_d_value
130,3 cell_discharge_temp_d_value
133,3 cell_charge_temp_d_value
136,3 bmm_discharge_vol_d_value
139,1 cluster_circulation_warning
140,1 zero2
141,1 hv_sample_volt_diff
142,158 gap2
300,1  cell_charge_vol
301,1  cell_discharge_vol
302,1  cell_charge_vol
303,1  module_discharge_vol
304,1  module_vol_d_charge
305,1  module_vol_d_discharge
306,1  cluster_charge_vol
307,1  cluster_discharge_vol
308,1  cluster_charge_cell_range_vol
309,1  cluster_discharge_cell_range_vol
310,1  cluster_charge_module_range_vol
311,1  cluster_discharge_module_range_vol
312,1  cluster_charge_curr
313,1  cluster_discharge_curr
314,1  cutoff_cell_high_temp
315,1  cutoff_cell_low_temp
316,1  cutoff_cluster_charge_cell_temp_d
317,1  cutoff_cluster_discharge_cell_temp_d
318, 682  gap3
1000,60  bmm_lost_status
"""
data2 = """
100, 1 State
101, 1 Max_Allowed_Charge_Power
102, 1 Max_Allowed_Discharge_Power
103, 1 Max_Charge_Voltage
104, 1 Max_Discharge_Voltage
105, 1 Max_Charge_Current
106, 1 Max_Discharge_Current
107, 1 DI1
108, 1 DI2
109, 1 DI3
110, 1 DI4
111, 1 DI5
112, 1 DI6
113, 1 DI7
114, 1 DI8
115, 1 Total_Voltage
116, 1 Total_Current
117, 1 Module_Temperature
118, 1 SOC
119, 1 SOH
120, 1 Insulation_Resistance 
121, 1 Average_cell_voltage 
122, 1 Average_cell_temperature 
123, 1 Max_cell_voltage 
124, 1 Max_cell_voltage_number 
125, 1 Min_cell_voltage 
126, 1 Min_cell_voltage_number 
127, 1 Max_cell_temperature 
128, 1 Max_cell_temperature_number 
129, 1 Min_cell_temperature 
130, 1 Min_cell_temperature_number 
131, 1 Max_cell_SOC 
132, 1 Max_cell_SOC_number 
133, 1 Min_cell_SOC 
134, 1 Min_cell_SOC_number 
135, 1 Max_cell_SOH 
136, 1 Max_cell_SOH_number 
137, 1 Min_cell_SOH 
138, 1 Min_cell_SOH_number 
139, 2 Accumulated_charge_capacity 
140, 2 Accumulated_discharge_capacity 
143, 2 Accumulated_single_charge_capacity 
145, 2 Accumulated_single_discharge_capacity 
147, 2 Recharge_capacity 
149, 2 Dischargeable_capacity 
151, 40 Pack_SOC, bmm
191, 700 cell_voltage, cell
891, 700 cell_temperature, cell
1591, 700 cell_SOC, cell
2291, 700 cell_SOH, cell
"""

xdata = """
1, 1 system_circuit_breaker
2, 1 system_total_voltage
3, 1 system_current
4, 1 system_SOC
5, 1 system_SOH
6, 1 min_pack_voltage
7, 1 max_pack_voltage
8, 1 min_battery_voltage
9, 1 max_battery_voltage
10, 1 min_battery_voltage_number
11, 1 min_battery_voltage_group
12, 1 max_battery_temperature
13, 1 max_battery_temperature_pack_number
14, 1 max_battery_temperature_group
15, 1 min_battery_temperature
16, 1 min_battery_temperature_pack_number
17, 1 min_battery_temperature_group
18, 2 accumulated_charging_capacity
20, 2 accumulated_discharging_capacity
22, 2 single_cumulative_charge_power
24, 2 single_cumulative_discharge_power
26, 2 system_charge_capacity
28, 2 system_discharge_capacity
30, 1 available_discharge_time
31, 1 available_charge_time
32, 1 max_discharge_power
33, 1 max_charge_power
34, 1 max_discharge_current
35, 1 max_charge_current
36, 1 num_daily_charges
37, 1 num_daily_discharges
38, 2 total_daily_discharge
40, 2 total_daily_charge
42, 1 operating_temperature
43, 1 state
44, 1 charge_state
45, 1 insulation_resistance
46, 1 pcs_bms_communication_failure
47, 1 ems_bms_communication_failure
48, 2 pile_cumulative_charging_time
50, 2 pile_cumulative_discharging_time
"""

# Process the data
lines = data.strip().split("\n")
config_block = "[sbmu:input]\n"
offset = 0  # Starting offset (you may need to adjust based on actual needs)

for line in lines:
    parts = line.split(',')
    print(parts)
    item_offset = int(parts[0].strip())  # this is assumed to be the offset in your description
    size_description = parts[1].strip().split(' ')
    size = int(size_description[0])
    description = ' '.join(size_description[1:])

    # Construct the entry for the configuration block
    config_block += f" {description}:{offset}:{size}\n"
    offset += size  # Increment the offset for the next item

# Print the formatted configuration block
print(config_block)
