from __future__ import annotations

from enum import IntEnum
from os import PathLike, fspath

import numpy as np

from . import _microvtk as _c

__version__ = "0.1.0"

Compression = _c.Compression
CompressionType = Compression


class CellType(IntEnum):
    Vertex = 1
    PolyVertex = 2
    Line = 3
    PolyLine = 4
    Triangle = 5
    TriangleStrip = 6
    Polygon = 7
    Pixel = 8
    Quad = 9
    Tetra = 10
    Voxel = 11
    Hexahedron = 12
    Wedge = 13
    Pyramid = 14


_DTYPE_SUFFIX = {
    np.dtype(np.int8): "i8",
    np.dtype(np.uint8): "u8",
    np.dtype(np.int16): "i16",
    np.dtype(np.uint16): "u16",
    np.dtype(np.int32): "i32",
    np.dtype(np.uint32): "u32",
    np.dtype(np.int64): "i64",
    np.dtype(np.uint64): "u64",
    np.dtype(np.float32): "f32",
    np.dtype(np.float64): "f64",
}


def _path_string(path) -> str:
    if isinstance(path, PathLike):
        return fspath(path)
    if isinstance(path, str):
        return path
    raise TypeError("filename must be a str or pathlib.Path.")


def _normalize_compression(compression):
    if compression is None:
        return Compression.NONE
    if isinstance(compression, str):
        normalized = compression.lower()
        if normalized in {"none", "no", "off"}:
            return Compression.NONE
        if normalized in {"zlib", "zip"}:
            return Compression.ZLIB
        if normalized == "lz4":
            return Compression.LZ4
        raise ValueError(f"unknown compression value {compression!r}.")
    if isinstance(compression, Compression):
        return compression
    raise TypeError("compression must be None, a string, or microvtk.Compression.")


def _array(obj, *, name: str) -> np.ndarray:
    arr = np.asarray(obj)
    if arr.dtype not in _DTYPE_SUFFIX or not arr.dtype.isnative:
        raise TypeError(f"dtype {arr.dtype} is not supported by microvtk.")
    if not arr.flags.c_contiguous:
        arr = np.ascontiguousarray(arr)
    return arr


def _suffix(arr: np.ndarray) -> str:
    return _DTYPE_SUFFIX[arr.dtype]


def _call_typed(impl, prefix: str, arr: np.ndarray, *args):
    return getattr(impl, f"{prefix}_{_suffix(arr)}")(arr, *args)


def _call_typed_field(impl, prefix: str, name: str, arr: np.ndarray, components: int):
    return getattr(impl, f"{prefix}_{_suffix(arr)}")(name, arr, components)


def _field_components(arr: np.ndarray, components, *, name: str) -> int:
    if components is not None:
        components = int(components)
        if components < 1:
            raise ValueError(f"{name} components must be positive.")
        if arr.size % components != 0:
            raise ValueError(f"{name} size is not divisible by components.")
        return components
    if arr.ndim == 1:
        return 1
    if arr.ndim == 2:
        if arr.shape[1] < 1:
            raise ValueError(f"{name} component dimension must be positive.")
        return int(arr.shape[1])
    raise ValueError(f"{name} must have shape (n,) or (n, k).")


def _vti_field_layout(arr: np.ndarray, components, *, name: str):
    if arr.ndim < 1:
        raise ValueError(f"{name} must be at least one-dimensional.")
    if components is not None:
        components = int(components)
        if components < 1:
            raise ValueError(f"{name} components must be positive.")
        if arr.ndim > 1 and arr.shape[-1] == components:
            grid_shape = arr.shape[:-1]
        else:
            if arr.size % components != 0:
                raise ValueError(f"{name} size is not divisible by components.")
            grid_shape = arr.shape
        return tuple(int(v) for v in grid_shape), components

    if arr.ndim == 4:
        if arr.shape[-1] < 1:
            raise ValueError(f"{name} component dimension must be positive.")
        return tuple(int(v) for v in arr.shape[:-1]), int(arr.shape[-1])
    if arr.ndim in (1, 2, 3):
        return tuple(int(v) for v in arr.shape), 1
    raise ValueError(f"{name} must have shape (nx, ny), (nx, ny, nz), or (..., k).")


def _extent_from_shape(shape) -> tuple[int, int, int, int, int, int]:
    if len(shape) not in (1, 2, 3):
        raise ValueError("VTI grid shape must have one, two, or three dimensions.")
    dims = tuple(int(v) for v in shape) + (1,) * (3 - len(shape))
    if any(v < 1 for v in dims):
        raise ValueError("VTI grid dimensions must be positive.")
    return (0, dims[0] - 1, 0, dims[1] - 1, 0, dims[2] - 1)


def _extent_dims(extent) -> tuple[int, int, int]:
    if extent is None or len(extent) != 6:
        raise ValueError("extent must contain six integers.")
    ext = tuple(int(v) for v in extent)
    dims = (ext[1] - ext[0] + 1, ext[3] - ext[2] + 1, ext[5] - ext[4] + 1)
    if any(v < 1 for v in dims):
        raise ValueError("extent must have non-negative axis lengths.")
    return dims


def _tuple3(values, *, name: str) -> tuple[float, float, float]:
    if len(values) != 3:
        raise ValueError(f"{name} must contain three values.")
    return tuple(float(v) for v in values)


class VtuWriter:
    def __init__(self):
        self._impl = _c._VtuWriter()
        self._keep_alive = []

    def set_compression(self, compression):
        self._impl.set_compression(_normalize_compression(compression))
        return self

    def set_points(self, points, input_dim=None):
        arr = _array(points, name="points")
        if arr.ndim == 2:
            if arr.shape[1] not in (1, 2, 3):
                raise ValueError("points must have shape (n, 1), (n, 2), or (n, 3).")
            if input_dim is not None and int(input_dim) != arr.shape[1]:
                raise ValueError("input_dim does not match points.shape[1].")
            input_dim = int(arr.shape[1])
        elif arr.ndim == 1:
            if input_dim is None:
                raise ValueError("flat points require input_dim.")
            input_dim = int(input_dim)
            if input_dim not in (1, 2, 3):
                raise ValueError("input_dim must be 1, 2, or 3.")
            if arr.size % input_dim != 0:
                raise ValueError("flat points size is not divisible by input_dim.")
        else:
            raise ValueError("points must be a one- or two-dimensional array.")

        flat = np.ravel(arr, order="C")
        self._keep_alive.append(flat)
        _call_typed(self._impl, "set_points", flat, input_dim)
        return self

    def set_cells(self, connectivity, offsets, types):
        connectivity = np.asarray(connectivity)
        offsets = np.asarray(offsets)
        types = np.asarray(types)
        if connectivity.dtype != np.dtype(np.int32):
            raise TypeError("cell connectivity must have dtype int32.")
        if offsets.dtype != np.dtype(np.int32):
            raise TypeError("cell offsets must have dtype int32.")
        if types.dtype != np.dtype(np.uint8):
            raise TypeError("cell types must have dtype uint8.")
        if connectivity.ndim != 1 or offsets.ndim != 1 or types.ndim != 1:
            raise ValueError(
                "cell connectivity, offsets, and types must be one-dimensional."
            )

        connectivity = np.ascontiguousarray(connectivity)
        offsets = np.ascontiguousarray(offsets)
        types = np.ascontiguousarray(types)
        self._keep_alive.extend((connectivity, offsets, types))
        self._impl.set_cells(connectivity, offsets, types)
        return self

    def add_point_data(self, name, data, components=None):
        arr = _array(data, name=f"point_data[{name!r}]")
        comps = _field_components(arr, components, name=f"point_data[{name!r}]")
        flat = np.ravel(arr, order="C")
        self._keep_alive.append(flat)
        _call_typed_field(self._impl, "add_point_data", str(name), flat, comps)
        return self

    def add_cell_data(self, name, data, components=None):
        arr = _array(data, name=f"cell_data[{name!r}]")
        comps = _field_components(arr, components, name=f"cell_data[{name!r}]")
        flat = np.ravel(arr, order="C")
        self._keep_alive.append(flat)
        _call_typed_field(self._impl, "add_cell_data", str(name), flat, comps)
        return self

    def write(self, filename):
        self._impl.write(_path_string(filename))
        return None


class VtiWriter:
    def __init__(
        self,
        extent,
        origin=(0.0, 0.0, 0.0),
        spacing=(1.0, 1.0, 1.0),
    ):
        self._extent = tuple(int(v) for v in extent)
        _extent_dims(self._extent)
        self._impl = _c._VtiWriter(
            self._extent,
            _tuple3(origin, name="origin"),
            _tuple3(spacing, name="spacing"),
        )
        self._keep_alive = []

    def set_compression(self, compression):
        self._impl.set_compression(_normalize_compression(compression))
        return self

    def add_point_data(self, name, data, components=None):
        arr = _array(data, name=f"point_data[{name!r}]")
        _, comps = _vti_field_layout(arr, components, name=f"point_data[{name!r}]")
        flat = np.ravel(arr, order="C")
        self._keep_alive.append(flat)
        _call_typed_field(self._impl, "add_point_data", str(name), flat, comps)
        return self

    def add_cell_data(self, name, data, components=None):
        arr = _array(data, name=f"cell_data[{name!r}]")
        _, comps = _vti_field_layout(arr, components, name=f"cell_data[{name!r}]")
        flat = np.ravel(arr, order="C")
        self._keep_alive.append(flat)
        _call_typed_field(self._impl, "add_cell_data", str(name), flat, comps)
        return self

    def write(self, filename):
        self._impl.write(_path_string(filename))
        return None


class PvdWriter:
    def __init__(self, filename):
        self._impl = _c._PvdWriter(_path_string(filename))

    def add_step(self, time, file):
        self._impl.add_step(float(time), _path_string(file))
        return self

    def save(self):
        self._impl.save()
        return None


def write_vtu(
    filename,
    points,
    cells,
    point_data=None,
    cell_data=None,
    compression=None,
):
    writer = VtuWriter()
    writer.set_compression(compression)
    writer.set_points(points)
    writer.set_cells(*cells)
    for name, data in (point_data or {}).items():
        writer.add_point_data(name, data)
    for name, data in (cell_data or {}).items():
        writer.add_cell_data(name, data)
    writer.write(filename)


def write_vti(
    filename,
    point_data=None,
    cell_data=None,
    extent=None,
    origin=(0.0, 0.0, 0.0),
    spacing=(1.0, 1.0, 1.0),
    compression=None,
):
    point_data = point_data or {}
    cell_data = cell_data or {}

    if extent is None:
        if not point_data:
            raise ValueError("VTI extent is ambiguous; pass extent explicitly.")
        inferred_shape = None
        for name, data in point_data.items():
            arr = _array(data, name=f"point_data[{name!r}]")
            shape, _ = _vti_field_layout(arr, None, name=f"point_data[{name!r}]")
            if inferred_shape is None:
                inferred_shape = shape
            elif shape != inferred_shape:
                raise ValueError("VTI extent is ambiguous; pass extent explicitly.")
        extent = _extent_from_shape(inferred_shape)

    writer = VtiWriter(extent=extent, origin=origin, spacing=spacing)
    writer.set_compression(compression)
    for name, data in point_data.items():
        writer.add_point_data(name, data)
    for name, data in cell_data.items():
        writer.add_cell_data(name, data)
    writer.write(filename)


def write_pvd(filename, steps):
    writer = PvdWriter(filename)
    for time, file in steps:
        writer.add_step(time, file)
    writer.save()


__all__ = [
    "CellType",
    "Compression",
    "CompressionType",
    "PvdWriter",
    "VtiWriter",
    "VtuWriter",
    "write_pvd",
    "write_vti",
    "write_vtu",
]
