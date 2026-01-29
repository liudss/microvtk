import vtk
import pytest

def test_hpc_data_correctness(hpc_vtu_file):
    """Verifies Kokkos Rank 3 and Cabana MultiDim array output."""
    reader = vtk.vtkXMLUnstructuredGridReader()
    reader.SetFileName(hpc_vtu_file)
    reader.Update()

    grid = reader.GetOutput()
    point_data = grid.GetPointData()

    # 1. Check Kokkos Rank 3 Tensor (StressTensor)
    if point_data.HasArray("StressTensor"):
        stress = point_data.GetArray("StressTensor")
        # Should be flat 9 components
        assert stress.GetNumberOfComponents() == 9

        # Check value at index 0
        # Expected: r*10 + c + (0%2) = r*10 + c
        # (0,0)->0, (0,1)->1 ... (2,2)->22
        t0 = stress.GetTuple9(0)
        assert t0[0] == 0.0
        assert t0[8] == 22.0

        # Check value at index 1
        # Expected: r*10 + c + 1
        t1 = stress.GetTuple9(1)
        assert t1[0] == 1.0 # 0 + 1
        assert t1[8] == 23.0
    else:
        pytest.fail("Missing 'StressTensor' array from Kokkos adapter")

    # 2. Check Cabana MultiDim Array (CabanaTensor)
    if point_data.HasArray("CabanaTensor"):
        cabana = point_data.GetArray("CabanaTensor")
        assert cabana.GetNumberOfComponents() == 9

        # Expected: (r+1)*100 + c
        # (0,0) -> 100, (2,2) -> 302
        t0 = cabana.GetTuple9(0)
        assert t0[0] == 100.0
        assert t0[8] == 302.0
