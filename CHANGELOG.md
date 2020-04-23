## [2.2.0]

### Added
 - API to retrieve FW status registers

### Changed

### Fixed


## [2.1.1]

### Added
 - Add tests and samples to install package.

### Changed
 - Linux: Set default device to /dev/mei0 as all modern Linux kernels sports /dev/meiX char device nodes.

### Fixed
 - Linux: include select.h to fix build with musl libc.


## [2.1.0]

### Added
 - Add DEFINE_GUID macro in Linux header.

### Changed
 - Use name GUID in all OSs.

### Fixed
