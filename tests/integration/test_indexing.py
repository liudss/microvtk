import pytest
import vtk
import numpy as np
import os
import subprocess

# Path to the compiled C++ example
EXAMPLE_BIN = "./build/example_indexing"

@pytest.fixture(scope="module")
def run_indexing_example():
    """Runs the C++ example once to generate the .vti files."""
    if not os.path.exists(EXAMPLE_BIN):
        pytest.skip(f"Example binary {EXAMPLE_BIN} not found. Build examples first.")

    ret = subprocess.run([EXAMPLE_BIN], check=True, capture_output=True)
    return ret

def test_morton_3d_output(run_indexing_example):
    """Verifies morton_3d.vti contains correct gradients."""
    filename = "morton_3d.vti"
    assert os.path.exists(filename)

    reader = vtk.vtkXMLImageDataReader()
    reader.SetFileName(filename)
    reader.Update()

    image = reader.GetOutput()
    dims = image.GetDimensions()
    assert dims == (64, 64, 64)

    # Check data range
    # Data was x + y + z.
    # Max at (63, 63, 63) -> 63*3 = 189
    arr = image.GetPointData().GetArray("Temperature")
    assert arr is not None
    rng = arr.GetRange()
    assert rng[0] == 0.0
    assert rng[1] == 189.0

    # Spot check specific points to ensure order is correct
    # If Morton logic was wrong, these values would be scrambled
    def get_val(x, y, z):
        idx = x + y * 64 + z * 64 * 64 # Raster index
        return arr.GetValue(idx)

    assert get_val(0, 0, 0) == 0.0
    assert get_val(10, 0, 0) == 10.0
    assert get_val(0, 10, 0) == 10.0
    assert get_val(0, 0, 10) == 10.0
    assert get_val(63, 63, 63) == 189.0

def test_morton_2d_output(run_indexing_example):
    """Verifies morton_2d.vti contains correct radial pattern."""
    filename = "morton_2d.vti"
    assert os.path.exists(filename)

    reader = vtk.vtkXMLImageDataReader()
    reader.SetFileName(filename)
    reader.Update()

    image = reader.GetOutput()
    dims = image.GetDimensions()
    assert dims == (128, 128, 1) # 2D slice

    arr = image.GetPointData().GetArray("Distance")
    assert arr is not None

    # Center should be near 0
    # Center index is (64, 64)
    cx, cy = 64, 64
    idx = cx + cy * 128
    val = arr.GetValue(idx)
    # Calculation: x - 64, y - 64. At 64,64 -> 0,0 -> sqrt(0)
    assert val == 0.0

    # Corner (0,0) -> (-64)^2 + (-64)^2 -> sqrt(4096+4096) = sqrt(8192) ~= 90.5
    idx_corner = 0
    val_corner = arr.GetValue(idx_corner)
    assert abs(val_corner - 90.509) < 0.01

def test_combined_output(run_indexing_example):
    """Verifies morton_aos_combined.vti (AoS + Indexing)."""
    filename = "morton_aos_combined.vti"
    assert os.path.exists(filename)

    reader = vtk.vtkXMLImageDataReader()
    reader.SetFileName(filename)
    reader.Update()

    image = reader.GetOutput()
    assert image.GetDimensions() == (32, 32, 32)

    arr = image.GetPointData().GetArray("ParticleTemp")
    assert arr is not None

    # Check max value: 31+31+31 = 93
    assert arr.GetRange()[1] == 93.0
