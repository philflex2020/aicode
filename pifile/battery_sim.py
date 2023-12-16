import numpy as np
from scipy.integrate import odeint

class BatteryECM:
    def __init__(self, ocv, rs, rc_pairs, capacity_mAh):
        # ocv: Open Circuit Voltage
        # rs: Series resistance
        # rc_pairs: List of tuples (resistance, capacitance) for each RC pair
        self.ocv = ocv
        self.rs = rs
        self.rc_pairs = rc_pairs
        self.capacity_mAh = capacity_mAh
        self.soc = 100  # State of Charge in percentage
        self.voltage = ocv
        self.current = 0  # Current in Amperes
        self.rc_pair_voltages = [0 for _ in rc_pairs]  # Initialize voltages for RC pairs

    def set_current(self, current):
        self.current = current
    def calculate_voltage(self, elapsed_time_seconds):
        # Convert elapsed time to hours for capacity calculation
        elapsed_time_hours = elapsed_time_seconds / 3600

        # Update SOC: Ensure current is in the same unit as capacity (Ampere-hours)
        delta_soc = (self.current * elapsed_time_hours) / (self.capacity_mAh / 1000) * 100
        self.soc = max(0, min(100, self.soc - delta_soc))

        # Update RC pair voltages
        self.update_rc_pair_voltages(elapsed_time_seconds)

        # Calculate total voltage
        rc_voltage = sum(self.rc_pair_voltages)
        self.voltage = self.ocv - (self.rs * self.current) + rc_voltage

        return self.voltage

    def xcalculate_voltage(self, elapsed_time):
        # Update SOC based on current and elapsed time
        delta_soc = (self.current * elapsed_time / 3600) / self.capacity_mAh * 100
        self.soc = max(0, min(100, self.soc - delta_soc))
        self.update_rc_pair_voltages(elapsed_time)
        rc_voltage = sum(self.rc_pair_voltages)
        self.voltage = self.ocv - (self.rs * self.current) + rc_voltage
        return self.voltage


    def simulate(self, current_profile, time):
        # current_profile: array of current values
        # time: array of time points
        initial_conditions = [0 for _ in self.rc_pairs]  # Initial voltage across each capacitor
        solution = odeint(self.model, initial_conditions, time, args=(current_profile,))
        return solution

    def model(self, y, t, current_profile):
        i = np.interp(t, time, current_profile)
        dydt = []
        for j, (r, c) in enumerate(self.rc_pairs):
            v_c = y[j]
            v_rc = i * r
            dv_c_dt = (v_rc - v_c) / (r * c)
            dydt.append(dv_c_dt)
        return dydt

    def get_battery_voltage(self, rc_voltages, current):
        voltage_drop_rs = self.rs * current
        voltage_rc_pairs = sum(v for v in rc_voltages)
        total_battery_voltage = self.ocv - voltage_drop_rs + voltage_rc_pairs
        return total_battery_voltage

    def update_rc_pair_voltages(self, time_delta):
        for i, (r, c) in enumerate(self.rc_pairs):
            # Simplified approach to update the voltage across each RC pair
            tau = r * c
            delta_v = self.current * r - self.rc_pair_voltages[i]
            self.rc_pair_voltages[i] += delta_v * (1 - np.exp(-time_delta / tau))

# Example usage
#ocv = 4.2  # Open-circuit voltage
#rs = 0.02  # Series resistance

# Example Usage
battery = BatteryECM(ocv=4.2, rs=0.02, rc_pairs=[(0.05, 1000), (0.1, 2000)], capacity_mAh=2000)

# Set the current
battery.set_current(3.5)  # 0.5 Amperes

# Get the voltage after a certain time
elapsed_time_seconds = 3600  # 1 hour
voltage = battery.calculate_voltage(elapsed_time_seconds)
print(f"Battery Voltage after {elapsed_time_seconds} seconds: {voltage} V")



#rc_pairs = [(0.05, 1000), (0.1, 2000)]  # Example RC pairs



#battery = BatteryECM(ocv, rs, rc_pairs)
#time = np.linspace(0, 3600, 3601)  # Simulate for 1 hour
#current_profile = np.full_like(time, -1.5)  # Discharging at 0.5A

#voltage_response = battery.simulate(current_profile, time)

#Example Usage
# Assuming you have a function to get the current RC pair voltages and current
#rc_pair_voltages = [voltage_response[-1][0], voltage_response[-1][1]]  # Last time point voltages
#current = current_profile[-1]  # Current at the last time point

#battery_voltage = battery.get_battery_voltage(rc_pair_voltages, current)
#print(f"Battery Voltage: {battery_voltage} V, Current: {current} A")
# print ("done")
# for v in voltage_response:
#     print(v)

