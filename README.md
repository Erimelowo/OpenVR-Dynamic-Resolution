# OpenVR Dynamic Resolution

OpenVR app that dynamically adjusts the HMD resolution to the GPU frametime.

This allows you to always have the maximum resolution your GPU can handle while hitting your target FPS (without reprojecting). Good for games in which GPU requirement varies a lot (e.g. VRChat).

This is **not** the same as SteamVR's Auto Resolution, which behaviour's is quite bizarre, but seems to be using some benchmarking approach that doesn't work for the same as OVRDR.

This won't work in games that require a restart for resolution changes, or that use their own dynamic resolution. For a list of working and non-working games, check out [WorkingGames.md](WorkingGames.md)

## Installation/Using It

_Make sure **SteamVR's Render Resolution** is set to **Custom** (which percentage doesn't matter)_

- Download the [latest release ](https://github.com/Louka3000/OpenVR-Dynamic-Resolution/releases/latest/download/OpenVR-Dynamic-Resolution.zip)
- Extract the .zip
- Launch `OpenVR-Dynamic-Resolution.exe`

Make sure to check the Settings Descriptions below, especially to auto-start the program with SteamVR.

## Settings Descriptions

Settings are found in the `settings.ini` file. Do not rename that file. It should be located in the same folder as your executable file (`OpenVR-Dynamic-Resolution.exe`).

- `autoStart`: (0 = disabled, 1 = enabled) Enabling it will launch the program with SteamVR automatically.

- `initialRes`: The resolution the program sets when starting.

- `minRes`: The minimum value the program will be allowed to set your HMD's resolution to.

- `maxRes`: The maximum value the program will be allowed to set your HMD's resolution to.

- `dataPullDelayMs`: The time in milliseconds (1000ms = 1sec) the program waits to pull and display new information.

- `resChangeDelayMs`: The delay in milliseconds (1000ms = 1sec) between each resolution change. Lowering it will make the resolution change more responsive, but will cause more stuttering from resolution changes.

- `minGpuTimeThreshold`: If GPU time in milliseconds is below this value, don't change resolution. Useful to avoid messing with resolution in the SteamVR void or during loading screens.

- `resIncreaseMin`: How many % to increase resolution when we have GPU headroom regardless of how much.

- `resDecreaseMin`: How many % to decrease resolution when GPU time is too high regardless of by how much.

- `resIncreaseScale`: How much to increase resolution depending on GPU frametime headroom available

- `resDecreaseScale`: How much to decrease resolution depending on GPU frametime difference needed.

- `resIncreaseThreshold`: Percentage of the target frametime at which the program will stop increase resolution

- `resDecreaseThreshold`: Percentage of the target frametime at which the program will stop decreasing resolution

- `dataAverageSamples`: Samples to take for the average GPU time and to know if GPU is the bottleneck or not. Depends on dataPullDelayMs.

- `alwaysReproject`: (0 = disabled, 1 = enabled) Enabling will double the target frametime, so if you're at a target FPS of 120, it'll target 60. Useful if you have a bad CPU but good GPU.

## Building from source

[For the time being, we only support Windows](https://github.com/Louka3000/OpenVR-Dynamic-Resolution/issues/8). We assume that you have a recent Visual Studio (2022), Git and CMake.

```
git clone --recursive https://github.com/Louka3000/OpenVR-Dynamic-Resolution.git
cd OpenVR-Dynamic-Resolution
cmake -B build
cmake --build build --config Release
```

You can then execute the newly built binary:

```
.\build\Release\OpenVR-Dynamic-Resolution.exe
```

You can also build the project directly from within Visual Studio, since recent versions of Visual Studio support both git and cmake.

## Licensing

BSD 3-Clause License

This work is based on [SlimeVR-Feeder-App](https://github.com/SlimeVR/SlimeVR-Feeder-App) which is licensed under the BSD 3-Clause License. We use the OpenVR library which is also available under the BSD 3-Clause License. Additionally, we use [simpleini](https://github.com/brofield/simpleini) which is available the MIT License and the fmt library which is available under a [Boost-like license](https://github.com/fmtlib/fmt/blob/master/LICENSE.rst).
