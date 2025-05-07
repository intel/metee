# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Intel Corporation
cmake_minimum_required(VERSION 3.8)

project(csharp-example VERSION 0.1.0 LANGUAGES CSharp)

include(CSharpUtilities)

configure_file(AssemblyInfo.cs.in ${CMAKE_CURRENT_SOURCE_DIR}/Properties/AssemblyInfo.cs)

add_executable(csharp-example
     Program.cs
     Properties/AssemblyInfo.cs
)

target_link_libraries(csharp-example Intel.Security.Metee)

set_property(TARGET csharp-example PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.6.1")

set_property(TARGET csharp-example PROPERTY VS_DOTNET_REFERENCES
    "System"
    "System.Core"
    "System.Data.DataSetExtensions"
    "Microsoft.CSharp"
    "System.Data"
)

install(TARGETS csharp-example RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})