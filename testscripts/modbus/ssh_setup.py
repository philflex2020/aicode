import os
import subprocess

# Step 1: Check if OpenSSH server (sshd) is installed and install if needed
def is_sshd_installed():
    try:
        subprocess.check_output(["rpm", "-q", "openssh-server"])
        return True
    except subprocess.CalledProcessError:
        return False

def install_sshd():
    try:
        subprocess.run(["yum", "install", "-y", "openssh-server"], check=True)
    except subprocess.CalledProcessError as e:
        print("Failed to install openssh-server.")
        print("Error:", e)
        exit(1)

# Step 2: Set up the necessary permissions for SSH files in /tmp/.ssh
def set_ssh_files_permissions():
    ssh_dir = '/tmp/.ssh'
    authorized_keys_path = os.path.join(ssh_dir, 'authorized_keys')

    # Create the /tmp/.ssh directory if it doesn't exist
    os.makedirs(ssh_dir, exist_ok=True)

    # Set the correct permissions for the directory and authorized_keys file
    os.chmod(ssh_dir, 0o700)
    os.chmod(authorized_keys_path, 0o600)

# Step 3: Modify SSH server configuration to enable no-password root SSH logins
def modify_sshd_config():
    sshd_config_path = '/etc/ssh/sshd_config'

    # Read the current sshd_config file
    with open(sshd_config_path, 'r') as f:
        sshd_config = f.read()

    # Modify the configuration to enable no-password root SSH logins
    sshd_config = sshd_config.replace('#PermitRootLogin yes', 'PermitRootLogin without-password')

    # Write the updated configuration back to sshd_config file
    with open(sshd_config_path, 'w') as f:
        f.write(sshd_config)

def main():
    if not is_sshd_installed():
        print("OpenSSH server (sshd) is not installed. Installing it now...")
        install_sshd()

    set_ssh_files_permissions()
    modify_sshd_config()

    print("SSH setup completed successfully.")

if __name__ == "__main__":
    main()
