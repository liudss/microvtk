import pytest
import os
import sys
import shutil
import subprocess

# -----------------------------------------------------------------------------
# Helper Functions
# -----------------------------------------------------------------------------

def find_executable(name):
    """Finds the build executable in likely locations."""
    # Possible build directories
    candidates = [
        "build_repro",
        "build",
        "build/debug",
        "build/release",
        "build/bin",
        "build/examples",
        "build/examples/Release",
        "build/examples/Debug",
        "build_release/examples",
        "build_release/examples/Release",
        "build/msvc-debug",
        "build/msvc-debug/Debug"
    ]

    # Adjust for Windows
    if sys.platform == "win32":
        name += ".exe"

    root_dir = os.getcwd()
    for sub in candidates:
        path = os.path.join(root_dir, sub, name)
        if os.path.exists(path):
            return path

    return None

def run_example(name, output_files):
    """
    Helper to run a C++ example executable.
    Handles Valgrind wrapping if MICROVTK_USE_VALGRIND=ON.
    Verifies return code and existence of output files.
    """
    exe = find_executable(name)
    if not exe:
        pytest.skip(f"{name} executable not found. Build the project first.")

    cmd = [exe]
    if os.environ.get("MICROVTK_USE_VALGRIND") == "ON":
        valgrind_exe = shutil.which("valgrind")
        if valgrind_exe:
             # --error-exitcode=1 ensures Valgrind errors fail the test
             # --leak-check=full gives detailed memory leak info
             cmd = [valgrind_exe, "--leak-check=full", "--error-exitcode=1", exe]

    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.returncode == 0, f"{name} failed: {result.stderr}"

    for f in output_files:
        assert os.path.exists(f), f"{f} was not created"

    return output_files

# -----------------------------------------------------------------------------
# Fixtures
# -----------------------------------------------------------------------------

@pytest.fixture(scope="module")
def basic_vtu_file():
    """Runs example_basic and yields the path to the generated vtu file."""
    output = "example.vtu"
    run_example("example_basic", [output])
    yield output
    if os.path.exists(output): os.remove(output)

@pytest.fixture(scope="module")
def pvd_files():
    """Runs example_time_series and yields the main PVD file."""
    output = "wave_simulation.pvd"
    # This example generates wave_simulation.pvd AND wave_0.vtu ... wave_9.vtu
    run_example("example_time_series", [output])

    yield output

    # Cleanup
    if os.path.exists(output): os.remove(output)
    for i in range(10):
        f = f"wave_{i}.vtu"
        if os.path.exists(f): os.remove(f)

@pytest.fixture(scope="module")
def complex_vtu_file():
    """Runs example_complex_grid and yields the path to the generated vtu file."""
    output = "complex_grid.vtu"
    run_example("example_complex_grid", [output])
    yield output
    if os.path.exists(output): os.remove(output)

@pytest.fixture(scope="module")
def compressed_vtu_file():
    """Runs example_compression and yields the path."""
    output = "compressed.vtu"
    run_example("example_compression", [output])
    yield output
    if os.path.exists(output): os.remove(output)

@pytest.fixture(scope="module")
def hpc_vtu_file():
    """Runs example_hpc and yields the path to the generated vtu file."""
    output = "hpc_example.vtu"

    # Special check for HPC since it's an optional build
    exe = find_executable("example_hpc")
    if not exe:
        pytest.skip("example_hpc executable not found. Build with MICROVTK_USE_KOKKOS=ON.")

    run_example("example_hpc", [output])
    yield output
    if os.path.exists(output): os.remove(output)

@pytest.fixture(scope="module")
def vti_adapt_file():
    """Runs example_vti_adapt and yields the path."""
    output = "vti_adapt.vti"
    run_example("example_vti_adapt", [output])
    yield output
    if os.path.exists(output): os.remove(output)

@pytest.fixture(scope="module")
def indexing_files():
    """Runs example_indexing and yields the output files."""
    outputs = ["morton_3d.vti", "morton_2d.vti", "morton_aos_combined.vti"]
    run_example("example_indexing", outputs)
    yield outputs
    for f in outputs:
        if os.path.exists(f): os.remove(f)
