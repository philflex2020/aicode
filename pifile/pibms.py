import socket
import threading
import json
import time
import heapq

# Timer Request Class
class TimerRequest:
    def __init__(self, start_time, callback, interval=None, extra_param=None):
        self.start_time = start_time
        self.callback = callback
        self.interval = interval
        self.extra_param = extra_param

    def __lt__(self, other):
        return self.start_time < other.start_time

# Timer Functionality
class Timer:
    def __init__(self):
        self.timer_queue = []
        self.condition = threading.Condition()
        self.start_time = time.time()
        threading.Thread(target=self.timer_thread_function, daemon=True).start()

    def timer_thread_function(self):
        while True:
            current_time = time.time() - self.start_time
            with self.condition:
                while self.timer_queue and self.timer_queue[0].start_time <= current_time:
                    timer_request = heapq.heappop(self.timer_queue)
                    threading.Thread(target=timer_request.callback, 
                                     args=(timer_request.extra_param,)).start()
                    if timer_request.interval:
                        next_time = timer_request.start_time + timer_request.interval
                        heapq.heappush(self.timer_queue, TimerRequest(next_time, timer_request.callback, 
                                                                     timer_request.interval, timer_request.extra_param))
                self.condition.wait(timeout=self.get_next_timeout())

    def get_next_timeout(self):
        if self.timer_queue:
            next_timer = self.timer_queue[0]
            return max(0, next_timer.start_time - (time.time() - self.start_time))
        return None

    def add_timer_request(self, timer_request):
        with self.condition:
            heapq.heappush(self.timer_queue, timer_request)
            self.condition.notify()


class Battery:
    def __init__(self, capacity):
        self.capacity = capacity
        self.charge_level = capacity  # Assuming the battery is initially fully charged
        self.state = "Idle"
        self.charge_rate = 1  # Units of charge per interval

    def increment_charge(self):
        if self.state == "Charging":
            self.charge_level = min(self.charge_level + self.charge_rate, self.capacity)

    def decrement_charge(self):
        if self.state == "Discharging":
            self.charge_level = max(self.charge_level - self.charge_rate, 0)


    def update_charge(self, amount):
        self.charge_level += amount
        self.charge_level = max(0, min(self.charge_level, self.capacity))

    def set_state(self, state):
        self.state = state

    def get_status(self):
        return {"charge_level": self.charge_level, "state": self.state}

class BatteryManagementSystem:
    def __init__(self, battery):
        self.battery = battery
        self.soc_update_thread = threading.Thread(target=self.update_soc, daemon=True)
        self.soc_update_interval = 5  # Time interval in seconds for SoC update
        self.soc_update_thread.start()

    def update_soc(self):
        while True:
            time.sleep(self.soc_update_interval)
            self.battery.increment_charge()
            self.battery.decrement_charge()

    def charge(self, amount):
        self.battery.update_charge(amount)
        self.battery.set_state("Charging")

    def discharge(self, amount):
        self.battery.update_charge(-amount)
        self.battery.set_state("Discharging")

    def idle(self):
        self.battery.set_state("Idle")

    def get_status(self):
        return self.battery.get_status()

class BmsServer:
    def __init__(self, host, port, bms):
        self.host = host
        self.port = port
        self.bms = bms
        self.timer = Timer()
        self.monitoring_clients = set()
        self.client_connections = []
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    def start(self):
        self.socket.bind((self.host, self.port))
        self.socket.listen(5)
        print(f"Server listening on {self.host}:{self.port}")
        self.timer.add_timer_request(TimerRequest(1, self.send_status_to_monitoring_clients, interval=5))  # Send status every 5 seconds

        #self.timer.add_timer_request(TimerRequest(1, self.send_status_to_clients, interval=5)) 
        #threading.Thread(target=self.handle_client, args=(client,)).start()

        # Send status every 5 seconds

        while True:
            client, addr = self.socket.accept()
            print(f"Connected to {addr}")
            self.client_connections.append(client)
            threading.Thread(target=self.handle_client, args=(client,)).start()

    def handle_client(self, client):
        while True:
            try:
                data = client.recv(1024).decode()
                if not data:
                    break
                command = json.loads(data)
                self.process_command(command, client)
            except Exception as e:
                print(f"Error: {e}")
                #break
        client.close()
        #self.client_connections.remove(client)
        self.monitoring_clients.discard(client)

    def send_status_to_monitoring_clients(self, _):
        status = json.dumps(self.bms.get_status())
        for client in list(self.monitoring_clients):
            try:
                client.send(status.encode())
                client.send("\n".encode())
            except Exception as e:
                print(f"Error sending status: {e}")
                self.monitoring_clients.discard(client)

    def send_status_to_clients(self, _):
        status = json.dumps(self.bms.get_status())
        for client in self.client_connections:
            try:
                client.send(status.encode())
                client.send("\n".encode())
            except Exception as e:
                print(f"Error sending status: {e}")

    def process_command(self, command, client):
        if command['action'] == 'charge':
            self.bms.charge(command.get('amount', 10))
        elif command['action'] == 'discharge':
            self.bms.discharge(command.get('amount', 10))
        elif command['action'] == 'status':
            status = self.bms.get_status()
            client.send(json.dumps(status).encode())
        elif command['action'] == 'idle':
            self.bms.idle()
        elif command.get("action") == "monitor":
            self.monitoring_clients.add(client)
        else:
            client.send(b'Invalid command')
            
def main():
    battery = Battery(100)  # 100 is the max capacity
    bms = BatteryManagementSystem(battery)
    server = BmsServer('localhost', 12345, bms)
    server.start()

if __name__ == "__main__":
    main()
