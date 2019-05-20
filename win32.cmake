# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2014-2019 Intel Corporation

if (BUILD_MSVC_RUNTIME_STATIC)
  add_compile_options (/MT$<$<CONFIG:Debug>:d>)
endif (BUILD_MSVC_RUNTIME_STATIC)

set(TEE_SOURCES src/Windows/metee_win.c src/Windows/metee_winhelpers.c)

add_library(${PROJECT_NAME} STATIC ${TEE_SOURCES})

target_link_libraries(${PROJECT_NAME}
  CfgMgr32.lib
)
target_compile_definitions (${PROJECT_NAME} PRIVATE UNICODE)
target_compile_definitions (${PROJECT_NAME} PRIVATE _UNICODE)

#Secure compile flags
add_definitions ("/GS /sdl")

#More warnings and warning-as-error
target_compile_options (${PROJECT_NAME} PRIVATE /W4)
target_compile_options (${PROJECT_NAME} PRIVATE /WX)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/Zi")
set_target_properties(${PROJECT_NAME}
  PROPERTIES
  PDB_NAME "${PROJECT_NAME}"
  PDB_NAME_DEBUG "${PROJECT_NAME}"
  COMPILE_PDB_NAME "${PROJECT_NAME}"
  COMPILE_PDB_NAME_DEBUG "${PROJECT_NAME}"
)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
  PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

get_target_property(t_pdb_name ${PROJECT_NAME} COMPILE_PDB_NAME)
get_target_property(t_pdb_name_debug ${PROJECT_NAME} COMPILE_PDB_NAME_DEBUG)
get_target_property(t_pdb_output_directory ${PROJECT_NAME} PDB_OUTPUT_DIRECTORY)

install(FILES
  "${t_pdb_output_directory}/\${CMAKE_INSTALL_CONFIG_NAME}/$<$<CONFIG:Debug>:${t_pdb_name_debug}>$<$<NOT:$<CONFIG:Debug>>:${t_pdb_name}>.pdb"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
