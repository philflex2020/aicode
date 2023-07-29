GCOM Processing Modules.

p wilshire
07_19_2023


### Overview

The GCOM system provides a few enhancements to a basic communications interface.
These enhancement have been needed  in one way or another to the communicatoins systems currently deployed.

( Note the GCOM System will be 100% compatible with current modbus and DNP23 configuration files.)

The Project can be divided up into a number of fundamental units and  the extra processing units.

### Fundamental Units

* Config Processor
* Fims Data Processor
* DNP3 Client and Server Modules. (TMW)
* Modbus Cleint and Server Modules. (latest Libmodbus)
* Cops Interface/Build data

### Additional Units

* Performance Monitoring
* No Spam Logging
* Full Heartbeat Management Subsystem
* Status Reporting
* Alarm and Fault  Reporting
* Fims Data Filter
* Timer Unit for triggering polls / scans etc.
* Full DNP3 Client all required data types.
* Full DNP3 Server including unsolicited exception data , deadband an Online / off line status reporting.


### More Detail

These components are described in more detail.

## No Config default values

There will be a lot of configuration options with the new GCOM system. Whereever possible the configuration complexity will be drastically reduced by selected reasonable defaultvalues.
THe Status reporting system will, if requested, provide the full list of configuration options with selected default or configured values.

## Performance Monitoring.

A flexible system that allows any performance metric to be monitored and reported. 
( Similar to the ess_controller Perf Module).
For Example Transmission Max / Min and Avg response times are recorded and the system will provide  measuresment status  and provide events on exceptions.
Other statistics like data rates / message sizes can easily be added to the system. 
Additional components will require  measurement options to be set up at compile time and enabled through configuration options.


## No Spam Logging.

The current logging system is very crude. Sufficient for a Min Viable Product (MVP) but not well suited to the enterprise grade standards now required.
The ease of use in the field  by commissioning and support tesms more than compensates for the little extra thought required at the design phase.
The same error messages tend to fill up log files , sometimes covering up more important messages.
The Logging system will issue a message the first time it sees a new error , if the error repeats the message will be repeated once again (10th instance) and then only ever again on a timed basis ( say 5 - 10 seconds ).
An internal,limited size,  buffer will be kept of the unreported messages. This buffer will be available using the status reporting system.
THe message buffering and reportimg timings can be reset on a per message type basis.

## Full Heartbeat management system.

The system can send heartbeat data to the remote Client or Server. This can be a reflected heartbeat wher in incoming value is incremented and passed on or it can be an independent heartbeat where an isolated data item is incremented, on a periodic basis between configurable limits. 
A good set of "No Config" Default values will be used to reduce configuration complexity.

The system will monitor incoming heartbeat signals using a full spec** hartbeat management system. This will, if set up via configuration, provide alarm and fault and recovery monitoring  with event generation to report errors.
This is sort of system is already available in the ess_controler.
Note that any fims data point can be used as a signal to monitor so the GCOM could provide this functioaity for other components in the system.

**full spec means that we have configurable  warning and fault times for loss of signal as well as a recovery time before reporting  signal restoration.

## Status reporting.

A wide range of system monitoring components will be available via the fims interface. This will be organised as overview and detail messages and will be suitable for interfacing to an UI if needed. A Simple API for status data will be produced.
Note that when you use a Commercial Peipheral component the status monitoring options are often overwhelming but useful.
The Current status monitoring options from FlexGen communications products are meager and , well, quite useless at the moment.

## Alarm and Fault Reporting.

Error conditions , loss of comms , heartbeat failure cna all be configured to produce Alarm and Fault messages.
The Message format will match those currently used by the ess and site controllers to make it simpler to intergrate the GCOM alarm and Fault events wir current or future UI system.

### Fims Data Filter
 
The Fims Filter system allowd the data transfer between the fims input or output and the modbus / DNP3 Data Transfer System (DTS) (Client / Server)  units to be controlled if needed.
In most systems the filter will not be used allowing data to be diredtly connected betwqeen  fims processor and the  DTS. In some cases incoming data rates may be too great for DTS or other external equipment.

Data Paths are:
1/ From fims set commands to the Client DTS and from the Server DTS to a fims set output.
2/ From fims pub commands to the Server DTS and from the Cliant DTS to a fims pub output.


Filtering can be of the following forms
1/ DIRECT ( by passws all filtering)
2/ BATCHED  ( if input data is present the filter only sends the last data received at the batch frequency)
    So if a BATCH frequency is set at 100mS and data arrives every 10mS the filter will output the latest data received every 100mS. 
    The intermediate samples will be discardrd. ( As as streach option we could output the Max Avg or Min value over the batch time period) 

## Timer System.

Thie system will have a timer system that will be used internally but can be used to send hearbeats (or system time) or other simple data values to other systems via  fims messages.



## Full DNP3 Client all required data types.

All requested Data Basic Types will be available.
( Probably not some of the file and terminal DNP3 options) 
This will include DNP3 integrity class and event scans
The DNP3 Client will be able to receive unsolicited data ,  producing  fims output from DNP3 Servers.

## Full DNP3 Server including unsolicited exception data , deadband an Online / off line status reporting.
All requested deadband , Flag data  and other DNP3 options, as provided by the TMW DNP3 package will be made available.


## DNP3 TLS Security 

Will be available as provided by the TMW package.

## DNP3 User Auth 

If required, Will be available as provided by the TMW package.


