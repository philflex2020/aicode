DNP3 (Distributed Network Protocol) is a set of communications protocols used between components in process automation systems. It is mainly used in utilities such as electric and water companies. It is primarily used for communications between a control center and remote terminal units (RTUs) or intelligent electronic devices (IEDs).

DNP3 defines a set of layered communication protocols that ensures reliable communication between the master station and outstations/RTUs/IEDs. The protocol is robust, efficient, and compatible with a wide range of equipment, making it a popular choice for SCADA (Supervisory Control and Data Acquisition) applications.

DNP3 supports multiple types of data, such as binary input, binary output, counter input, analog input, analog output, etc.

Input events in DNP3 are data points that are generated when there is a change in the status or value of an input. For example, a binary input event is generated when the status of a binary input (e.g., a switch) changes from OFF to ON or from ON to OFF.

There are several types of input events in DNP3:

1. Binary Input: Represents the status of a binary input, such as a switch or a sensor. The status can be either ON or OFF.
2. Analog Input: Represents the value of an analog input, such as a temperature sensor or a voltage measurement.
3. Counter Input: Represents the value of a counter, such as the number of revolutions of a meter or the number of times a switch has been activated.

Input events are sent from the outstation to the master station when there is a change in the status or value of an input. The master station can request the outstation to send the current status or value of an input, or it can configure the outstation to send input events automatically when there is a change.

The master station can also configure the outstation to send input events periodically, even if there is no change, to ensure that the master station has the most up-to-date information.

Input events are an essential part of the DNP3 protocol as they allow the master station to monitor and control the outstation in real-time.

In the DNP3 protocol, data is categorized into different groups and variations. Each group represents a specific type of data (e.g., binary input, analog input, counter, etc.), and each variation represents a specific format of that data. This categorization allows for efficient and flexible communication.

Here are some common groups and their variations:

1. **Binary Input (Group 1):** Represents the status of binary inputs.
   - Variation 0: Undefined
   - Variation 1: Single-bit binary input status
   - Variation 2: Binary input status with flags

2. **Binary Output (Group 10):** Represents the status of binary outputs.
   - Variation 0: Undefined
   - Variation 1: Single-bit binary output status
   - Variation 2: Binary output status with flags

3. **Counter Input (Group 20):** Represents the value of counters.
   - Variation 0: Undefined
   - Variation 1: 32-bit counter
   - Variation 2: 16-bit counter
   - Variation 5: 32-bit counter with flag
   - Variation 6: 16-bit counter with flag

4. **Analog Input (Group 30):** Represents the value of analog inputs.
   - Variation 0: Undefined
   - Variation 1: 32-bit analog input
   - Variation 2: 16-bit analog input
   - Variation 3: 32-bit analog input with flag
   - Variation 4: 16-bit analog input with flag

5. **Analog Output (Group 40):** Represents the status or value of analog outputs.
   - Variation 0: Undefined
   - Variation 1: 32-bit analog output status
   - Variation 2: 16-bit analog output status
   - Variation 3: 32-bit analog output set point
   - Variation 4: 16-bit analog output set point

There are many more groups and variations in the DNP3 protocol, and these are just a few examples. Each variation has a specific format and may include additional information such as flags, which provide more details about the status or value of the data.

The master station and outstation must agree on the groups and variations to use for communication. The master station can request a specific variation from the outstation, and the outstation will respond with the requested variation if it is supported. If the requested variation is not supported, the outstation may respond with a different variation or with an error.


DNP3 (Distributed Network Protocol version 3) has a wide range of data groups, each with different variations. These groups and variations are used to represent different types of data and control commands. Below are some common DNP3 groups and their variations:

**Group 1: Binary Input**
- Variation 0: Undefined
- Variation 1: Single-bit binary input
- Variation 2: Binary input with status

**Group 10: Binary Output**
- Variation 0: Undefined
- Variation 1: Single-bit binary output status
- Variation 2: Binary output status with flags

**Group 20: Counter Input**
- Variation 0: Undefined
- Variation 1: 32-bit counter
- Variation 2: 16-bit counter
- Variation 5: 32-bit counter with flags
- Variation 6: 16-bit counter with flags

**Group 30: Analog Input**
- Variation 0: Undefined
- Variation 1: 32-bit analog input
- Variation 2: 16-bit analog input
- Variation 3: 32-bit analog input with flags
- Variation 4: 16-bit analog input with flags

**Group 40: Analog Output**
- Variation 0: Undefined
- Variation 1: 32-bit analog output status
- Variation 2: 16-bit analog output status
- Variation 3: 32-bit analog output set point
- Variation 4: 16-bit analog output set point

**Group 50: Time and Date**
- Variation 1: Absolute time
- Variation 2: Relative time

**Group 60: Class Data**
- Variation 1: Class 0 data
- Variation 2: Class 1 data
- Variation 3: Class 2 data
- Variation 4: Class 3 data

**Group 70: Control Commands**
- Variation 1: CROB (Control Relay Output Block)

**Group 80: Internal Indications**
- Variation 1: IIN (Internal Indications)

This is just a subset of the groups and variations defined in the DNP3 protocol. There are many more groups and variations available to represent a wide range of data and control commands. The complete list of groups and variations can be found in the DNP3 specification document, which is available from the DNP Users Group.

Group 60 in the DNP3 protocol is used for class data. The DNP3 protocol defines four classes of data: Class 0, Class 1, Class 2, and Class 3. Each class represents a different type of data or event.

- **Class 0** data includes all static or current values of the outstation. This includes the current status of all binary inputs, analog inputs, counters, etc.

- **Class 1** data includes all events that have occurred since the last time Class 1 data was requested. This includes any changes in binary inputs, analog inputs, counters, etc.

- **Class 2** data is similar to Class 1 data but is of a lower priority. It includes any changes in binary inputs, analog inputs, counters, etc., that are not included in Class 1 data.

- **Class 3** data is similar to Class 1 and Class 2 data but is of the lowest priority. It includes any changes in binary inputs, analog inputs, counters, etc., that are not included in Class 1 or Class 2 data.

**Group 60 variations:**

- **Variation 1:** Requests all Class 0 data.
- **Variation 2:** Requests all Class 1 data.
- **Variation 3:** Requests all Class 2 data.
- **Variation 4:** Requests all Class 3 data.

For example, if the master station sends a request for Group 60, Variation 2, the outstation will respond with all Class 1 data, which includes all events that have occurred since the last time Class 1 data was requested.

The master station can also configure the outstation to send Class 1, Class 2, or Class 3 data automatically when there is a change, or periodically, even if there is no change. This allows the master station to receive updates from the outstation in real-time or at regular intervals.

The use of classes allows the master station to request only the data it needs, which can help reduce the amount of data that needs to be transmitted and processed. This can be especially important in systems with many outstations or with limited bandwidth.

Each input datapoint in the outstation can be assigned to one or more classes (Class 1, Class 2, Class 3). This assignment determines how and when the data is reported to the master station.

- **Class 0:** This includes static or current data. All datapoints are typically a member of Class 0 by default. The master will typically poll for Class 0 data to get the current status of all points.
  
- **Class 1, 2, 3:** These include event data, which are changes or updates to the datapoints. Datapoints can be assigned to Class 1, 2, or 3 based on the importance or priority of the data. For example, critical events that need immediate attention can be assigned to Class 1, while less critical events can be assigned to Class 2 or 3.

The outstation will store events in the appropriate class data buffers as they occur. The master station can then request the data for a specific class or configure the outstation to send the data automatically, either when there is a change or at regular intervals.

This classification helps optimize the communication between the master and the outstation by allowing the master to request only the data it needs. For example, the master might frequently request Class 1 data to get critical updates in real-time, but only request Class 2 and Class 3 data at longer intervals, or when there is a specific need for that data.
