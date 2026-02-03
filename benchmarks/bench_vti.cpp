#include <benchmark/benchmark.h>
#include <filesystem>
#include <microvtk/microvtk.hpp>
#include <vector>

using namespace microvtk;

static void BM_WriteVti(benchmark::State& state) {
  auto N = static_cast<int>(state.range(0));
  std::array<int, 6> extent = {0, N - 1, 0, N - 1, 0, N - 1};
  std::vector<double> data(static_cast<size_t>(N * N * N), 1.0);

  std::string filename = "bench_vti_" + std::to_string(N) + ".vti";

  for (auto _ : state) {
    (void)_;
    VtiWriter writer(extent);
    writer.addPointData("Scalars", data);
    writer.write(filename);
  }

  uint64_t bytes_written = static_cast<uint64_t>(N) * N * N * sizeof(double);
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(bytes_written));

  std::filesystem::remove(filename);
}
// N=32 (32k), 64 (256k), 128 (2M), 256 (16M)
BENCHMARK(BM_WriteVti)->Range(32, 256);

static void BM_WriteVtiCompressed(benchmark::State& state) {
  auto N = static_cast<int>(state.range(0));
  std::array<int, 6> extent = {0, N - 1, 0, N - 1, 0, N - 1};
  std::vector<double> data(static_cast<size_t>(N * N * N), 1.0);

  auto type = static_cast<core::CompressionType>(state.range(1));
  std::string filename = "bench_vti_comp_" + std::to_string(N) + ".vti";

  for (auto _ : state) {
    (void)_;
    VtiWriter writer(extent);
    writer.setCompression(type);
    writer.addPointData("Scalars", data);
    writer.write(filename);
  }

  std::filesystem::remove(filename);
}
BENCHMARK(BM_WriteVtiCompressed)
    ->Args({128, static_cast<int>(core::CompressionType::None)})
    ->Args({128, static_cast<int>(core::CompressionType::ZLib)})
    ->Args({128, static_cast<int>(core::CompressionType::LZ4)})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
