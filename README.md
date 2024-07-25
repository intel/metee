# Intel(R) ME TEE Library

ME TEE Library is a C library to access CSE/CSME/GSC firmware via a mei interface.
ME TEE provides a single cross-platform API to access to MEI devices on Linux and Windows.
MEI TEE API simplify connection and communication with the mei device, and firmware status
registers retrieval.

## CMake Build

ME TEE library uses CMake for both Linux and Windows builds.

### Windows

From the "Developer Command Prompt for VS 2019" with C compiler and CMake component installed:

1. Go to sources directory: `cd <srcdir>`
2. Create `build` directory: `mkdir build`
3. Run `cmake -G "Visual Studio 16 2019" -A <build_arch> <srcdir>` from the `build` directory (best to set *build_arch* to Win32)
4. Run `cmake --build . --config Release --target package -j <nproc>` from the `build` directory to build an archive with all executables and libraries, *nproc* is the number of parallel threads in compilation, best to set to number of processor threads available

By default, CMake links with dynamic runtime (/MD), set BUILD_MSVC_RUNTIME_STATIC to ON to link with static runtime (/MT):
`cmake -G "Visual Studio 16 2019" -A <build_arch> -DBUILD_MSVC_RUNTIME_STATIC=ON <srcdir>`

### Linux

1. Create `build` directory
2. Run `cmake <srcdir>` from the `build` directory
3. Run `make -j$(nproc) package` from the `build` directory to build .deb and .rpm packages and .tgz archive


## Meson Build

ME TEE library also supports Meson for both Linux and Windows builds.

### General Setup

```sh
meson setup build/
meson configure -Dbuildtype=debug/release build
ninja -v -C build/
```

### Windows Visual Studio

In order to use Visual Studio project meson requires to run under Visual Studio Command Prompt.
In case of Visual Studio 2019, you can use either *"x64 Native Tools Command Prompt for VS 2019"*
or under powershell enter VisualStudio DevShell and then just continue with the [general setup](#general-setup)

#### Powershell example setting:

```powershell
 $installPath = &"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -version 16.0 -property installationpath
 Import-Module (Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
 Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation
```

## Thread safety

The library supports multithreading but is not thread-safe.
Every thread should either initialize and use its own handle
or a locking mechanism should be implemented by the caller to ensure
that only one thread uses the handle at any time.
The only exception is ability to call Disconnect to exit from read
blocked on another thread.