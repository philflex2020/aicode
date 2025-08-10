#!/usr/bin/env python3
import subprocess, time, csv, os
from datetime import datetime

# Thresholds
MAX_SSH_SESSIONS = 50
MAX_SSHD_PROCESSES = 100
MAX_FILE_USAGE_PERCENT = 80
MAX_CLOSE_WAIT = 100
MAX_MEM_USAGE_PERCENT = 90
MAX_SSHD_MEM_MB = 500
MAX_SSHD_VIRT_MB = 2000
MAX_SWAP_USAGE_PERCENT = 80

SAMPLE_INTERVAL = 60   # seconds
LOG_FILE = "system_monitor.csv"

def run_cmd(cmd):
    result = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    return result.stdout.strip()

def count_ssh_sessions():
    out = run_cmd("who | grep 'pts' | wc -l")
    return int(out) if out.isdigit() else 0

def count_sshd_processes():
    out = run_cmd("ps -C sshd --no-headers | wc -l")
    return int(out) if out.isdigit() else 0

def get_file_usage():
    try:
        with open("/proc/sys/fs/file-nr") as f:
            vals = f.read().split()
            allocated = int(vals[0])
            max_files = int(vals[2])
            percent = (allocated / max_files) * 100 if max_files > 0 else 0.0
            return allocated, max_files, percent
    except:
        return 0, 0, 0.0

def count_close_wait():
    out = run_cmd("ss -tan | awk '{print $1}' | grep CLOSE-WAIT | wc -l")
    return int(out) if out.isdigit() else 0

def get_system_mem_usage():
    meminfo = {}
    with open("/proc/meminfo") as f:
        for line in f:
            key, val = line.split(":")
            meminfo[key.strip()] = int(val.strip().split()[0])  # kB

    mem_total = meminfo.get("MemTotal", 1)
    mem_avail = meminfo.get("MemAvailable", mem_total)
    used = mem_total - mem_avail
    percent = (used / mem_total) * 100

    swap_total = meminfo.get("SwapTotal", 1)
    swap_free = meminfo.get("SwapFree", swap_total)
    swap_used = swap_total - swap_free
    swap_percent = (swap_used / swap_total * 100) if swap_total > 0 else 0

    return used / 1024, mem_total / 1024, percent, swap_used / 1024, swap_total / 1024, swap_percent

def get_sshd_mem_usage():
    out = run_cmd("ps -C sshd -o rss= | awk '{sum+=$1} END {print sum}'")
    return int(out)//1024 if out.isdigit() else 0  # MB

def get_sshd_vms_usage():
    out = run_cmd("ps -C sshd -o vsz= | awk '{sum+=$1} END {print sum}'")
    return int(out)//1024 if out.isdigit() else 0  # MB

def check_thresholds(metrics):
    issues = []
    ssh_sessions, sshd_procs, fd_percent, close_wait_count, mem_percent, swap_percent, sshd_mem_rss, sshd_mem_vms = metrics

    if ssh_sessions > MAX_SSH_SESSIONS:
        issues.append(f"Too many SSH sessions ({ssh_sessions} > {MAX_SSH_SESSIONS})")
    if sshd_procs > MAX_SSHD_PROCESSES:
        issues.append(f"Excessive sshd processes ({sshd_procs} > {MAX_SSHD_PROCESSES})")
    if fd_percent > MAX_FILE_USAGE_PERCENT:
        issues.append(f"File descriptor usage high ({fd_percent:.1f}% > {MAX_FILE_USAGE_PERCENT}%)")
    if close_wait_count > MAX_CLOSE_WAIT:
        issues.append(f"Too many CLOSE_WAIT sockets ({close_wait_count} > {MAX_CLOSE_WAIT})")
    if mem_percent > MAX_MEM_USAGE_PERCENT:
        issues.append(f"System RAM usage high ({mem_percent:.1f}% > {MAX_MEM_USAGE_PERCENT}%)")
    if swap_percent > MAX_SWAP_USAGE_PERCENT:
        issues.append(f"System swap usage high ({swap_percent:.1f}% > {MAX_SWAP_USAGE_PERCENT}%)")
    if sshd_mem_rss > MAX_SSHD_MEM_MB:
        issues.append(f"sshd RSS memory too high ({sshd_mem_rss}MB > {MAX_SSHD_MEM_MB}MB)")
    if sshd_mem_vms > MAX_SSHD_VIRT_MB:
        issues.append(f"sshd virtual memory too high ({sshd_mem_vms}MB > {MAX_SSHD_VIRT_MB}MB)")

    return issues

def write_csv_header():
    if not os.path.exists(LOG_FILE):
        with open(LOG_FILE, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "timestamp",
                "ssh_sessions",
                "sshd_procs",
                "fd_percent",
                "close_wait_count",
                "mem_percent",
                "swap_percent",
                "sshd_mem_rss",
                "sshd_mem_vms"
            ])

def append_csv(metrics):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with open(LOG_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([ts] + list(metrics))

def detect_increasing_trend(history, key_index, window=5):
    """
    Check last `window` samples for monotonic increase.
    history = list of metric tuples
    key_index = which metric to check
    """
    if len(history) < window:
        return False

    values = [h[key_index] for h in history[-window:]]
    return all(values[i] < values[i+1] for i in range(len(values)-1))

def sample_metrics():
    ssh_sessions = count_ssh_sessions()
    sshd_procs = count_sshd_processes()
    _, _, fd_percent = get_file_usage()
    close_wait_count = count_close_wait()
    _, _, mem_percent, _, _, swap_percent = get_system_mem_usage()
    sshd_mem_rss = get_sshd_mem_usage()
    sshd_mem_vms = get_sshd_vms_usage()
    return (ssh_sessions, sshd_procs, fd_percent, close_wait_count, mem_percent, swap_percent, sshd_mem_rss, sshd_mem_vms)

def main_loop():
    print(f"📊 Starting continuous monitoring, sampling every {SAMPLE_INTERVAL}s...")
    write_csv_header()
    history = []  # store last N samples

    while True:
        metrics = sample_metrics()
        append_csv(metrics)

        # Print snapshot
        print(f"[{datetime.now()}] SSH={metrics[0]} | sshd={metrics[1]} | FD={metrics[2]:.1f}% | CLOSE_WAIT={metrics[3]} | MEM={metrics[4]:.1f}% | SWAP={metrics[5]:.1f}% | sshdRSS={metrics[6]}MB | sshdVMS={metrics[7]}MB")

        # Check thresholds
        issues = check_thresholds(metrics)
        if issues:
            print("🚨 Issues:")
            for issue in issues:
                print("  - " + issue)

        # Track history for trend detection
        history.append(metrics)

        # Detect if memory is creeping up
        if detect_increasing_trend(history, key_index=4, window=5):  # mem_percent
            print("⚠️ WARNING: System memory usage is steadily increasing!")

        # Detect if sshd RSS is creeping up
        if detect_increasing_trend(history, key_index=6, window=5):
            print("⚠️ WARNING: sshd RSS is steadily increasing!")

        time.sleep(SAMPLE_INTERVAL)

if __name__ == "__main__":
    try:
        main_loop()
    except KeyboardInterrupt:
        print("\nStopping monitor.")
