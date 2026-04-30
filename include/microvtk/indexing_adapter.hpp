#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <tuple>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace microvtk {

namespace detail {

// Portable 3D Morton encoding (Z-order curve)
// Interleaves bits of x, y, z.
constexpr uint64_t split_by_3_generic(uint32_t a) noexcept {
  uint64_t x = a & 0x1fffff;  // mask to 21 bits
  x = (x | x << 32) & 0x1f00000000ffff;
  x = (x | x << 16) & 0x1f0000ff0000ff;
  x = (x | x << 8) & 0x100f00f00f00f00f;
  x = (x | x << 4) & 0x10c30c30c30c30c3;
  x = (x | x << 2) & 0x1249249249249249;
  return x;
}

// Portable 2D Morton encoding
constexpr uint64_t split_by_2_generic(uint32_t a) noexcept {
  uint64_t x = a & 0xffffffff;
  x = (x | x << 32) & 0x00000000ffffffff;
  x = (x | x << 16) & 0x0000ffff0000ffff;
  x = (x | x << 8) & 0x00ff00ff00ff00ff;
  x = (x | x << 4) & 0x0f0f0f0f0f0f0f0f;
  x = (x | x << 2) & 0x3333333333333333;
  x = (x | x << 1) & 0x5555555555555555;
  return x;
}

#ifdef __BMI2__
// Hardware-accelerated splitting using PDEP
inline uint64_t split_by_3_bmi2(uint32_t a) noexcept {
  // 0x1249... has bits 0, 3, 6... set
  return _pdep_u64(a, 0x1249249249249249ULL);
}

inline uint64_t split_by_2_bmi2(uint32_t a) noexcept {
  // 0x5555... has bits 0, 2, 4... set
  return _pdep_u64(a, 0x5555555555555555ULL);
}
#endif

// -----------------------------------------------------------------------------
// Dispatchers
// -----------------------------------------------------------------------------

inline uint64_t morton_encode_3d(uint32_t x, uint32_t y, uint32_t z) noexcept {
#ifdef __BMI2__
  return split_by_3_bmi2(x) | (split_by_3_bmi2(y) << 1) |
         (split_by_3_bmi2(z) << 2);
#else
  return split_by_3_generic(x) | (split_by_3_generic(y) << 1) |
         (split_by_3_generic(z) << 2);
#endif
}

inline uint64_t morton_encode_2d(uint32_t x, uint32_t y) noexcept {
#ifdef __BMI2__
  return split_by_2_bmi2(x) | (split_by_2_bmi2(y) << 1);
#else
  return split_by_2_generic(x) | (split_by_2_generic(y) << 1);
#endif
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Views
// -----------------------------------------------------------------------------

namespace views {

namespace detail {

inline size_t checked_product(size_t lhs, size_t rhs) {
  if (rhs != 0 && lhs > std::numeric_limits<size_t>::max() / rhs) {
    throw std::invalid_argument(
        "reorder_z_curve: dimensions overflow output size.");
  }
  return lhs * rhs;
}

template <size_t N>
size_t total_size(const std::array<size_t, N>& dims) {
  size_t total = 1;
  for (const auto dim : dims) {
    if (dim == 0) {
      throw std::invalid_argument(
          "reorder_z_curve: dimensions must be non-zero.");
    }
    total = checked_product(total, dim);
  }
  return total;
}

inline uint64_t max_morton_index(std::array<size_t, 3> dims) {
  constexpr size_t max3dCoordinate = 0x1fffff;
  for (const auto dim : dims) {
    if ((dim - 1) > max3dCoordinate) {
      throw std::invalid_argument(
          "reorder_z_curve: 3D dimensions exceed Morton coordinate range.");
    }
  }
  return microvtk::detail::morton_encode_3d(static_cast<uint32_t>(dims[0] - 1),
                                            static_cast<uint32_t>(dims[1] - 1),
                                            static_cast<uint32_t>(dims[2] - 1));
}

inline uint64_t max_morton_index(std::array<size_t, 2> dims) {
  constexpr auto max2dCoordinate =
      static_cast<size_t>(std::numeric_limits<uint32_t>::max());
  for (const auto dim : dims) {
    if ((dim - 1) > max2dCoordinate) {
      throw std::invalid_argument(
          "reorder_z_curve: 2D dimensions exceed Morton coordinate range.");
    }
  }
  return microvtk::detail::morton_encode_2d(static_cast<uint32_t>(dims[0] - 1),
                                            static_cast<uint32_t>(dims[1] - 1));
}

template <std::ranges::sized_range R, size_t N>
size_t validate_z_curve_input(const R& range,
                              const std::array<size_t, N>& dims) {
  const auto total = total_size(dims);
  const auto maxIndex = max_morton_index(dims);
  if (maxIndex >= std::ranges::size(range)) {
    throw std::invalid_argument(
        "reorder_z_curve: input range is too small for dimensions.");
  }
  return total;
}

// Implementation of the view logic (3D)
template <std::ranges::random_access_range R>
  requires std::ranges::sized_range<R>
auto reorder_z_curve_impl(R&& range, std::array<size_t, 3> dims) {
  size_t total = validate_z_curve_input(range, dims);
  size_t nx = dims[0];
  size_t ny = dims[1];

  return std::views::iota(size_t{0}, total) |
         std::views::transform(
             [range = std::views::all(std::forward<R>(range)), nx,
              ny](size_t linear_idx) -> std::ranges::range_value_t<R> {
               // Decode Raster Order (x fast, y medium, z slow)
               size_t z = linear_idx / (nx * ny);
               size_t rem = linear_idx % (nx * ny);
               size_t y = rem / nx;
               size_t x = rem % nx;

               // Encode Morton Order
               size_t morton_idx = microvtk::detail::morton_encode_3d(
                   static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                   static_cast<uint32_t>(z));

               return range[morton_idx];
             });
}

// Implementation of the view logic (2D)
template <std::ranges::random_access_range R>
  requires std::ranges::sized_range<R>
auto reorder_z_curve_impl(R&& range, std::array<size_t, 2> dims) {
  size_t total = validate_z_curve_input(range, dims);
  size_t nx = dims[0];

  return std::views::iota(size_t{0}, total) |
         std::views::transform(
             [range = std::views::all(std::forward<R>(range)),
              nx](size_t linear_idx) -> std::ranges::range_value_t<R> {
               // Decode Raster Order (x fast, y slow)
               size_t y = linear_idx / nx;
               size_t x = linear_idx % nx;

               // Encode Morton Order
               size_t morton_idx = microvtk::detail::morton_encode_2d(
                   static_cast<uint32_t>(x), static_cast<uint32_t>(y));

               return range[morton_idx];
             });
}

// Adaptor closure for pipe support
template <size_t N>
struct z_curve_adaptor_closure {
  std::array<size_t, N> dims;

  template <std::ranges::random_access_range R>
    requires std::ranges::sized_range<R>
  friend auto operator|(R&& r, const z_curve_adaptor_closure& closure) {
    return reorder_z_curve_impl(std::forward<R>(r), closure.dims);
  }
};

}  // namespace detail

/**
 * @brief Adapts a random-access range stored in Z-order (Morton) layout to be
 *        accessed in Raster order (Standard VTK/C layout).
 *
 * Usage (Pipe):
 *   auto vtk_view = morton_data | views::reorder_z_curve({nx, ny, nz}); // 3D
 *   auto vtk_view_2d = morton_data_2d | views::reorder_z_curve({nx, ny}); // 2D
 *
 * Usage (Direct):
 *   auto vtk_view = views::reorder_z_curve(morton_data, {nx, ny, nz});
 */

// 1. Pipeable factory (3D)
inline auto reorder_z_curve(std::array<size_t, 3> dims) {
  return detail::z_curve_adaptor_closure<3>{dims};
}

// 2. Pipeable factory (2D)
inline auto reorder_z_curve(std::array<size_t, 2> dims) {
  return detail::z_curve_adaptor_closure<2>{dims};
}

// 3. Direct call (3D)
template <std::ranges::random_access_range R>
  requires std::ranges::sized_range<R>
auto reorder_z_curve(R&& range, std::array<size_t, 3> dims) {
  return detail::reorder_z_curve_impl(std::forward<R>(range), dims);
}

// 4. Direct call (2D)
template <std::ranges::random_access_range R>
  requires std::ranges::sized_range<R>
auto reorder_z_curve(R&& range, std::array<size_t, 2> dims) {
  return detail::reorder_z_curve_impl(std::forward<R>(range), dims);
}

}  // namespace views
}  // namespace microvtk
