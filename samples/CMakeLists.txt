# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2020 Intel Corporation
cmake_minimum_required(VERSION 3.5)
project(metee_sample)

add_executable(metee-mkhi metee_mkhi.c)
target_link_libraries(metee-mkhi metee)
install(TARGETS metee-mkhi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(BUILD_MSVC_RUNTIME_STATIC)
  target_compile_options(metee-mkhi PRIVATE /MT$<$<CONFIG:Debug>:d>)
endif()

add_executable(metee-gsc metee_gsc.c)
target_link_libraries(metee-gsc metee)
install(TARGETS metee-gsc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(BUILD_MSVC_RUNTIME_STATIC)
  target_compile_options(metee-gsc PRIVATE /MT$<$<CONFIG:Debug>:d>)
endif()

if(UNIX)
  add_executable(metee-connect metee_connect.c meiuuid.c)

  target_link_libraries(metee-connect metee)

  install(TARGETS metee-connect RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif(UNIX)
