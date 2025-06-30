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
    LABEL_COUNTER,      // 延迟计数器，带标签分布
    LABEL_COUNTER_WITH_VALUE, // 带值、带标签的计数器，
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
// │ COUNTER           │ 只增不减，单调递增        │ 总操作数、总失败数、总请求数   │ increase(), rate(), deriv() │
// │                   │ 重启后从0开始            │ 连接失败次数、查找失败次数     │                           │
// │                   │ 适合计算速率和增量        │                             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ COUNTER_WITH_VALUE│ 可增加任意数值           │ 总延迟时间、总处理数据量       │ increase(), rate()         │
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
// │ LABEL_COUNTER     │ 带标签的延迟计数器        │ 精确延迟统计(0.1ms间隔)       │ 按标签分组查询              │
// │                   │ 提供精确延迟分布          │ 延迟范围分布统计              │                           │
// │                   │ 支持标签过滤             │                             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ LABEL_COUNTER_WITH_VALUE│ 带值、带标签的计数器│ 按标签统计失败数量           │ 按标签分组查询              │
// │                   │ 支持标签和数值累加        │ 按错误类型统计失败数          │ increase(), rate()         │
// │                   │ 适合分类统计             │ 按slot统计失败数             │                           │
// └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// │ 生产环境优化特性                                                                                         │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 特性              │ 说明                    │ 优势                        │ 应用场景                    │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 亚毫秒级精度      │ 延迟指标提供0.1ms间隔     │ 满足高性能场景需求           │ 低延迟应用监控              │
// │                   │ 支持0.1-1000ms范围      │ 精确性能分析                 │ 实时推荐系统                │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 精细分桶设计      │ Histogram分桶覆盖全范围   │ 支持分位数分析               │ 性能瓶颈定位                │
// │                   │ 0.1ms到1000ms动态分桶    │ 异常检测和告警               │ SLA监控                    │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 采样率控制        │ 环境变量控制采样率        │ 减少性能开销                 │ 高并发场景                  │
// │                   │ GLIDEKV_METRIC_SAMPLE_RATE│ 按需调整监控精度            │ 资源受限环境                │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 灵活开关控制      │ 所有指标支持环境变量开关   │ 按需启用监控                 │ 开发/测试/生产环境          │
// │                   │ GLIDEKV_ENABLE_METRICS   │ 降低运维复杂度               │ 故障排查时临时启用           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 内存优化          │ 使用Counter而非Gauge      │ 避免重启后数据丢失           │ 长期趋势分析                │
// │                   │ 支持rate()计算           │ 更准确的速率计算             │ 性能趋势监控                │
// └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘
//
// ┌─────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// │ 指标使用最佳实践                                                                                         │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 场景              │ 推荐指标类型             │ 查询示例                     │ 说明                        │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 吞吐量监控        │ COUNTER_WITH_VALUE       │ rate(total_keys[5m])        │ 计算每秒处理keys数量        │
// │                   │ TOTAL_KEYS               │                             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 延迟监控          │ HISTOGRAM                │ histogram_quantile(0.95, ...)│ 95%分位数延迟               │
// │                   │ TOTAL_LATENCY_HISTOGRAM  │                             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 错误率监控        │ COUNTER                  │ increase(failed_keys[5m]) / │ 计算失败率                  │
// │                   │ FAILED_KEYS              │ increase(total_keys[5m])    │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 缓存性能          │ COUNTER_WITH_VALUE       │ increase(cache_hit_keys[5m])/│ 缓存命中率                  │
// │                   │ CACHE_HIT_KEYS           │ increase(total_keys[5m])    │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 趋势分析          │ COUNTER                  │ deriv(rate(total_keys[5m])) │ 吞吐量变化趋势              │
// │                   │ TOTAL_KEYS               │                             │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 分类错误统计      │ LABEL_COUNTER_WITH_VALUE │ increase(label_failed_keys  │ 按错误类型统计失败率        │
// │                   │ LABEL_FAILED_KEYS        │ {error_type="timeout"}[5m]) │                           │
// ├─────────────────────────────────────────────────────────────────────────────────────────────────────────┤
// │ 精确延迟分析      │ LABEL_COUNTER            │ increase(label_latency      │ 按延迟范围统计分布          │
// │                   │ LABEL_LATENCY            │ {range="0.1-1ms"}[5m])      │                           │
// └─────────────────────────────────────────────────────────────────────────────────────────────────────────┘

// 精细分桶设计 - 生产环境优化
// ┌─────────┬─────────┬─────────────────────────────────────────────┐
// │ 区间    │ 间隔    │ 说明                                        │
// ├─────────┼─────────┼─────────────────────────────────────────────┤
// │ 0-1ms   │ 0.1ms   │ 亚毫秒级精度，监控高性能场景               │
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
    static const MetricConfig LOOKUP_OPS_TOTAL = {
        "LOOKUP_OPS_TOTAL", 
        MetricType::COUNTER, 
        "glidekv_aerospike_lookup_ops_total", 
        "Total number of GlideKV Aerospike lookup operations - 用于计算操作速率和成功率", 
        {}
    };
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
    static const MetricConfig LOOKUP_LATENCY_MS = {
        "LOOKUP_LATENCY_MS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_lookup_latency_ms", 
        "Total lookup latency in milliseconds - 用于计算平均查找延迟", 
        {}
    };
    static const MetricConfig LOOKUP_LATENCY_HISTOGRAM = {
        "LOOKUP_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_lookup_latency_histogram", 
        "Lookup operation latency distribution - 用于分位数分析和异常检测", 
        // 使用 BucketHelpers 创建完整的延迟桶边界
        BucketHelpers::CreateFullLatencyBuckets()
    };
    
    // 吞吐量指标 - 业务监控
    static const MetricConfig TOTAL_KEYS = {
        "TOTAL_KEYS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_total_keys", 
        "Total number of keys requested for lookup operations - 用于计算吞吐量和成功率", 
        {}
    };
    static const MetricConfig LABEL_FAILED_KEYS = {
        "LABEL_FAILED_KEYS", 
        MetricType::LABEL_COUNTER_WITH_VALUE, 
        "glidekv_aerospike_label_failed_keys", 
        "Total number of keys that failed during lookup operations by label - 用于计算失败率和错误分析", 
        {}
    };
    
    // 总延迟指标 - 端到端性能
    static const MetricConfig TOTAL_LATENCY_MS = {
        "TOTAL_LATENCY_MS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_total_latency_ms", 
        "Total operation latency including cache and lookup - 用于端到端性能分析", 
        {}
    };
    static const MetricConfig TOTAL_LATENCY_HISTOGRAM = {
        "TOTAL_LATENCY_HISTOGRAM", 
        MetricType::HISTOGRAM, 
        "glidekv_aerospike_total_latency_histogram", 
        "Total operation latency distribution including cache - 用于SLA监控和性能告警", 
        // 使用 BucketHelpers 创建完整的延迟桶边界
        BucketHelpers::CreateFullLatencyBuckets()
    };

    // 缓存性能指标 - 优化监控
    static const MetricConfig CACHE_HIT_KEYS = {
        "CACHE_HIT_KEYS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_cache_hit_keys", 
        "Total number of keys that hit the TBB cache - 用于缓存命中率分析和性能优化", 
        {}
    };
    
    // 兼容性指标 - 保持向后兼容
    static const MetricConfig FAILED_KEYS = {
        "FAILED_KEYS", 
        MetricType::COUNTER_WITH_VALUE, 
        "glidekv_aerospike_failed_keys", 
        "Total number of keys that failed during lookup operations - 用于计算失败率和错误分析（兼容性指标）", 
        {}
    };

    static const MetricConfig SLOT_ID_TOTAL_KEYS = {
        "SLOT_ID_TOTAL_KEYS", 
        MetricType::LABEL_COUNTER_WITH_VALUE, 
        "glidekv_aerospike_slot_id_total_keys", 
        "Total number of keys that requested during lookup operations by slot id - 用于计算失败率和错误分析（兼容性指标）", 
        {}
    };

    static const MetricConfig SLOT_ID_FAILED_KEYS = {
        "SLOT_ID_FAILED_KEYS", 
        MetricType::LABEL_COUNTER_WITH_VALUE, 
        "glidekv_aerospike_slot_id_failed_keys", 
        "Total number of keys that failed during lookup operations by slot id - 用于计算失败率和错误分析（兼容性指标）", 
        {}
    };
    
    // 指标分类配置 - 便于管理和查询
    static const std::vector<MetricConfig> ALL_CONFIGS = {
        // 操作计数指标 - 基础监控
        LOOKUP_OPS_TOTAL,        // 总操作数
        LOOKUP_FAILURES_TOTAL,   // 查找失败数
        CONNECTION_FAILURES_TOTAL, // 连接失败数
        
        // 吞吐量指标 - 业务监控
        TOTAL_KEYS,              // 总请求keys数
        LABEL_FAILED_KEYS,             // 失败keys数
        
        // 延迟指标 - Counter方式 (用于平均值计算)
        LOOKUP_LATENCY_MS,       // 查找延迟累计
        TOTAL_LATENCY_MS,        // 总延迟累计
        
        // 延迟指标 - Histogram方式 (用于分位数分析)
        LOOKUP_LATENCY_HISTOGRAM,  // 查找延迟分布
        TOTAL_LATENCY_HISTOGRAM,   // 总延迟分布

        // 缓存指标 - 性能优化
        CACHE_HIT_KEYS,          // 缓存命中keys数
        FAILED_KEYS,             // 失败keys数
    };
}


} // namespace lookup
} // namespace tensorflow 