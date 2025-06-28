# GlideKV Prometheus 查询指南

## 吞吐量计算查询

### 使用Counter计算真实吞吐量（推荐）

使用 `TOTAL_KEYS`、`FAILED_KEYS` 和 `LOOKUP_LATENCY_MS` 三个Counter来计算真实的吞吐量：

```promql
# 计算过去10分钟的平均吞吐量 (keys/sec)
(increase(glidekv_aerospike_total_keys[10m]) - increase(glidekv_aerospike_failed_keys[10m])) / (increase(glidekv_aerospike_lookup_latency_ms[10m]) / 1000)
```

## 性能监控查询

### 延迟分析

```promql
# 过去10分钟的平均查找延迟 (ms)
increase(glidekv_aerospike_lookup_latency_ms[10m]) / increase(glidekv_aerospike_lookup_ops_total[10m])

# 过去10分钟的平均总延迟 (ms)
increase(glidekv_aerospike_total_latency_ms[10m]) / increase(glidekv_aerospike_lookup_ops_total[10m])

# lookup延迟分位数 (使用Histogram)
histogram_quantile(0.95, increase(glidekv_aerospike_lookup_latency_histogram_bucket[10m]))
histogram_quantile(0.99, increase(glidekv_aerospike_lookup_latency_histogram_bucket[10m]))

# 总延迟分位数 (使用Histogram)
histogram_quantile(0.95, increase(glidekv_aerospike_total_latency_histogram_bucket[10m]))
histogram_quantile(0.99, increase(glidekv_aerospike_total_latency_histogram_bucket[10m]))
```

### 错误率监控

```promql
# 连接失败次数
increase(glidekv_aerospike_connection_failures_total[10m])

# 查找失败率
increase(glidekv_aerospike_lookup_failures_total[10m]) / increase(glidekv_aerospike_lookup_ops_total[10m])

# Keys失败率 (使用Counter计算)
increase(glidekv_aerospike_failed_keys[10m]) / increase(glidekv_aerospike_total_keys[10m])

```

### 系统资源监控

```promql
# CPU使用率
glidekv_system_cpu_usage_percent

# 内存使用率
glidekv_system_memory_usage_percent

# 可用内存 (GB)
glidekv_system_memory_available_gb
```

## 告警规则示例

## 优势分析

### 使用Counter的优势

1. **准确性**：基于累积数据，不受单次请求异常值影响
2. **稳定性**：长期趋势更稳定，适合监控和告警
3. **灵活性**：可以计算任意时间窗口的吞吐量
4. **标准性**：符合Prometheus最佳实践
5. **可扩展性**：支持rate()、deriv()等时间序列函数

## 建议

- **生产环境**：使用Counter方式计算吞吐量
- **监控告警**：基于Counter计算的吞吐量设置告警
- **性能分析**：结合延迟和错误率指标全面分析性能

### 计算逻辑说明

1. **成功keys数量** = `TOTAL_KEYS - FAILED_KEYS`
2. **平均处理时间** = `LOOKUP_LATENCY_MS / LOOKUP_OPS_TOTAL`
3. **吞吐量** = `成功keys数量 / 平均处理时间`

注意：`LOOKUP_LATENCY_MS` 是累积的延迟时间（毫秒）。 
