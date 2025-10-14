from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, get, load
import os


def get_version():
    """Read version from VERSION file"""
    try:
        return load(None, os.path.join(os.path.dirname(__file__), "VERSION")).strip()
    except Exception:
        return "1.0.0"  # Fallback version


class CNanoLogConan(ConanFile):
    name = "cnanolog"
    version = get_version()
    license = "MIT"
    author = "Zach <your-email@example.com>"
    url = "https://github.com/zachgenius/CNanoLog"
    description = "Ultra-fast, low-latency binary logging library for C11 with nanosecond precision and lock-free performance"
    topics = ("logging", "performance", "binary-logging", "c11", "low-latency")

    # Settings
    settings = "os", "compiler", "build_type", "arch"

    # Options
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tools": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": True,
    }

    # Sources
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "include/*",
        "tools/*",
        "LICENSE",
    )

    def config_options(self):
        """Remove options that don't make sense for the current settings"""
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        """Configure the package"""
        if self.options.shared:
            self.options.rm_safe("fPIC")

        # Pure C library - remove C++ stdlib
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        """Define the layout of the package"""
        cmake_layout(self)

    def requirements(self):
        """Define dependencies"""
        # CNanoLog has no dependencies - zero-dependency library!
        pass

    def build_requirements(self):
        """Define build-time dependencies"""
        pass

    def generate(self):
        """Generate necessary files for the build"""
        tc = CMakeToolchain(self)
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """Build the package"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package the built artifacts"""
        cmake = CMake(self)
        cmake.install()

        # Copy license
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

        # Optionally remove tools if not requested
        if not self.options.with_tools:
            tools_dir = os.path.join(self.package_folder, "bin")
            if os.path.exists(tools_dir):
                self.output.info("Removing tools (with_tools=False)")
                import shutil
                shutil.rmtree(tools_dir, ignore_errors=True)

    def package_info(self):
        """Define package information for consumers"""
        self.cpp_info.set_property("cmake_file_name", "CNanoLog")
        self.cpp_info.set_property("cmake_target_name", "CNanoLog::cnanolog")

        # Library
        self.cpp_info.libs = ["cnanolog"]

        # System libraries
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("pthread")
            self.cpp_info.system_libs.append("m")
        elif self.settings.os == "Macos":
            self.cpp_info.frameworks.append("CoreFoundation")

        # Definitions
        self.cpp_info.defines = []

        # Tools (decompressor)
        if self.options.with_tools:
            bin_path = os.path.join(self.package_folder, "bin")
            self.output.info(f"Decompressor tool available at: {bin_path}")
            self.buildenv_info.prepend_path("PATH", bin_path)
