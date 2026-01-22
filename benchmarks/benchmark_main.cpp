#include <array>
#include <benchmark/benchmark.h>
#include <filesystem>
#include <list>
#include <microvtk/core/compressor.hpp>
#include <microvtk/microvtk.hpp>
#include <numeric>
#include <random>
#include <vector>

using namespace microvtk;

// Generate random data
static std::vector<double> GeneratePoints(size_t num_points) {
  std::vector<double> points;
  points.reserve(num_points * 3);
  std::mt19937 gen(42);
  std::uniform_real_distribution<> dis(0.0, 100.0);
  for (size_t i = 0; i < num_points * 3; ++i) {
    points.push_back(dis(gen));
  }
  return points;
}

static void BM_SetPoints(benchmark::State& state) {
  auto points = GeneratePoints(static_cast<size_t>(state.range(0)));

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    writer.setPoints(points);
    benchmark::DoNotOptimize(writer);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          state.range(0) * 3 *
                          static_cast<int64_t>(sizeof(double)));
}
BENCHMARK(BM_SetPoints)->Range(1024, 1024L * 1024L);

static void BM_WriteVtu(benchmark::State& state) {
  auto num_points = static_cast<size_t>(state.range(0));
  auto points = GeneratePoints(num_points);
  std::vector<int32_t> conn(num_points);
  std::iota(conn.begin(), conn.end(), 0);
  std::vector<int32_t> offsets(num_points / 4);  // Fake tets
  for (size_t i = 0; i < offsets.size(); ++i) {
    offsets[i] = static_cast<int32_t>((i + 1) * 4);
  }
  std::vector<uint8_t> types(num_points / 4,
                             static_cast<uint8_t>(CellType::Tetra));

  std::string filename = "bench_" + std::to_string(num_points) + ".vtu";

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    writer.setPoints(points);
    writer.setCells(conn, offsets, types);
    writer.write(filename);
  }

  // Cleanup
  std::filesystem::remove(filename);
}
BENCHMARK(BM_WriteVtu)->Range(1024, 256L * 1024L);  // Limit size for IO bench

static void BM_WriteVtu_Large(benchmark::State& state) {
  auto num_points = static_cast<size_t>(state.range(0));
  auto points = GeneratePoints(num_points);
  std::vector<int32_t> conn(num_points);
  std::iota(conn.begin(), conn.end(), 0);
  std::vector<int32_t> offsets(num_points / 4);
  for (size_t i = 0; i < offsets.size(); ++i) {
    offsets[i] = static_cast<int32_t>((i + 1) * 4);
  }
  std::vector<uint8_t> types(num_points / 4,
                             static_cast<uint8_t>(CellType::Tetra));

  std::string filename = "bench_large_" + std::to_string(num_points) + ".vtu";

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    writer.setPoints(points);
    writer.setCells(conn, offsets, types);
    writer.write(filename);
  }

  uint64_t bytes_written =
      (num_points * 3 * sizeof(double)) + (num_points * sizeof(int32_t)) +
      (offsets.size() * sizeof(int32_t)) + (types.size() * sizeof(uint8_t));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(bytes_written));

  // Cleanup
  std::filesystem::remove(filename);
}
// Test up to ~8 million points (approx 200MB+ data) to see memory impact
// indirectly via speed
BENCHMARK(BM_WriteVtu_Large)
    ->Range(1024L * 1024L, static_cast<int64_t>(8) * 1024 * 1024);

static void BM_WriteCompressed(benchmark::State& state) {
  auto num_points = static_cast<size_t>(state.range(0));
  auto points = GeneratePoints(num_points);
  std::vector<int32_t> conn(num_points);
  std::iota(conn.begin(), conn.end(), 0);
  std::vector<int32_t> offsets(num_points / 4);
  for (size_t i = 0; i < offsets.size(); ++i) {
    offsets[i] = static_cast<int32_t>((i + 1) * 4);
  }
  std::vector<uint8_t> types(num_points / 4,
                             static_cast<uint8_t>(CellType::Tetra));

  auto type = static_cast<core::CompressionType>(state.range(1));

  std::string filename = "bench_comp_" + std::to_string(num_points) + "_" +
                         std::to_string(state.range(1)) + ".vtu";

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    writer.setCompression(type);
    writer.setPoints(points);
    writer.setCells(conn, offsets, types);
    writer.write(filename);
  }

  // Cleanup
  std::filesystem::remove(filename);
}

// Compare None(0), ZLib(1), LZ4(2) for 256k points
BENCHMARK(BM_WriteCompressed)
    ->Args({262144, static_cast<int>(core::CompressionType::None)})
    ->Args({262144, static_cast<int>(core::CompressionType::ZLib)})
    ->Args({262144, static_cast<int>(core::CompressionType::LZ4)})
    ->Unit(benchmark::kMillisecond);

static void BM_WriteNonContiguous(benchmark::State& state) {
  auto num_points = static_cast<size_t>(state.range(0));
  // std::list is a non-contiguous container
  std::list<double> points_list;
  {
    std::vector<double> tmp = GeneratePoints(num_points);
    std::ranges::copy(tmp, std::back_inserter(points_list));
  }

  std::string filename = "bench_list_" + std::to_string(num_points) + ".vtu";

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    writer.setPoints(points_list);
    writer.write(filename);
  }
  std::filesystem::remove(filename);
}
BENCHMARK(BM_WriteNonContiguous)->Range(1024, 65536);

struct Particle {
  double mass;
  std::array<double, 3> vel;
};

static void BM_AdaptAoS(benchmark::State& state) {
  auto num_parts = static_cast<size_t>(state.range(0));
  std::vector<Particle> parts(num_parts);

  for (auto _ : state) {
    (void)_;
    VtuWriter writer(DataFormat::Appended);
    // Measure the cost of adapting and appending
    writer.addPointData("Mass", adapt(parts, &Particle::mass));
    benchmark::DoNotOptimize(writer);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          state.range(0) *
                          static_cast<int64_t>(sizeof(double)));
}
BENCHMARK(BM_AdaptAoS)->Range(1024, 1024L * 1024L);

BENCHMARK_MAIN();
