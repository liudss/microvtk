#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <ranges>
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

#if defined(__BMI2__)
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
#if defined(__BMI2__)
  return split_by_3_bmi2(x) | (split_by_3_bmi2(y) << 1) |
         (split_by_3_bmi2(z) << 2);
#else
  if (std::is_constant_evaluated()) {
    return split_by_3_generic(x) | (split_by_3_generic(y) << 1) |
           (split_by_3_generic(z) << 2);
  } else {
    return split_by_3_generic(x) | (split_by_3_generic(y) << 1) |
           (split_by_3_generic(z) << 2);
  }
#endif
}

inline uint64_t morton_encode_2d(uint32_t x, uint32_t y) noexcept {
#if defined(__BMI2__)
  return split_by_2_bmi2(x) | (split_by_2_bmi2(y) << 1);
#else
  if (std::is_constant_evaluated()) {
    return split_by_2_generic(x) | (split_by_2_generic(y) << 1);
  } else {
    return split_by_2_generic(x) | (split_by_2_generic(y) << 1);
  }
#endif
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Views
// -----------------------------------------------------------------------------

namespace views {

namespace detail {

// Implementation of the view logic (3D)
template <std::ranges::random_access_range R>
auto reorder_z_curve_impl(R&& range, std::array<size_t, 3> dims) {
  size_t nx = dims[0];
  size_t ny = dims[1];
  size_t nz = dims[2];
  size_t total_size = nx * ny * nz;

  return std::views::iota(size_t{0}, total_size) |
         std::views::transform([range = std::views::all(std::forward<R>(range)),
                                nx, ny](size_t linear_idx) -> decltype(auto) {
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
auto reorder_z_curve_impl(R&& range, std::array<size_t, 2> dims) {
  size_t nx = dims[0];
  size_t ny = dims[1];
  size_t total_size = nx * ny;

  return std::views::iota(size_t{0}, total_size) |
         std::views::transform([range = std::views::all(std::forward<R>(range)),
                                nx](size_t linear_idx) -> decltype(auto) {
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
auto reorder_z_curve(R&& range, std::array<size_t, 3> dims) {
  return detail::reorder_z_curve_impl(std::forward<R>(range), dims);
}

// 4. Direct call (2D)
template <std::ranges::random_access_range R>
auto reorder_z_curve(R&& range, std::array<size_t, 2> dims) {
  return detail::reorder_z_curve_impl(std::forward<R>(range), dims);
}

}  // namespace views
}  // namespace microvtk
