## [3.0.0]

**Note:** below changes break API and ABI compatibility with older library versions.

### Changed
 - Windows: Add separate TeeInitGUID API to init library by using device class GUID.

### Fixed
 - Fix miscellaneous code issues in samples.
 - Windows: return right size of processed data.
 - Windows: fix connect to the externally provided guid.
 - Windows: enlarge debug output buffer.
 - Modernize CMake configuration.
 - Fix spelling mistakes all over the library.
 - Windows: check return pointers for NULL before write.
 - Windows: fix reconnect if underlying client was disconnected from FW side.
 - Doxygen documentation cleaned up and expanded.
 - Linux: import libmei 1.4 code.

### Added
 - Windows: ability to connect to device by path.
 - API to use externally opened file handle.
 - Tests with long buffers.
 - Ability to build dynamic library.
 - Support for meson build system.
 - Option to write output to console instead of Syslog/DbgView

## [2.2.2]

### Added

### Changed

### Fixed
 - Windows: fix package target in cmake.

## [2.2.1]

### Added
 - Enable gitlint, cmake-format, codespell checking.
 - Added .gitattributes.

### Changed
 - Reformat cmake files
 - Update .gitignore
 - convert CHANGELOG to Markdown format

### Fixed
 - Add -D_GNU_SOURCE for strdup and other functions.
 - Fix spelling errors in the library.
 - Windows: enlarge debug output buffer.
 - Windows: return right size of processed data.
 - Windows: fix connect to the externally provided guid.
 - Fix debug print.
 - Fix miscellaneous code issues.


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
