# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2019 Intel Corporation
set(TEE_SOURCES src/linux/metee_linux.c src/linux/mei.c)

add_library(${PROJECT_NAME} STATIC ${TEE_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE src/linux
)

#More warnings and warning-as-error
target_compile_options(${PROJECT_NAME}
    PRIVATE -Wall -Werror
    PRIVATE -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wformat -Wformat-security
)

if(NOT DEFINED CONSOLE_OUTPUT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DSYSLOG)
endif()
