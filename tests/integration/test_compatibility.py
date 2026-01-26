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

    assert os.path.exists(datasets[0].attrib["file"]), "Referenced .vtu file missing"


