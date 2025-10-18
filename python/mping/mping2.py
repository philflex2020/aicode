import concurrent.futures
from ping3 import ping
import time

def check_host(ip_address):
    """Pings a single IP address and returns its status, or None if down."""
    # ping() returns a float (seconds) on success, None on timeout
    response_time = ping(ip_address, timeout=1)
    
    if response_time is not None:
        # If the ping was successful, return the formatted status string
        return f"Host {ip_address} is UP (Response time: {response_time:.2f}s)"
    else:
        # If the ping timed out, return None
        return None

def main():
    """Pings all 254 usable IPs on the 192.168.86.xxx subnet in parallel."""
    subnet = "192.168.86."
    ip_addresses = [f"{subnet}{i}" for i in range(1, 255)]
    
    start_time = time.time()
    
    # Use a ThreadPoolExecutor for parallel execution
    # The number of workers can be adjusted for performance
    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
        # Submit the check_host function for each IP address
        future_to_ip = {executor.submit(check_host, ip): ip for ip in ip_addresses}
        
        # As each task completes, get the result and process it
        for future in concurrent.futures.as_completed(future_to_ip):
            result = future.result()
            if result:
                print(result)
    
    end_time = time.time()
    print(f"\nScan completed in {end_time - start_time:.2f} seconds.")

if __name__ == "__main__":
    print("Starting network ping sweep...")
    main()
