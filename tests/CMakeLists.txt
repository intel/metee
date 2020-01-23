# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2020 Intel Corporation
cmake_minimum_required(VERSION 3.1)
project(metee_test)

set(CMAKE_CXX_STANDARD 11)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest-src)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/googletest-src
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  )
else(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest-src)
  # Download and unpack googletest at configure time
  configure_file(gtest.cmake.in googletest-download/CMakeLists.txt)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
  )
  if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
  )
  if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
  endif()
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest-src)

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE
)

# Add googletest directly to our build. This defines the gtest and gtest_main
# targets.
add_subdirectory(
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
  ${CMAKE_CURRENT_BINARY_DIR}/googletest-build EXCLUDE_FROM_ALL
)

add_executable(${PROJECT_NAME} Main.cpp metee_test.cpp)

target_link_libraries(${PROJECT_NAME} metee gtest_main)

target_include_directories(${PROJECT_NAME}
  PRIVATE ${CMAKE_SOURCE_DIR}/src/Windows
)

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

