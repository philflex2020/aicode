import subprocess

def run_ssh_cmd(host, user, command):
    try:
        result = subprocess.run(
            ["ssh", f"{user}@{host}", command],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
            check=True,
            universal_newlines=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        return f"SSH failed: {e.stderr.strip()}"
    except subprocess.TimeoutExpired:
        return "SSH timeout"

# Test it
host = "192.168.86.26"
user = "root"

print("Testing SSH uptime:")
output = run_ssh_cmd(host, user, "uptime")
print(output)
