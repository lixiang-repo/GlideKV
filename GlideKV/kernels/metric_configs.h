#pragma once
#include <vector>
#include <string>

namespace tensorflow {
namespace lookup {

// 指标类型枚举
enum class MetricType {
    COUNTER,            // 普通计数器，每次+1
    COUNTER_WITH_VALUE, // 带值的计数器，可以增加任意数值
    GAUGE,              // 仪表盘，可以设置任意值
    HISTOGRAM,          // 直方图，用于分位数统计
    VALUE_COUNTER     // 延迟计数器，带标签分布
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
// 
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ 指标类型说明                                                             │
// ├─────────────────────────────────────────────────────────────────────────┤
// │ COUNTER        │ 累计计数器，只增不减，用于统计总操作数、总失败数等       │
// │ GAUGE          │ 可增可减的当前值，用于吞吐量、CPU使用率、内存使用率等   │
// │ HISTOGRAM      │ 观察值分布，用于延迟分布、成功率分布等，提供分位数统计   │
// │ LATENCY_COUNTER│ 带标签的延迟计数器，提供精确的0.1ms间隔延迟统计         │
// └─────────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ 生产环境优化特性                                                         │
// ├─────────────────────────────────────────────────────────────────────────┤
// │ 亚毫秒级精度   │ 延迟指标提供0.1ms间隔，满足高性能场景需求               │
// │ 精细分桶设计   │ Histogram分桶覆盖0.1ms到1000ms，支持分位数分析          │
// │ 采样率控制     │ 支持环境变量控制采样率，减少性能开销                    │
// │ 灵活开关控制   │ 所有指标支持环境变量开关，可按需启用                    │
// └─────────────────────────────────────────────────────────────────────────┘
namespace MetricConfigs {
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
    static const MetricConfig LOOKUP_FAILURES_TOTAL = {
        "LOOKUP_FAILURES_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_lookup_failures_total", 
        "Total number of GlideKV Aerospike lookup failures", 
        {}
    };
    static const MetricConfig LOOKUP_LATENCY_MS = {
        "LOOKUP_LATENCY_MS", 
        MetricType::VALUE_COUNTER, 
        "glidekv_aerospike_lookup_latency_ms", 
        "Total number of lookup operations by latency range (0.1ms intervals)", 
        {}
    };
    static const MetricConfig LOOKUP_LATENCY_HISTOGRAM = {
        "LOOKUP_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_lookup_latency_histogram", 
        "Lookup operation latency distribution", 
        // 精细分桶设计 - 生产环境优化
        // ┌─────────┬─────────┬─────────────────────────────────────────────┐
        // │ 区间    │ 间隔    │ 说明                                        │
        // ├─────────┼─────────┼─────────────────────────────────────────────┤
        // │ 0-1ms   │ 0.1ms   │ 亚毫秒级精度，监控高性能场景               │
        // │ 1-10ms  │ 0.5-2.5 │ 常见延迟范围，精细监控                     │
        // │ 10-100ms│ 5-25ms  │ 异常延迟监控，性能问题预警                 │
        // │ 100-1s  │ 50-250ms│ 严重性能问题，系统故障检测                 │
        // └─────────┴─────────┴─────────────────────────────────────────────┘
        {
            0.1,  0.2,  0.3,  0.4,  0.5,  0.6,  0.7,  0.8,  0.9,  1.0,  // 0-1ms: 0.1ms间隔
            1.5,  2.0,  2.5,  3.0,  4.0,  5.0,  7.5,  10.0,              // 1-10ms: 精细间隔
            15.0, 20.0, 30.0, 50.0, 75.0, 100.0,                         // 10-100ms: 适中间隔
            150.0, 200.0, 300.0, 500.0, 750.0, 1000.0                    // 100-1000ms: 粗间隔
        }
    };
    static const MetricConfig TOTAL_KEYS = {
        "TOTAL_KEYS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_total_keys", 
        "Total number of keys requested for lookup operations", 
        {}
    };
    static const MetricConfig FAILED_KEYS = {
        "FAILED_KEYS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_failed_keys", 
        "Total number of keys that failed during lookup operations", 
        {}
    };
    
    static const MetricConfig TOTAL_LATENCY_MS = {
        "TOTAL_LATENCY_MS", 
        MetricType::VALUE_COUNTER, 
        "glidekv_aerospike_total_latency_ms", 
        "Total number of operations by total latency range (0.1ms intervals)", 
        {}
    };
    static const MetricConfig TOTAL_LATENCY_HISTOGRAM = {
        "TOTAL_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_total_latency_histogram", 
        "Total operation latency distribution", 
        // 精细分桶设计 - 生产环境优化
        // ┌─────────┬─────────┬─────────────────────────────────────────────┐
        // │ 区间    │ 间隔    │ 说明                                        │
        // ├─────────┼─────────┼─────────────────────────────────────────────┤
        // │ 0-1ms   │ 0.1ms   │ 亚毫秒级精度，监控高性能场景               │
        // │ 1-10ms  │ 0.5-2.5 │ 常见延迟范围，精细监控                     │
        // │ 10-100ms│ 5-25ms  │ 异常延迟监控，性能问题预警                 │
        // │ 100-1s  │ 50-250ms│ 严重性能问题，系统故障检测                 │
        // └─────────┴─────────┴─────────────────────────────────────────────┘
        {
            0.1,  0.2,  0.3,  0.4,  0.5,  0.6,  0.7,  0.8,  0.9,  1.0,  // 0-1ms: 0.1ms间隔
            1.5,  2.0,  2.5,  3.0,  4.0,  5.0,  7.5,  10.0,              // 1-10ms: 精细间隔
            15.0, 20.0, 30.0, 50.0, 75.0, 100.0,                         // 10-100ms: 适中间隔
            150.0, 200.0, 300.0, 500.0, 750.0, 1000.0                    // 100-1000ms: 粗间隔
        }
    };

    static const MetricConfig SYSTEM_CPU_USAGE_PERCENT = {
        "SYSTEM_CPU_USAGE_PERCENT", 
        MetricType::GAUGE, 
        "glidekv_system_cpu_usage_percent", 
        "System CPU usage percentage", 
        {}
    };
    static const MetricConfig SYSTEM_MEMORY_USAGE_PERCENT = {
        "SYSTEM_MEMORY_USAGE_PERCENT", 
        MetricType::GAUGE, 
        "glidekv_system_memory_usage_percent", 
        "System memory usage percentage", 
        {}
    };
    static const MetricConfig SYSTEM_MEMORY_AVAILABLE_GB = {
        "SYSTEM_MEMORY_AVAILABLE_GB", 
        MetricType::GAUGE, 
        "glidekv_system_memory_available_gb", 
        "System available memory in GB", 
        {}
    };
    
    static const std::vector<MetricConfig> ALL_CONFIGS = {
        // 操作计数指标
        LOOKUP_OPS_TOTAL,
        LOOKUP_FAILURES_TOTAL,
        CONNECTION_FAILURES_TOTAL,
        
        // 吞吐量指标
        TOTAL_KEYS,
        FAILED_KEYS,
        
        // 延迟指标 - value Counter方式
        LOOKUP_LATENCY_MS,
        TOTAL_LATENCY_MS,
        
        // 延迟指标 - Histogram方式
        LOOKUP_LATENCY_HISTOGRAM,
        TOTAL_LATENCY_HISTOGRAM,
        
        // 系统资源指标
        SYSTEM_CPU_USAGE_PERCENT,
        SYSTEM_MEMORY_USAGE_PERCENT,
        SYSTEM_MEMORY_AVAILABLE_GB
    };
}

} // namespace lookup
} // namespace tensorflow 