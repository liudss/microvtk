#include <benchmark/benchmark.h>
#include <microvtk/microvtk.hpp>
#include <vector>
#include <filesystem>
#include <random>
#include <numeric>

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
    auto points = GeneratePoints(state.range(0));
    
    for (auto _ : state) {
        VtuWriter writer(DataFormat::Appended);
        writer.setPoints(points);
        benchmark::DoNotOptimize(writer);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)) * 3 * sizeof(double));
}
BENCHMARK(BM_SetPoints)->Range(1024, 1024 * 1024);

static void BM_WriteVtu(benchmark::State& state) {
    size_t num_points = state.range(0);
    auto points = GeneratePoints(num_points);
    std::vector<int32_t> conn(num_points);
    std::iota(conn.begin(), conn.end(), 0);
    std::vector<int32_t> offsets(num_points / 4); // Fake tets
    for(size_t i=0; i<offsets.size(); ++i) offsets[i] = (i+1)*4;
    std::vector<uint8_t> types(num_points / 4, static_cast<uint8_t>(CellType::Tetra));

    std::string filename = "bench_" + std::to_string(num_points) + ".vtu";

    for (auto _ : state) {
        VtuWriter writer(DataFormat::Appended);
        writer.setPoints(points);
        writer.setCells(conn, offsets, types);
        writer.write(filename);
    }
    
    // Cleanup
    std::filesystem::remove(filename);
}
BENCHMARK(BM_WriteVtu)->Range(1024, 256 * 1024); // Limit size for IO bench

static void BM_WriteVtu_Large(benchmark::State& state) {
    size_t num_points = state.range(0);
    auto points = GeneratePoints(num_points);
    std::vector<int32_t> conn(num_points);
    std::iota(conn.begin(), conn.end(), 0);
    std::vector<int32_t> offsets(num_points / 4);
    for(size_t i=0; i<offsets.size(); ++i) offsets[i] = (i+1)*4;
    std::vector<uint8_t> types(num_points / 4, static_cast<uint8_t>(CellType::Tetra));

    std::string filename = "bench_large_" + std::to_string(num_points) + ".vtu";

    for (auto _ : state) {
        VtuWriter writer(DataFormat::Appended);
        writer.setPoints(points);
        writer.setCells(conn, offsets, types);
        writer.write(filename);
    }
    
    uint64_t bytes_written = num_points * 3 * sizeof(double) + 
                             num_points * sizeof(int32_t) + 
                             offsets.size() * sizeof(int32_t) + 
                             types.size() * sizeof(uint8_t);
    state.SetBytesProcessed(int64_t(state.iterations()) * bytes_written);

    // Cleanup
    std::filesystem::remove(filename);
}
// Test up to ~8 million points (approx 200MB+ data) to see memory impact indirectly via speed
BENCHMARK(BM_WriteVtu_Large)->Range(1024 * 1024, 8 * 1024 * 1024);

struct Particle {
    double mass;
    double vel[3];
};

static void BM_AdaptAoS(benchmark::State& state) {
    size_t num_parts = state.range(0);
    std::vector<Particle> parts(num_parts);
    
    for (auto _ : state) {
        VtuWriter writer(DataFormat::Appended);
        // Measure the cost of adapting and appending
        writer.addPointData("Mass", adapt(parts, &Particle::mass));
        benchmark::DoNotOptimize(writer);
    }
     state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0)) * sizeof(double));
}
BENCHMARK(BM_AdaptAoS)->Range(1024, 1024 * 1024);

BENCHMARK_MAIN();
