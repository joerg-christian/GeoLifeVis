# Building and running GeoLife Visualisation

This guide explains how to build and run the application on **Windows**, **Linux** and **macOS**.
The project uses **CMake**, so the steps are almost the same on every platform: install the
prerequisites, configure, build, run.

- [1. Overview](#1-overview)
- [2. Hardware and driver requirements](#2-hardware-and-driver-requirements)
- [3. Windows](#3-windows)
- [4. Linux](#4-linux)
- [5. macOS](#5-macos)
- [6. Getting the full GeoLife data set](#6-getting-the-full-geolife-data-set)
- [7. Build options](#7-build-options)
- [8. Debug builds and validation layers](#8-debug-builds-and-validation-layers)
- [9. Troubleshooting](#9-troubleshooting)

---

## 1. Overview

| Dependency | How it is provided |
|------------|--------------------|
| C++17 compiler | MSVC 2019+ / GCC 9+ / Clang 10+ / Apple Clang (Xcode 12+) |
| CMake ≥ 3.20 | Installed by you |
| Git | Installed by you (CMake uses it to download GLFW and GLM if needed) |
| Vulkan headers + loader | Vulkan SDK or system packages |
| `glslc` or `glslangValidator` | Vulkan SDK or system packages (compiles the GLSL shaders to SPIR-V) |
| GLFW 3.3+ | System package if found, **otherwise downloaded automatically** |
| GLM | System package if found, **otherwise downloaded automatically** |
| Dear ImGui, stb_image | Included in `third_party/` |

What the build does:

1. Compiles `shaders/shader.{vert,frag,comp}` to `vert.spv`, `frag.spv`, `comp.spv`.
2. Compiles the application `GeoLifeVis`.
3. Copies the SPIR-V shaders, `assets/textures/` and a data set file (named
   `geoLifeDataSet.txt`) **next to the executable**.

The program loads these files relative to its working directory. If they are not found there,
it switches to the directory of the executable, so you can start it from anywhere.

> **Note:** On the first configure CMake may download GLFW and GLM from GitHub. That step
> needs an internet connection and can take a minute.

---

## 2. Hardware and driver requirements

- A GPU and driver with **Vulkan** support (NVIDIA, AMD, Intel, or Apple Silicon/AMD Macs via MoltenVK)
- Compute shader support with **≥ 1024 invocations per work group** (the KDE shader uses `local_size_x = 1024`)
- Anisotropic sampling (`samplerAnisotropy`), which almost all GPUs support
- For the **full** data set: about 1.5 GB of free RAM and at least 1 GB of free GPU memory
  (the ~18 million points are uploaded to a single storage buffer)

Make sure your graphics driver is up to date. You can check whether Vulkan works with
`vulkaninfo` (included in the Vulkan SDK or the `vulkan-tools` package) or `vkcube`.

---

## 3. Windows

### 3.1 Install the prerequisites

1. **Visual Studio 2022** (the free Community edition is fine) with the workload
   **"Desktop development with C++"**. This includes the MSVC compiler and CMake.
   *Alternatively:* the "Build Tools for Visual Studio 2022" plus a standalone
   [CMake](https://cmake.org/download/) ≥ 3.20.
2. **Vulkan SDK** from LunarG: <https://vulkan.lunarg.com/sdk/home#windows>.
   Run the installer with the default options. It sets the `VULKAN_SDK` environment variable
   and includes `glslc`.
3. **Git** for Windows: <https://git-scm.com/download/win>.

Open a **new** terminal after the installation so that the new environment variables are
available. Check the installation:

```powershell
cmake --version
git --version
echo $env:VULKAN_SDK
```

### 3.2 Get the source code

```powershell
git clone https://github.com/joerg-christian/GeoLifeVis.git
cd GeoLifeVis
```

### 3.3 Build from the command line

```powershell
cmake -S . -B build
cmake --build build --config Release
```

CMake automatically uses the newest installed Visual Studio. To pick one explicitly, add for
example `-G "Visual Studio 17 2022" -A x64` to the first command.

### 3.4 Run

```powershell
.\build\Release\GeoLifeVis.exe
```

To load a different data set file:

```powershell
.\build\Release\GeoLifeVis.exe C:\path\to\geoLifeDataSet.txt
```

### 3.5 Build inside Visual Studio (optional)

**Option A: open the folder.** In Visual Studio choose *File → Open → Folder…* and select
the repository folder. Visual Studio detects `CMakeLists.txt` and configures the project.
Select `GeoLifeVis.exe` as the startup item and press **F5**.

**Option B: generate a solution.** After running the `cmake -S . -B build` command above, open
`build\GeoLifeVis.sln`. `GeoLifeVis` is already set as the startup project and its debugger
working directory is set to the output folder. Select *Release* and press **F5**.

> A *Debug* build turns on the Vulkan validation layers. These are included in the Vulkan SDK,
> so Debug builds work on Windows without extra steps.

---

## 4. Linux

Install the Vulkan driver for your GPU, the development packages and the shader compiler with
your distribution's package manager. Then build as described in [4.4](#44-build-and-run).

### 4.1 Ubuntu / Debian / Linux Mint / Pop!_OS

Ubuntu 23.04 and newer, Debian 12 and newer:

```bash
sudo apt update
sudo apt install build-essential cmake git \
                 libvulkan-dev vulkan-tools glslc \
                 libglfw3-dev libglm-dev \
                 mesa-vulkan-drivers
```

Ubuntu 22.04 has no `glslc` package. Install `glslang-tools` instead; the build then uses
`glslangValidator` automatically:

```bash
sudo apt install build-essential cmake git \
                 libvulkan-dev vulkan-tools glslang-tools \
                 libglfw3-dev libglm-dev \
                 mesa-vulkan-drivers
```

> `mesa-vulkan-drivers` provides the Vulkan drivers for AMD and Intel GPUs. With an NVIDIA
> GPU, install the proprietary NVIDIA driver instead (for example with
> `sudo ubuntu-drivers autoinstall`). It includes Vulkan support.

Alternatively, install the [LunarG Vulkan SDK for Linux](https://vulkan.lunarg.com/sdk/home#linux)
and run `source ~/vulkan/<version>/setup-env.sh` before running CMake.

### 4.2 Fedora

```bash
sudo dnf install gcc-c++ cmake git \
                 vulkan-loader-devel vulkan-headers vulkan-tools glslc \
                 glfw-devel glm-devel \
                 mesa-vulkan-drivers
```

### 4.3 Arch Linux / Manjaro

```bash
sudo pacman -S --needed base-devel cmake git \
                        vulkan-icd-loader vulkan-headers vulkan-tools shaderc \
                        glfw glm
```

Also install the Vulkan driver for your GPU: `vulkan-radeon` (AMD), `vulkan-intel` (Intel), or
`nvidia-utils` (NVIDIA).

> **GLFW from source:** if your distribution does not provide a GLFW package, CMake downloads
> and builds GLFW itself. That needs the X11/Wayland development headers. On Ubuntu/Debian:
> `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev wayland-protocols`.

### 4.4 Build and run

```bash
git clone https://github.com/joerg-christian/GeoLifeVis.git
cd GeoLifeVis
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/GeoLifeVis
```

To load a different data set file:

```bash
./build/GeoLifeVis /path/to/geoLifeDataSet.txt
```

Optional: install [Ninja](https://ninja-build.org/) (`ninja-build` package) and add `-G Ninja`
to the configure command for faster builds.

> **WSL:** GPU-accelerated Vulkan inside WSL2 is limited. For the best results, build and
> run natively on Windows or Linux.

---

## 5. macOS

macOS has no native Vulkan driver. Vulkan runs through **MoltenVK**, a translation layer to
Apple's Metal API. MoltenVK is included in the Vulkan SDK and in Homebrew. The program enables
the extensions MoltenVK needs (`VK_KHR_portability_enumeration` and `VK_KHR_portability_subset`)
automatically.

> **Status:** macOS support is **experimental**. The application builds against MoltenVK. The
> KDE compute shader synchronises work groups with spin-wait loops on a storage buffer, and
> Metal does not guarantee that this works. If the heat map does not update or the program
> hangs, please open an issue.

### 5.1 Install the prerequisites

1. **Xcode Command Line Tools** (compiler, Git):

   ```bash
   xcode-select --install
   ```

2. **CMake** via [Homebrew](https://brew.sh):

   ```bash
   brew install cmake
   ```

3. **Vulkan**. Choose one of the two options.

   **Option A: LunarG Vulkan SDK (recommended)**

   Download the macOS installer from <https://vulkan.lunarg.com/sdk/home#mac> and install it.
   To make the SDK available system-wide (in `/usr/local`), tick *"System Global Installation"*
   in the installer. Otherwise, run this in each new terminal before running CMake:

   ```bash
   source ~/VulkanSDK/<version>/setup-env.sh
   ```

   **Option B: Homebrew**

   ```bash
   brew install vulkan-headers vulkan-loader molten-vk shaderc glfw glm
   ```

   `shaderc` provides `glslc`. With Homebrew, pass `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`
   to CMake (shown below).

### 5.2 Build and run

```bash
git clone https://github.com/joerg-christian/GeoLifeVis.git
cd GeoLifeVis

# LunarG SDK:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# or Homebrew:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"

cmake --build build --parallel
./build/GeoLifeVis
```

**Xcode (optional):** generate a project with `cmake -S . -B build-xcode -G Xcode`, open
`build-xcode/GeoLifeVis.xcodeproj`, select the `GeoLifeVis` scheme and run it.

---

## 6. Getting the full GeoLife data set

The repository contains a **small sample** (`data/geoLifeDataSetSample.txt`, about 1 000
points) so that the program runs right after building. The full, preprocessed data set has
**18 238 686 points (~700 MB)**. That is too large for a Git repository, so you download it
separately:

1. Download `geoLifeDataSet.zip` from the **Releases** page of this repository.
2. Extract `geoLifeDataSet.txt` into the `data/` folder of the repository:

   ```
   data/geoLifeDataSet.txt
   ```

3. Run CMake again (configure and build). If `data/geoLifeDataSet.txt` exists, CMake copies it
   next to the executable instead of the sample:

   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```

Alternatively, keep the file anywhere and pass its path on the command line:

```bash
./build/GeoLifeVis /path/to/geoLifeDataSet.txt
```

Loading the full data set takes a while. The console shows the loading progress.

### File format

The preprocessed file is plain text:

- Lines 1–4: the geographic bounding box of the map: `minLat`, `maxLat`, `minLong`, `maxLong`
  (`39.75`, `40.1`, `116.15`, `116.6`, which is Beijing, matching `assets/textures/mapTexture.png`)
- Every following line is one GPS point, with tab-separated values:

  ```
  x   y   year   month   day   hour   minute   second   label
  ```

  `x`/`y` are the position normalised to `[0, 1]` within the bounding box, the time is UTC
  (the program converts it to Beijing time), and `label` is the means of transport:
  `0` unknown, `1` walk, `2` bike, `3` bus, `4` car/taxi, `5` subway, `6` train, `7` airplane,
  `8` boat, `9` motorcycle.

The original raw data is available from
[Microsoft Research](https://www.microsoft.com/en-us/download/details.aspx?id=52367).

---

## 7. Build options

Pass these options to the configure command with `-D<OPTION>=<VALUE>`:

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release` or `Debug` (single-config generators such as Makefiles/Ninja) |
| `GEOLIFE_USE_SYSTEM_DEPS` | `ON` | Use GLFW/GLM from the system if found. Set to `OFF` to always download them |
| `GEOLIFE_DATASET` | *(auto)* | Path of the data set file that is copied next to the executable |
| `CMAKE_PREFIX_PATH` | – | Extra search prefix for dependencies, for example `$(brew --prefix)` on macOS |

Example:

```bash
cmake -S . -B build -DGEOLIFE_USE_SYSTEM_DEPS=OFF -DGEOLIFE_DATASET=/data/geoLifeDataSet.txt
```

---

## 8. Debug builds and validation layers

In **Debug** builds (`NDEBUG` not defined) the program enables the Khronos validation layer
`VK_LAYER_KHRONOS_validation` and fails at startup if the layer is not installed:

- **Windows / macOS:** the layer is part of the Vulkan SDK.
- **Ubuntu/Debian:** `sudo apt install vulkan-validationlayers` (Ubuntu 22.04: `vulkan-validationlayers-dev`)
- **Fedora:** `sudo dnf install vulkan-validation-layers`
- **Arch:** `sudo pacman -S vulkan-validation-layers`

**Release** builds do not need the validation layers.

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

---

## 9. Troubleshooting

| Problem | Solution |
|---------|----------|
| `Could NOT find Vulkan` | Install the Vulkan SDK or `libvulkan-dev` / `vulkan-loader-devel`. On Windows, open a new terminal after installing the SDK so that `VULKAN_SDK` is set. On macOS, `source .../setup-env.sh` or pass `-DCMAKE_PREFIX_PATH="$(brew --prefix)"`. |
| `Neither glslc nor glslangValidator was found` | Install `glslc` (or `glslang-tools`/`shaderc`), or the Vulkan SDK. |
| `ERROR: validation layers requested, but not available!` | You built in Debug mode. Install the validation layers (see [section 8](#8-debug-builds-and-validation-layers)) or build in Release mode. |
| `Failed to find GPUs with Vulkan support!` / `Failed to find a suitable GPU!` | Install or update your GPU driver. Check with `vulkaninfo --summary`. On Linux, make sure the Mesa or NVIDIA Vulkan driver is installed. On macOS, make sure MoltenVK is installed. |
| `Failed to open file vert.spv` / `Failed to load texture image` | The program could not find its runtime files. Start it from the build output folder or rebuild so that the post-build copy step runs. |
| `No data points loaded from ...` | The data set file does not exist or is empty. Check the path, or see [section 6](#6-getting-the-full-geolife-data-set). |
| CMake cannot download GLFW/GLM | Check your internet connection or proxy, or install the system packages (`libglfw3-dev libglm-dev`, `glfw-devel glm-devel`, `brew install glfw glm`). |
| Linux: GLFW build fails with missing X11/Wayland headers | Install the headers listed in the note in [section 4.3](#43-arch-linux--manjaro), or install your distribution's GLFW package. |
| The window opens but the heat map is empty | You are probably using the small sample data set. Download the full data set ([section 6](#6-getting-the-full-geolife-data-set)). |
