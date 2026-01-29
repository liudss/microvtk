#include <microvtk/microvtk.hpp>
#include <vector>

// This example serves as a reference for integrating Kokkos Views.
// It will only be built if MICROVTK_USE_KOKKOS is enabled in CMake.

#ifdef MICROVTK_HAS_KOKKOS
#include <Kokkos_Core.hpp>
#endif

#ifdef MICROVTK_HAS_CABANA
#include <Cabana_Core.hpp>
#endif

int main(int argc, char* argv[]) {
#if defined(MICROVTK_HAS_KOKKOS)
  Kokkos::initialize(argc, argv);
  {
    using namespace microvtk;
    VtuWriter writer;

    int N = 100;
    // 1. Host Space View (Contiguous)
    Kokkos::View<double* [3], Kokkos::HostSpace> points("points", N);

    Kokkos::parallel_for(
        "FillPoints",
        Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
        KOKKOS_LAMBDA(const int i) {
          points(i, 0) = i * 1.0;
          points(i, 1) = 0.0;
          points(i, 2) = 0.0;
        });

    writer.setPoints(adapt(points));

    // 2. Rank 3 View (LayoutLeft / Non-Contiguous)
    // Represents a tensor field (3x3) on each point
    Kokkos::View<double* [3][3], Kokkos::LayoutLeft, Kokkos::HostSpace> tensor(
        "stress", N);
    Kokkos::parallel_for(
        "FillTensor",
        Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
        KOKKOS_LAMBDA(const int i) {
          for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
              // Fill with identifiable pattern
              tensor(i, r, c) = r * 10 + c + (i % 2);
            }
          }
        });
    writer.addPointData("StressTensor", adapt(tensor), 9);

    // 3. Cabana Slice with MultiDim Array (if enabled)
#if defined(MICROVTK_HAS_CABANA)
    using DataTypes = Cabana::MemberTypes<double[3][3]>;
    Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("particles", N);
    auto slice = Cabana::slice<0>(aosoa);

    Kokkos::parallel_for(
        "FillCabana",
        Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
        KOKKOS_LAMBDA(const int i) {
          for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
              slice(i, r, c) = (r + 1) * 100 + c;
            }
          }
        });

    writer.addPointData("CabanaTensor", adapt(slice), 9);
#endif

    // Topology (Cells)
    std::vector<int32_t> conn(N);
    std::vector<int32_t> offsets(N);
    std::vector<uint8_t> types(N, static_cast<uint8_t>(CellType::Vertex));
    for (int i = 0; i < N; ++i) {
      conn[i] = i;
      offsets[i] = i + 1;
    }
    writer.setCells(conn, offsets, types);

    writer.write("hpc_example.vtu");
  }
  Kokkos::finalize();
#else
  (void)argc;
  (void)argv;
#endif
  return 0;
}
