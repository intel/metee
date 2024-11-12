# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2024 Intel Corporation

if(BUILD_MSVC_RUNTIME_STATIC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(TEE_SOURCES
    src/Windows/metee_win.c
    src/Windows/metee_winhelpers.c
    ${PROJECT_BINARY_DIR}/metee.rc
)

add_library(${PROJECT_NAME} ${TEE_SOURCES})

target_link_libraries(${PROJECT_NAME} CfgMgr32.lib)
target_compile_definitions(${PROJECT_NAME} PRIVATE
                           UNICODE _UNICODE
                           $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL>
                           $<$<BOOL:BUILD_SHARED_LIBS>:METEE_DLL_EXPORT>
)
if(NOT CONSOLE_OUTPUT)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSYSLOG)
endif()

configure_file (
    "${PROJECT_SOURCE_DIR}/src/Windows/metee.rc.in"
    "${PROJECT_BINARY_DIR}/metee.rc"
)

# Secure compile flags
target_compile_options(${PROJECT_NAME}
                       PRIVATE /GS /sdl)

# More warnings and warning-as-error
target_compile_options(${PROJECT_NAME}
                       PRIVATE /W4 /WX
)

set_target_properties(${PROJECT_NAME}
   PROPERTIES COMPILE_FLAGS "/Zi"
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES PDB_NAME "${PROJECT_NAME}"
             PDB_NAME_DEBUG "${PROJECT_NAME}"
             COMPILE_PDB_NAME "${PROJECT_NAME}"
             COMPILE_PDB_NAME_DEBUG "${PROJECT_NAME}"
)

if(${BUILD_SHARED_LIBS})
  install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
else()
  install(FILES "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.pdb" DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
endif()