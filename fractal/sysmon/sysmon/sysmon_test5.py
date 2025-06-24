import paramiko

host = "192.168.86.26"
user = "root"
key_path = "/home/pi5/.ssh/id_rsa"

try:
    print("Connecting to {}@{} with key {}".format(user, host, key_path))
    key = paramiko.RSAKey.from_private_key_file(key_path)
    
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    ssh.connect(
        hostname=host,
        port=22,
        username=user,
        pkey=key,
        look_for_keys=False,
        allow_agent=False,
        timeout=5
    )
    
    stdin, stdout, stderr = ssh.exec_command("uptime")
    print("Connected. Output:")
    print(stdout.read().decode().strip())
    ssh.close()

except paramiko.AuthenticationException:
    print("❌ Authentication failed.")
except paramiko.SSHException as e:
    print("❌ SSH error:", e)
except Exception as e:
    print("❌ Other error:", e)
