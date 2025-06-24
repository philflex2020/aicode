import paramiko

host = "192.168.86.26"
user = "root"
port = 22

# You can specify a password, or let it try your private key
password = None  # or set your root password here

try:
    print("Connecting to {}@{}...".format(user, host))
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    ssh.connect(host, port=port, username=user, password=password, timeout=5)

    stdin, stdout, stderr = ssh.exec_command("uptime")
    print("Connected. Uptime:")
    print(stdout.read().decode().strip())

    ssh.close()
except Exception as e:
    print("SSH connection failed:", str(e))
