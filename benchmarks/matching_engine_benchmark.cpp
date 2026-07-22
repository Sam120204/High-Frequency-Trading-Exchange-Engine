#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/utsname.h>
#include <vector>

#include "matching_engine.h"

namespace {
using Clock = std::chrono::steady_clock;

template<typename T>
auto clear(Common::LFQueue<T> &queue) -> void {
  while (queue.getNextToRead()) {
    queue.updateReadIndex();
  }
}

auto percentile(const std::vector<long long> &samples, double value) -> long long {
  const auto index = static_cast<size_t>((samples.size() - 1) * value);
  return samples[index];
}

auto cpu_model() -> std::string {
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while (std::getline(cpuinfo, line)) {
    constexpr auto prefix = "model name\t: ";
    if (line.starts_with(prefix)) return line.substr(std::char_traits<char>::length(prefix));
  }
  return "unknown";
}
}

int main(int argc, char **argv) {
  const size_t iterations = argc > 1 ? std::stoull(argv[1]) : 1000;
  const size_t warmup = argc > 2 ? std::stoull(argv[2]) : 100;
  if (iterations == 0) {
    std::cerr << "iterations must be greater than zero\n";
    return EXIT_FAILURE;
  }

  Exchange::ClientRequestLFQueue requests(64);
  Exchange::ClientResponseLFQueue responses(64);
  Exchange::MEMarketUpdateLFQueue updates(64);
  Exchange::MatchingEngine engine(&requests, &responses, &updates, 64, 1048576);

  auto measure = [&](Common::OrderId order_id, bool record, std::vector<long long> &samples) {
    const Exchange::MEClientRequest add{Exchange::ClientRequestType::NEW, 1, 0, order_id,
                                        Common::Side::BUY, 100, 1};
    const auto start = Clock::now();
    engine.processClientRequest(&add);
    const auto end = Clock::now();
    clear(responses);
    clear(updates);

    const Exchange::MEClientRequest cancel{Exchange::ClientRequestType::CANCEL, 1, 0, order_id,
                                           Common::Side::BUY, 100, 1};
    engine.processClientRequest(&cancel);
    clear(responses);
    clear(updates);
    if (record) {
      samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
  };

  std::vector<long long> samples;
  samples.reserve(iterations);
  for (size_t i = 0; i < warmup; ++i) measure(i + 1, false, samples);
  for (size_t i = 0; i < iterations; ++i) measure(warmup + i + 1, true, samples);
  std::sort(samples.begin(), samples.end());

  utsname platform{};
  uname(&platform);
  long long sum = 0;
  for (const auto sample : samples) sum += sample;

  std::cout << "{\n"
            << "  \"benchmark\": \"synchronous_new_order_processing\",\n"
            << "  \"boundary\": \"MatchingEngine::processClientRequest(NEW), instrumented build\",\n"
            << "  \"iterations\": " << iterations << ",\n"
            << "  \"warmup_iterations\": " << warmup << ",\n"
            << "  \"compiler\": \"" << __VERSION__ << "\",\n"
            << "  \"cpu_model\": \"" << cpu_model() << "\",\n"
            << "  \"system\": \"" << platform.sysname << " " << platform.release << " " << platform.machine << "\",\n"
            << "  \"min_ns\": " << samples.front() << ",\n"
            << "  \"mean_ns\": " << sum / static_cast<long long>(samples.size()) << ",\n"
            << "  \"p50_ns\": " << percentile(samples, 0.50) << ",\n"
            << "  \"p99_ns\": " << percentile(samples, 0.99) << ",\n"
            << "  \"p999_ns\": " << percentile(samples, 0.999) << ",\n"
            << "  \"max_ns\": " << samples.back() << "\n"
            << "}\n";
}
