# MicroVTK Spack Recipe

This directory contains a candidate Spack recipe for MicroVTK.

The recipe is kept here as source-controlled packaging material. Spack does not
discover this file directly from the project checkout. To use it, copy
`package.py` into a Spack package repository.

## Local Spack Repository

Create a local Spack repository:

```bash
mkdir -p /path/to/spack-repo/packages/microvtk
cp spack/package.py /path/to/spack-repo/packages/microvtk/package.py
cp spack/repo.yaml /path/to/spack-repo/repo.yaml
```

`/path/to/spack-repo` can be any stable directory. With Spack v1, user-managed
package repositories commonly live under `$HOME/.spack/package_repos`; for
example:

```text
$HOME/.spack/package_repos/microvtk_repo
```

Do not put a custom repository inside Spack's hashed cache directories such as
`$HOME/.spack/package_repos/<hash>`. Those are managed by Spack for package
repositories it downloads, such as the builtin repository.

The repository metadata is included in `spack/repo.yaml`:

```yaml
repo:
  namespace: microvtk_repo
  api: v2.2
```

Use that repository from a Spack environment:

```yaml
spack:
  repos:
  - /path/to/spack-repo

  specs:
  - microvtk@master +zlib +lz4 ~kokkos ~cabana

  view: true
  concretizer:
    unify: true
```

Then install:

```bash
spack -e . concretize -f
spack -e . install --fail-fast
```

## Consuming MicroVTK

After installation, point CMake at the Spack view or installation prefix:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/env/.spack-env/view
```

Then use the installed CMake package:

```cmake
find_package(microvtk CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE microvtk::microvtk)
```

## Variants

The recipe defines these variants:

- `+zlib`: enable ZLIB compression support
- `+lz4`: enable LZ4 compression support
- `+kokkos`: enable Kokkos adapter support
- `+cabana`: enable Cabana adapter support
- `+tests`: build MicroVTK unit tests
- `+examples`: build MicroVTK examples
- `+benchmarks`: build MicroVTK benchmarks

Cabana support requires Kokkos support, so `+cabana~kokkos` is invalid.

## Source Version

`microvtk@master` uses the GitHub `master` branch:

```python
git = "https://github.com/liudss/microvtk.git"
version("master", branch="master")
```

This is useful for development but is a floating version. For reproducible
package use, prefer adding tagged releases to the recipe when MicroVTK publishes
release tags.
