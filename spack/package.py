from spack_repo.builtin.build_systems.cmake import CMakePackage

from spack.package import *


class Microvtk(CMakePackage):
    """Header-only C++20 library for writing VTK XML artifacts."""

    homepage = "https://github.com/liudss/microvtk"
    git = "https://github.com/liudss/microvtk.git"

    license("MIT")

    version("master", branch="master")

    variant("zlib", default=True, description="Enable ZLIB compression support")
    variant("lz4", default=True, description="Enable LZ4 compression support")
    variant("kokkos", default=False, description="Enable Kokkos adapter support")
    variant("cabana", default=False, description="Enable Cabana adapter support")
    variant("tests", default=False, description="Build MicroVTK unit tests")
    variant("examples", default=False, description="Build MicroVTK examples")
    variant("benchmarks", default=False, description="Build MicroVTK benchmarks")

    depends_on("cxx", type="build")
    depends_on("cmake@3.25:", type="build")
    depends_on("pkgconf", type="build", when="+lz4")

    depends_on("zlib-api", type=("build", "link"), when="+zlib")
    depends_on("lz4 build_system=cmake", type=("build", "link"), when="+lz4")
    depends_on("kokkos cxxstd=20", type=("build", "link"), when="+kokkos")
    depends_on("cabana", type=("build", "link"), when="+cabana")
    depends_on("googletest cxxstd=20", type=("build", "link"), when="+tests")
    depends_on("google-benchmark", type=("build", "link"), when="+benchmarks")

    conflicts("+cabana", when="~kokkos", msg="Cabana adapters require Kokkos support.")

    def cmake_args(self):
        return [
            self.define("MICROVTK_USE_CPM", False),
            self.define("MICROVTK_INSTALL", True),
            self.define_from_variant("MICROVTK_USE_ZLIB", "zlib"),
            self.define_from_variant("MICROVTK_USE_LZ4", "lz4"),
            self.define_from_variant("MICROVTK_USE_KOKKOS", "kokkos"),
            self.define_from_variant("MICROVTK_USE_CABANA", "cabana"),
            self.define_from_variant("MICROVTK_BUILD_TESTS", "tests"),
            self.define_from_variant("MICROVTK_BUILD_EXAMPLES", "examples"),
            self.define_from_variant("MICROVTK_BUILD_BENCHMARKS", "benchmarks"),
            self.define("CMAKE_CXX_STANDARD", "20"),
            self.define("CMAKE_CXX_STANDARD_REQUIRED", True),
        ]
