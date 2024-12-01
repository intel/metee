## [4.3.0]

### Fixed
 - Windows: process access denied on open
 - UEFI: Add driver logic for fixed clients

### Added
 - Windows: add resource
 - cmake: add 32 bit release preset
 - add getters for maxMsgLen and prtocolVer

## [4.2.1]

### Fixed
 - EFI: fix open multiple connections to the different clients
 - Linux: import libmei 1.6.3: fix for traffic dump in release mode and build on 32bit Linux.

## [4.2.0]

**Note:** ABI breakage in this version, struct tee_device_address was expanded

### Fixed
 - tests: make tests robust
 - Linux: fix variable type
 - Windows: init read/write timeout to INT_MAX
 - tests: add connect retries

### Added
 - samples: add basic sample
 - UEFI: initial implementation
 - CMake: Windows: add static build

### Changed
 - Linux: pull libmei 1.6.1
 - CMake: minimum CMake to 3.15 and fix static build

## [4.1.0]

### Fixed
 - Linux: add newlines to messages
 - Linux: cancel waiting operation on disconnect
 - Windows: rewrite overlapped operations
 - Linux: define GUID in more compatible way
 - tests: fix GetDriverVersion test
 - tests: fix crash in failed 1000 handles test
 - Windows: do not retake last error from ioctl
 - Windows: demote print to debug
 - CMake: use PDB path generator expression
 - Linux: fix timeout and signedness

### Added
 - README: add thread safety section
 - tests: add test for interrupted read
 - CMake: enable several extended compilation warnings

### Changed
 - Linux: pull libmei 1.6.0

### Removed
 - Linux: drop unused define
 - Windows: remove unused layers
 - Doxygen: disable HTML timestamps

## [4.0.0]

**Note:** Backward compatible API change

### Fixed
 - Windows: initilaize variables
 - conan: support higher version of the Linux compilers
 - linux: fix print in get handle
 - linux: do not set log if init failed
 - Windows: fix close on exit
 - Windows: add missing newline
 - samples: return log level to original
 - Linux: fix timeout treatment
 - tests: disconnect at the end of test
 - log: demote FUNC_ENTRY/EXIT to debug

### Added
 - add CONTRIBUTING.md file
 - add GetTRC API
 - add init api with all parameters
 - add option for log callback
 - git: add full VS gitignore
 - CMake: add preset for full debug build

### Changed
 - Linux: pull libmei 1.5.6
 - samples: use stdbool on windows
 - windows: replace spaces with tabs

### Removed
 - CMake: disable doc build by default
 - CMake: drop system ver requirement

## [3.2.4]

### Fixed
 - Linux: pull in libmei 1.5.4 with reconnect-on-error fix

### Added
 - README: add nproc explanation, refine grammar and formatting

## [3.2.3]

### Fixed
 - tests: fix windows tests
 - README: update Windows build instructions

## [3.2.2]

### Fixed
 - Linux: fix return value treatment of mei_set_log_level

## [3.2.1]

### Added
 - set security policy for the project

### Fixed
 - Windows: fix wrong return status
 - Windows: move to_int inline function to metee_win.h
 - Windows: remove unused labels

## [3.2.0]

### Added
 - Linux: pull in libmei 1.5.3
 - implement log level APIs
 - Windows: remove redundant function WaitForOperationEnd
 - Windows: add license for MSFT WDK samples

## [3.1.6]

### Fixed
 - CMake: fix secure compiler options
 - Windows: convert win32 errors to library errors
 - doc: update doxygen config
 - Linux: pull in libmei 1.5.2
 - Windows: return with error on memcpy failure
 - Windows: Drop non needed variable initialization on declaration

### Added
 - CMake: add preset to compile all code

## [3.1.5]

### Fixed
 - Linux: return better errors from TeeInit
 - samples: mkhi: account for errors from init
 - tests: update tests for fixed linux init

## [3.1.4]

### Fixed
 - Linux: replace select with poll
 - Linux: use positive errno number in strerror
 - conan: drop cxx requirement

### Changed
 - build: take gtest 1.12.1
 - tests: use fixed gtest version in download

### Added
 - CMake: add Windows presets
 - test: add test for big file descriptor

## [3.1.3]

### Fixed
 - samples: fix assignment and add explicit type-cast
 - Linux: pull-in libmei 1.5.1 with fix for reconnect if client not found

## [3.1.2]

### Fixed

 - tests: fix gtest branch name
 - CHANGELOG: fix the indentation

## [3.1.1]

### Fixed

 - Windows: push DebugPrint down
 - Windows: tests: disable pending write test on Windows
 - doc: add missing return values documentation

### Changed

 - CMake: make docs optional


## [3.1.0]

**Note:** Backward compatible API change

### Added

 - Add TEE_PERMISSION_DENIED error code
 - Add stress tests

## [3.0.1]

### Added
 - add SPDX license also on .cmake-format config file
 - samples: add GSC sample
 - Build: add conan packaging.

## Fixed
 - test: initialize deviceHandle in constructor
 - README.md and more verbose description
 - CMake: add secure compile and link flag

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
