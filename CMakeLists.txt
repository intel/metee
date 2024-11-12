# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2024 Intel Corporation
cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
project(metee)

option(BUILD_DOCS "Build docs" NO)
option(BUILD_TEST "Build self-test" NO)
option(BUILD_SAMPLES "Build samples" NO)
option(BUILD_MSVC_RUNTIME_STATIC "Build with static runtime libraries on MSVC"
       NO
)
option(BUILD_SHARED_LIBS "Build shared library" NO)
option(CONSOLE_OUTPUT "Push debug and error output to console (instead of syslog)" NO)

include(GNUInstallDirs)

set(LICENSE Apache)
include(version.cmake)

if(WIN32)
  include(win32.cmake)
else(WIN32)
  include(linux.cmake)
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/metee.h)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${TEE_VERSION_STRING})
set_target_properties(
  ${PROJECT_NAME} PROPERTIES SOVERSION ${TEE_VERSION_STRING}
)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC include)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

install(
  TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  PUBLIC_HEADER DESTINATION include
)

# Documentation
if(BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/include/Doxyfile.in
      ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY
    )

    add_custom_target(
      doc ALL
      ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating documentation with Doxygen"
      VERBATIM
    )

    if(UNIX)
      install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man/man3
            DESTINATION ${CMAKE_INSTALL_MANDIR}
      )
      install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
              DESTINATION ${CMAKE_INSTALL_DOCDIR}
      )
    else()
      install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION doc)
    endif()
  endif()
endif()

# CPack
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${TEE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${TEE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${TEE_VERSION_PATCH})
set(CPACK_PACKAGE_VENDOR "Intel, Inc")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ME TEE Interface Library")
set(CPACK_PACKAGE_CONTACT "linux-mei@linux.intel.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE ${PROJECT_SOURCE_DIR}/DESCRIPTION)
set(CPACK_SOURCE_GENERATOR "TBZ2;ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME ${PROJECT_NAME}-${TEE_VERSION_STRING})
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

# RPM Specific
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE ${LICENSE})
set(CPACK_RPM_PACKAGE_GROUP "Development")

# Debian Specific
set(CPACK_DEBIAN_PACKAGE_VERSION 1)
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")

if(UNIX)
  set(CPACK_GENERATOR "TGZ")

  find_program(RPM_EXE rpmbuild)
  if(RPM_EXE)
    list(APPEND CPACK_GENERATOR "RPM")
  endif(RPM_EXE)

  find_program(DEB_EXE debuild)
  if(DEB_EXE)
    list(APPEND CPACK_GENERATOR "DEB")
  endif(DEB_EXE)
else(UNIX)
  set(CPACK_GENERATOR "ZIP")
endif(UNIX)

include(CPack)

if(BUILD_TEST)
  add_subdirectory(tests)
endif(BUILD_TEST)
if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif(BUILD_SAMPLES)
