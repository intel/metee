# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020 Intel Corporation
from conans import ConanFile, CMake, tools
from conans.tools import load
import os

class MeteeConan(ConanFile):
    name = "metee"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    generators = "cmake", "visual_studio"
    exports_sources = "*"

    def set_version(self):
        content = load(os.path.join(self.recipe_folder, "VERSION"))
        self.version = content

    def build(self):
        cmake = CMake(self)
        if self.settings.os == "Windows":
             cmake.definitions["BUILD_MSVC_RUNTIME_STATIC"]="on"
        cmake.configure(source_folder="")
        cmake.build()

    def package(self):
        self.copy("*.h", dst="include")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so*", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["metee"]
