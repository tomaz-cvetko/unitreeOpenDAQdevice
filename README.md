# Unitree Go2 openDAQ device module

An openDAQ module that subscribes to the Unitree Go2 `rt/lowstate` DDS topic (via CycloneDDS) and exposes it as an openDAQ device, **RobotDog**. The device's channel publishes foot forces, IMU, 12 motors (q, dq, tau, temperature), battery, power, temperatures and fan speeds on one 500 Hz time domain. Any openDAQ client can use it, including DewesoftX.

## Getting started (step by step)

Follow these steps in order on a fresh Windows PC. Steps 1–4 are needed only once per machine. Later sections explain the details.

### 1. Install the tools

1. **Visual Studio 2022 (17.4 or newer) or Visual Studio 2026.** The free Community edition is enough: https://visualstudio.microsoft.com/
   - In the Visual Studio Installer, tick the **Desktop development with C++** workload.
   - In the panel on the right, make sure **C++ CMake tools for Windows** is ticked (it is by default). It provides CMake and Ninja.
   - Already have Visual Studio? Open *Visual Studio Installer* → **Modify** on your installation and check the same two items.
2. **Git for Windows:** https://git-scm.com/download/win. Keep the default options; it must end up on `PATH`. Check by opening a new PowerShell window and running `git --version`.
3. Optional, for viewing the data without DewesoftX: **Python 3** (https://www.python.org/), with *Add python.exe to PATH* ticked.

### 2. Get the source code

Open **PowerShell** (Start menu → type *PowerShell*) and go to a folder for your projects, e.g. `C:\work`. Avoid very long paths: the build creates deep folder trees, and Windows has a 260-character path limit.

```powershell
cd C:\work
git clone https://github.com/anamahau/unitreeOpenDAQdevice.git
cd unitreeOpenDAQdevice
```

If you were told to use a specific branch, switch to it now with `git checkout <branch-name>`.

### 3. Build and install CycloneDDS (≈10 minutes)

Still in PowerShell, in the `unitreeOpenDAQdevice` folder:

```powershell
powershell -ExecutionPolicy Bypass -File tools\setup_cyclonedds.ps1
```

It ends with a green line *"CycloneDDS 11.0.1 (Debug + Release) installed to: C:\Users\<you>\cyclonedds-11.0.1"*. If it stops with a red error, check that Visual Studio (step 1) and Git are installed, then run it again.

### 4. Tell the project where CycloneDDS is

```powershell
setx CYCLONEDDS_ROOT "$env:USERPROFILE\cyclonedds-11.0.1"
```

This sets a permanent user environment variable. Programs only see it after they are started again, so **close all Visual Studio windows now**, if any are open.

### 5. Open the project in Visual Studio

1. Start Visual Studio. On the start window choose **Open a local folder** (or, if VS is already open, **File → Open → Folder…**) and select the `unitreeOpenDAQdevice` folder, the one containing `CMakePresets.json`.
   - Don't use *Open a project or solution*. There is no `.sln` file; Visual Studio works directly from the CMake files.
2. Visual Studio starts **configuring** the project automatically. Watch the progress in the **Output** window (**View → Output**, then *Show output from:* **CMake**).
   - **The first time takes a long time** (roughly 20 minutes or more), because openDAQ is downloaded and prepared. Wait until the Output window says *"CMake generation finished"*.
   - If it says *"CycloneDDS 11.0.1 (C and C++) not found"*, step 4 didn't take effect. Close Visual Studio completely, start it again, and use **Project → Delete Cache and Reconfigure**.
3. In the toolbar at the top, the configuration drop-down shows **x64 Debug**. Leave it for development, or pick **x64 Release** for a fast, optimised build. Each choice is configured and built separately, so the first build of each one takes long.

### 6. Build

Use **Build → Build All** (or `Ctrl+Shift+B`). The first build compiles openDAQ and takes a while. When it finishes, the Output window (*Show output from:* **Build**) ends with *"Build All succeeded"*.

### 7. Run

1. In the toolbar, click the small arrow next to the green ▶ button (*Select Startup Item*) and choose **server_example.exe**.
2. Press **F5** to run with the debugger, or **Ctrl+F5** to run without it. A console window opens and shows, among other lines:
   ```
   creating device
   Listening for Unitree Go2 sensor data on topic 'rt/lowstate'...
   Press "enter" to exit the application...
   ```
   This means the RobotDog device is running and waiting for data from the dog. If the robot is reachable on the network (see *Connecting to the robot* below), data starts flowing immediately.
3. Keep the console window open while you look at the data (next step). Press **Enter** in it to stop the program.

### 8. Look at the data

- **openDAQ Python GUI.** In a new PowerShell window:
  ```powershell
  py -m pip install openDAQ
  py -m openDAQ
  ```
  The **Robot dog** device shows up in the list of discovered devices. Connect to it and open its signals (forces, IMU, motors, …).
- **DewesoftX:** add the device through the openDAQ device list. It appears as **Robot dog**.

### Connecting to the robot

- The program receives the dog's data over the network (DDS, domain 0, topic `rt/lowstate`). Connect the PC to the robot's Ethernet port. Following Unitree's developer documentation, give the PC's wired network adapter a static address in the robot's subnet (for the Go2: `192.168.123.x`, e.g. `192.168.123.99`, mask `255.255.255.0`).
- If the PC has several network adapters (Wi-Fi plus Ethernet, VPN, …) and no data arrives, tell CycloneDDS which adapter to use before starting `server_example`. Find the adapter name (e.g. `Ethernet 2`) with `Get-NetAdapter` in PowerShell, then:
  ```powershell
  [Environment]::SetEnvironmentVariable("CYCLONEDDS_URI", "<CycloneDDS><Domain><General><Interfaces><NetworkInterface name='Ethernet 2'/></Interfaces></General></Domain></CycloneDDS>", "User")
  ```
  Restart Visual Studio afterwards, as in step 4.

### Updating later

To get newer code, run `git pull` in the project folder, then build again (step 6). Steps 1–4 don't need repeating. If Visual Studio behaves strangely after an update, use **Project → Delete Cache and Reconfigure**.

---

## Prerequisites (reference)

- **Visual Studio 2022 (17.4 or newer) or Visual Studio 2026**, with the *Desktop development with C++* workload. That workload includes *C++ CMake tools for Windows*, which provides CMake and Ninja.
- **Git**. It must be on `PATH`, because openDAQ and CycloneDDS are fetched with it.
- CycloneDDS **11.0.1**, C and C++, installed **in both Debug and Release** as described below.

## CycloneDDS setup (once per machine)

The project needs CycloneDDS and CycloneDDS-CXX **11.0.1**. The Unitree message types in `unitree_module/include/unitree_module/*State.hpp` are generated code, and they only compile against that version.

> Older notes and generated headers mention "Cyclone DDS v0.11.0". That was the version string of CycloneDDS's development `master` branch before it was released as 11.0.0. There is no 0.11.0 release, and 11.0.1 is the matching release.

### Why a special install is needed

With MSVC, a Debug build must link the Debug CycloneDDS libraries and a Release build the Release ones. Mixing them gives `LNK2038: mismatch detected for '_ITERATOR_DEBUG_LEVEL'` or crashes at runtime. CycloneDDS's default install uses the same file names (`ddsc.dll`, `ddscxx.dll`) for both configurations, so they can't share a folder, and separate Debug/Release folders confuse CMake.

The fix: install both configurations into **one** folder, with Debug libraries suffixed `d` (`ddscd.dll`, `ddscxxd.dll`). CMake then picks the right one automatically for every configuration.

### Automatic (recommended)

From a PowerShell window in the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools\setup_cyclonedds.ps1
```

The script:

1. Clones `cyclonedds` and `cyclonedds-cxx` at tag `11.0.1` into `%USERPROFILE%\cyclonedds-build`.
2. Builds and installs each in Debug and Release into `%USERPROFILE%\cyclonedds-11.0.1`.
3. Turns off OpenSSL/DDS-Security (not used by the Go2), so the result has no extra DLL dependencies.

It takes about 5–10 minutes. Options:
- `-Prefix <dir>`: install somewhere else.
- `-Generator "Visual Studio 17 2022"`: pick a specific Visual Studio. By default the newest installed one is used.

Then tell the project where CycloneDDS is, and **restart Visual Studio** so it picks up the variable:

```powershell
setx CYCLONEDDS_ROOT "$env:USERPROFILE\cyclonedds-11.0.1"
```

> If you use both VS 2022 and VS 2026, build CycloneDDS with **VS 2022** (`-Generator "Visual Studio 17 2022"`). Libraries built by an older compiler work with a newer one, not the other way round.

### Manual

Equivalent commands, if you prefer to run them yourself. `<prefix>` is any folder, for example `C:\sdk\cyclonedds-11.0.1`.

```powershell
git clone --depth 1 --branch 11.0.1 https://github.com/eclipse-cyclonedds/cyclonedds.git
git clone --depth 1 --branch 11.0.1 https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git

cmake -S cyclonedds -B build-c -A x64 -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_DEBUG_POSTFIX=d -DENABLE_SSL=OFF -DENABLE_SECURITY=OFF -DENABLE_LTO=OFF -DBUILD_DDSPERF=OFF
cmake --build build-c --config Debug   --target install
cmake --build build-c --config Release --target install

cmake -S cyclonedds-cxx -B build-cxx -A x64 -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<prefix> -DCMAKE_DEBUG_POSTFIX=d -DENABLE_LTO=OFF
cmake --build build-cxx --config Debug   --target install
cmake --build build-cxx --config Release --target install
```

Build **Release last**. `idlc.exe` (the IDL compiler) has no Debug suffix, so the last build installed is the one that stays.

Check the result: `<prefix>\bin` must contain `ddsc.dll`, `ddscd.dll`, `ddscxx.dll`, `ddscxxd.dll` and `idlc.exe`.

### Pointing the project at CycloneDDS

The CMake cache variable `CYCLONEDDS_ROOT` must point to `<prefix>`. It defaults to the environment variable of the same name. Instead of the environment variable, you can create a `CMakeUserPresets.json` next to `CMakePresets.json`. That file is ignored by git, so it can hold machine-specific paths:

```json
{
    "version": 4,
    "configurePresets": [
        {
            "name": "my-debug",
            "displayName": "My x64 Debug",
            "inherits": "x64-debug",
            "cacheVariables": { "CYCLONEDDS_ROOT": "C:/sdk/cyclonedds-11.0.1" }
        }
    ]
}
```

Configuration stops with an explicit error if CycloneDDS is missing, has the wrong version, or lacks the Debug or Release libraries needed by the selected preset.

## Building in Visual Studio (2022 or 2026)

1. **File → Open → Folder…** and select the repository root. Visual Studio detects `CMakePresets.json`.
2. In the toolbar's configuration drop-down, pick **x64 Debug**, **x64 Release** or **x64 Release with debug info**. Each preset has its own build folder under `out/build/<preset>`, so switching between them is safe.
3. The first configure downloads and builds openDAQ v3.20.1, which takes a while (~20 min per configuration). Later builds are incremental.
4. **Build → Build All**.
5. To run or debug, select **server_example.exe** as the startup item and press F5. The required DLLs (openDAQ, CycloneDDS) are copied next to the executables and the module automatically.

## Building from the command line

Open a **Developer PowerShell for VS 2022/2026** (it puts `cl.exe` and Ninja on `PATH`), then:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Use `x64-release` for Release. Output goes to `out/build/<preset>/bin`.

## Testing the module

`server_example` creates the RobotDog device as the root device, adds an openDAQ native streaming server, and enables discovery. With the dog connected on the same network, the signals then show data. Connect with the openDAQ Python GUI (the latest Python version is recommended):

```
py -m pip install openDAQ
py -m openDAQ
```

You can also add the device in DewesoftX.

### Testing without the robot: `dog_simulator`

`dog_simulator.exe` is built next to `server_example.exe`. It publishes fake Go2 `LowState` messages on the same DDS topic, at 500 Hz, with `tick` advancing by 2 ms per message, just like the robot. Start `server_example` (e.g. with F5 in Visual Studio), then in a PowerShell window:

```powershell
cd out\build\x64-debug\bin
.\dog_simulator.exe
```

The simulator prints a status line every second; `matched_readers=1` means the module is receiving. Stop it with Ctrl+C. The values are fixed patterns, so they are easy to recognise in a client:

| Signal | Simulated value |
|---|---|
| `FL_force` … `RR_force` | 20 ± 15 N sine (0.5 Hz), phase-shifted per leg |
| `motorXXX_q` | 0.1·i ± 0.5 rad sine (1 Hz) |
| `motorXXX_temperature` | 30 … 41 °C (motor index + 30), constant |
| `current` | 1.5 ± 0.5 A sine (0.2 Hz) |
| `voltage` | 28.5 V; `battery` 80 %; `CPU_temperature` 45 °C |

To reproduce network problems:

```powershell
.\dog_simulator.exe --drop 2                     # randomly drop 2 % of messages (gaps in tick)
.\dog_simulator.exe --stall 200 --stall-every 5  # every 5 s hold messages back for 200 ms, then send them as a burst
.\dog_simulator.exe --clock-ppm 500              # the dog's clock runs 500 ppm fast (the module corrects this drift)
```

To check what a client actually receives (sample rate, values, gaps, and whether the timestamps drift away from the PC clock), run `tools\check_stream.py` while `server_example` is running. It needs the openDAQ Python package (`py -m pip install openDAQ`):

```powershell
py tools\check_stream.py --seconds 20
```

`.\dog_simulator.exe --help` lists all options (rate, duration, starting tick, DDS domain). The simulator works on the same PC as `server_example` or on another PC in the same network. Don't run it while the real robot is connected to the same network, because both publish on the same topic.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Configure error *"CycloneDDS 11.0.1 (C and C++) not found"* | `CYCLONEDDS_ROOT` is not set or is wrong. Set it, then in Visual Studio use **Project → Delete Cache and Reconfigure** (or restart VS after `setx`). |
| Configure error *"… has no DEBUG libraries"* | CycloneDDS was installed in one configuration only. Re-run `tools\setup_cyclonedds.ps1`. |
| `Error loading module "unitree_module…": … Error: 126` | A DLL next to the module is missing (usually `ddsc*.dll`/`ddscxx*.dll`). Rebuild the `unitree_module` target, which copies them. |
| `LNK2038 … _ITERATOR_DEBUG_LEVEL` | Debug and Release libraries are being mixed. Use a CycloneDDS install made as described above. |

## Useful links

- https://support.unitree.com/home/en/developer/Basic_services (explanation of LowState, MotorState, IMUState and BmsState data)
- https://support.unitree.com/home/en/developer/sports_services (explanation of SportModeState and IMUState data)
- https://docs.opendaq.com/
- https://cyclonedds.io/docs/
