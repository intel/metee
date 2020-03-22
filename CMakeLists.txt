# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2019 Intel Corporation
cmake_minimum_required (VERSION 3.1)
project(metee)

option (BUILD_TEST "Build self-test" NO)
option (BUILD_SAMPLES "Build samples" NO)
option (BUILD_MSVC_RUNTIME_STATIC
    "Build with static runtime libraries on MSVC" NO)

include (GNUInstallDirs)

if(WIN32)
  include(win32.cmake)
else(WIN32)
  include(linux.cmake)
endif(WIN32)

set(LICENSE Apache)
include (version.cmake)

set_target_properties(
	${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/metee.h)
set_target_properties(
	${PROJECT_NAME} PROPERTIES VERSION ${TEE_VERSION_STRING})
set_target_properties(
	${PROJECT_NAME} PROPERTIES SOVERSION ${TEE_VERSION_STRING})
set_target_properties(
	${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME}
  PUBLIC include
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  PUBLIC_HEADER DESTINATION include
)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
set(DOXYGEN_INPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating documentation with Doxygen" VERBATIM)

add_dependencies(${PROJECT_NAME} doc)

if(UNIX)
install(DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/man/man3
  DESTINATION ${CMAKE_INSTALL_MANDIR}
)
install(DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/html
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
else(UNIX)
install(DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/html
  DESTINATION doc
)
endif(UNIX)

endif(DOXYGEN_FOUND)

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

#Debian Specific
set(CPACK_DEBIAN_PACKAGE_VERSION 1)
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")

if(UNIX)
  set(CPACK_GENERATOR "TGZ")
  find_program(RPM_EXE rpmbuild)
  find_program(DEB_EXE debuild)
  if (RPM_EXE)
    list (APPEND CPACK_GENERATOR "RPM")
  endif (RPM_EXE)
  if (DEB_EXE)
    list (APPEND CPACK_GENERATOR "DEB")
  endif (DEB_EXE)
else(UNIX)
  set(CPACK_GENERATOR "ZIP")
endif(UNIX)

include(CPack)

if (BUILD_TEST)
  add_subdirectory (tests)
endif (BUILD_TEST)
if (BUILD_SAMPLES)
  add_subdirectory (samples)
endif (BUILD_SAMPLES)
