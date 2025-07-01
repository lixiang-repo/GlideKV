#pragma once
#include <random>
#include <thread>
#include <chrono>
#include <functional>

namespace tensorflow {
namespace lookup {

// 线程本地随机数生成器
class ThreadLocalRandomGenerator {
private:
    static thread_local std::mt19937 generator_;
    static thread_local std::uniform_real_distribution<double> distribution_;
    static thread_local bool initialized_;
    static void Initialize() {
        if (!initialized_) {
            auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            auto time_seed = std::chrono::steady_clock::now().time_since_epoch().count();
            auto combined_seed = thread_id ^ time_seed;
            generator_.seed(combined_seed);
            initialized_ = true;
        }
    }
public:
    static double GetRandomValue() {
        if (!initialized_) {
            Initialize();
        }
        return distribution_(generator_);
    }
};

} // namespace lookup
} // namespace tensorflow 