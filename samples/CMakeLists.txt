# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2024 Intel Corporation
cmake_minimum_required(VERSION 3.15)
project(metee_sample)

add_executable(metee-mkhi metee_mkhi.c)
target_link_libraries(metee-mkhi metee)
if(MSVC)
    target_compile_definitions(metee-mkhi PRIVATE -D_CRT_SECURE_NO_WARNINGS)
endif()
install(TARGETS metee-mkhi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(metee-gsc metee_gsc.c)
target_link_libraries(metee-gsc metee)
install(TARGETS metee-gsc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX)
  add_executable(metee-connect metee_connect.c meiuuid.c)

  target_link_libraries(metee-connect metee)

  install(TARGETS metee-connect RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif(UNIX)

add_executable(metee-trc metee_trc.c)
target_link_libraries(metee-trc metee)
install(TARGETS metee-trc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(metee-basic metee_basic.c)
target_link_libraries(metee-basic metee)
install(TARGETS metee-basic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_SHARED_LIBS AND WIN32)
  add_subdirectory(csharp)
endif(BUILD_SHARED_LIBS AND WIN32)