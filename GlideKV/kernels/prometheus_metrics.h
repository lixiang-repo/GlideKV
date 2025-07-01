#pragma once

#include <prometheus/counter.h>
#include <prometheus/histogram.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cmath>
#include <variant>
#include <mutex>
#include <sstream>
#include <iomanip>

// 包含系统监控头文件
#include "thread_local_random.h"
#include "metric_configs.h"

namespace tensorflow {
namespace lookup {

// Forward declaration for system monitor initialization
void InitializeGlobalSystemMonitor();

// 指标值类型
using MetricValue = std::variant<prometheus::Counter*, prometheus::Histogram*, prometheus::Gauge*>;

// 精简的指标管理器 - 使用单例模式
class GlideKVPrometheusMetricsManager {
private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;

    // 使用 map 维护所有指标 - 避免16字节原子操作
    // 新增：带值的计数器映射
    std::unordered_map<std::string, prometheus::Counter*> counters_;
    std::unordered_map<std::string, prometheus::Gauge*> gauges_;
    std::unordered_map<std::string, prometheus::Histogram*> histograms_;
    // 缓存 Counter Family 以避免重复创建
    std::unordered_map<std::string, prometheus::Family<prometheus::Counter>*> label_counter_;

    // 初始化标志
    std::atomic<bool> initialized_{false};
    double global_rate_{2.0};
    
    // 私有构造函数 - 单例模式
    GlideKVPrometheusMetricsManager() = default;
    GlideKVPrometheusMetricsManager(const GlideKVPrometheusMetricsManager&) = delete;
    GlideKVPrometheusMetricsManager& operator=(const GlideKVPrometheusMetricsManager&) = delete;
    
    static GlideKVPrometheusMetricsManager& getInstance() {
        static GlideKVPrometheusMetricsManager instance;
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            std::cout << "🔄 GlideKVPrometheusMetricsManager singleton initialized!" << std::endl;
        });
        return instance;
    }
    
    // 优化的指标查找函数
public:
    static bool is_global_metrics_enabled() {
        const char* env = std::getenv("GLIDEKV_METRICS_ENABLED");
        bool global_enabled_ = env && (std::string(env) == "1" || std::string(env) == "true" || std::string(env) == "enabled");
        return global_enabled_;
    }

    static bool is_metric_enabled(const char* metric_name) {
        std::string env_var = std::string("GLIDEKV_METRIC_") + metric_name;
        const char* env = std::getenv(env_var.c_str());
        
        // 修复逻辑：如果环境变量不存在，默认启用；如果存在，检查值
        bool enabled = true;  // 默认启用
        if (env) {
            std::string env_str(env);
            enabled = (env_str == "1" || env_str == "true" || env_str == "enabled" || env_str == "on");
        }
        return enabled;
    }

    static double get_global_sampling_rate() {
        auto& instance = getInstance();
        if(instance.global_rate_ > 1.0) {
            const char* env = std::getenv("GLIDEKV_METRICS_SAMPLING_RATE");
            double rate = 1.0;  // 默认全局采样率为 1.0（100%）
            if (env) {
                rate = std::atof(env);
                // 确保采样率在 [0.0, 1.0] 范围内
                rate = std::max(0.0, std::min(1.0, rate));
            }
            instance.global_rate_ = rate;
        }
        return instance.global_rate_;
    }

    static bool is_port_available(const std::string& host, int port) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) return false;
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        int result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
        close(sockfd);
        return result == 0;
    }

    static void Initialize(const std::string& listen_address = "127.0.0.1:8080") {
        auto& instance = getInstance();
        
        // 检查是否已初始化 - 避免重复初始化
        if (instance.initialized_.load(std::memory_order_acquire)) {
            std::cout << "📊 GlideKV Prometheus Metrics: Already initialized, skipping..." << std::endl;
            return;
        }
        
        // 检查全局开关
        bool global_enabled_ = is_global_metrics_enabled();
        if (!global_enabled_) {
            std::cout << "📊 GlideKV Prometheus Metrics: DISABLED" << std::endl;
            return;
        }
        
        std::cout << "📊 GlideKV Prometheus Metrics: Initializing..." << std::endl;
        
        // 解析地址和端口
        std::string host = "127.0.0.1";
        int port = 8080;
        size_t colon_pos = listen_address.find(':');
        if (colon_pos != std::string::npos) {
            host = listen_address.substr(0, colon_pos);
            port = std::stoi(listen_address.substr(colon_pos + 1));
        }
        
        std::string final_address;
        bool success = false;
        
        // 自动端口选择模式 - 优雅处理端口冲突
        for (int attempt = 0; attempt < 10; ++attempt) {
            int try_port = port + attempt;
            final_address = host + ":" + std::to_string(try_port);
            if (!is_port_available(host, try_port)) {
                continue;
            }
            instance.registry_ = std::make_shared<prometheus::Registry>();
            instance.exposer_ = std::make_unique<prometheus::Exposer>(final_address);
            instance.exposer_->RegisterCollectable(instance.registry_);
            success = true;
            if (attempt > 0) {
                std::cout << "📊 Found available port: " << final_address << std::endl;
            }
            break;
        }
        
        if (!success) {
            std::cerr << "❌ Failed to find available port in range " << port << "-" << (port + 9) << std::endl;
            std::cerr << "   GlideKV Prometheus Metrics will be disabled." << std::endl;
            return;
        }
        
        // 安全检查：确保只绑定到本地地址，除非明确指定
        if (host == "0.0.0.0") {
            std::cout << "⚠️  WARNING: Binding to 0.0.0.0 may expose metrics to external networks!" << std::endl;
            std::cout << "   Consider using 127.0.0.1 for local-only access." << std::endl;
        }
        
        std::cout << "📊 GlideKV Prometheus Metrics: ENABLED" << std::endl;
        std::cout << "  Metrics endpoint: http://" << final_address << "/metrics" << std::endl;
        std::cout << "  🔄 Auto port selection: enabled" << std::endl;
        std::cout << "  ⚠️  SECURITY: Ensure this endpoint is not exposed to external networks!" << std::endl;
        
        // 使用 MetricConfigs 中的配置创建指标
        for (size_t i = 0; i < MetricConfigs::ALL_CONFIGS.size(); ++i) {
            const MetricConfig& config = MetricConfigs::ALL_CONFIGS[i];
            
            // 使用统一的指标启用检查
            if (!is_metric_enabled(config.name) || !global_enabled_) {
                std::cout << "  ⏭️  Skipping disabled metric: " << config.name << std::endl;
                continue;
            }
            
            switch (config.type) {
                case MetricType::COUNTER: {
                    // 带值的 Counter
                    auto& counter_family = prometheus::BuildCounter()
                        .Name(config.prometheus_name)
                        .Help(config.description)
                        .Register(*instance.registry_);
                    instance.counters_[config.name] = &counter_family.Add({});
                    break;
                }
                case MetricType::GAUGE: {
                    auto& gauge_family = prometheus::BuildGauge()
                        .Name(config.prometheus_name)
                        .Help(config.description)
                        .Register(*instance.registry_);
                    instance.gauges_[config.name] = &gauge_family.Add({});
                    break;
                }
                case MetricType::HISTOGRAM: {
                    auto& histogram_family = prometheus::BuildHistogram()
                        .Name(config.prometheus_name)
                        .Help(config.description)
                        .Register(*instance.registry_);
                    // 使用 prometheus::Histogram::BucketBoundaries 来创建分桶边界
                    prometheus::Histogram::BucketBoundaries buckets(config.buckets);
                    instance.histograms_[config.name] = &histogram_family.Add({}, buckets);
                    break;
                }
                case MetricType::LABEL_COUNTER: {
                    // 带值的延迟 Counter - 使用带标签的 Counter Family
                    auto& label_counter_ = prometheus::BuildCounter()
                        .Name(config.prometheus_name)
                        .Help(std::string(config.description))
                        .Register(*instance.registry_);
                    instance.label_counter_[config.name] = &label_counter_;
                    break;
                }
            }
        }
        
        // 设置初始化标志 - 最后设置，确保所有初始化完成
        instance.initialized_.store(true, std::memory_order_release);
        
        std::cout << "📊 GlideKV Prometheus Metrics: Initialization completed!" << std::endl;
        PrintConfig();
    }
    
    static void PrintConfig() {
        if (!is_global_metrics_enabled()) {
            return;
        }
        
        auto& instance = getInstance();
        
        // 显示全局采样率
        double global_sampling_rate = get_global_sampling_rate();
        
        // 打印所有指标状态
        std::cout << "  Metrics: (Sampling rate: " << global_sampling_rate * 100 << "% of all metrics)" << std::endl;
        for (const auto& metric : instance.counters_) {
            const std::string& metric_name = metric.first;
            bool enabled = is_metric_enabled(metric_name.c_str());
            std::cout << "    - " << metric_name << ": " 
                      << (enabled ? "✅" : "❌") << std::endl;
        }
        for (const auto& metric : instance.gauges_) {
            const std::string& metric_name = metric.first;
            bool enabled = is_metric_enabled(metric_name.c_str());
            std::cout << "    - " << metric_name << ": " 
                      << (enabled ? "✅" : "❌") << std::endl;
        }
        for (const auto& metric : instance.histograms_) {
            const std::string& metric_name = metric.first;
            bool enabled = is_metric_enabled(metric_name.c_str());
            std::cout << "    - " << metric_name << ": " 
                      << (enabled ? "✅" : "❌") << std::endl;
        }
        for (const auto& metric : instance.label_counter_) {
            const std::string& metric_name = metric.first;
            bool enabled = is_metric_enabled(metric_name.c_str());
            std::cout << "    - " << metric_name << ": " 
                      << (enabled ? "✅" : "❌") << std::endl;
        }

    }

    // 只保留 GetMetric 静态函数
    static void* GetMetric(const std::string& metric_name) {
        auto& instance = getInstance();
        if (!instance.initialized_.load(std::memory_order_acquire)) {
            return nullptr;
        }

        auto counter_it = instance.counters_.find(metric_name);
        if (counter_it != instance.counters_.end()) {
            return counter_it->second;
        }
        
        auto gauge_it = instance.gauges_.find(metric_name);
        if (gauge_it != instance.gauges_.end()) {
            return gauge_it->second;
        }
        
        auto histogram_it = instance.histograms_.find(metric_name);
        if (histogram_it != instance.histograms_.end()) {
            return histogram_it->second;
        }

        auto label_counter_it = instance.label_counter_.find(metric_name);
        if (label_counter_it != instance.label_counter_.end()) {
            return label_counter_it->second;
        }
        
        return nullptr;
    }

    static void RecordLabelCounterWithValue(const std::string& metric_name, const std::string& label_name, const std::string& label, double value) {
        auto& instance = getInstance();
        if (!instance.initialized_.load(std::memory_order_acquire)) {
            return;
        }

        auto label_counter_it = instance.label_counter_.find(metric_name);
        if (label_counter_it == instance.label_counter_.end()) {
            return; // 延迟指标未启用
        }

        auto& label_counter = *label_counter_it->second;
        auto& counter = label_counter.Add({{label_name, label}});
        counter.Increment(value);
    }
};

// 宏定义
#define GLIDEKV_METRIC_INCREMENT(metric_name, value, random_value) \
    do { \
        if (random_value < GlideKVPrometheusMetricsManager::get_global_sampling_rate()) { \
            auto* metric = static_cast<prometheus::Counter*>(GlideKVPrometheusMetricsManager::GetMetric(MetricConfigs::metric_name.name)); \
            if (metric) { \
                metric->Increment(value); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_SET(metric_name, value, random_value) \
    do { \
        if (random_value < GlideKVPrometheusMetricsManager::get_global_sampling_rate()) { \
            auto* metric = static_cast<prometheus::Gauge*>(GlideKVPrometheusMetricsManager::GetMetric(MetricConfigs::metric_name.name)); \
            if (metric) { \
                metric->Set(value); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_HISTOGRAM_OBSERVE(metric_name, value, random_value) \
    do { \
        if (random_value < GlideKVPrometheusMetricsManager::get_global_sampling_rate()) { \
            auto* metric = static_cast<prometheus::Histogram*>(GlideKVPrometheusMetricsManager::GetMetric(MetricConfigs::metric_name.name)); \
            if (metric) { \
                metric->Observe(value); \
            } \
        } \
    } while(0)

#define GLIDEKV_METRIC_LABEL_COUNTER(metric_name, label_name, label, value, random_value) \
    do { \
        if (random_value < GlideKVPrometheusMetricsManager::get_global_sampling_rate()) { \
            GlideKVPrometheusMetricsManager::RecordLabelCounterWithValue(MetricConfigs::metric_name.name, label_name, label, value); \
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
