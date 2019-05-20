Intel(R) ME TEE Library
=======

Cross-platform access library for Intel(R) CSME HECI interface.

## Build

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

