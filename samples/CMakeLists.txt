# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2026 Intel Corporation
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

if(EFI)
  set(_metee_gsc_stage "${CMAKE_BINARY_DIR}/MeTeePkg/Samples/MeTeeGsc")
  configure_file(
    "${PROJECT_SOURCE_DIR}/metee_gsc.c"
    "${_metee_gsc_stage}/metee_gsc.c" COPYONLY
  )
  configure_file(
    "${PROJECT_SOURCE_DIR}/MeTeeGscMain.c"
    "${_metee_gsc_stage}/MeTeeGscMain.c" COPYONLY
  )
  configure_file(
    "${PROJECT_SOURCE_DIR}/MeTeeGsc.inf.in"
    "${_metee_gsc_stage}/MeTeeGsc.inf"
  )
endif(EFI)

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

if(EFI)
  set(_metee_basic_stage "${CMAKE_BINARY_DIR}/MeTeePkg/Samples/MeTeeBasic")
  configure_file(
    "${PROJECT_SOURCE_DIR}/metee_basic.c"
    "${_metee_basic_stage}/metee_basic.c" COPYONLY
  )
  configure_file(
    "${PROJECT_SOURCE_DIR}/MeTeeBasicMain.c"
    "${_metee_basic_stage}/MeTeeBasicMain.c" COPYONLY
  )
  configure_file(
    "${PROJECT_SOURCE_DIR}/MeTeeBasic.inf.in"
    "${_metee_basic_stage}/MeTeeBasic.inf"
  )
endif(EFI)

add_executable(meteepp-basic meteepp_basic.cpp)
target_link_libraries(meteepp-basic metee)
install(TARGETS meteepp-basic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_SHARED_LIBS AND WIN32)
  add_subdirectory(csharp)
endif(BUILD_SHARED_LIBS AND WIN32)
