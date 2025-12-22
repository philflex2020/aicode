Nice spec dump. 
That’s exactly the kind of thing your current system is designed to encode.

Given how huge and a bit irregular that table is (Chinese/English mixed, line wraps, some blanks like `10s/60s`, etc.), I’ll do this in a *controlled* way:

- Define how it maps into your three CSVs.
- Fully work out **Alarms 1–3** as concrete CSV rows you can drop into your files.
- Show how to continue the pattern for the rest.

If you want, we can iterate and fill out more alarms later, but this gets you a solid starting point and verifies the mapping logic.

---

### 1. CSV schemas (matching your current code)

These match what your Flask app already uses.

#### `alarm_definition.csv`

Columns (header row):

```csv
alarm_num,alarm_name,num_levels,measured_variable,limits_structure,comparison_type,alarm_variable,latched_variable,notes
```

- `alarm_num`: integer, unique per alarm (from the “serial number / NO.” in your table).
- `alarm_name`: human-readable name (“Cell over-voltage”, etc.).
- `num_levels`: 1–3.
- `measured_variable`: optional; you can fill with things like `sbmu:input:cell_voltage_max` later.
- `limits_structure`: string key linking to `limits_values.csv` (e.g. `cell_over_voltage`).
- `comparison_type`: one of `greater_than`, `less_than`, etc. (we just wired this up).
- `alarm_variable`, `latched_variable`: optional; leave blank for now.
- `notes`: free text; I’ll stash bilingual descriptions there.

#### `alarm_level_actions.csv`

Columns:

```csv
alarm_num,alarm_name,level,enabled,duration,actions,notes
```

- `level`: 1, 2, 3.
- `enabled`: `1` or `0`.
- `duration`: seconds (strip the “s”, “S”).
- `actions`: freeform string (you can later standardize).
- `notes`: extra Chinese/English text if useful.

#### `limits_values.csv`

Columns:

```csv
limits_structure,alarm_num,alarm_name,level1_limit,level2_limit,level3_limit,hysteresis,last_updated,notes
```

- One row per limits structure (usually “one alarm’s worth of thresholds”).
- `level1_limit/2/3`: thresholds per level.
- `hysteresis`: if you want to encode `threshold - return_diff` as a single number. Where ambiguous, we can leave at `0` for now.
- `last_updated`: ISO timestamp or empty.
- `notes`: any clarifications.

---

### 2. Alarm 1 – Cell over-voltage

From your spec:

- NO: 1
- Project / name: “Cell over-voltage (V) / High cell voltage (V)”
- Levels:

| Level | Trigger threshold | Return difference | Duration | Action                                                        |
|-------|-------------------|-------------------|----------|----------------------------------------------------------------|
| 1     | 3.5               | 3.45              | 3 s      | Report alarm                                                  |
| 2     | 3.6               | 3.55              | 3 s      | Forbid charging, allow discharging, set group SOC = 100%     |
| 3     | 3.65              | 3.6               | 3 s      | Output dry contact, delayed 3 s trip                         |

We’ll call the limits structure `cell_over_voltage`.

#### `alarm_definition.csv` (Alarm 1 row)

```csv
alarm_num,alarm_name,num_levels,measured_variable,limits_structure,comparison_type,alarm_variable,latched_variable,notes
1,"Cell over-voltage",3,,"cell_over_voltage","greater_than",,,"High cell voltage (V); Spec row 1"
```

#### `alarm_level_actions.csv` (Alarm 1 rows)

```csv
alarm_num,alarm_name,level,enabled,duration,actions,notes
1,"Cell over-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
1,"Cell over-voltage",2,1,3,"forbid_charging;allow_discharging;set_group_soc_100","告警触发阀值-回差值后30S / Charging disallowed; discharging allowed; SOC=100%"
1,"Cell over-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"
```

You can later standardize `actions` into enums; for now, strings are fine.

#### `limits_values.csv` (Alarm 1 row)

Thresholds and return differences:

- L1: 3.5, return at 3.45 ⇒ hysteresis ≈ 0.05
- L2: 3.6, return at 3.55 ⇒ 0.05
- L3: 3.65, return at 3.60 ⇒ 0.05

We’ll encode:

```csv
limits_structure,alarm_num,alarm_name,level1_limit,level2_limit,level3_limit,hysteresis,last_updated,notes
"cell_over_voltage",1,"Cell over-voltage",3.5,3.6,3.65,0.05,,"Return differences: L1 3.45, L2 3.55, L3 3.60"
```

---

### 3. Alarm 2 – Cell under-voltage

From your spec:

- NO: 2
- Name: “Cell under-voltage (V) / The cell voltage is too low (V)”
- Levels:

| Level | Threshold | Return diff | Duration | Action                                                      |
|-------|-----------|-------------|----------|--------------------------------------------------------------|
| 1     | 2.95      | 0.1         | 3 s      | Report alarm                                                |
| 2     | 2.5       | (blank, but ref to “threshold+return diff”) | 3 s | Allow charging, no discharge; set SOC 0%                    |
| 3     | 2.4       | (blank, threshold+return diff)             | 3 s | Output dry contact, delayed 3 s trip                        |

We’ll call this `cell_under_voltage`.

#### `alarm_definition.csv` row

```csv
2,"Cell under-voltage",3,,"cell_under_voltage","less_than",,,"The cell voltage is too low (V); Spec row 2"
```

(Full CSV with header + rows further below.)

#### `alarm_level_actions.csv` rows

```csv
2,"Cell under-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
2,"Cell under-voltage",2,1,3,"allow_charging;forbid_discharging;set_group_soc_0","允许充电；禁止放电 / Group SOC calibration set 0%"
2,"Cell under-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"
```

#### `limits_values.csv` row

We’ll use the thresholds directly and keep hysteresis at `0.1` (from L1) as a first pass:

```csv
"cell_under_voltage",2,"Cell under-voltage",2.95,2.5,2.4,0.1,,"Return diff ~0.1 according to spec"
```

---

### 4. Alarm 3 – Cell voltage difference

From your spec:

- NO: 3
- Name: “Cell voltage difference (mV) / Single cell voltage dropout (mV)”
- Levels:

| Level | Threshold (mV) | Return diff (mV) | Duration | Action                                                         |
|-------|----------------|------------------|----------|-----------------------------------------------------------------|
| 1     | 500            | 100              | 3 s      | Report alarm; Charging reduces current by 50%                  |
| 2     | 600            | 100              | 3 s      | Prohibit charging; Prohibit discharging                        |
| 3     | 800            | 200              | 3 s      | Output dry contact, delayed 3 s trip                           |

We’ll call this `cell_voltage_diff`.

#### `alarm_definition.csv` row

```csv
3,"Cell voltage difference",3,,"cell_voltage_diff","greater_than",,,"Single cell voltage dropout (mV); Spec row 3"
```

#### `alarm_level_actions.csv` rows

```csv
3,"Cell voltage difference",1,1,3,"report_alarm;reduce_charge_current_50","上报告警充电降流50% / Report alarm, charge current -50%"
3,"Cell voltage difference",2,1,3,"forbid_charging;forbid_discharging","禁止充电；禁止放电 / Prohibit charging and discharging"
3,"Cell voltage difference",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"
```

#### `limits_values.csv` row

Hysteresis: first two levels use 100 mV, third uses 200 mV. To keep the `limits_values` model simple (one hysteresis per structure), we can:

- Either pick the *smallest* (100)
- Or pick 0 and encode the exact return behavior in `notes`/actions.

I’ll pick 100 for now and document special L3 in notes:

```csv
"cell_voltage_diff",3,"Cell voltage difference",500,600,800,100,,"Return diff: L1=100mV, L2=100mV, L3=200mV"
```

---

### 5. Putting it together – actual CSV fragments

Here are the three CSVs for alarms 1–3 with headers.

#### `alarm_definition.csv` (partial)

```csv
alarm_num,alarm_name,num_levels,measured_variable,limits_structure,comparison_type,alarm_variable,latched_variable,notes
1,"Cell over-voltage",3,,"cell_over_voltage","greater_than",,,"High cell voltage (V); Spec row 1"
2,"Cell under-voltage",3,,"cell_under_voltage","less_than",,,"The cell voltage is too low (V); Spec row 2"
3,"Cell voltage difference",3,,"cell_voltage_diff","greater_than",,,"Single cell voltage dropout (mV); Spec row 3"
```

You can append rows 4, 5, … using the same pattern.

#### `alarm_level_actions.csv` (partial)

```csv
alarm_num,alarm_name,level,enabled,duration,actions,notes
1,"Cell over-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
1,"Cell over-voltage",2,1,3,"forbid_charging;allow_discharging;set_group_soc_100","告警触发阀值-回差值后30S / Prohibit charging; allow discharge; SOC=100%"
1,"Cell over-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"

2,"Cell under-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
2,"Cell under-voltage",2,1,3,"allow_charging;forbid_discharging;set_group_soc_0","允许充电；禁止放电 / Group SOC calibration set 0%"
2,"Cell under-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"

3,"Cell voltage difference",1,1,3,"report_alarm;reduce_charge_current_50","上报告警充电降流50% / Report alarm, charge current -50%"
3,"Cell voltage difference",2,1,3,"forbid_charging;forbid_discharging","禁止充电；禁止放电 / Prohibit charging and discharging"
3,"Cell voltage difference",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip"
```

#### `limits_values.csv` (partial)

```csv
limits_structure,alarm_num,alarm_name,level1_limit,level2_limit,level3_limit,hysteresis,last_updated,notes
"cell_over_voltage",1,"Cell over-voltage",3.5,3.6,3.65,0.05,,"Return diffs: L1=3.45, L2=3.55, L3=3.60"
"cell_under_voltage",2,"Cell under-voltage",2.95,2.5,2.4,0.1,,"Return diff ~0.1 (see spec)"
"cell_voltage_diff",3,"Cell voltage difference",500,600,800,100,,"Return diff: L1=100mV, L2=100mV, L3=200mV"
```

---

### 6. How to extend this to the full table

For each new alarm in your spec:

1. Pick a canonical **`alarm_name`** (English or concise English+Chinese).
2. Set **`alarm_num`** to the table’s “serial number / NO.”
3. Count how many levels (1–3) → `num_levels`.
4. Create a **`limits_structure`** name (e.g. `high_charge_temp`, `low_discharge_temp`, `rack_total_voltage_high`, etc.).
5. Map each level:
   - Threshold → into `levelN_limit`.
   - Duration → into `duration` (strip “s”).
   - Action text → into `actions`/`notes`.
6. Decide `comparison_type`:
   - “Too high” / “over-voltage” / “over-temperature” ⇒ `greater_than`.
   - “Too low” / “undervoltage” / “low temperature” ⇒ `less_than`.
7. Fill `limits_values.csv`:
   - One row per `limits_structure`.
   - Thresholds for L1/L2/L3.
   - Optional: pick a reasonable `hysteresis` (or leave 0, and keep exact behavior in `notes`).

---

If you want, next time we can:

- Do another batch (e.g. alarms 4–8) together, to make sure the mapping conventions are exactly what you want before you burn time populating the full set.
- Or, once you decide the JSON format for the C++ target, we can auto‑export this structure directly into that JSON so you don’t have to think about CSV at all on the embedded side.


Good, then I’ll push it through systematically.

I’ll continue the same mapping rules we agreed on, and give you CSV *content* you can paste into your three files. I’ll do this in sections so it’s readable. You can always adjust `limits_structure` names or `actions` strings later.

I’ll assume:

- 3 CSVs:  
  - `alarm_definition.csv`  
  - `alarm_level_actions.csv`  
  - `limits_values.csv`  
- One `limits_structure` per “parameter family” (per row group in your spec), even when thresholds are expressed also as pack voltages, etc.

I’ll first restate the full headers, then append rows. I’ll number alarms based on your NO. field.

---

### 1. `alarm_definition.csv` – header + all rows

**Header:**

```csv
alarm_num,alarm_name,num_levels,measured_variable,limits_structure,comparison_type,alarm_variable,latched_variable,notes
```

**Rows 1–19 (cell/rack/box/BMM alarms):**

```csv
1,"Cell over-voltage",3,,"cell_over_voltage","greater_than",,,"High cell voltage (V); Spec row 1"
2,"Cell under-voltage",3,,"cell_under_voltage","less_than",,,"Cell voltage too low (V); Spec row 2"
3,"Cell voltage difference",3,,"cell_voltage_diff","greater_than",,,"Single cell voltage dropout (mV); Spec row 3"
4,"High temperature of battery charging unit",3,,"high_charge_cell_temp","greater_than",,,"High battery rechargeable cell temperature (°C); Spec row 4"
5,"Low temperature of battery charging unit",3,,"low_charge_cell_temp","less_than",,,"Low temperature of battery charging cell (°C); Spec row 5"
6,"High temperature of battery discharge cell",3,,"high_discharge_cell_temp","greater_than",,,"High discharge cell temperature (°C); Spec row 6"
7,"Low temperature of battery discharge cell",3,,"low_discharge_cell_temp","less_than",,,"Low cell discharge temperature (°C); Spec row 7"
8,"Temperature difference of individual cells within the rack",3,,"cell_temp_diff_within_rack","greater_than",,,"Large temperature difference of single cells in cluster (°C); Spec row 8"
9,"Low SOC",3,,"low_soc","less_than",,,"Low SOC (1%); Spec row 9"
10,"High SOC",3,,"high_soc","greater_than",,,"SOC high (1%); Spec row 10"
11,"Rack total voltage high",3,,"rack_total_voltage_high","greater_than",,,"Rack total voltage high (V); Spec row 11"
12,"Rack total voltage low",3,,"rack_total_voltage_low","less_than",,,"Rack total voltage low (V); Spec row 12"
13,"BMM pole temperature over-temperature",3,,"bmm_pole_temp_high","greater_than",,,"Slave Pole Temperature Overtemperature Alarm (°C); Spec row 13"
14,"Charging overcurrent",3,,"charge_overcurrent","greater_than",,,"Charge Overcurrent (A); Spec row 14"
15,"Discharging overcurrent",3,,"discharge_overcurrent","greater_than",,,"Discharge Overcurrent (A); Spec row 15"
16,"Low insulation",3,,"low_insulation","less_than",,,"Low insulation (KΩ/V*[rated voltage]); Spec row 16"
17,"High total voltage of battery box",3,,"box_total_voltage_high","greater_than",,,"Total voltage of battery box too high (V); Spec row 17"
18,"Low total voltage of battery box",3,,"box_total_voltage_low","less_than",,,"Total voltage of battery box too low (V); Spec row 18"
19,"High voltage box pole temperature over-temperature",3,,"hv_box_pole_temp_high","greater_than",,,"High voltage box pole temperature overtemperature alarm (°C); Spec row 19"
```

**Rows 20–38 (master fault list):**

```csv
20,"Communication between master and slave faulty",1,,"bcm_bmm_comm_fault","event",,,"BCM–BMM communication failure; Spec row 20"
21,"Single cell voltage acquisition fault",1,,"single_cell_voltage_acq_fault","event",,,"Failure of single voltage acquisition; Spec row 21"
22,"Individual temperature acquisition fault",1,,"single_temp_acq_fault","event",,,"Failure of single body temperature collection; Spec row 22"
23,"Temperature rise alarm",1,,"temp_rise_alarm","event",,,"Temperature rise rate >=10°C/min; Spec row 23"
24,"BCM circuit breaker malfunction",1,,"bcm_breaker_fault","event",,,"Main control: Circuit breaker failure; Spec row 24"
25,"BCM contactor malfunction",1,,"bcm_contactor_fault","event",,,"Main control: Contactor failure; Spec row 25"
26,"Slave peripheral failure (DI detection)",1,,"bmm_peripheral_fault","event",,,"BMM peripheral failure (DI detection); Spec row 26"
27,"Hall sensor communication failure",1,,"hall_comm_fault","event",,,"Hall sensor communication malfunction; Spec row 27"
28,"Hall sensor malfunction",1,,"hall_sensor_fault","event",,,"Hall sensor hardware exception; Spec row 28"
29,"Circulation overflow",1,,"circulation_overcurrent","event",,,"Circulation overcurrent >5A; Spec row 29"
30,"Thermal runaway",1,,"thermal_runaway","event",,,"Thermal runaway (see spec description); Spec row 30"
31,"Main positive contactor feedback abnormal",1,,"main_pos_contactor_fb_abnormal","event",,,"Main positive contactor cannot be closed/disconnected; Spec row 31"
32,"Main negative contactor feedback abnormal",1,,"main_neg_contactor_fb_abnormal","event",,,"Main negative contactor cannot be closed/disconnected; Spec row 32"
33,"Precharge failed",1,,"precharge_failed","event",,,"Pre charging failed; Spec row 33"
34,"BCM T0~T4 copper bar over-temperature",3,,"bcm_copperbar_temp_high","greater_than",,,"BCM T0~T4 copper bar temperature over temperature; Spec row 34"
35,"Cell–pole temperature deviation too large",1,,"cell_pole_temp_deviation_large","greater_than",,,"Deviation between cell temperature and pole temperature too large; Spec row 35"
36,"Cell accumulation vs total pressure deviation >30V",1,,"cell_sum_vs_total_deviation","greater_than",,,"Deviation between accumulation and total pressure >30V; Spec row 36"
37,"Ultimate overvoltage",1,,"ultimate_overvoltage","greater_than",,,"Vmax>=3.8V; Spec row 37"
38,"Limit undervoltage",1,,"limit_undervoltage","less_than",,,"Vmin<=2V and Tmin>0°C OR Vmin<=1.6V and Tmin<=0°C; Spec row 38"
```

**Display & control fault list (continuing numbering):**

```csv
39,"Display–master communication faulty",1,,"esmu_bcm_comm_fault","event",,,"ESMU and BCM communication malfunction; Display fault list row 1"
40,"Faults with current differences between clusters",3,,"rack_current_difference_fault","event",,,"Rack current difference fault; Display fault list row 2"
41,"Inter-cluster current imbalance (slight)",1,,"inter_cluster_current_imbalance_slight","event",,,"Slight inter-cluster current imbalance alarm; Display fault list row 3"
42,"Cluster loop abnormality",1,,"rack_loop_abnormal","event",,,"Rack circuit abnormality; Display fault list row 4"
43,"Combiner cabinet breaker fails to close",1,,"combiner_cb_close_fail","event",,,"Combiner cabinet circuit breaker closing failure alarm; Display fault list row 5"
44,"PCS control failure",1,,"pcs_control_failure","event",,,"PCS control failure alarm; Display fault list row 6"
45,"UPS communication abnormal",1,,"ups_comm_abnormal","event",,,"UPS communication abnormal; Display fault list row 7"
46,"PCS communication abnormal",1,,"pcs_comm_abnormal","event",,,"PCS communication abnormal; Display fault list row 8"
47,"Inter-cluster current imbalance (A)",3,,"rack_current_imbalance_a","event",,,"Rack current imbalance alarm (A); Display fault list row 9"
48,"QF1 circuit breaker feedback abnormal",1,,"qf1_cb_fb_abnormal","event",,,"QF1 circuit breaker feedback abnormality; Display fault list row 10"
49,"UPS failure",1,,"ups_failure","event",,,"UPS Failure; Display fault list row 11"
50,"UPS mains power faulty",2,,"ups_mains_fault","event",,,"UPS reports mains power failure; Display fault list row 12"
51,"TMS communication abnormal",1,,"tms_comm_abnormal","event",,,"TMS communication abnormality; Display fault list row 13"
52,"TMS failure",2,,"tms_fault","event",,,"TMS reports Level1/Level2 faults; Display fault list row 14"
53,"Cell accumulation vs total pressure deviation",1,,"cell_sum_vs_total_deviation2","greater_than",,,"Deviation between accumulation and total pressure; Display fault list row 15"
```

Note: for many of 20+ the “comparison_type” is effectively “event/boolean”; I used `"event"` to mark that these are not simple threshold comparisons. Your C++/Python logic can treat `"event"` as special.

---

### 2. `limits_values.csv` – all limits structures

**Header:**

```csv
limits_structure,alarm_num,alarm_name,level1_limit,level2_limit,level3_limit,hysteresis,last_updated,notes
```

Now rows, grouped by the same structures used above.

```csv
"cell_over_voltage",1,"Cell over-voltage",3.5,3.6,3.65,0.05,,"Return: L1 3.45, L2 3.55, L3 3.60 (V)"
"cell_under_voltage",2,"Cell under-voltage",2.95,2.5,2.4,0.1,,"Return diff ~0.1 V; see spec"
"cell_voltage_diff",3,"Cell voltage difference",500,600,800,100,,"Return diff: L1 100mV, L2 100mV, L3 200mV"

"high_charge_cell_temp",4,"High temperature of battery charging unit",55,60,65,5,,"Return diff ~5°C; Level1: 55->50, etc."
"low_charge_cell_temp",5,"Low temperature of battery charging unit",5,0,-30,5,,"Return diff ~5°C for Level1; others see text"
"high_discharge_cell_temp",6,"High temperature of battery discharge cell",55,60,65,5,,"High discharge cell temperature; return diff ~5°C"
"low_discharge_cell_temp",7,"Low temperature of battery discharge cell",5,0,-10,5,,"Low discharge cell temperature; return diff ~5°C"

"cell_temp_diff_within_rack",8,"Temperature difference of individual cells within the rack",8,10,15,5,,"Return diff ~5°C; see spec"
"low_soc",9,"Low SOC",5,0,0,3,,"L1:5%, L2:0%, L3:0%; return diff ~3% where applicable"
"high_soc",10,"High SOC",98,100,100,2,,"L1:98%, L2:100%, L3:100%; return diff ~2% where applicable"

"rack_total_voltage_high",11,"Rack total voltage high",1476.8,1510.08,1539.2,41.6,,"Based on 416 cells: see spec formulas"
"rack_total_voltage_low",12,"Rack total voltage low",1227.2,1040,998.4,41.6,,"Based on 416 cells: see spec formulas"

"bmm_pole_temp_high",13,"BMM pole temperature over-temperature",95,100,105,5,,"Return diff ~5°C"
"charge_overcurrent",14,"Charging overcurrent",221,231,241,20,,"Return diff ~20A; see spec"
"discharge_overcurrent",15,"Discharging overcurrent",221,231,241,20,,"Return diff ~20A; see spec"

"low_insulation",16,"Low insulation",3393,2661,1331,200,,"L1:1.5kΩ/V*1500V=2250kΩ? Spec shows 3393 etc.; values from table; hysteresis rough 200"
"box_total_voltage_high",17,"High total voltage of battery box",369.2,377.5,384.8,10.4,,"Based on 104 cells: see spec formulas"
"box_total_voltage_low",18,"Low total voltage of battery box",306.8,260,249.6,10,,"Based on 104 cells; L1 return diff 10V; see spec"

"hv_box_pole_temp_high",19,"High voltage box pole temperature over-temperature",95,100,105,5,,"Return diff ~5°C"

"bcm_copperbar_temp_high",34,"BCM T0~T4 copper bar over-temperature",105,110,115,5,,"Return diff ~5°C"

"cell_pole_temp_deviation_large",35,"Cell–pole temperature deviation too large",30,0,0,5,,"Level1 only: 30°C deviation; 5°C return diff approx"
"cell_sum_vs_total_deviation",36,"Cell accumulation vs total pressure deviation >30V",30,0,0,0,,">30V; event-type threshold"
"ultimate_overvoltage",37,"Ultimate overvoltage",3.8,0,0,0,,"Vmax>=3.8V; additional logic on Tmin"
"limit_undervoltage",38,"Limit undervoltage",2.0,1.6,0,0,,"Vmin<=2V (T>0°C) or Vmin<=1.6V (T<=0°C); see spec"
```

For most “event” style alarms (20–53) I’m not creating additional `limits_structure` rows; they are mostly boolean/event conditions. If you want threshold-like tuning for e.g. current differences (60/80/100A) in display-list alarm 9, we can add a `limits_structure` row for that too. Here’s one example for that alarm:

```csv
"rack_current_imbalance_a",47,"Inter-cluster current imbalance (A)",60,80,100,10,,"Current imbalance thresholds at 60/80/100A; approx 10A hysteresis"
```

You can either include that in `limits_values.csv` or skip it if you’ll hardcode those thresholds in code.

---

### 3. `alarm_level_actions.csv` – actions for all alarms

**Header:**

```csv
alarm_num,alarm_name,level,enabled,duration,actions,notes
```

I’ll keep `enabled=1` for all defined levels. Durations are in seconds; where spec says `10s/60s` I’ll store `10` and note the 60s in `notes`.

#### 3.1. Alarms 1–3 (already started, now in final form)

```csv
1,"Cell over-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
1,"Cell over-voltage",2,1,3,"forbid_charging;allow_discharging;set_group_soc_100","Prohibit charging; allow discharge; Group SOC calibration set 100%; 告警触发阀值-回差值后30S"
1,"Cell over-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip; Restart and release on fault reset"

2,"Cell under-voltage",1,1,3,"report_alarm","告警触发阀值-回差值后30S / Report alarm"
2,"Cell under-voltage",2,1,3,"allow_charging;forbid_discharging;set_group_soc_0","允许充电；禁止放电 / Group SOC calibration set 0%; clear when alarm removed and all racks have charging current"
2,"Cell under-voltage",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip; clear on alarm remove + ESMU fault reset"

3,"Cell voltage difference",1,1,3,"report_alarm;reduce_charge_current_50","上报告警充电降流50% / Report alarm, reduce charging current by 50%"
3,"Cell voltage difference",2,1,3,"forbid_charging;forbid_discharging","禁止充电；禁止放电 / Prohibit charging and discharging; clear after 30min delay"
3,"Cell voltage difference",3,1,3,"trip_via_dry_contact","输出干结点，延迟3s执行跳机流程 / Output dry contact, delayed 3s trip; fault reset required"
```

#### 3.2. Alarms 4–8 (temperature-related & temp difference)

```csv
4,"High temperature of battery charging unit",1,1,3,"report_alarm;reduce_charge_current_50","上报告警充电降流50% / Report alarm, reduce charging current by 50%"
4,"High temperature of battery charging unit",2,1,3,"forbid_charging;allow_discharging","Prohibit charging; allow discharge; Fault remove clears"
4,"High temperature of battery charging unit",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; fault reset required"

5,"Low temperature of battery charging unit",1,1,3,"report_alarm;reduce_charge_current_50","Report alarm, reduce charging current by 50%"
5,"Low temperature of battery charging unit",2,1,3,"forbid_charging;allow_discharging","Prohibit charging; allow discharge; Fault remove clears"
5,"Low temperature of battery charging unit",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; fault reset required"

6,"High temperature of battery discharge cell",1,1,3,"report_alarm","Report alarm"
6,"High temperature of battery discharge cell",2,1,3,"allow_charging;forbid_discharging","Charging allowed; discharging disallowed; Fault remove clears"
6,"High temperature of battery discharge cell",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; fault reset required"

7,"Low temperature of battery discharge cell",1,1,3,"report_alarm;reduce_discharge_current_50","Report alarm, reduce discharge current by 50%"
7,"Low temperature of battery discharge cell",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging; prohibit discharging; Fault remove clears"
7,"Low temperature of battery discharge cell",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; fault reset required"

8,"Temperature difference of individual cells within the rack",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Report alarm, charge/discharge current reduction of 50%"
8,"Temperature difference of individual cells within the rack",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging; prohibit discharging; Fault remove clears"
8,"Temperature difference of individual cells within the rack",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; fault reset required"
```

#### 3.3. Alarms 9–12 (SOC and rack voltage)

```csv
9,"Low SOC",1,1,3,"alarm_only","Only alarm"
9,"Low SOC",2,1,3,"alarm_only","Only alarm"
9,"Low SOC",3,1,3,"alarm_only","Only alarm"

10,"High SOC",1,1,3,"none","No action defined (/) in spec"
10,"High SOC",2,1,3,"none","No action defined (/) in spec"
10,"High SOC",3,1,3,"none","No action defined (/) in spec"

11,"Rack total voltage high",1,1,3,"report_alarm","Report alarm"
11,"Rack total voltage high",2,1,3,"forbid_charging;allow_discharging","Prohibit charging; allow discharge; clear when alarm removed and all racks have charging current"
11,"Rack total voltage high",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart and ESMU fault reset to clear"

12,"Rack total voltage low",1,1,3,"report_alarm","Report alarm"
12,"Rack total voltage low",2,1,3,"allow_charging;forbid_discharging","Charging allowed; discharging disallowed; clear when alarm removed and all racks have charging current"
12,"Rack total voltage low",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart and ESMU fault reset to clear"
```

#### 3.4. Alarms 13–19 (BMM / box temps, currents, insulation)

```csv
13,"BMM pole temperature over-temperature",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Report alarm, charge/discharge current reduction 50%"
13,"BMM pole temperature over-temperature",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging; prohibit discharging; Fault remove clears"
13,"BMM pole temperature over-temperature",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart to clear and ESMU fault reset"

14,"Charging overcurrent",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Report alarm; reduce current 50%"
14,"Charging overcurrent",2,1,3,"forbid_charging;allow_discharging","Prohibit charging; allow discharge; clear on alarm remove + (delay 30min or all racks have discharge current)"
14,"Charging overcurrent",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart + ESMU reset"

15,"Discharging overcurrent",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Report alarm; reduce current 50%"
15,"Discharging overcurrent",2,1,3,"allow_charging;forbid_discharging","Allow charging; prohibit discharging; clear on alarm remove + (delay 30min or all racks have charging current)"
15,"Discharging overcurrent",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart + ESMU reset"

16,"Low insulation",1,1,10,"report_alarm;reduce_charge_discharge_current_50","10s/60s; report alarm, reduce current 50%"
16,"Low insulation",2,1,10,"forbid_charging;forbid_discharging","Discharging disallowed, charging disallowed; Fault remove clears"
16,"Low insulation",3,1,10,"trip_via_dry_contact","Output dry contact, delayed 3s trip; 10s after threshold+return diff; ESMU reset required"

17,"High total voltage of battery box",1,1,3,"report_alarm","Report alarm"
17,"High total voltage of battery box",2,1,3,"forbid_charging;allow_discharging","Prohibit charging; allow discharge; clear when alarm removed and all racks have discharge current"
17,"High total voltage of battery box",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart + ESMU reset"

18,"Low total voltage of battery box",1,1,3,"report_alarm","Report alarm"
18,"Low total voltage of battery box",2,1,3,"allow_charging;forbid_discharging","Charging allowed; discharging disallowed; clear when alarm removed and all racks have charging current"
18,"Low total voltage of battery box",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart + ESMU reset"

19,"High voltage box pole temperature over-temperature",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Report alarm; reduce current 50%"
19,"High voltage box pole temperature over-temperature",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging; prohibit discharging; Fault remove clears"
19,"High voltage box pole temperature over-temperature",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; restart + ESMU reset"
```

#### 3.5. Master fault list (20–38)

These are all Level 3 (or as specified), duration mostly 3s, action = trip, plus text.

```csv
20,"Communication between master and slave faulty",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; clears when communication restored + ESMU reset"
21,"Single cell voltage acquisition fault",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; clears on data sampling recovery + ESMU reset"
22,"Individual temperature acquisition fault",3,1,3,"trip_via_dry_contact","Output dry contact, delayed 3s trip; clears on data sampling recovery + ESMU reset"
23,"Temperature rise alarm",3,1,3,"trip_via_dry_contact","Temperature rise rate >=10°C/min; trip; restart release; fault remove to clear"
24,"BCM circuit breaker malfunction",3,1,3,"trip_via_dry_contact","Trip; detect recovery; BCM restart to clear"
25,"BCM contactor malfunction",3,1,3,"trip_via_dry_contact","Trip; detect recovery; BCM restart to clear"
26,"Slave peripheral failure (DI detection)",3,1,3,"trip_via_dry_contact","Trip; fault remove + ESMU reset to clear"
27,"Hall sensor communication failure",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging and discharging; clears when communication restored; fault remove"
28,"Hall sensor malfunction",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging and discharging; clears when fault removed"
29,"Circulation overflow",3,1,5,"trip_via_dry_contact","Circulation current >5A; 5s; trip; clears when fault condition no longer met + ESMU reset"
30,"Thermal runaway",3,1,3,"trip_via_dry_contact","Thermal runaway; trip; restart release; ESMU reset to clear"
31,"Main positive contactor feedback abnormal",3,1,3,"trip_via_dry_contact","Trip; restart release; ESMU reset to clear"
32,"Main negative contactor feedback abnormal",3,1,3,"trip_via_dry_contact","Trip; restart release; ESMU reset to clear"
33,"Precharge failed",3,1,3,"trip_via_dry_contact","Trip; restart release; ESMU reset to clear"
34,"BCM T0~T4 copper bar over-temperature",1,1,3,"report_alarm;reduce_charge_discharge_current_50","Level1: report alarm; reduce current 50%"
34,"BCM T0~T4 copper bar over-temperature",2,1,3,"allow_charging;forbid_discharging","Level2: allow charging; no discharge; clears on ESMU reset"
34,"BCM T0~T4 copper bar over-temperature",3,1,3,"trip_via_dry_contact","Level3: trip; clears on ESMU reset"
35,"Cell–pole temperature deviation too large",3,1,30,"trip_via_dry_contact","Trip; 30s; ESMU reset to clear"
36,"Cell accumulation vs total pressure deviation >30V",3,1,30,"trip_via_dry_contact","Trip; 30s; restart release + ESMU reset to clear"
37,"Ultimate overvoltage",3,1,3,"trip_via_dry_contact","Vmax>=3.8V; lock fault; ESMU reset to clear"
38,"Limit undervoltage",3,1,3,"trip_via_dry_contact","Vmin<=2V or 1.6V depending on Tmin; lock fault; ESMU reset to clear"
```

#### 3.6. Display & control fault list (39–53)

```csv
39,"Display–master communication faulty",2,1,3,"forbid_charging;forbid_discharging","Prohibit charging and discharging; clears when communication restored; fault remove"
40,"Faults with current differences between clusters",1,1,30,"report_alarm;reduce_charge_discharge_current_50","Level1: only alarm; reduce current 50%; clears when fault condition not met"
40,"Faults with current differences between clusters",2,1,30,"forbid_charging;forbid_discharging","Level2: prohibit charging & discharging; clears when fault condition not met (and delay 30min)"
40,"Faults with current differences between clusters",3,1,30,"trip_via_dry_contact","Level3: trip; clears on ESMU reset"

41,"Inter-cluster current imbalance (slight)",1,1,60,"alarm_only","Slight imbalance; 60s; clears when fault condition not met"

42,"Cluster loop abnormality",3,1,30,"trip_via_dry_contact","Rack circuit abnormal; trip; clears on fault remove + ESMU reset"

43,"Combiner cabinet breaker fails to close",3,1,3,"trip_via_dry_contact","Closing fails 3 times; trip; clears on fault remove + ESMU reset"

44,"PCS control failure",3,1,3,"trip_via_dry_contact","PCS control failure; trip; clears on fault remove + ESMU reset"

45,"UPS communication abnormal",1,1,3,"alarm_only;reduce_charge_discharge_current_50","Only alarm; reduce current 50%; clears on communication restoration"

46,"PCS communication abnormal",3,1,3,"trip_via_dry_contact","Trip; clears on communication restoration + ESMU reset"

47,"Inter-cluster current imbalance (A)",1,1,10,"alarm_only;reduce_charge_discharge_current_50","Level1: 60A; 10s; only alarm; reduce current 50%"
47,"Inter-cluster current imbalance (A)",2,1,10,"forbid_charging;forbid_discharging","Level2: 80A; 10s; no charging, no discharging; restart release + ESMU reset to clear"
47,"Inter-cluster current imbalance (A)",3,1,10,"trip_via_dry_contact","Level3: 100A; 10s; trip; restart release + ESMU reset"

48,"QF1 circuit breaker feedback abnormal",2,1,3,"forbid_charging;forbid_discharging","No charging, no discharging; clears when signal disappears for 3s + ESMU reset"

49,"UPS failure",1,1,3,"alarm_only;reduce_charge_discharge_current_50","Only alarm; reduce current 50%; clears when signal disappears for 3s + ESMU reset"

50,"UPS mains power faulty",1,1,60,"alarm_only;reduce_charge_discharge_current_50","Level1: only alarm; reduce current 50%; clears when signal disappears 30s + ESMU reset"
50,"UPS mains power faulty",3,1,7200,"trip_via_dry_contact","Level3: 2 hours; trip; clears when signal disappears 30s + ESMU reset"

51,"TMS communication abnormal",1,1,3,"alarm_only;reduce_charge_discharge_current_50","Only alarm; reduce current 50%; clears on communication restoration + ESMU reset"

52,"TMS failure",1,1,3,"alarm_only;reduce_charge_discharge_current_50","TMS Level2 fault; alarm only; reduce current 50%; clears when fault disappears + ESMU reset"
52,"TMS failure",2,1,3,"alarm_only;reduce_charge_discharge_current_50","TMS Level1 fault; alarm only; reduce current 50%; clears when fault disappears + ESMU reset"

53,"Cell accumulation vs total pressure deviation",3,1,30,"trip_via_dry_contact","Deviation >30V; 30s; trip; restart release + ESMU reset"
```

---

### 4. How to use this

1. Create/overwrite your three CSV files with the headers above.
2. Paste the corresponding sections under each header.
3. Point your Flask app’s import to the directory and run your existing `import-csv` flow.
4. Open your UI and verify:
   - 53 alarms listed.
   - 3 levels where we’ve defined 3-level actions.
   - Limits entries appear for the numeric-threshold alarms.

If you want, we can next:

- Trim or normalize `actions` to a strict small set (`REPORT`, `FORBID_CHARGE`, etc.) so your C++ logic can just `switch` on them.
- Generate a JSON export from these DB rows, matching your C++ structure exactly.
