Systemd manages core dumps through its `systemd-coredump` service, and the core dumps are typically stored in a journal. 

They are not saved as regular files in the filesystem unless configured otherwise.

Here’s how to locate core dumps managed by systemd:

---

### **View Core Dumps**
Use the following command to list stored core dumps:
```bash
coredumpctl list
```

This displays information about available core dumps, such as the executable name, PID, and timestamp.

---

### **Inspect a Specific Core Dump**
To view details about a specific core dump:
```bash
coredumpctl info <PID>
```

---

### **Extract a Core Dump to a File**
If you need the core dump as a file:
```bash
coredumpctl dump <PID> --output=core.dump
```

Replace `<PID>` with the PID of the process whose core dump you want to extract.

---

### **Default Storage Location**
By default, `systemd-coredump` stores core dumps in the journal, not as files on the disk. If you want core dumps saved as files, you can configure this.

#### Modify Core Dump Storage Configuration:
1. Edit the systemd-coredump configuration file:
   ```bash
   sudo nano /etc/systemd/coredump.conf
   ```
2. Look for the `Storage=` option and set it to `external`:
   ```ini
   [Coredump]
   Storage=external
   ```
   This saves core dumps as files in `/var/lib/systemd/coredump/`.

3. Restart the `systemd-coredump` service:
   ```bash
   sudo systemctl restart systemd-coredump
   ```

---

### **Classic Core Dump Location**
If you’re using the classic core dump mechanism (`ulimit`), core dumps are written to the current working directory of the crashed process or to a directory specified by `/proc/sys/kernel/core_pattern`.

To check or modify the core dump pattern:
```bash
cat /proc/sys/kernel/core_pattern
```

- Common values:
  - `core` (current directory of the process)
  - `/var/lib/systemd/coredump/core`
  - Custom paths for specific configurations

To change this setting temporarily:
```bash
echo "/path/to/dump/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern
```

For a permanent change, modify the `/etc/sysctl.conf` or a file in `/etc/sysctl.d/`.

---

### **Check Core Dump Limits**
Ensure core dumps are enabled using `ulimit`:
```bash
ulimit -c unlimited
```

To make this change permanent, add it to the shell configuration file (e.g., `~/.bashrc` or `/etc/security/limits.conf`):
```plaintext
* soft core unlimited
* hard core unlimited
```

---

### **Summary**
- Use `coredumpctl` for systemd-managed core dumps.
- Configure `/etc/systemd/coredump.conf` to change storage to files.
- Check `/proc/sys/kernel/core_pattern` for the core dump location when using traditional mechanisms.