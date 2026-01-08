# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2026 Intel Corporation
set(TEE_SOURCES src/linux/metee_linux.c src/linux/mei.c)

add_library(${PROJECT_NAME} ${TEE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src/linux)
target_compile_definitions(${PROJECT_NAME} PRIVATE
			   $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL>
			   $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL_EXPORT>
)

# Check for new IOCTLs
include(CheckSymbolExists)
check_symbol_exists(IOCTL_MEI_CONNECT_CLIENT_VTAG linux/mei.h HAVE_VTAG)
if(NOT HAVE_VTAG)
  include_directories(BEFORE "src/linux/include")
endif()

# More warnings and warning-as-error
set(COMPILE_OPTIONS
  -Wall -Werror)
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wshadow WARNING_SHADOW)
if(WARNING_SHADOW)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wshadow)
endif()
check_c_compiler_flag(-Wnull-dereference WARNING_NULL_DEREFERENCE)
if(WARNING_NULL_DEREFERENCE)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wnull-dereference)
endif()
check_c_compiler_flag(-Wfloat-conversion WARNING_FLOAT_CONVERSION)
if(WARNING_FLOAT_CONVERSION)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wfloat-conversion)
endif()
check_c_compiler_flag(-Wsign-conversion WARNING_SIGN_CONVERSION)
if(WARNING_SIGN_CONVERSION)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wsign-conversion)
endif()
check_c_compiler_flag(-Wstringop-truncation WARNING_STRINGOP_TRUNCATION)
if(WARNING_STRINGOP_TRUNCATION)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wstringop-truncation)
endif()
check_c_compiler_flag(-Wjump-misses-init WARNING_JUMP_MISSES_INIT)
if(WARNING_JUMP_MISSES_INIT)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wjump-misses-init)
endif()
check_c_compiler_flag(-Wunsuffixed-float-constants WARNING_UNSUFFIXED_FLOAT_CONSTANTS)
if(WARNING_UNSUFFIXED_FLOAT_CONSTANTS)
  set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -Wunsuffixed-float-constants)
endif()
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE ${COMPILE_OPTIONS})
# Security options
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -O2 -Wformat
          -Wformat-security
  PRIVATE $<$<C_COMPILER_ID:GNU>:-fno-strict-overflow>
  PRIVATE $<$<C_COMPILER_ID:GNU>:-fno-delete-null-pointer-checks>
  PRIVATE -fwrapv
)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack -z relro -z now")
set_target_properties(${PROJECT_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

if(NOT CONSOLE_OUTPUT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSYSLOG)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -D_GNU_SOURCE)
if(ANDROID)
  target_link_libraries(${PROJECT_NAME} PRIVATE log)
endif()
