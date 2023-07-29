# GCOM System Spec.

**Author:** P. Wilshire  
**Date:** 2023-07-20  
**Version:** 0.95

## Revision History:
- P. Wilshire - 07_19_2023 bad spelling and initial / revised draft.
- P. Wilshire - 07_20_2023 modify / improve format.

## Overview:

The GCOM system provides a few enhancements to a basic communications interface. These enhancements have been needed in one way or another for the communications systems currently deployed.

(Note: The GCOM System will be 100% compatible with current Modbus and DNP3 configuration files.)

## Fundamental Components:
- Config Processor
- Fims Data Processor
- DNP3 Client and Server Modules (TMW)
- Modbus Client and Server Modules (latest Libmodbus)
- Cops Interface/Build Data

### More Detail on Fundamental Components:

1. Config Processor:
   The Config Processor is responsible for handling configuration options for the GCOM system. It will use default values if specified for each argument, and any args not specified will be left as null. The Config Processor will drastically reduce configuration complexity by using selected reasonable default values. It will also provide status reporting with a full list of configuration options and their selected default or configured values.

2. Fims Data Processor:
   The Fims Data Processor will be responsible for processing data between the Fims input or output and the Modbus/DNP3 Data Transfer System (DTS) (Client/Server) units. 
   The Fims Data Processor will also support Fims Pubs/Sets with options for naked or clothed data points.

3. DNP3 Client and Server Modules (TMW):
   The DNP3 Client and Server Modules will handle all required data types, integrity class, and event scans. The DNP3 Client will be able to receive unsolicited data and produce Fims output from DNP3 Servers. The DNP3 Server will include unsolicited exception data, deadband, and online/offline status reporting. DNP3 TLS Security and User Auth will be available if required.

4. Modbus Client and Server Modules (latest Libmodbus):
   The Modbus Client and Server Modules will handle Modbus communication using the latest Libmodbus library.

5. Cops Interface/Build Data:
   The Cops Interface/Build Data will be responsible for interacting with the Cops system and handling the system build information used in the system build process.

## Additional Components:
- Performance Monitoring
- "No Spam Logging"
- Full Heartbeat Management Subsystem
- Status Reporting
- Alarm and Fault Reporting
- Timer Module
- Fims Data Filters
- DNP3 Flag Data and  Deadband
- DNP3 Online/Offline Status Reporting
- DNP3 TLS Security
- DNP3 User Auth
- Sparse Data

1. Performance Monitoring:
   The Performance Monitoring system will allow any performance metric to be monitored and reported. It will provide measurement status and events on exceptions, allowing easy addition of statistics like transmission max/min and average response times, data rates, and message sizes.

2. No Spam Logging:
   The Logging system will issue a message the first time it sees a new error, if the error repeats, the message will be repeated once again (10th instance), and then only ever again on a timed basis (5-10 seconds). An internal, limited-size buffer will be kept for unreported messages, available using the status reporting system.
   The GCOM logger is able to perform a logger dump to specified log file locations when errors are detected.
   This can be coordinated with traiggering external event like network data capture to help diagnose network problems.
   

3. Full Heartbeat Management Subsystem:
   The system will send heartbeat data to the remote Client or Server, either reflected or independent. 
   A reflected hearbeat will be derived from an incoming signal that will be incremented and used as a heartbeat output.
   An independent hearbeat will be a value that is incremented between limits regardless of any input data and used as a heartbeat output.
   It will monitor incoming heartbeat signals with configurable warning and fault times, providing alarm, fault, and recovery monitoring with event generation to report errors.

4. Status Reporting:
   The system will provide a wide range of system monitoring components via the Fims interface. 
   The system can, if needed, provide a special status pub message at a defined interval.   
   The  status response  can be organized as overview and/or detail messages suitable for interfacing with a UI if needed. 
   A simple API for status data will be provided.

5. Alarm and Fault Reporting:
   Error conditions, loss of comms, heartbeat failure, etc., can be configured to produce Alarm and Fault messages in a format compatible with the ess and site controller fault messages.

6. Timer Module:
   The system will have a timer module used internally, but can also be used to send heartbeats or other data values to other systems via Fims messages.
   
7. Fims Data Filtering
    This provides options for filtering data transfer between Fims and DTS, and filtering can be of the following forms:
    - DIRECT (bypasses all filtering)
    - BATCHED (sends the last data received at the batch frequency if new data points gave been received)
    - INTERVAL(sends the last data recieved regardless of data input)

8. DNP3 Deadband and Flag Data:
   The DNP3 Server will include options for deadband and flag data, as provided by the TMW DNP3 package.

9. DNP3 Online/Offline Status Reporting:
   The DNP3 Server will provide online/offline status reporting.
   The RESET, ON-LINE and COMMS-LOST status flags will reflect the variable conditions.

10. DNP3 TLS Security:
   DNP3 TLS security will be available as provided by the TMW package.

11. DNP3 User Auth:
    DNP3 User Authentication will be available if required.

12. DNP3 Redundant sessions.

12. Fims Pubs/Sets:
    Fims Pubs/Sets can be clothed or naked on a per data point basis. DNP3 Flag data can also be pub in flag variables ActivePower and ActivePowerFlags.


## Unit Interation and System Test

All GCOM components will be unit tested. The test output will be made available to the Build / Test systems.
Integration testing will involve checking the interaction between software modules.
The TMW DNP3 can function in a Peer mode where a Client is internally connected to a Server and data passed between the two.  The Modbus system will be adapted to use the same technique.
System Testing will involve running tests between Client and Server system running on different compute nodes.
A test report of a system test will be included in the build/release documentation.


