from pathlib import Path

import numpy as np
import pytest

import microvtk as vtk


def _tetra():
    points = np.array(
        [
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float64,
    )
    connectivity = np.array([0, 1, 2, 3], dtype=np.int32)
    offsets = np.array([4], dtype=np.int32)
    types = np.array([vtk.CellType.Tetra], dtype=np.uint8)
    return points, (connectivity, offsets, types)


def test_import_public_api():
    assert vtk.VtuWriter is not None
    assert vtk.VtiWriter is not None
    assert vtk.PvdWriter is not None
    assert vtk.Compression.LZ4 is not None
    assert int(vtk.CellType.Tetra) == 10


def test_write_vtu_with_supported_dtypes(tmp_path):
    points, cells = _tetra()
    out = tmp_path / "mesh.vtu"

    vtk.write_vtu(
        out,
        points=points,
        cells=cells,
        point_data={
            "f32": np.arange(4, dtype=np.float32),
            "f64": np.arange(4, dtype=np.float64),
            "i32": np.arange(4, dtype=np.int32),
            "u8": np.arange(4, dtype=np.uint8),
            "velocity": np.arange(12, dtype=np.float32).reshape(4, 3),
        },
        cell_data={"material": np.array([7], dtype=np.int32)},
    )

    text = out.read_bytes()
    assert b'Name="velocity"' in text
    assert b'NumberOfComponents="3"' in text


def test_vtu_read_back_with_official_vtk(tmp_path):
    vtk_module = pytest.importorskip("vtk")
    points, cells = _tetra()
    out = tmp_path / "mesh.vtu"

    vtk.write_vtu(
        out,
        points=points,
        cells=cells,
        point_data={"temperature": np.arange(4, dtype=np.float32)},
    )

    reader = vtk_module.vtkXMLUnstructuredGridReader()
    reader.SetFileName(str(out))
    reader.Update()
    grid = reader.GetOutput()

    assert grid.GetNumberOfPoints() == 4
    assert grid.GetNumberOfCells() == 1
    temperature = grid.GetPointData().GetArray("temperature")
    assert temperature.GetNumberOfComponents() == 1


def test_write_vti_scalar_and_vector(tmp_path):
    out = tmp_path / "field.vti"

    vtk.write_vti(
        out,
        point_data={
            "density": np.ones((2, 3, 4), dtype=np.float32),
            "velocity": np.ones((2, 3, 4, 3), dtype=np.float64),
        },
    )

    text = out.read_bytes()
    assert b'WholeExtent="0 1 0 2 0 3"' in text
    assert b'Name="velocity"' in text
    assert b'NumberOfComponents="3"' in text


def test_pvd_helpers(tmp_path):
    out = tmp_path / "series.pvd"

    vtk.write_pvd(out, [(0.0, Path("step_000.vtu")), (0.1, "step_001.vtu")])

    text = out.read_text()
    assert 'timestep="0' in text
    assert 'file="step_001.vtu"' in text


def test_compression_strings_and_enums(tmp_path):
    points, cells = _tetra()

    vtk.write_vtu(
        tmp_path / "zlib.vtu",
        points=points,
        cells=cells,
        point_data={"values": np.arange(4, dtype=np.float64)},
        compression="zlib",
    )
    vtk.write_vtu(
        tmp_path / "lz4.vtu",
        points=points,
        cells=cells,
        point_data={"values": np.arange(4, dtype=np.float64)},
        compression=vtk.Compression.LZ4,
    )


def test_non_contiguous_arrays_are_copied(tmp_path):
    points, cells = _tetra()
    base = np.arange(8, dtype=np.float32)
    non_contiguous = base[::2]
    assert not non_contiguous.flags.c_contiguous

    vtk.write_vtu(
        tmp_path / "non_contiguous.vtu",
        points=points,
        cells=cells,
        point_data={"values": non_contiguous},
    )


def test_unsupported_dtype_raises(tmp_path):
    points, cells = _tetra()

    with pytest.raises(TypeError, match="complex64"):
        vtk.write_vtu(
            tmp_path / "bad.vtu",
            points=points,
            cells=cells,
            point_data={"bad": np.ones(4, dtype=np.complex64)},
        )


def test_invalid_shapes_raise(tmp_path):
    points, cells = _tetra()
    writer = vtk.VtuWriter()

    with pytest.raises(ValueError, match="points must have shape"):
        writer.set_points(np.ones((4, 4), dtype=np.float64))

    with pytest.raises(ValueError, match="extent is ambiguous"):
        vtk.write_vti(
            tmp_path / "bad.vti",
            cell_data={"c": np.ones(1, dtype=np.float32)},
        )

    with pytest.raises(TypeError, match="int32"):
        vtk.write_vtu(
            tmp_path / "bad_cells.vtu",
            points=points,
            cells=(cells[0].astype(np.int64), cells[1], cells[2]),
        )
