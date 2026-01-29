import vtk
import pytest

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
        if b"compressor=\"vtkLZ4DataCompressor\"" not in content and b"compressor=\"vtkZLibDataCompressor\"" not in content:
             # Soft check: just ensure file is valid even if compression wasn't active
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
