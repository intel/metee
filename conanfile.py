# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020 Intel Corporation
from conans import ConanFile, CMake, tools
from conans.tools import load
from conans.model.version import Version
import os

class MeteeConan(ConanFile):
    name = "metee"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    generators = "cmake", "visual_studio"
    exports_sources = "*"

    def package_id(self):
        v = Version(str(self.settings.compiler.version))
        if self.settings.compiler == "gcc" and (v >= "6" and v <= "11"):
            self.info.settings.compiler.version = "GCC version between 6 and 11"
        if self.settings.compiler == "clang" and (v >= "6" and v <= "11"):
            self.info.settings.compiler.version = "clang version between 6 and 11"

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
        self.copy("include/metee.h", dst="include", keep_path=False)
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so*", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["metee"]
