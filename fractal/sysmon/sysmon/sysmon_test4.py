import paramiko

key_path = "/home/pi5/.ssh/id_rsa"  # after PEM conversion
pkey = paramiko.RSAKey.from_private_key_file(key_path)

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(
    hostname="192.168.86.26",
    username="root",
    port=22,
    pkey=pkey,
    look_for_keys=False,
    allow_agent=False,
    timeout=5
)
stdin, stdout, stderr = ssh.exec_command("uptime")
print(stdout.read().decode())
ssh.close()
