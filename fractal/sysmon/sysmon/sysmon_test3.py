import paramiko

host = "192.168.86.26"
user = "root"
port = 22
keyfile = "/home/pi5/.ssh/id_rsa"  # Change to exact full path

try:
    print("Connecting to {}@{} with key {}...".format(user, host, keyfile))
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())  # Accept unknown hosts

    pkey = paramiko.RSAKey.from_private_key_file(keyfile)

    ssh.connect(hostname=host, port=port, username=user, pkey=pkey, look_for_keys=False, allow_agent=False, timeout=5)

    stdin, stdout, stderr = ssh.exec_command("uptime")
    print("Connected. Uptime:")
    print(stdout.read().decode().strip())

    ssh.close()
except Exception as e:
    print("SSH connection failed:", str(e))
