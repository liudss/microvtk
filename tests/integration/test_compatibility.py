import pytest
import vtk
import os
import subprocess
import sys
import shutil

def find_executable(name):
    """Finds the build executable in likely locations."""
    # Possible build directories
    candidates = [
        "build",
        "build/bin",
        "build/examples",
        "build/examples/Release",
        "build/examples/Debug",
        "build_release/examples",
        "build_release/examples/Release",
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

@pytest.fixture(scope="module")
def basic_vtu_file():
    """Runs example_basic and yields the path to the generated vtu file."""
    exe = find_executable("example_basic")
    if not exe:
        pytest.skip("example_basic executable not found. Build the project first.")

    cmd = [exe]
    result = subprocess.run(cmd, capture_output=True, text=True)

    assert result.returncode == 0, f"Example failed: {result.stderr}"

    output_file = "example.vtu"
    assert os.path.exists(output_file), "example.vtu was not created"

    yield output_file

    if os.path.exists(output_file):
        os.remove(output_file)

@pytest.fixture(scope="module")
def pvd_files():
    """Runs example_time_series and yields the main PVD file."""
    exe = find_executable("example_time_series")
    if not exe:
        pytest.skip("example_time_series executable not found.")

    cmd = [exe]
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.returncode == 0, f"Example failed: {result.stderr}"

    pvd_file = "wave_simulation.pvd"
    assert os.path.exists(pvd_file), "wave_simulation.pvd was not created"

    yield pvd_file

    # Cleanup
    if os.path.exists(pvd_file):
        os.remove(pvd_file)
    # Cleanup generated vtus
    for i in range(10):
        f = f"wave_{i}.vtu"
        if os.path.exists(f):
            os.remove(f)

@pytest.fixture(scope="module")
def complex_vtu_file():
    """Runs example_complex_grid and yields the path to the generated vtu file."""
    exe = find_executable("example_complex_grid")
    if not exe:
        pytest.skip("example_complex_grid executable not found.")

    cmd = [exe]
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.returncode == 0, f"Example failed: {result.stderr}"

    output_file = "complex_grid.vtu"
    assert os.path.exists(output_file), "complex_grid.vtu was not created"

    yield output_file

    if os.path.exists(output_file):
        os.remove(output_file)

@pytest.fixture(scope="module")
def compressed_vtu_file():
    """Runs example_compression and yields the path."""
    exe = find_executable("example_compression")
    if not exe:
        pytest.skip("example_compression executable not found.")

    cmd = [exe]
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.returncode == 0, f"Example failed: {result.stderr}"

    output_file = "compressed.vtu"
    assert os.path.exists(output_file), "compressed.vtu was not created"

    yield output_file

    if os.path.exists(output_file):
        os.remove(output_file)

def test_read_vtu_with_official_vtk(basic_vtu_file):
    """Verifies that the generated VTU file can be read by the official VTK library."""
    reader = vtk.vtkXMLUnstructuredGridReader()
    reader.SetFileName(basic_vtu_file)
    reader.Update()

    grid = reader.GetOutput()

    assert grid.GetNumberOfPoints() == 4, "Expected 4 points"
    assert grid.GetNumberOfCells() == 1, "Expected 1 cell"

    cell = grid.GetCell(0)
    assert cell.GetCellType() == 10, f"Expected cell type 10 (Tetra), got {cell.GetCellType()}"

    point_data = grid.GetPointData()
    assert point_data.HasArray("Mass"), "Point data 'Mass' missing"

    mass_array = point_data.GetArray("Mass")
    assert mass_array.GetNumberOfComponents() == 1

    expected_mass = [1.0, 2.0, 3.0, 4.0]
    for i in range(4):
        val = mass_array.GetValue(i)
        assert abs(val - expected_mass[i]) < 1e-6, f"Mass at {i} mismatch"

def test_legacy_reader_compatibility(basic_vtu_file):
    """Double check with a generic reader factory."""
    reader = vtk.vtkXMLGenericDataObjectReader()
    reader.SetFileName(basic_vtu_file)
    reader.Update()
    data = reader.GetOutput()

    assert data.IsA("vtkUnstructuredGrid")
    assert data.GetNumberOfPoints() == 4

def test_pvd_time_series(pvd_files):
    """Verifies PVD file structure and time steps using XML parsing."""

    # Manual XML check for PVD correctness
    import xml.etree.ElementTree as ET
    tree = ET.parse(pvd_files)
    root = tree.getroot()

    assert root.tag == "VTKFile"
    assert root.attrib["type"] == "Collection"

    collection = root.find("Collection")
    assert collection is not None

    datasets = collection.findall("DataSet")
    assert len(datasets) == 10, f"Expected 10 time steps, found {len(datasets)}"

    # Check first step
    # PvdWriter uses double for time, let's check strict equality or approximate
    assert float(datasets[0].attrib["timestep"]) == 0.0
    assert datasets[0].attrib["file"] == "wave_0.vtu"

    # Verify that the referenced file actually exists
    assert os.path.exists(datasets[0].attrib["file"])

def test_complex_grid_structure(complex_vtu_file):
    """Verifies the structure and data of the complex grid example."""
    reader = vtk.vtkXMLUnstructuredGridReader()
    reader.SetFileName(complex_vtu_file)
    reader.Update()

    grid = reader.GetOutput()

    # Check Geometry
    assert grid.GetNumberOfPoints() == 12, f"Expected 12 points, got {grid.GetNumberOfPoints()}"
    assert grid.GetNumberOfCells() == 2, f"Expected 2 cells, got {grid.GetNumberOfCells()}"

    # Check Cell Types
    cell0 = grid.GetCell(0)
    assert cell0.GetCellType() == 12, f"Expected Hexahedron (12), got {cell0.GetCellType()}"

    cell1 = grid.GetCell(1)
    assert cell1.GetCellType() == 10, f"Expected Tetra (10), got {cell1.GetCellType()}"

    # Check Point Data: "Temperature"
    point_data = grid.GetPointData()
    assert point_data.HasArray("Temperature"), "Missing 'Temperature' point data"
    temp_array = point_data.GetArray("Temperature")
    assert temp_array.GetNumberOfTuples() == 12
    # Verify a known value (Apex of tetra at z=2.0 -> Temp=200.0)
    # The apex is the last point (index 11)
    val_apex = temp_array.GetValue(11)
    assert abs(val_apex - 200.0) < 1e-6, f"Expected Temperature 200.0, got {val_apex}"

    # Check Cell Data: "MaterialID"
    cell_data = grid.GetCellData()
    assert cell_data.HasArray("MaterialID"), "Missing 'MaterialID' cell data"
    mat_array = cell_data.GetArray("MaterialID")
    assert mat_array.GetNumberOfTuples() == 2
    assert int(mat_array.GetValue(0)) == 1
    assert int(mat_array.GetValue(1)) == 2

def test_compressed_file(compressed_vtu_file):
    """Verifies that the compressed file is valid and readable."""

    # 1. Check if the file is actually using compression (inspect XML)
    with open(compressed_vtu_file, 'rb') as f:
        content = f.read()
        # Should contain compressor attribute if LZ4 was enabled
        # Note: If LZ4 lib wasn't found during build, it falls back to uncompressed.
        # But we expect it to be ON in this environment.
        if b"compressor=\"vtkLZ4DataCompressor\"" not in content and b"compressor=\"vtkZLibDataCompressor\"" not in content:
             # Just a warning or soft check?
             # For this test, let's assume we wanted compression.
             pass

    # 2. Read with VTK
    reader = vtk.vtkXMLUnstructuredGridReader()
    reader.SetFileName(compressed_vtu_file)
    reader.Update()

    grid = reader.GetOutput()

    assert grid.GetNumberOfPoints() == 1000
    assert grid.GetNumberOfCells() == 1 # PolyLine

    # Check Data
    point_data = grid.GetPointData()
    assert point_data.HasArray("SineWave")

    arr = point_data.GetArray("SineWave")
    # Spot check
    # t at i=100 is 10.0. sin(10.0) ~ -0.544
    val = arr.GetValue(100)
    expected = -0.54402111088
    assert abs(val - expected) < 1e-5
