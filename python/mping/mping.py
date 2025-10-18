# ping them all
import concurrent.futures
from ping3 import ping, verbose_ping
import time

def check_host(ip_address):
    """Pings a single IP address and returns its status."""
    # `ping` returns a float of the RTT in seconds if successful, or False if failed.
    response_time = ping(ip_address, timeout=1)
    if response_time is not False:
        return f"Host {ip_address} is UP (Response time: {response_time:.2f}s)"
    else:
        # Host could be down or not responding to pings
        return None

def main():
    """Pings all 254 usable IPs on the 192.168.86.xxx subnet in parallel."""
    subnet = "192.168.86."
    ip_addresses = [f"{subnet}{i}" for i in range(1, 255)]
    
    start_time = time.time()
    
    # Use a ThreadPoolExecutor for parallel execution
    # The number of workers can be tuned based on your system's resources
    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
        # Map the check_host function to each IP address
        results = executor.map(check_host, ip_addresses)
        
        # Process the results as they come in
        for result in results:
            if result:
                print(result)

    end_time = time.time()
    print(f"\nScan completed in {end_time - start_time:.2f} seconds.")

if __name__ == "__main__":
    print("Starting network ping sweep...")
    main()
