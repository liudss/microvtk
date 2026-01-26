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
        
    # Run in a temp dir or just current dir? 
    # example_basic writes to "example.vtu" in CWD.
    # We'll run it in the tests/integration folder to keep things clean, 
    # or just root if that's where the example expects.
    # The example writes to "example.vtu" (relative).
    
    # Let's run in the project root to match typical user behavior
    cmd = [exe]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    assert result.returncode == 0, f"Example failed: {result.stderr}"
    
    output_file = "example.vtu"
    assert os.path.exists(output_file), "example.vtu was not created"
    
    yield output_file
    
    # Cleanup
    if os.path.exists(output_file):
        os.remove(output_file)

def test_read_vtu_with_official_vtk(basic_vtu_file):
    """Verifies that the generated VTU file can be read by the official VTK library."""
    
    reader = vtk.vtkXMLUnstructuredGridReader()
    reader.SetFileName(basic_vtu_file)
    reader.Update()
    
    grid = reader.GetOutput()
    
    # Verify Geometry
    # The example creates a tetra with 4 points
    assert grid.GetNumberOfPoints() == 4, "Expected 4 points"
    assert grid.GetNumberOfCells() == 1, "Expected 1 cell"
    
    # Verify Cell Type
    cell = grid.GetCell(0)
    # VTK_TETRA is 10
    assert cell.GetCellType() == 10, f"Expected cell type 10 (Tetra), got {cell.GetCellType()}"
    
    # Verify Point Data
    # The example adds "Mass" array
    point_data = grid.GetPointData()
    assert point_data.HasArray("Mass"), "Point data 'Mass' missing"
    
    mass_array = point_data.GetArray("Mass")
    assert mass_array.GetNumberOfComponents() == 1
    
    # Check values: 1.0, 2.0, 3.0, 4.0
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
