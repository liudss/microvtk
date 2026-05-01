#include <benchmark/benchmark.h>
#include <microvtk/indexing_adapter.hpp>
#include <microvtk/microvtk.hpp>
#include <numeric>
#include <random>
#include <vector>

#if defined(MICROVTK_HAS_KOKKOS) || defined(MICROVTK_HAS_CABANA)
#include <Kokkos_Core.hpp>
#endif

#ifdef MICROVTK_HAS_CABANA
#include <Cabana_Core.hpp>
#endif

using namespace microvtk;

// ----------------------------------------------------------------------------
// Baseline: std::vector
// ----------------------------------------------------------------------------
static void BM_Vector_Iterate(benchmark::State& state) {
  size_t size = state.range(0);
  std::vector<double> v(size);
  std::iota(v.begin(), v.end(), 0.0);

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : v) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Vector_Iterate)->Range(1024, 1024LL * 1024LL);

// ----------------------------------------------------------------------------
// Indexing Adapter Benchmarks (Morton / Z-Curve)
// ----------------------------------------------------------------------------

// 1. Manual Index Calculation (Reference for overhead)
// Simulates what a user might write manually to iterate in Raster order
// but access data stored in Morton order.
static void BM_Indexing_Manual(benchmark::State& state) {
  size_t n = static_cast<size_t>(std::cbrt(state.range(0)));
  size_t size = n * n * n;  // ensure cube

  std::vector<double> v(size);
  std::iota(v.begin(), v.end(), 0.0);

  for (auto _ : state) {
    double sum = 0.0;
    // Iterate in Raster Order (z, y, x nested loops flattened)
    for (size_t i = 0; i < size; ++i) {
      // Decode linear -> (x,y,z)
      size_t z = i / (n * n);
      size_t rem = i % (n * n);
      size_t y = rem / n;
      size_t x = rem % n;

      // Encode (x,y,z) -> Morton
      size_t morton_idx = microvtk::detail::morton_encode_3d(
          static_cast<uint32_t>(x), static_cast<uint32_t>(y),
          static_cast<uint32_t>(z));

      sum += v[morton_idx];
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Indexing_Manual)->Range(4096, 262144);  // 16^3 to 64^3 approx

// 2. Adapter (View)
// Measures the overhead of the abstraction layer
static void BM_Indexing_Adapter(benchmark::State& state) {
  size_t n = static_cast<size_t>(std::cbrt(state.range(0)));
  size_t size = n * n * n;
  std::array<size_t, 3> dims = {n, n, n};

  std::vector<double> v(size);
  std::iota(v.begin(), v.end(), 0.0);

  // Create view once (lightweight)
  auto view = v | microvtk::views::reorder_z_curve(dims);

  for (auto _ : state) {
    double sum = 0.0;
    // The writer will iterate this view linearly
    for (auto val : view) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Indexing_Adapter)->Range(4096, 262144);

// ----------------------------------------------------------------------------
// Indexing Adapter Benchmarks (2D)
// ----------------------------------------------------------------------------

static void BM_Indexing_Manual_2D(benchmark::State& state) {
  size_t n = static_cast<size_t>(std::sqrt(state.range(0)));
  size_t size = n * n;

  std::vector<double> v(size);
  std::iota(v.begin(), v.end(), 0.0);

  for (auto _ : state) {
    double sum = 0.0;
    for (size_t i = 0; i < size; ++i) {
      // Decode linear -> (x,y)
      size_t y = i / n;
      size_t x = i % n;

      // Encode (x,y) -> Morton
      size_t morton_idx = microvtk::detail::morton_encode_2d(
          static_cast<uint32_t>(x), static_cast<uint32_t>(y));

      sum += v[morton_idx];
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Indexing_Manual_2D)
    ->Args({4096})    // 64^2
    ->Args({16384})   // 128^2
    ->Args({262144}); // 512^2

static void BM_Indexing_Adapter_2D(benchmark::State& state) {
  size_t n = static_cast<size_t>(std::sqrt(state.range(0)));
  size_t size = n * n;
  std::array<size_t, 2> dims = {n, n};

  std::vector<double> v(size);
  std::iota(v.begin(), v.end(), 0.0);

  auto view = v | microvtk::views::reorder_z_curve(dims);

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : view) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Indexing_Adapter_2D)
    ->Args({4096})    // 64^2
    ->Args({16384})   // 128^2
    ->Args({262144}); // 512^2

// ----------------------------------------------------------------------------
// Kokkos Benchmarks
// ----------------------------------------------------------------------------
#ifdef MICROVTK_HAS_KOKKOS

static void BM_Kokkos_Adapt_1D(benchmark::State& state) {
  size_t size = state.range(0);
  Kokkos::View<double*, Kokkos::HostSpace> view("v", size);

  for (auto _ : state) {
    auto span = microvtk::adapt(view);
    benchmark::DoNotOptimize(span);
  }
}
BENCHMARK(BM_Kokkos_Adapt_1D)->Range(1024, 1024LL * 1024LL);

static void BM_Kokkos_Iterate_1D(benchmark::State& state) {
  size_t size = state.range(0);
  Kokkos::View<double*, Kokkos::HostSpace> view("v", size);
  Kokkos::parallel_for(
      "init", size, KOKKOS_LAMBDA(const int i) { view(i) = (double)i; });
  Kokkos::fence();

  auto span = microvtk::adapt(view);

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : span) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(size) * sizeof(double));
}
BENCHMARK(BM_Kokkos_Iterate_1D)->Range(1024, 1024LL * 1024LL);

static void BM_Kokkos_Iterate_2D(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  Kokkos::View<double* [3], Kokkos::LayoutRight, Kokkos::HostSpace> view(
      "v", num_tuples);

  // Fill data
  for (size_t i = 0; i < num_tuples; ++i) {
    view(i, 0) = static_cast<double>(i);
    view(i, 1) = static_cast<double>(i);
    view(i, 2) = static_cast<double>(i);
  }

  auto span = microvtk::adapt(view);
  size_t total_size = num_tuples * 3;

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : span) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(total_size) * sizeof(double));
}
BENCHMARK(BM_Kokkos_Iterate_2D)->Range(1024, 1024LL * 1024LL);

static void BM_Kokkos_Iterate_2D_LayoutLeft(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  // LayoutLeft: Column Major (Not C-contiguous for rank 2 if we view it as
  // array of structs)
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  Kokkos::View<double* [3], Kokkos::LayoutLeft, Kokkos::HostSpace> view(
      "v_ll", num_tuples);

  // Fill data
  for (size_t i = 0; i < num_tuples; ++i) {
    view(i, 0) = static_cast<double>(i);
    view(i, 1) = static_cast<double>(i);
    view(i, 2) = static_cast<double>(i);
  }

  // This should trigger the Slow Path (iota | transform)
  auto range = microvtk::adapt(view);
  size_t total_size = num_tuples * 3;

  for (auto _ : state) {
    double sum = 0.0;
    // Iterate through the virtual flattened range
    for (auto val : range) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(total_size) * sizeof(double));
}
BENCHMARK(BM_Kokkos_Iterate_2D_LayoutLeft)->Range(1024, 1024LL * 1024LL);

#endif  // MICROVTK_HAS_KOKKOS

// ----------------------------------------------------------------------------
// Cabana Benchmarks
// ----------------------------------------------------------------------------
#ifdef MICROVTK_HAS_CABANA

static void BM_Cabana_Adapt_Scalar(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  using DataTypes = Cabana::MemberTypes<double>;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  for (auto _ : state) {
    auto flattened = microvtk::adapt(slice);
    benchmark::DoNotOptimize(flattened);
  }
}
BENCHMARK(BM_Cabana_Adapt_Scalar)->Range(1024, 1024LL * 1024LL);

static void BM_Cabana_Iterate_Scalar(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  using DataTypes = Cabana::MemberTypes<double>;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  // Fill
  for (size_t i = 0; i < num_tuples; ++i) slice(i) = static_cast<double>(i);

  auto flattened = microvtk::adapt(slice);

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : flattened) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(num_tuples) * sizeof(double));
}
BENCHMARK(BM_Cabana_Iterate_Scalar)->Range(1024, 1024LL * 1024LL);

static void BM_Cabana_Iterate_Array(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  using DataTypes = Cabana::MemberTypes<double[3]>;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  // Fill
  for (size_t i = 0; i < num_tuples; ++i) {
    slice(i, 0) = static_cast<double>(i);
    slice(i, 1) = static_cast<double>(i);
    slice(i, 2) = static_cast<double>(i);
  }

  auto flattened = microvtk::adapt(slice);
  size_t total_elements = num_tuples * 3;

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : flattened) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(total_elements) *
                          sizeof(double));
}
BENCHMARK(BM_Cabana_Iterate_Array)->Range(1024, 1024LL * 1024LL);

#endif  // MICROVTK_HAS_CABANA

// ----------------------------------------------------------------------------
// Custom Main
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
#if defined(MICROVTK_HAS_KOKKOS) || defined(MICROVTK_HAS_CABANA)
  Kokkos::initialize(argc, argv);
#endif

  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
#if defined(MICROVTK_HAS_KOKKOS) || defined(MICROVTK_HAS_CABANA)
    Kokkos::finalize();
#endif
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();

#if defined(MICROVTK_HAS_KOKKOS) || defined(MICROVTK_HAS_CABANA)
  Kokkos::finalize();
#endif
  return 0;
}
