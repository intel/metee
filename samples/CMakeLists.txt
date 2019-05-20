# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2019 Intel Corporation
cmake_minimum_required (VERSION 3.1)
project (metee_sample)

add_executable (metee-mkhi
  metee_mkhi.c
)

target_link_libraries (metee-mkhi
  metee
)

if(UNIX)
add_executable (metee-connect
  metee_connect.c
  meiuuid.c
)

target_link_libraries (metee-connect
  metee
)
endif(UNIX)