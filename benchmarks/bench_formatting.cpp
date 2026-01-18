#include <benchmark/benchmark.h>
#include <sstream>
#include <string>
#include <format>
#include <vector>
#include <iostream>

// Simulate the workload of writing XML attributes
// e.g. <DataArray type="Float64" Name="Points" NumberOfComponents="3" ... >

static void BM_Stream_Attributes(benchmark::State& state) {
    std::string type = "Float64";
    std::string name = "Points";
    int components = 3;
    size_t offset = 123456;

    // Use a stringstream to avoid disk I/O noise, measuring formatting overhead
    std::stringstream ss; 
    
    for (auto _ : state) {
        ss.str(""); // clear buffer
        ss << "<DataArray";
        ss << " type=\"" << type << "\"";
        ss << " Name=\"" << name << "\"";
        ss << " NumberOfComponents=\"" << components << "\"";
        ss << " format=\"appended\"";
        ss << " offset=\"" << offset << "\"";
        ss << "/>";
        benchmark::DoNotOptimize(ss.str());
    }
}
BENCHMARK(BM_Stream_Attributes);

static void BM_Format_Attributes(benchmark::State& state) {
    std::string type = "Float64";
    std::string name = "Points";
    int components = 3;
    size_t offset = 123456;

    std::string buffer;
    buffer.reserve(256);

    for (auto _ : state) {
        buffer.clear();
        std::format_to(std::back_inserter(buffer),
            R"(<DataArray type="{}" Name="{}" NumberOfComponents="{}" format="appended" offset="{}"/>)",
            type, name, components, offset);
        benchmark::DoNotOptimize(buffer);
    }
}
BENCHMARK(BM_Format_Attributes);

// Also test just the number formatting which is common
static void BM_Stream_Int(benchmark::State& state) {
    int val = 123456789;
    std::stringstream ss;
    for (auto _ : state) {
        ss.str("");
        ss << val;
        benchmark::DoNotOptimize(ss.str());
    }
}
BENCHMARK(BM_Stream_Int);

static void BM_Format_Int(benchmark::State& state) {
    int val = 123456789;
    std::string buffer;
    for (auto _ : state) {
        buffer.clear();
        std::format_to(std::back_inserter(buffer), "{}", val);
        benchmark::DoNotOptimize(buffer);
    }
}
BENCHMARK(BM_Format_Int);

static void BM_Format_Attributes_Optimized(benchmark::State& state) {
    std::string type = "Float64";
    std::string name = "Points";
    int components = 3;
    size_t offset = 123456;

    std::string buffer;
    buffer.resize(256); // Pre-allocate size

    for (auto _ : state) {
        auto res = std::format_to_n(buffer.data(), buffer.size(),
            R"(<DataArray type="{}" Name="{}" NumberOfComponents="{}" format="appended" offset="{}"/>)",
            type, name, components, offset);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Format_Attributes_Optimized);

BENCHMARK_MAIN();
