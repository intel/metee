# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2020-2022 Intel Corporation
from conans import ConanFile, CMake, tools
from conans.tools import load
from conans.model.version import Version
import os

class MeteeConan(ConanFile):
    name = "metee"
    license = "SPDX-License-Identifier: Apache-2.0"
    url = "https://github.com/intel/metee"
    description = "Cross-platform access library for Intel(R) CSME HECI interface."
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared": False}
    generators = "cmake", "visual_studio"
    exports_sources = "*"

    def configure(self):
        del self.settings.compiler.libcxx

    def package_id(self):
        v = Version(str(self.settings.compiler.version))
        if self.settings.compiler == "gcc" and (v >= "6" and v <= "11"):
            self.info.settings.compiler.version = "GCC version between 6 and 11"
        if self.settings.compiler == "clang" and (v >= "6" and v <= "11"):
            self.info.settings.compiler.version = "clang version between 6 and 11"

    def set_version(self):
        content = load(os.path.join(self.recipe_folder, "VERSION"))
        self.version = content

    def _configure_cmake(self):
        cmake = CMake(self)
        if self.settings.os == "Windows" and "MT" in self.settings.compiler.runtime:
            cmake.definitions["BUILD_MSVC_RUNTIME_STATIC"]="on"
        cmake.configure(source_folder="")
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["metee"]
