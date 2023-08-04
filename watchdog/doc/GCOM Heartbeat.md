# Heartbeat Detector Specification

## Overview

The Heartbeat Detector is a component of the GCOM project responsible for monitoring the heartbeat of a variable and generating heartbeat signals when required. It detects changes in the variable's value and transitions between different states: Normal, Warning, Fault, and Recovery. Additionally, it can act as a Heartbeat Generator, incrementing a value and sending it out periodically.

## Configuration

The Heartbeat Detector configuration is specified as part of the GCOM configuration file. The relevant configuration parameters for the heartbeat component are as follows:

```json
{
  "system": {
  < stuff missing> 
  "component_heartbeat_read_uri": "/test/analog/analog_1",
  "component_heartbeat_write_uri": "/testcli/TestHeartbeat/TestHeartbeat",
  "component_heartbeat_write_reference": "/test/analog/analog_2",
  "component_heartbeat_write_freq": 1000,
  "component_heartbeat_min": 0,
  "component_heartbeat_max": 500,
  "component_heartbeat_incr": 1,
  "component_heartbeat_warning_timeout": 1000,
  "component_heartbeat_fault_timeout": 2000,
  "component_heartbeat_recovery_timeout": 500
  }
}
```

- `component_heartbeat_read_uri`: The URI to read the variable's value to be monitored.
- `component_heartbeat_write_uri`: The URI to write the heartbeat value when acting as a Heartbeat Generator.
- `component_heartbeat_write_reference`: The URI to read the reference value for the Heartbeat Generator (optional).
- `component_heartbeat_write_freq`: The frequency in milliseconds to generate heartbeat signals when acting as a Heartbeat Generator.
- `component_heartbeat_min`: The minimum value of the heartbeat variable when acting as a Heartbeat Generator.
- `component_heartbeat_max`: The maximum value of the heartbeat variable when acting as a Heartbeat Generator.
- `component_heartbeat_incr`: The increment value for the heartbeat variable when acting as a Heartbeat Generator.
- `component_heartbeat_warning_timeout`: The duration in milliseconds for no value change to trigger a Warning state.
- `component_heartbeat_fault_timeout`: The duration in milliseconds for no value change in Warning state to trigger a Fault state.
- `component_heartbeat_recovery_timeout`: The duration in milliseconds for value change in Fault state to trigger a Recovery state.

## Heartbeat Detector States

The Heartbeat Detector has the following states:

### 1. Normal State

- Description: The normal state indicates that the variable's value is changing, and no fault or warning condition is detected.
- Behavior: The Heartbeat Detector will continuously monitor the variable's value and transition to the Warning state if no value change is detected within the warning timeout.

### 2. Warning State

- Description: The warning state indicates that no value change is detected for a period exceeding the warning timeout.
- Behavior: The Heartbeat Detector will wait for the fault timeout to transition to the Fault state if no value change occurs. If a value change occurs, it will immediately transition to the Recovery state.

### 3. Fault State

- Description: The fault state indicates that no value change is detected for a period exceeding the fault timeout.
- Behavior: The Heartbeat Detector will wait for a value change to transition to the Recovery state.

### 4. Recovery State

- Description: The recovery state indicates that a value change occurred after being in the Fault or Warning state.
- Behavior: The Heartbeat Detector will wait for the recovery timeout to transition back to the Normal state.

## Heartbeat Generator

The Heartbeat Detector can also act as a Heartbeat Generator when the `component_heartbeat_write_uri` is specified. In this mode, it will increment a value within the specified range and send it out periodically.

- If `component_heartbeat_write_reference` is specified, the Heartbeat Generator will read the reference value from the specified URI and use it as the starting value.
- If `component_heartbeat_write_reference` is not specified, the Heartbeat Generator will use the minimum value as the starting value.

The Heartbeat Generator will increment the value by the specified increment and wrap around to the minimum value once it reaches the maximum value.

The generated heartbeat signals will be written to the `component_heartbeat_write_uri` at the specified frequency (`component_heartbeat_write_freq`).

## Conclusion

The Heartbeat Detector is a critical component of the GCOM project, responsible for monitoring the heartbeat of a variable and generating heartbeat signals when required. It operates in four states: Normal, Warning, Fault, and Recovery, allowing the system to respond appropriately to changing conditions. Additionally, it can serve as a Heartbeat Generator, ensuring a continuous stream of heartbeat signals when needed.

By following this specification, the Heartbeat Detector will ensure the reliability and stability of the GCOM system, making it a robust and dependable solution for various applications.