# Unitree Go2 → openDAQ server (ready-to-run bundle)

This folder runs the **RobotDog** openDAQ device on a Windows PC that is connected to the Unitree Go2. It receives the robot's `rt/lowstate` data over DDS and serves it over the openDAQ native protocol. DewesoftX and other openDAQ clients on the network can then add the device.

Nothing has to be installed. Visual Studio, CycloneDDS, openDAQ and the Visual C++ runtime are all included in `bin\`. Keep the folder together: the programs load their DLLs and modules from `bin\`.

## Requirements

- Windows 10 or 11, 64-bit.
- A wired network connection to the robot. Following Unitree's developer documentation, the PC's adapter needs a static address in the robot's subnet: for the Go2, `192.168.123.x` (for example `192.168.123.99`, mask `255.255.255.0`).

## Start

1. Double-click **`start_server.bat`**. A console window opens and shows, among other lines:
   ```
   creating device
   Listening for Unitree Go2 sensor data on topic 'rt/lowstate'...
   Press "enter" to exit the application...
   ```
   As soon as data arrives from the robot, it also prints `on_subscription_matched` and `Time axis anchored: ...`.
2. **Windows Firewall:** the first time, Windows asks whether `server_example.exe` may communicate on networks. Allow it, at least on *private* networks. Without that, no data arrives from the robot (DDS uses UDP) and clients can't connect (openDAQ uses TCP).
3. Keep the window open while measuring. Press **Enter** in it to stop.

## Connecting a client

- **DewesoftX:** add the openDAQ device **Robot dog**. It is found automatically on the local network.
- **openDAQ Python GUI:** `py -m pip install openDAQ`, then `py -m openDAQ`.

## Several network adapters (CYCLONEDDS_URI)

CycloneDDS picks a network adapter automatically. If the PC has several (Wi-Fi plus Ethernet, VPN, virtual adapters, …) and no data arrives, tell it which one to use. Find the adapter name (for example `Ethernet 2`) with `Get-NetAdapter` in PowerShell, then run:

```powershell
[Environment]::SetEnvironmentVariable("CYCLONEDDS_URI", "<CycloneDDS><Domain><General><Interfaces><NetworkInterface name='Ethernet 2'/></Interfaces></General></Domain></CycloneDDS>", "User")
```

and start `start_server.bat` again from a **new** window. This is a per-user setting on this PC, and it applies to every CycloneDDS program the user runs. To remove it:

```powershell
[Environment]::SetEnvironmentVariable("CYCLONEDDS_URI", $null, "User")
```

## Checking without the robot, and checking the data

- `bin\dog_simulator.exe` publishes fake robot data at 500 Hz, on this PC or on another PC in the same network. Run it while `start_server.bat` is running. Don't run it while the real robot is connected, because both publish on the same topic. `bin\dog_simulator.exe --help` lists the options.
- `tools\check_stream.py` connects to the server, reads a few signals, and reports the sample rate, gaps, and how far the timestamps lag behind this PC's clock. It needs Python and the openDAQ package:
  ```powershell
  py -m pip install openDAQ
  py tools\check_stream.py --seconds 20
  ```
  From another PC, add `--url daq.nd://<server PC address>`.
