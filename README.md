# OpenVR Dynamic Resolution

[![Windows build testing](https://github.com/Louka3000/OpenVR-Dynamic-Resolution/actions/workflows/vs17.yml/badge.svg)](https://github.com/Louka3000/OpenVR-Dynamic-Resolution/actions/workflows/vs17.yml)

OpenVR app that dynamically adjusts the HMD's resolution to the GPU frametime, CPU frametime and VRAM.

This allows you to always have the maximum resolution your GPU can handle while hitting your target FPS (without reprojecting). Useful for games in which GPU requirement varies a lot (e.g. VRChat).

This is **not** the same as SteamVR's Auto Resolution, which seems to be using some benchmarking approach that does not work for the same as OVRDR.

Pro tip: if you want a higher resolution (for supersampling), you can lower your HMD's framerate to increase the target frametime.

**Check out [WorkingGames.md](WorkingGames.md) for a list of working and non-working games.**

## Installation/Using It

**_Make sure SteamVR's Render Resolution is set to Custom_**

- Download the [latest release ](https://github.com/Louka3000/OpenVR-Dynamic-Resolution/releases/latest/download/OpenVR-Dynamic-Resolution.zip)
- Extract the .zip
- Launch `OpenVR-Dynamic-Resolution.exe`

## Settings Descriptions

Settings are found in the `settings.ini` file. Do not rename that file. It should be located in the same folder as your executable file (`OpenVR-Dynamic-Resolution.exe`).

- `autoStart`: (0 = disabled, 1 = enabled) Enabling it will launch the program with SteamVR automatically.

- `minimizeOnStart`: (0 = show, 1 = minimize, 2 = hide) Will automatically minimize or hide the window on launch. If set to 2 (hide), you won't be able to exit the program manually, but it will automatically exit with SteamVR.

- `initialRes`: The resolution the program sets your HMD's resolution when starting. Also the resolution that is targeted in vramOnlyMode.

- `minRes`: The minimum value the program will be allowed to set your HMD's resolution to.

- `maxRes`: The maximum value the program will be allowed to set your HMD's resolution to.

- `dataPullDelayMs`: The time in milliseconds (1000ms = 1s) the program waits to pull and display new information. Adjust dataAverageSamples accordingly.

- `resChangeDelayMs`: The delay in milliseconds (1000ms = 1s) between each resolution change. Lowering it will make the resolution change more responsive, but will cause more stuttering from resolution changes.

- `minCpuTimeThreshold`: Don't increase resolution when CPU time in milliseconds is below this value. Useful to avoid the resolution increasing in the SteamVR void or during loading screens. Also see resetOnThreshold.

- `resIncreaseMin`: How many static % to increase resolution when we have GPU or/and VRAM headroom.

- `resDecreaseMin`: How many static % to decrease resolution when GPU frametime or/and VRAM usage is too high.

- `resIncreaseScale`: How much to increase resolution depending on GPU frametime headroom.

- `resDecreaseScale`: How much to decrease resolution depending on GPU frametime excess.

- `resIncreaseThreshold`: Percentage of the target frametime at which the program will stop increase resolution

- `resDecreaseThreshold`: Percentage of the target frametime at which the program will stop decreasing resolution

- `dataAverageSamples`: Number of samples to use for the average GPU time. Depends on dataPullDelayMs.

- `resetOnThreshold`: (0 = disabled, 1 = enabled) Enabling will reset the resolution to initialRes whenever minCpuTimeThreshold is met. Useful if you wanna go from playing a supported game to an unsuported games without having to reset your resolution/the program/SteamVR.

- `alwaysReproject`: (0 = disabled, 1 = enabled) Enabling will double the target frametime, so if you're at a target FPS of 120, it'll target 60. Useful if you have a bad CPU but good GPU.

- `vramTarget`: The target VRAM usage in percents. Once your VRAM usage exceeds this amount, the resolution will stop increasing.

- `vramLimit`: The maximum VRAM usage in percents. Once your VRAM usage exceeds this amount, the resolution will start decreasing.

- `vramMonitorEnabled`: (0 = disabled, 1 = enabled) If enabled, vram specific features will be enabled, otherwise it is assumed that free vram is always available.

- `vramOnlyMode`: (0 = disabled, 1 = enabled) Only adjust resolution based off VRAM; ignore GPU and CPU frametimes. Will always stay at initialRes or lower (if VRAM limit is reached).

- `preferReprojection`: (0 = disabled, 1 = enabled) If enabled, the GPU target frametime will double as soon as the CPU frametime is over the target frametime; else, the CPU frametime needs to be 2 times greater than the target frametime for the GPU target frametime to double.

- `ignoreCpuTime`: (0 = disabled, 1 = enabled) Don't use the CPU frametime to adjust resolution.

## Building from source

We assume that you already have Git and CMake installed.
To build the project, run:

```
git clone --recursive https://github.com/Louka3000/OpenVR-Dynamic-Resolution.git
cd OpenVR-Dynamic-Resolution
cmake -B build
cmake --build build --config Release
```

You can then execute the newly built binary by running:

```
.\build\Release\OpenVR-Dynamic-Resolution.exe
```

## Licensing

BSD 3-Clause License

This work is based on [SlimeVR-Feeder-App](https://github.com/SlimeVR/SlimeVR-Feeder-App) which is licensed under the BSD 3-Clause License. We use the OpenVR library which is also available under the BSD 3-Clause License. Additionally, we use [simpleini](https://github.com/brofield/simpleini) which is available the MIT License and the fmt library which is available under a [Boost-like license](https://github.com/fmtlib/fmt/blob/master/LICENSE.rst).
