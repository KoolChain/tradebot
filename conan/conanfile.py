from conans import ConanFile, CMake, tools

from os import path


class TradebotConan(ConanFile):
    """ Conan recipe for Tradebot """

    name = "tradebot"
    license = "MIT License"
    url = "https://github.com/Adnn/tradebot"
    description = "Trading, without the emotion."
    #topics = ("", "", ...)
    settings = ("os", "compiler", "build_type", "arch")
    options = {
        "build_tests": [True, False],
        "shared": [True, False],
        "visibility": ["default", "hidden"],
    }
    default_options = {
        "build_tests": False,
        "shared": False,
        "visibility": "hidden"
    }

    requires = (
        ("boost/1.76.0",),
        ("cpr/1.6.0",),
        ("jsonformoderncpp/3.7.0",),
        ("openssl/1.1.1j",),
        ("spdlog/1.8.5",),
        ("sqlite3/3.35.5",),
    )


    build_requires = ("cmake/3.16.9",)

    build_policy = "missing"
    generators = "cmake_paths", "cmake", "cmake_find_package"
    revision_mode = "scm"

    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto",
        "submodule": "recursive",
    }

    def configure(self):
        if tools.is_apple_os(self.settings.os):
            self.options["cpr"].with_openssl = False


    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.definitions["BUILD_tests"] = self.options.build_tests
        cmake.definitions["CMAKE_CXX_VISIBILITY_PRESET"] = self.options.visibility
        cmake.definitions["CMAKE_PROJECT_Tradebot_INCLUDE"] = \
            path.join(self.source_folder, "cmake", "conan", "customconan.cmake")
        cmake.configure()
        return cmake


    def build(self):
        cmake = self._configure_cmake()
        cmake.build()


    def package(self):
        cmake = self._configure_cmake()
        cmake.install()


    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
