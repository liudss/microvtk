#pragma once

#ifdef MICROVTK_HAS_KOKKOS
#include <Kokkos_Core.hpp>
#include <span>

namespace microvtk {

// Adapt Kokkos View to std::span (if contiguous)
template <typename DataType, typename... Properties>
[[nodiscard]] auto adapt(const Kokkos::View<DataType, Properties...>& view) {
  // Runtime check for contiguity could be added if needed,
  // but for now we assume the user knows what they are doing or
  // we rely on span construction.

  // We only support Views that are accessible from Host
  static_assert(
      Kokkos::SpaceAccessibility<
          Kokkos::HostSpace, typename Kokkos::View<DataType, Properties...>::
                                 memory_space>::accessible,
      "MicroVTK: View must be accessible from HostSpace");

  // Check strict contiguity for simple adaptation
  // Kokkos::View's span() method returns the memory span of the allocation
  // but we want the logical data.
  // For unmanaged/contiguous layouts:
  return std::span(view.data(), view.span());
}

}  // namespace microvtk
#endif
