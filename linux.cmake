# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2020 Intel Corporation
set(TEE_SOURCES src/linux/metee_linux.c src/linux/mei.c)

add_library(${PROJECT_NAME} ${TEE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src/linux)
target_compile_definitions(${PROJECT_NAME} PRIVATE
			   $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL>
			   $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL_EXPORT>
)

# More warnings and warning-as-error
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE -Wall -Werror
  PRIVATE -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wformat
          -Wformat-security
)
set_target_properties(${PROJECT_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

if(NOT DEFINED CONSOLE_OUTPUT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSYSLOG)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -D_GNU_SOURCE)
