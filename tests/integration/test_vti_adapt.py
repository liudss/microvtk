import pytest
import vtk
import os

def test_read_vti_adapt(vti_adapt_file):
    """Verifies the VTI file generated with adapt()"""
    reader = vtk.vtkXMLImageDataReader()
    reader.SetFileName(vti_adapt_file)
    reader.Update()

    image = reader.GetOutput()

    # Check dimensions: 10x10x1 points
    dims = image.GetDimensions()
    assert dims == (10, 10, 1)

    # Check Point Data
    pd = image.GetPointData()
    assert pd.HasArray("Intensity")
    assert pd.HasArray("Category")

    intensity = pd.GetArray("Intensity")
    category = pd.GetArray("Category")

    # Verify a few values
    # x=5, y=5
    # val = sin(0.5) * cos(0.5) ~= 0.479 * 0.877 ~= 0.420
    idx = 5 * 10 + 5 # y*width + x
    val = intensity.GetValue(idx)
    cat = category.GetValue(idx)

    import math
    expected = math.sin(0.5) * math.cos(0.5)

    assert abs(val - expected) < 1e-6
    assert cat == 1 # 0.42 > 0

    # Test negative value
    # Check (0,0) -> sin(0) = 0
    assert intensity.GetValue(0) == 0.0
    assert category.GetValue(0) == 0
