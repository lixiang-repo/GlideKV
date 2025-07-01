#pragma once
#include <random>
#include <thread>
#include <chrono>
#include <atomic>

namespace tensorflow {
namespace lookup {

// 线程本地随机数生成器
class ThreadLocalRandomGenerator {
private:
    static thread_local std::mt19937 generator_;
    static thread_local std::uniform_real_distribution<double> distribution_;
    static thread_local std::atomic<bool> initialized_;
    static void Initialize() {
        if (!initialized_.load(std::memory_order_acquire)) {
            auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            auto time_seed = std::chrono::steady_clock::now().time_since_epoch().count();
            auto combined_seed = thread_id ^ time_seed;
            generator_.seed(combined_seed);
            initialized_.store(true, std::memory_order_release);
        }
    }
public:
    static double GetRandomValue() {
        if (!initialized_.load(std::memory_order_acquire)) {
            Initialize();
        }
        return distribution_(generator_);
    }
};

} // namespace lookup
} // namespace tensorflow 