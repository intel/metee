# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2020 Intel Corporation
cmake_minimum_required(VERSION 3.5)
project(metee_sample)

add_executable(metee-mkhi metee_mkhi.c)

target_link_libraries(metee-mkhi metee)

install(TARGETS metee-mkhi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX)
  add_executable(metee-connect metee_connect.c meiuuid.c)

  target_link_libraries(metee-connect metee)

  install(TARGETS metee-connect RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif(UNIX)
