#pragma once
#include <vector>
#include <string>

namespace tensorflow {
namespace lookup {

// 指标类型枚举
enum class MetricType {
    COUNTER,        // 计数器，可以增加任意数值
    GAUGE,          // 仪表盘，可以设置任意值
    HISTOGRAM,      // 直方图，用于分位数统计
    LABEL_COUNTER,  // 带标签的计数器
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
// ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// │ 指标类型详细说明                                                                                         │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 类型              │ 特性                    │ 用途示例                    │ Prometheus函数支持           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ COUNTER           │ 可增加任意数值           │ 总延迟时间、总处理数据量       │ increase(), rate()         │
// │                   │ 支持浮点数累加           │ 总成功/失败keys数量          │                           │
// │                   │ 适合计算平均值和总量      │ 缓存命中keys数量             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ GAUGE             │ 可增可减，当前值          │ CPU使用率、内存使用率         │ 直接查询，无需rate()        │
// │                   │ 重启后重置               │ 当前连接数、队列长度          │                           │
// │                   │ 适合监控当前状态          │ 缓存大小、活跃线程数          │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ HISTOGRAM         │ 观察值分布统计           │ 延迟分布、响应时间分布         │ histogram_quantile()       │
// │                   │ 自动分桶统计             │ 成功率分布、错误率分布         │ rate() + histogram_*       │
// │                   │ 支持分位数计算           │ 数据大小分布                  │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ LABEL_COUNTER     │ 带标签的计数器           │ 按标签统计失败数量           │ 按标签分组查询              │
// │                   │ 支持标签和数值累加        │ 按错误类型统计失败数          │ increase(), rate()         │
// │                   │ 适合分类统计             │ 按slot统计失败数             │                           │
// └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘

// 精细分桶设计 - 生产环境优化
// ┌─────────┬─────────┬─────────────────────────────────────────────┐
// │ 区间    │ 间隔    │ 说明                                        │
// ├─────────┼─────────┼─────────────────────────────────────────────┤
// │ 0-1ms   │ 0.05ms  │ 亚毫秒级精度，监控高性能场景               │
// │ 1-10ms  │ 0.5-2.5 │ 常见延迟范围，精细监控                     │
// │ 10-100ms│ 5-25ms  │ 异常延迟监控，性能问题预警                 │
// │ 100-1s  │ 50-250ms│ 严重性能问题，系统故障检测                 │
// └─────────┴─────────┴─────────────────────────────────────────────┘
namespace BucketHelpers {
    // 创建完整的延迟桶边界 (0.05ms - 1000ms)
    inline std::vector<double> CreateFullLatencyBuckets() {
        return {
            0.05, 0.1,  0.15, 0.2,  0.25, 0.3,  0.35, 0.4,  0.45, 0.5,   // 0-0.5ms: 0.05ms间隔
            0.6, 0.65,  0.7, 0.75,  0.8,  0.85,  0.9,  0.95,  1.0,       // 0.5-1ms: 0.05ms间隔
            1.5,  2.0,  2.5,  3.0,  4.0,  5.0, 6.5,  7.5, 8.0, 9.0, 10.0,// 1-10ms: 精细间隔
            15.0, 20.0, 30.0, 50.0, 75.0, 100.0,                         // 10-100ms: 适中间隔
            150.0, 200.0, 300.0, 500.0, 750.0, 1000.0                    // 100-1000ms: 粗间隔
        };
    }
}

namespace MetricConfigs {
    // 操作计数指标 - 基础监控
    static const MetricConfig CONNECTION_FAILURES_TOTAL = {
        "CONNECTION_FAILURES_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_connection_failures_total", 
        "Total number of GlideKV Aerospike connection failures - 用于监控连接稳定性", 
        {}
    };
    
    static const MetricConfig LOOKUP_FAILURES_TOTAL = {
        "LOOKUP_FAILURES_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_lookup_failures_total", 
        "Total number of GlideKV Aerospike lookup failures - 用于计算查找失败率", 
        {}
    };
    
    // 延迟监控指标 - 性能分析
    static const MetricConfig LOOKUP_LATENCY_HISTOGRAM = {
        "LOOKUP_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_lookup_latency_histogram", 
        "Lookup operation latency distribution - 用于分位数分析和异常检测", 
        BucketHelpers::CreateFullLatencyBuckets()
    };
    
    // 总延迟指标 - 端到端性能
    static const MetricConfig TOTAL_LATENCY_HISTOGRAM = {
        "TOTAL_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_total_latency_histogram", 
        "Total operation latency distribution including cache - 用于SLA监控和性能告警", 
        BucketHelpers::CreateFullLatencyBuckets()
    };

    // 缓存性能指标 - 优化监控
    static const MetricConfig CACHE_HIT_KEYS = {
        "CACHE_HIT_KEYS", 
        MetricType::COUNTER, 
        "glidekv_aerospike_cache_hit_keys", 
        "Total number of keys that hit the TBB cache - 用于缓存命中率分析和性能优化", 
        {}
    };
    
    // 按slot分类的指标 - 精细监控
    static const MetricConfig SLOT_ID_NUM_KEYS = {
        "SLOT_ID_NUM_KEYS", 
        MetricType::LABEL_COUNTER, 
        "glidekv_aerospike_slot_id_num_keys", 
        "Total number of keys processed by slot id - 用于按slot分析性能和错误分布", 
        {}
    };

    static const MetricConfig SLOT_ID_FAILED_KEYS = {
        "SLOT_ID_FAILED_KEYS", 
        MetricType::LABEL_COUNTER, 
        "glidekv_aerospike_slot_id_failed_keys", 
        "Total number of keys that failed by slot id - 用于按slot分析失败率和错误模式", 
        {}
    };

    // 数据一致性指标 - 质量监控
    static const MetricConfig VALUE_SIZE_NOT_EQUAL_TO_VALUE_FLAT_DIM = {
        "VALUE_SIZE_NOT_EQUAL_TO_VALUE_FLAT_DIM", 
        MetricType::COUNTER, 
        "glidekv_aerospike_value_size_not_equal_to_value_dim", 
        "Total number of keys with value size mismatch - 用于监控数据一致性问题", 
        {}
    };
    
    // 指标分类配置 - 便于管理和查询
    static const std::vector<MetricConfig> ALL_CONFIGS = {
        // 操作计数指标 - 基础监控
        LOOKUP_FAILURES_TOTAL,      // 查找失败数
        CONNECTION_FAILURES_TOTAL,  // 连接失败数
        
        // 延迟指标 - Histogram方式 (用于分位数分析)
        LOOKUP_LATENCY_HISTOGRAM,   // 查找延迟分布
        TOTAL_LATENCY_HISTOGRAM,    // 总延迟分布

        // 缓存指标 - 性能优化
        CACHE_HIT_KEYS,             // 缓存命中keys数

        // 按slot分类指标 - 精细监控
        SLOT_ID_NUM_KEYS,           // 按slot id统计总keys数
        SLOT_ID_FAILED_KEYS,        // 按slot id统计失败keys数

        // 数据一致性指标 - 质量监控
        VALUE_SIZE_NOT_EQUAL_TO_VALUE_FLAT_DIM, // 值大小不匹配
    };

}

} // namespace lookup
} // namespace tensorflow