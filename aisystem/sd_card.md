
https://forums.developer.nvidia.com/t/super-nano-not-reading-sd-card-image/320580/12

The serial debug console is available on the J50 header on the Jetson Nano. The J50 header is located on the edge of the carrier board opposite the I/O connectors. The connector is underneath the Jetson module, directly below the SD Card reader.

Jetson Nano J50 Pin 4 (TXD) → Cable RXD (White Wire)
Jetson Nano J50 Pin 3 (RXD) → Cable TXD (Green Wire)
Jetson Nano J50 Pin 7 (GND) → Cable GND (Black Wire)

Hi, I have been working on Jetson Orin Nano board for a project and previously, was using a microSD card to run JetPack 5.1.3 (Ubuntu 20.04) on the board with ROS. The requirements for my project to work then changed and I had to be on Ubuntu 22.04 instead now, so to switch to JetPack 6, which runs that version of Ubuntu, I tried following the steps to update the QSPI bootloaders and reboot system. However, when my system was rebooting and the update was in progreess (~7%), the system power was accidentally unplugged. Since then, whether I use my old microSD card or a new one that contains JetPack 6 image, my Orin Nano board does not show anything on my monitor. When plugging in the power cord, the green light does turn on and the fan temporarily starts spinning for a few seconds before stopping.

However, my monitor still remains blank. I tried connecting Pins 9 and 10 to force it into recovery mode so I could try using SDK Manager to reinstall everything, including the QSPI bootloaders to fix it, but my computer does not even recognize the Orin Nano board (unplugging it and replugging it does not change lsusb or journalctl logs), but unplugging and replugging eg. my USB C Video Adapter does.

I’m currently struggling to figure out what’s going on and why my computer is unable to detect the board? I’m honestly not even sure if its successfully in Recovery Mode? Nothing seems different besides the header I have placed on the pins. Any help would be appreciated, thank you!


The Jetson Orin Nano Developer Kit consists of a P3767 System on Module (SOM) that is connected to a P3768 carrier board. Part number P3766 designates the complete Jetson Orin Nano Developer Kit. The SOM and carrier board each have an EEPROM where the board ID is saved. The Developer Kit can be used without any software configuration modifications.

Before you use the SOM with a carrier board other than the P3768, you must first change the kernel device tree, the MB1 configuration, the MB2 configuration, the ODM data, and the flashing configuration to correspond to the new carrier board. The next section provides more information about the changes.

No, you cannot directly copy the `Jetson_Linux_R36.4.3_aarch64.tbz2` file to an SD card using Etcher because it's not a bootable image. Instead, you need to follow the appropriate procedure to flash your Jetson device or create a bootable SD card.

Here’s how to proceed:

---

### **Create a Bootable SD Card**
1. **Extract the Archive:**
   - Extract the `Jetson_Linux_R36.4.3_aarch64.tbz2` file on your Linux PC:
     ```bash
     tar -xvf Jetson_Linux_R36.4.3_aarch64.tbz2
     ```
   - This will create a directory, usually named `Linux_for_Tegra/`.

2. **Insert the SD Card:**
   - Insert your SD card into your Linux PC and identify its device name (e.g., `/dev/sdX`) using:
     ```bash
     lsblk
     ```

3. **Prepare the SD Card Image:**
   - Navigate to the `Linux_for_Tegra/tools` directory:
     ```bash
     cd Linux_for_Tegra/tools
     ```
   - Use the `create-jetson-nano-sd-card-image.sh` script to create an SD card image. For example:
     ```bash
     ./create-jetson-nano-sd-card-image.sh -o jetson_sd.img -s 64G
     ```
     Replace `64G` with the size of your SD card if different.

4. **Flash the SD Card:**
   - Use a tool like `dd` or `Etcher` to flash the generated `.img` file (`jetson_sd.img`) to the SD card:
     - **Using `dd`:**
       ```bash
       sudo dd if=jetson_sd.img of=/dev/sdX bs=1M status=progress
       ```
       Replace `/dev/sdX` with your SD card's device name.
     - **Using Etcher:**
       - Open Etcher, select the `.img` file, and choose your SD card as the target. Then flash it.

5. **Boot from SD Card:**
   - Insert the SD card into your Jetson device.
   - Power it on. The device should boot from the SD card.

---

### **Flash Internal eMMC Using the File**
If your Jetson device uses eMMC storage (e.g., Xavier, Orin modules):
1. Put the Jetson device into recovery mode:
   - Power off the Jetson.
   - Hold the **recovery button** while powering it on.
   - Connect the device to your Linux PC via USB.

2. On your Linux PC:
   - Navigate to the extracted `Linux_for_Tegra/` directory.
   - Flash the device using the `flash.sh` script:
     ```bash
     sudo ./flash.sh jetson-agx-xavier-devkit mmcblk0p1
     ```
     Replace `jetson-agx-xavier-devkit` with the appropriate configuration for your device.

3. Wait for the process to complete. Once done, reboot your Jetson.

---

### **Additional Notes**
- Ensure you use the correct version of the L4T (Linux for Tegra) files compatible with your Jetson hardware.
- Back up any important data before flashing or creating the SD card, as the process can overwrite existing files.

Let me know if you need further clarification!


