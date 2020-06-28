# Intel(R) ME TEE Library

Cross-platform access library for Intel(R) CSME HECI interface.

## CMake Build

ME TEE library uses CMake for both Linux and Windows builds.

### Windows

1. Create `build` directory
2. Run `cmake -G "Visual Studio 15 2017" -A <Build_arch> <srcdir>` from the `build` directory (best to set build_arch to Win32)
3. Run `cmake --build . --config Release --target package -j <nproc>` from the `build` directory to build an archive with all executables and libraries

By default CMake links with dynamic runtime (/MD), set BUILD_MSVC_RUNTIME_STATIC to ON to link with static runtime (/MT)

### Linux

1. Create `build` directory
2. Run `cmake <srcdir>` from the `build` directory
3. Run `make -j$(nproc) package` from the `build` directory to build .deb and .rpm packages and .tgz archive


## Meson Build

ME TEE library also supports Meson  for both Linux and Windows builds.

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
