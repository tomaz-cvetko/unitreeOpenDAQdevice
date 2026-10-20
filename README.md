# Unitree Go2 openDAQ device module

An openDAQ module that subscribes to the Unitree Go2 `rt/lowstate` DDS topic (via CycloneDDS) and exposes it as an openDAQ device, **RobotDog**. The device's channel publishes foot forces, IMU, 12 motors (q, dq, tau, temperature), battery, power, temperatures and fan speeds on one 500 Hz time domain. Any openDAQ client can use it, including DewesoftX.

## Prerequisites

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

`cyclone_example` and `test_main` are small CycloneDDS sanity checks.

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
