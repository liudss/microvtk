import vtk
import pytest

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
