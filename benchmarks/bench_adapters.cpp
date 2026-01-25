#include <benchmark/benchmark.h>
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
  state.SetBytesProcessed(state.iterations() * size * sizeof(double));
}
BENCHMARK(BM_Vector_Iterate)->Range(1024, 1024 * 1024);

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
BENCHMARK(BM_Kokkos_Adapt_1D)->Range(1024, 1024 * 1024);

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
  state.SetBytesProcessed(state.iterations() * size * sizeof(double));
}
BENCHMARK(BM_Kokkos_Iterate_1D)->Range(1024, 1024 * 1024);

static void BM_Kokkos_Iterate_2D(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  Kokkos::View<double* [3], Kokkos::LayoutRight, Kokkos::HostSpace> view(
      "v", num_tuples);

  // Fill data
  for (size_t i = 0; i < num_tuples; ++i) {
    view(i, 0) = i;
    view(i, 1) = i;
    view(i, 2) = i;
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
  state.SetBytesProcessed(state.iterations() * total_size * sizeof(double));
}
BENCHMARK(BM_Kokkos_Iterate_2D)->Range(1024, 1024 * 1024);

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
BENCHMARK(BM_Cabana_Adapt_Scalar)->Range(1024, 1024 * 1024);

static void BM_Cabana_Iterate_Scalar(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  using DataTypes = Cabana::MemberTypes<double>;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  // Fill
  for (size_t i = 0; i < num_tuples; ++i) slice(i) = (double)i;

  auto flattened = microvtk::adapt(slice);

  for (auto _ : state) {
    double sum = 0.0;
    for (auto val : flattened) {
      sum += val;
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetBytesProcessed(state.iterations() * num_tuples * sizeof(double));
}
BENCHMARK(BM_Cabana_Iterate_Scalar)->Range(1024, 1024 * 1024);

static void BM_Cabana_Iterate_Array(benchmark::State& state) {
  size_t num_tuples = state.range(0);
  using DataTypes = Cabana::MemberTypes<double[3]>;
  Cabana::AoSoA<DataTypes, Kokkos::HostSpace> aosoa("aosoa", num_tuples);
  auto slice = Cabana::slice<0>(aosoa);

  // Fill
  for (size_t i = 0; i < num_tuples; ++i) {
    slice(i, 0) = (double)i;
    slice(i, 1) = (double)i;
    slice(i, 2) = (double)i;
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
  state.SetBytesProcessed(state.iterations() * total_elements * sizeof(double));
}
BENCHMARK(BM_Cabana_Iterate_Array)->Range(1024, 1024 * 1024);

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
