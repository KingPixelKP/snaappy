#include "math/math.hpp"

#include <benchmark/benchmark.h>

static void BM_VectorAdd(benchmark::State& state) {
  const math::Vector left(1, 2);
  const math::Vector right(3, -1);

  for (auto _ : state) {
    auto sum = left.add(right);
    benchmark::DoNotOptimize(sum);
  }
}

BENCHMARK(BM_VectorAdd);
