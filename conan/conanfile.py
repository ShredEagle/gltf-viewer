from conans import ConanFile, tools
from conan.tools.cmake import CMake

from os import path


class GltfViewerConan(ConanFile):
    """ Conan recipe for Gltf-viewer """

    name = "gltf-viewer"
    license = "MIT License"
    url = "https://github.com/Adnn/gltf-viewer"
    description = "Custom glTF asset viewer."
    topics = ("CG", "3D", "assets")
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
        ("boost/1.77.0"),
        ("imgui/1.87"),

        ("graphics/335d54bdbb@adnn/develop"),
        ("handy/bfed056640@adnn/develop"),
        ("math/4234fd5aaf@adnn/develop"),
    )

    # Note: It seems conventionnal to add CMake build requirement
    # directly to the build profile.
    #build_requires = ()

    build_policy = "missing"
    generators = "cmake_paths", "cmake_find_package", "CMakeToolchain"

    # Otherwise, conan removes the imported imgui backends after build()
    # they are still required for the CMake config phase of package()
    keep_imports = True

    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto",
        "submodule": "recursive",
    }

    def _generate_cmake_configfile(self):
        """ Generates a conanuser_config.cmake file which includes the file generated by """
        """ cmake_paths generator, and forward the remaining options to CMake. """
        with open("conanuser_config.cmake", "w") as config:
            config.write("message(STATUS \"Including user generated conan config.\")\n")
            # avoid path.join, on Windows it outputs '\', which is a string escape sequence.
            config.write("include(\"{}\")\n".format("${CMAKE_CURRENT_LIST_DIR}/conan_paths.cmake"))
            config.write("set({} {})\n".format("BUILD_tests", self.options.build_tests))


    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake


    def configure(self):
        tools.check_min_cppstd(self, "20")


    def generate(self):
           self._generate_cmake_configfile()


    def imports(self):
        # see: https://blog.conan.io/2019/06/26/An-introduction-to-the-Dear-ImGui-library.html
        # the imgui package is designed this way: consumer has to import desired backends.
        self.copy("imgui_impl_glfw.cpp",         src="./res/bindings", dst="conan_imports/imgui_backends")
        self.copy("imgui_impl_opengl3.cpp",      src="./res/bindings", dst="conan_imports/imgui_backends")
        self.copy("imgui_impl_glfw.h",           src="./res/bindings", dst="conan_imports/imgui_backends")
        self.copy("imgui_impl_opengl3.h",        src="./res/bindings", dst="conan_imports/imgui_backends")
        self.copy("imgui_impl_opengl3_loader.h", src="./res/bindings", dst="conan_imports/imgui_backends")


    def build(self):
        cmake = self._configure_cmake()
        cmake.build()


    def package(self):
        cmake = self._configure_cmake()
        cmake.install()


    def package_info(self):
        pass
