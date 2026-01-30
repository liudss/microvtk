#pragma once

#ifdef MICROVTK_HAS_KOKKOS
#include <Kokkos_Core.hpp>
#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace microvtk {

// Adapt Kokkos View to std::span (if contiguous C-layout) or a generic range
// (if not).
template <typename DataType, typename... Properties>
[[nodiscard]] auto adapt(const Kokkos::View<DataType, Properties...>& view) {
  using ViewType = Kokkos::View<DataType, Properties...>;
  constexpr size_t Rank = ViewType::rank;
  using Layout = typename ViewType::array_layout;

  // Runtime check for accessibility
  static_assert(
      Kokkos::SpaceAccessibility<Kokkos::HostSpace,
                                 typename ViewType::memory_space>::accessible,
      "MicroVTK: View must be accessible from HostSpace");

  // Determine if we can use the Fast Path (std::span)
  // We must use COMPILE-TIME checks to ensure consistent return type
  // (std::span vs transform_view).
  //
  // Supported Fast Paths (Contiguous + Logical Order matches Memory):
  // 1. Rank 0 (Scalar)
  // 2. Rank 1 with Standard Layouts (Left/Right) - contiguous vectors.
  // 3. Rank >= 2 with LayoutRight (Row-Major) - contiguous C-style tuples.

  constexpr bool is_layout_right = std::is_same_v<Layout, Kokkos::LayoutRight>;
  constexpr bool is_layout_left = std::is_same_v<Layout, Kokkos::LayoutLeft>;

  constexpr bool use_span =
      (Rank == 0) || (Rank == 1 && (is_layout_right || is_layout_left)) ||
      (Rank >= 2 && is_layout_right);

  if constexpr (use_span) {
    // Return std::span
    return std::span(view.data(), view.span());
  } else {
    // Slow Path: Logical indexing via Transform View
    // Supports arbitrary Rank and non-contiguous layouts (e.g. LayoutLeft).
    // We map the flat index 'k' to logical indices (i0, i1, ...) assuming
    // Row-Major order (VTK standard).
    size_t total_size = view.size();

    return std::views::iota(size_t{0}, total_size) |
           std::views::transform([view, Rank](size_t k) ->
                                 typename ViewType::non_const_value_type {
                                   if constexpr (Rank == 1) {
                                     return view(k);
                                   } else if constexpr (Rank == 2) {
                                     size_t cols = view.extent(1);
                                     return view(k / cols, k % cols);
                                   } else {
                                     // Generic Rank > 2
                                     // Map flat index k to (i0, i1, ... iR)
                                     // i_last = k % extent_last; k /=
                                     // extent_last; ...
                                     std::array<size_t, Rank> indices;
                                     size_t temp = k;

                                     // Compute indices from last dimension to
                                     // first
                                     for (size_t d = Rank - 1; d > 0; --d) {
                                       indices[d] = temp % view.extent(d);
                                       temp /= view.extent(d);
                                     }
                                     indices[0] = temp;

                                     // Apply indices to view
                                     return [&]<size_t... Is>(
                                                std::index_sequence<Is...>) {
                                       return view(indices[Is]...);
                                     }(std::make_index_sequence<Rank>{});
                                   }
                                 });
  }
}

}  // namespace microvtk
#endif
