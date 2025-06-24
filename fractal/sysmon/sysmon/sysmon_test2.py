import paramiko
import os

host = "192.168.86.26"
user = "root"
port = 22
key_path = os.path.expanduser("~/.ssh/id_rsa")  # adjust if needed

try:
    print("Connecting to {}@{}...".format(user, host))
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    pkey = paramiko.RSAKey.from_private_key_file(key_path)

    ssh.connect(hostname=host, port=port, username=user, pkey=pkey, timeout=5)

    stdin, stdout, stderr = ssh.exec_command("uptime")
    print("Connected. Uptime:")
    print(stdout.read().decode().strip())

    ssh.close()
except Exception as e:
    print("SSH connection failed:", str(e))
