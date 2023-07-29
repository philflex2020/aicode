
# GCOM System Spec

Author: P. Wilshire
Date: 2023-07-19
Version: More logging details

Revision History:
* P. Wilshire - 07_19_2023 - Bad spelling and initial / revised draft.

### Overview

The GCOM system provides enhancements to a basic communications interface, improving the current communications systems deployed. It will be fully compatible with current Modbus and DNP3 configuration files.

### Fundamental Components

1. Config Processor
2. Fims Data Processor
3. DNP3 Client and Server Modules (TMW)
4. Modbus Client and Server Modules (latest Libmodbus)
5. Cops Interface/Build data

### More Detail on Fundamental Components

#### Config Processor

The Config Processor handles the configuration options for the GCOM system. It will reduce configuration complexity by selecting reasonable default values wherever possible. The Status Reporting system will provide a full list of configuration options with selected default or configured values upon request.

#### Fims Data Processor

The Fims Data Processor allows data transfer between the Fims input or output and the Modbus/DNP3 Data Transfer System (DTS) (Client/Server) units to be controlled. It can be used to manage data rates, filter data, or perform data value assertions.

#### DNP3 Client and Server Modules (TMW)

The DNP3 Client and Server Modules will provide full DNP3 support, including all required data types, integrity class, and event scans. The DNP3 Client can receive unsolicited data and produce Fims output from DNP3 Servers.

#### Modbus Client and Server Modules (latest Libmodbus)

The Modbus Client and Server Modules will offer complete Modbus support. The Modbus Server will be enhanced to support single/multi naked or clothed data, and the issue with single data input  will be resolved.

#### Cops Interface/Build Data

The Cops Interface/Build Data will be responsible for interacting with the Cops system and handling the system build information used in the data build process. This component will facilitate the data build process by managing system-specific information required for data processing and communication.
The Cops Interface/Build Data will be responsible for interacting with the Cops system and handling the data build process.

### Additional Components

1. Performance Monitoring
2. No Spam Logging
3. Full Heartbeat Management Subsystem
4. Status Reporting
5. Alarm and Fault Reporting
6. Fims Data Filter
7. Timer Unit for triggering polls/scans, etc.
8. Full DNP3 Client with all required data types
9. Full DNP3 Server, including unsolicited exception data, deadband, and online/offline status reporting

### More Detail on Additional Components

#### Performance Monitoring

The Performance Monitoring system will provide flexible monitoring of performance metrics such as transmission max/min/avg response times, data rates, and message sizes. Events and measurement status will be generated on exceptions.

#### No Spam Logging

The logging system will retain a number of the latest error messages, and these messages will be available using the system status reporting. The system will avoid spamming repetitive error messages and will only report them at specified intervals. This improvement ensures that log files remain concise and relevant, providing more meaningful information to support teams during commissioning and maintenance.

#### Full Heartbeat Management Subsystem

The system will be able to send heartbeat data to remote Clients or Servers. It will also monitor incoming heartbeat signals using a full spec heartbeat management system, providing alarm, fault, and recovery monitoring with event generation.

#### Status Reporting

A wide range of system monitoring components will be available via the Fims interface, organized as overview and/or detail messages suitable for interfacing with a UI. A Simple API for status data will be provided.

#### Alarm and Fault Reporting

Error conditions, loss of comms, heartbeat failure, etc., can be configured to produce alarm and fault messages. The message format will match those currently used by the ess and site controllers.

#### Fims Data Filter

The Fims Filter system allows control of data transfer between the Fims processor and the DTS units. Filtering options include direct and batched filtering.

#### Timer Unit

The Timer Unit will be used internally and can send heartbeat or system time messages to other systems via Fims messages.

#### Full DNP3 Client

The Full DNP3 Client will support all requested Data Basic Types and DNP3 integrity class and event scans.

#### Full DNP3 Server

The Full DNP3 Server will support all requested deadband, flag data, and other DNP3 options, including unsolicited exception data, deadband, and online/offline status reporting.

#### DNP3 TLS Security

DNP3 TLS Security will be available as provided by the TMW package.

#### DNP3 User Auth

If required, DNP3 User Authentication will be available as provided by the TMW package.

#### Fims Pubs/Sets

The Fims Pubs/Sets feature can be used for naked or clothed data points, and DNP3 Flag data can also be published. Sparse data will be supported.

### Note

The GCOM system aims to improve communication systems' performance, monitoring capabilities, and reporting. It will be 100% compatible with current Modbus and DNP3 configuration files.

Please review this revised outline spec and let me know if you'd like any further changes or additions.