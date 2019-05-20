# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2019 Intel Corporation
set(TEE_SOURCES src/linux/metee_linux.c src/linux/mei.c)

add_library(${PROJECT_NAME} STATIC ${TEE_SOURCES})

target_include_directories(${PROJECT_NAME}
  PRIVATE src/linux
)

#More warnings and warning-as-error
target_compile_options (${PROJECT_NAME} PRIVATE -Wall)
target_compile_options (${PROJECT_NAME} PRIVATE -Werror)

if(NOT DEFINED CONSOLE_OUTPUT)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSLOG")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSYSLOG")
endif(NOT DEFINED CONSOLE_OUTPUT)

#Secure compile flags
add_definitions (-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wformat -Wformat-security)
