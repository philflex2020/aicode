
System Data Architecture Overview

1. System Data Collection
Operational data is systematically collected from various hardware interfaces and sensors throughout the Battery Management System (BMS):
AFE (Analog Front End): Captures cell-level voltage and temperature data directly.
Peripherals: Gathers data from external components (e.g., cooling systems, external meters) via communication protocols like Modbus (RTU or TCP), CAN, or RS485.
Performance Metrics: Internal data related to system operational status and health is monitored.

2. SBMU (System Battery Management Unit)
The SBMU serves as a primary aggregation point for system-level metrics and controls:
Total Pack Volts, Current, Power
Accumulated Power (Charge/Discharge capacity)
Faults/Alarms status
Communications Metrics (e.g., uptime, error rates)
The SBMU communicates with peripheral devices via Modbus, CAN, and/or RS485.

3. RBMS (Rack Battery Management System) Processor
The RBMS acts as the rack-level coordinator and processing unit:
Module/Cell Data: Monitors individual Battery/Module Volts, Temperature, SOC (State of Charge), SOH (State of Health), and SOE (State of Energy).
Rack Metrics: Calculates Rack power, capacity, charge, and discharge limits.
Communication Statistics: Tracks SBMU communications and other peripheral statistics (RS485, Modbus, CAN).
Data Location: Data is synchronized and shared between distinct memory spaces: rack-level memory and the local RTOS M33 real-time processor memory space.

4. Data Transfer and Security
The SBMU maintains synchronized shared memory with the RBMS using a proprietary secure data transfer protocol leveraging NGINX as a security wrapper around standard TCP and WebSocket data paths. This establishes a secure, robust communication channel between key system components.

5. Data Categories and Usage
All data within the system falls into two primary categories: Operational and Configuration.
Configuration Data: Defines system parameters established during design and deployment (e.g., capacity limits, addressing schemes). Modification is typically controlled and requires specific access permissions.
Operational Data: Real-time metrics that can be monitored for metrics analysis (AVG, MAX, MIN values are important). This data is also used to manage system protections.

6. System Protections and Definitions
System protections are implemented at up to three levels: Warning, Alarm, and Fault.
Defining an operational data item as a "protection candidate" automatically assigns default protection limits (up to 3 levels) and corresponding protection actions for each level.
Example: If SOC goes low, system protection would stop discharge but allow charge operations, and vice versa.

7. Data Access and Addressing (Paths)
Variables are assigned human-readable names (e.g., cell_voltage, cell_temp). Data values can be accessed via several internal "paths":
Modbus TCP or RTU
Shared memory (e.g., sm16 address)
CAN interface
Data values can also be assigned to outbound data streams. In these cases, the 16-bit or 32-bit raw data is presented using specific format statements (e.g., scaling factors, offsets, data types) to convert raw values into engineering units.
Examples of Data Paths:
cell_voltage might be located at:
rtos|sm16|0xc800 + cell number * 2
rtos|input|8000
The same cell_voltage is accessible hierarchically; for example, Cell 0 may also be accessed as module_0:cell_0, and Cell 16 as module_1:cell_0.


Common Peripherals the BMS Communicates With 

The peripherals generally fall into two categories: 
power management and environment control. 
Peripheral Type 	Example Devices	Why the BMS Communicates with Them

Power Conversion	Inverters, AC-DC Chargers, DC-DC Converters	The BMS instructs these devices when to start/stop charging/discharging, and dictates the limits (e.g., maximum voltage/current) to operate safely.

Environmental Control	Cooling Fans, Heaters, HVAC Units	The BMS monitors internal battery temperatures and activates cooling or heating systems to keep cells within their optimal operating range.

Safety Equipment	Circuit Breakers, Contactors, Fire Suppression Systems	For protection, the BMS can instantly open contactors to isolate the battery pack in case of a fault (e.g., overcurrent, short circuit).

Monitoring/Display	HMI (Human-Machine Interface) Panels, Data Loggers	Provides operational data for local monitoring or long-term performance analysis.

External Sensors	Ambient temperature sensors, smoke detectors	Gathers input data not directly connected to the internal AFE


Data and Data Paths

Lets consider a uint16 address 0xC800
at this address there are 480 cell voltage measurements.

These measurements are also available via Modbus at an address  rtos:input:4000.

The value  may also be available at Can address 