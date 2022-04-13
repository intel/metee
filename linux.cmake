# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2022 Intel Corporation
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
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE -Wall -Werror

)
# Security options
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wformat
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
