#pragma once

#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include <variant>

namespace tensorflow {
namespace lookup {

// 指标类型枚举
enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM
};

// 指标配置结构 - 统一管理所有指标信息
struct MetricConfig {
    const char* name;
    MetricType type;
    const char* prometheus_name;
    const char* description;
    std::vector<double> buckets;
};

// 指标配置常量 - 统一管理
namespace MetricConfigs {
    // 计数器指标
    static const MetricConfig LOOKUP_OPS_TOTAL = {
        "LOOKUP_OPS_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_lookup_ops_total", 
        "Total number of GlideKV Aerospike lookup operations", 
        {}
    };
    
    static const MetricConfig CONNECTION_FAILURES_TOTAL = {
        "CONNECTION_FAILURES_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_connection_failures_total", 
        "Total number of GlideKV Aerospike connection failures", 
        {}
    };
    
    static const MetricConfig BATCH_READ_FAILURES_TOTAL = {
        "BATCH_READ_FAILURES_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_batch_read_failures_total", 
        "Total number of GlideKV Aerospike batch read failures", 
        {}
    };
    
    static const MetricConfig KEYS_FOUND_TOTAL = {
        "KEYS_FOUND_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_keys_found_total", 
        "Total number of keys found in GlideKV Aerospike", 
        {}
    };
    
    static const MetricConfig KEYS_NOT_FOUND_TOTAL = {
        "KEYS_NOT_FOUND_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_keys_not_found_total", 
        "Total number of keys not found in GlideKV Aerospike", 
        {}
    };
    
    // 直方图指标
    static const MetricConfig TOTAL_LATENCY_US = {
        "TOTAL_LATENCY_US", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_total_latency_us", 
        "Distribution of total GlideKV Aerospike operation latency in microseconds", 
        {10, 50, 100, 500, 1000, 5000, 10000}
    };
    
    static const MetricConfig BATCH_READ_LATENCY_US = {
        "BATCH_READ_LATENCY_US", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_batch_read_latency_us", 
        "Distribution of GlideKV Aerospike batch read latency in microseconds", 
        {10, 50, 100, 500, 1000, 5000, 10000}
    };
    
    static const MetricConfig BATCH_SIZE = {
        "BATCH_SIZE", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_batch_size", 
        "Distribution of GlideKV Aerospike batch sizes", 
        {1, 5, 10, 50, 100, 500, 1000}
    };
    
    static const MetricConfig SUCCESS_RATE = {
        "SUCCESS_RATE", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_success_rate", 
        "Distribution of GlideKV Aerospike lookup success rates", 
        {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0}
    };
    
    // 仪表指标
    static const MetricConfig ACTIVE_CONNECTIONS = {
        "ACTIVE_CONNECTIONS", 
        MetricType::GAUGE, 
        "glidekv_aerospike_active_connections", 
        "Number of active Aerospike connections", 
        {}
    };
    
    static const MetricConfig CACHE_HIT_RATE = {
        "CACHE_HIT_RATE", 
        MetricType::GAUGE, 
        "glidekv_aerospike_cache_hit_rate", 
        "Current cache hit rate percentage", 
        {}
    };
    
    // 获取所有指标配置的数组
    static const MetricConfig* const ALL_CONFIGS[] = {
        &LOOKUP_OPS_TOTAL,
        &CONNECTION_FAILURES_TOTAL,
        &BATCH_READ_FAILURES_TOTAL,
        &KEYS_FOUND_TOTAL,
        &KEYS_NOT_FOUND_TOTAL,
        &TOTAL_LATENCY_US,
        &BATCH_READ_LATENCY_US,
        &BATCH_SIZE,
        &SUCCESS_RATE,
        &ACTIVE_CONNECTIONS,
        &CACHE_HIT_RATE
    };
    
    static constexpr size_t CONFIG_COUNT = sizeof(ALL_CONFIGS) / sizeof(ALL_CONFIGS[0]);
}

// 环境变量缓存 - 避免重复读取
class EnvVarCache {
private:
    static std::atomic<bool> global_enabled_;
    static std::unordered_map<std::string, std::atomic<bool>> metric_enabled_cache_;
    
public:
    static bool IsGlobalMetricsEnabled() {
        if (!global_enabled_.load(std::memory_order_acquire)) {
            InitializeGlobalCache();
        }
        return global_enabled_.load(std::memory_order_acquire);
    }
    
    static bool IsMetricEnabled(const char* metric_name) {
        // 首先检查全局开关
        if (!IsGlobalMetricsEnabled()) {
            return false;
        }
        
        // 从缓存中查找
        auto it = metric_enabled_cache_.find(metric_name);
        if (it != metric_enabled_cache_.end()) {
            return it->second.load(std::memory_order_acquire);
        }
        
        // 如果缓存中没有，初始化缓存
        InitializeMetricCache();
        
        // 再次查找
        it = metric_enabled_cache_.find(metric_name);
        if (it != metric_enabled_cache_.end()) {
            return it->second.load(std::memory_order_acquire);
        }
        
        // 如果缓存中没有，默认启用
        return true;
    }
    
private:
    static void InitializeGlobalCache() {
        bool expected = false;
        if (!global_enabled_.compare_exchange_strong(expected, true, 
                                                   std::memory_order_acq_rel)) {
            return;
        }
        
        const char* env = std::getenv("GLIDEKV_METRICS_ENABLED");
        bool enabled = env && (std::string(env) == "1" || std::string(env) == "true" || std::string(env) == "enabled");
        global_enabled_.store(enabled, std::memory_order_release);
    }
    
    static void InitializeMetricCache() {
        // 使用统一的指标名称
        for (size_t i = 0; i < MetricConfigs::CONFIG_COUNT; ++i) {
            const char* name = MetricConfigs::ALL_CONFIGS[i]->name;
            std::string env_var = std::string("GLIDEKV_METRIC_") + name;
            const char* env = std::getenv(env_var.c_str());
            
            // 修复逻辑：如果环境变量不存在，默认启用；如果存在，检查值
            bool enabled = true;  // 默认启用
            if (env) {
                std::string env_str(env);
                enabled = (env_str == "1" || env_str == "true" || env_str == "enabled" || env_str == "on");
            }
            metric_enabled_cache_[name].store(enabled, std::memory_order_release);
        }
        
        std::atomic_thread_fence(std::memory_order_release);
    }
};

// 静态成员初始化
std::atomic<bool> EnvVarCache::global_enabled_{false};
std::unordered_map<std::string, std::atomic<bool>> EnvVarCache::metric_enabled_cache_;

// 采样率缓存 - 避免重复读取环境变量
class SamplingRateCache {
private:
    static std::unordered_map<std::string, double> rate_cache_;
    static std::atomic<double> global_rate_;
    static std::atomic<bool> initialized_;
    
public:
    static double GetSamplingRate(const char* metric_name) {
        // 从缓存中查找 - 无锁读操作
        auto it = rate_cache_.find(metric_name);
        if (it != rate_cache_.end()) {
            return it->second;
        }
        
        // 如果缓存中没有，初始化缓存
        InitializeCache();
        
        // 再次查找
        it = rate_cache_.find(metric_name);
        if (it != rate_cache_.end()) {
            return it->second;
        }
        
        // 如果缓存中没有，使用全局采样率
        return GetGlobalSamplingRate();
    }
    
    static double GetGlobalSamplingRate() {
        if (!initialized_) {
            InitializeGlobalRate();
        }
        return global_rate_.load(std::memory_order_acquire);
    }
    
private:
    static void InitializeGlobalRate() {
        if (initialized_) {
            return;
        }
        
        const char* env = std::getenv("GLIDEKV_METRICS_SAMPLING_RATE");
        
        double rate = 1.0;  // 默认全局采样率为 1.0（100%）
        if (env) {
            rate = std::atof(env);
            // 确保采样率在 [0.0, 1.0] 范围内
            rate = std::max(0.0, std::min(1.0, rate));
        }
        initialized_ = true;
        global_rate_.store(rate, std::memory_order_release);
    }
    
    static void InitializeCache() {
        // 使用统一的指标名称
        for (size_t i = 0; i < MetricConfigs::CONFIG_COUNT; ++i) {
            const char* name = MetricConfigs::ALL_CONFIGS[i]->name;
            std::string env_var = "GLIDEKV_METRIC_" + std::string(name) + "_SAMPLING_RATE";
            const char* env = std::getenv(env_var.c_str());
            
            if (env) {
                double rate = std::atof(env);
                rate_cache_[name] = std::max(0.0, std::min(1.0, rate));
            } else {
                rate_cache_[name] = GetGlobalSamplingRate();
            }
        }
        
        // 确保所有数据写入完成
        std::atomic_thread_fence(std::memory_order_release);
    }
};

// 静态成员初始化
std::unordered_map<std::string, double> SamplingRateCache::rate_cache_;
std::atomic<double> SamplingRateCache::global_rate_{1.0};
std::atomic<bool> SamplingRateCache::initialized_{false};

// 线程本地随机数生成器缓存 - 真正的优化版本
class ThreadLocalRandomGenerator {
private:
    static thread_local std::mt19937 generator_;
    static thread_local std::uniform_real_distribution<double> distribution_;
    static thread_local bool initialized_;
    
public:
    static double GetRandomValue() {
        if (!initialized_) {
            Initialize();
        }
        return distribution_(generator_);
    }
    
private:
    static void Initialize() {
        if (!initialized_) {
            // 使用线程ID和当前时间作为种子，确保每个线程有不同的随机序列
            auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            auto time_seed = std::chrono::steady_clock::now().time_since_epoch().count();
            auto combined_seed = thread_id ^ time_seed;
            
            generator_.seed(combined_seed);
            initialized_ = true;
        }
    }
};

// 线程本地静态成员初始化
thread_local std::mt19937 ThreadLocalRandomGenerator::generator_;
thread_local std::uniform_real_distribution<double> ThreadLocalRandomGenerator::distribution_(0.0, 1.0);
thread_local bool ThreadLocalRandomGenerator::initialized_ = false;

// 检查是否应该采样（基于采样率）- 真正的高性能版本
inline bool ShouldSample(const char* metric_name) {
    double sampling_rate = SamplingRateCache::GetSamplingRate(metric_name);
    
    // 快速路径：常见的采样率值
    if (sampling_rate <= 0.0) {
        return false;
    }
    if (sampling_rate >= 1.0) {
        return true;
    }
    
    // 使用优化的线程本地随机数生成器
    return ThreadLocalRandomGenerator::GetRandomValue() <= sampling_rate;
}

// 指标值类型
using MetricValue = std::variant<prometheus::Counter*, prometheus::Histogram*, prometheus::Gauge*>;

// 精简的指标管理器 - 使用 map 维护指标
class GlideKVPrometheusMetricsManager {
private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    
    // 使用 map 维护所有指标
    std::unordered_map<std::string, std::atomic<MetricValue>> metrics_;
    
    // 初始化标志
    std::atomic<bool> initialized_{false};
    
    GlideKVPrometheusMetricsManager() = default;
    GlideKVPrometheusMetricsManager(const GlideKVPrometheusMetricsManager&) = delete;
    GlideKVPrometheusMetricsManager& operator=(const GlideKVPrometheusMetricsManager&) = delete;
    
    static GlideKVPrometheusMetricsManager& getInstance() {
        static GlideKVPrometheusMetricsManager instance;
        return instance;
    }
    
    // 优化的指标查找函数
public:
    static void Initialize(const std::string& listen_address = "127.0.0.1:8080") {
        if (!EnvVarCache::IsMetricEnabled(MetricConfigs::LOOKUP_OPS_TOTAL.name)) {
            std::cout << "📊 GlideKV Prometheus Metrics: DISABLED" << std::endl;
            return;
        }
        
        auto& instance = getInstance();
        
        // 简单检查是否已初始化
        if (instance.initialized_.load(std::memory_order_acquire)) {
            return; // 已经初始化过了
        }
        
        // 安全检查：确保只绑定到本地地址，除非明确指定
        std::string safe_address = listen_address;
        if (safe_address.find("0.0.0.0") == 0) {
            std::cout << "⚠️  WARNING: Binding to 0.0.0.0 may expose metrics to external networks!" << std::endl;
            std::cout << "   Consider using 127.0.0.1 for local-only access." << std::endl;
        }
        
        // 创建注册表和暴露器
        instance.registry_ = std::make_shared<prometheus::Registry>();
        instance.exposer_ = std::make_unique<prometheus::Exposer>(safe_address);
        
        // 注册指标收集器
        instance.exposer_->RegisterCollectable(instance.registry_);
        
        std::cout << "📊 GlideKV Prometheus Metrics: ENABLED" << std::endl;
        std::cout << "  Metrics endpoint: http://" << safe_address << "/metrics" << std::endl;
        std::cout << "  ⚠️  SECURITY: Ensure this endpoint is not exposed to external networks!" << std::endl;
        
        // 使用 MetricConfigs 中的配置创建指标
        for (size_t i = 0; i < MetricConfigs::CONFIG_COUNT; ++i) {
            const MetricConfig* config = MetricConfigs::ALL_CONFIGS[i];
            
            // 使用统一的指标启用检查
            if (!EnvVarCache::IsMetricEnabled(config->name)) {
                std::cout << "  ⏭️  Skipping disabled metric: " << config->name << std::endl;
                continue;
            }
            
            switch (config->type) {
                case MetricType::COUNTER: {
                    auto& counter_family = prometheus::BuildCounter()
                        .Name(config->prometheus_name)
                        .Help(config->description)
                        .Register(*instance.registry_);
                    instance.metrics_[config->name].store(&counter_family.Add({}), std::memory_order_release);
                    break;
                }
                case MetricType::GAUGE: {
                    auto& gauge_family = prometheus::BuildGauge()
                        .Name(config->prometheus_name)
                        .Help(config->description)
                        .Register(*instance.registry_);
                    instance.metrics_[config->name].store(&gauge_family.Add({}), std::memory_order_release);
                    break;
                }
                case MetricType::HISTOGRAM: {
                    auto& histogram_family = prometheus::BuildHistogram()
                        .Name(config->prometheus_name)
                        .Help(config->description)
                        .Register(*instance.registry_);
                    instance.metrics_[config->name].store(&histogram_family.Add({}, config->buckets), std::memory_order_release);
                    break;
                }
            }
            
            std::cout << "  ✅ Created metric: " << config->name << std::endl;
        }
        
        // 设置初始化标志
        instance.initialized_.store(true, std::memory_order_release);
        
        PrintConfig();
    }
    
    static void PrintConfig() {
        if (!EnvVarCache::IsMetricEnabled(MetricConfigs::LOOKUP_OPS_TOTAL.name)) {
            return;
        }
        
        auto& instance = getInstance();
        
        // 显示全局采样率
        double global_sampling_rate = SamplingRateCache::GetGlobalSamplingRate();
        std::cout << "  Global Sampling Rate: " << (global_sampling_rate * 100) << "%" << std::endl;
        
        // 打印所有指标状态
        std::cout << "  Metrics:" << std::endl;
        for (const auto& metric : instance.metrics_) {
            const std::string& metric_name = metric.first;
            double sampling_rate = SamplingRateCache::GetSamplingRate(metric_name.c_str());
            bool enabled = EnvVarCache::IsMetricEnabled(metric_name.c_str());
            std::cout << "    - " << metric_name << ": " 
                      << (enabled ? "✅" : "❌")
                      << " (sampling: " << (sampling_rate * 100) << "%)" << std::endl;
        }
    }
    
    static std::string GetMetricsEndpoint() {
        auto& instance = getInstance();
        if (instance.exposer_) {
            // 从 exposer 获取实际的监听地址
            return "http://127.0.0.1:8080/metrics";  // 默认地址，实际应该从 exposer 获取
        }
        return "";
    }

    // 只保留 GetMetric 静态函数
    static void* GetMetric(const std::string& metric_name) {
        auto& instance = getInstance();
        if (!instance.initialized_.load(std::memory_order_acquire)) {
            return nullptr;
        }
        
        auto it = instance.metrics_.find(metric_name);
        if (it != instance.metrics_.end()) {
            auto metric_value = it->second.load(std::memory_order_acquire);
            if (!metric_value.valueless_by_exception()) {
                return std::visit([](auto* ptr) -> void* { return ptr; }, metric_value);
            }
        }
        return nullptr;
    }
};

// 宏定义
#define GLIDEKV_METRIC_INCREMENT(metric_name) \
    do { \
        if (EnvVarCache::IsMetricEnabled(MetricConfigs::metric_name.name) && ShouldSample(MetricConfigs::metric_name.name)) { \
            auto* metric = static_cast<prometheus::Counter*>(GlideKVPrometheusMetricsManager::GetMetric(std::string(MetricConfigs::metric_name.name))); \
            if (metric) { \
                metric->Increment(); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_INCREMENT_BY(metric_name, value) \
    do { \
        if (EnvVarCache::IsMetricEnabled(MetricConfigs::metric_name.name) && ShouldSample(MetricConfigs::metric_name.name)) { \
            auto* metric = static_cast<prometheus::Counter*>(GlideKVPrometheusMetricsManager::GetMetric(std::string(MetricConfigs::metric_name.name))); \
            if (metric) { \
                metric->Increment(value); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_SET(metric_name, value) \
    do { \
        if (EnvVarCache::IsMetricEnabled(MetricConfigs::metric_name.name) && ShouldSample(MetricConfigs::metric_name.name)) { \
            auto* metric = static_cast<prometheus::Gauge*>(GlideKVPrometheusMetricsManager::GetMetric(std::string(MetricConfigs::metric_name.name))); \
            if (metric) { \
                metric->Set(value); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_OBSERVE(metric_name, value) \
    do { \
        if (EnvVarCache::IsMetricEnabled(MetricConfigs::metric_name.name) && ShouldSample(MetricConfigs::metric_name.name)) { \
            auto* metric = static_cast<prometheus::Histogram*>(GlideKVPrometheusMetricsManager::GetMetric(std::string(MetricConfigs::metric_name.name))); \
            if (metric) { \
                metric->Observe(value); \
            } \
        } \
    } while(0)

// 初始化函数
inline void InitializeGlideKVPrometheusMetrics(const std::string& listen_address = "127.0.0.1:8080") {
    GlideKVPrometheusMetricsManager::Initialize(listen_address);
}

// 打印配置函数
inline void PrintPrometheusMetricsConfig() {
    GlideKVPrometheusMetricsManager::PrintConfig();
}

// 安全检查函数
inline void PrintSecurityRecommendations() {
    std::cout << "\n🔒 GlideKV Metrics Security Recommendations:" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "1. ✅ Use 127.0.0.1 instead of 0.0.0.0 for local-only access" << std::endl;
    std::cout << "2. ✅ Configure firewall to block external access to metrics port" << std::endl;
    std::cout << "3. ✅ Use reverse proxy with authentication if external access needed" << std::endl;
    std::cout << "4. ✅ Monitor metrics endpoint for unauthorized access attempts" << std::endl;
    std::cout << "5. ✅ Regularly review metrics data for sensitive information" << std::endl;
    std::cout << "6. ✅ Disable metrics in production if not needed" << std::endl;
    std::cout << "7. ✅ Use environment variables to control metrics behavior" << std::endl;
    std::cout << "8. ✅ Consider using TLS/HTTPS for metrics transport" << std::endl;
    std::cout << std::endl;
}

} // namespace lookup
} // namespace tensorflow
