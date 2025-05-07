# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Intel Corporation
cmake_minimum_required(VERSION 3.8)

project(Intel.Security.Metee VERSION 0.1.0 LANGUAGES CSharp)

include(CSharpUtilities)

configure_file(AssemblyInfo.cs.in ${CMAKE_CURRENT_SOURCE_DIR}/Properties/AssemblyInfo.cs)

add_library(Intel.Security.Metee
     Metee.cs
     Properties/AssemblyInfo.cs
)

add_dependencies(Intel.Security.Metee metee)

set_property(TARGET Intel.Security.Metee PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.6.1")

set_property(TARGET Intel.Security.Metee PROPERTY VS_DOTNET_REFERENCES
    "System"
    "System.Core"
    "System.Data.DataSetExtensions"
    "Microsoft.CSharp"
    "System.Data"    
)

install(TARGETS Intel.Security.Metee RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})