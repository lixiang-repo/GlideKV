# GlideKV Prometheus 查询指南

## 概述

本文档提供了 GlideKV 的 Prometheus 监控查询语句，包括性能监控、错误分析、缓存优化等场景的查询示例。

## 查看原始指标

在开始使用 Prometheus 查询之前，您可以通过以下命令查看 GlideKV 暴露的原始指标：

```bash
# 查看所有指标
curl http://localhost:8080/metrics

# 查看特定指标 (例如只查看 GlideKV 相关指标)
curl http://localhost:8080/metrics | grep glidekv

# 查看指标并格式化输出
curl http://localhost:8080/metrics | grep glidekv | sort

# 查看指标数量统计
curl http://localhost:8080/metrics | grep glidekv | wc -l
```

### 指标示例输出

```bash
# 连接失败计数
glidekv_aerospike_connection_failures_total 0

# 查找失败计数  
glidekv_aerospike_lookup_failures_total 0

# 缓存命中计数
glidekv_aerospike_cache_hit_keys 1234

# 查找延迟直方图
glidekv_aerospike_lookup_latency_histogram_bucket{le="0.05"} 100
glidekv_aerospike_lookup_latency_histogram_bucket{le="0.1"} 200
glidekv_aerospike_lookup_latency_histogram_bucket{le="0.2"} 300
glidekv_aerospike_lookup_latency_histogram_bucket{le="+Inf"} 500
glidekv_aerospike_lookup_latency_histogram_count 500
glidekv_aerospike_lookup_latency_histogram_sum 75.5

# 按slot统计的keys数
glidekv_aerospike_slot_id_num_keys{slot_id="1"} 1000
glidekv_aerospike_slot_id_num_keys{slot_id="2"} 1500
glidekv_aerospike_slot_id_failed_keys{slot_id="1"} 5
glidekv_aerospike_slot_id_failed_keys{slot_id="2"} 8
```

## 核心指标说明

| 指标名称 | 类型 | 用途 | 关键查询 |
|---------|------|------|----------|
| `glidekv_aerospike_connection_failures_total` | Counter | 连接失败数 | 监控连接稳定性 |
| `glidekv_aerospike_lookup_failures_total` | Counter | 查找失败数 | 计算查找失败率 |
| `glidekv_aerospike_cache_hit_keys` | Counter | 缓存命中数 | 计算缓存命中率 |
| `glidekv_aerospike_slot_id_num_keys` | Label Counter | 按slot统计总keys数 | 按slot分析性能 |
| `glidekv_aerospike_slot_id_failed_keys` | Label Counter | 按slot统计失败keys数 | 按slot分析失败率 |
| `glidekv_aerospike_lookup_latency_histogram` | Histogram | 查找延迟分布 | 分位数分析 |
| `glidekv_aerospike_total_latency_histogram` | Histogram | 总延迟分布 | SLA监控 |
| `glidekv_aerospike_value_size_not_equal_to_value_dim` | Counter | 数据大小不匹配数 | 监控数据一致性 |

## 性能监控查询

### 1. 延迟分析

```promql
# 95%分位数延迟 (查找操作)
histogram_quantile(0.95, sum(increase(glidekv_aerospike_lookup_latency_histogram_bucket[5m])) by (le))

# 99%分位数延迟 (查找操作)
histogram_quantile(0.99, sum(increase(glidekv_aerospike_lookup_latency_histogram_bucket[5m])) by (le))

# 95%分位数延迟 (总操作，包含缓存)
histogram_quantile(0.95, sum(increase(glidekv_aerospike_total_latency_histogram_bucket[5m])) by (le))

# 99%分位数延迟 (总操作，包含缓存)
histogram_quantile(0.99, sum(increase(glidekv_aerospike_total_latency_histogram_bucket[5m])) by (le))

# 平均延迟 (查找操作)
histogram_quantile(0.5, sum(increase(glidekv_aerospike_lookup_latency_histogram_bucket[5m])) by (le))

# 平均延迟 (总操作)
histogram_quantile(0.5, sum(increase(glidekv_aerospike_total_latency_histogram_bucket[5m])) by (le))

```

### 2. 吞吐量监控

```promql
# 总吞吐量 (keys/sec) - 过去5分钟
(sum(increase(glidekv_aerospike_slot_id_num_keys[5m])) - sum(increase(glidekv_aerospike_cache_hit_keys[5m])) - sum(increase(glidekv_aerospike_slot_id_failed_keys[5m])) ) / sum(glidekv_aerospike_lookup_latency_histogram_sum[5m]) * 1000
```

### 3. 缓存性能监控

```promql
# 缓存命中率 (过去5分钟)
sum(increase(glidekv_aerospike_cache_hit_keys[5m])) / sum(increase(glidekv_aerospike_slot_id_num_keys[5m]))
```

### 4. 错误率监控

```promql
# 查找操作失败率
sum(increase(glidekv_aerospike_lookup_failures_total[5m])) / sum(increase(glidekv_aerospike_total_latency_histogram_count[5m]))

# 连接失败次数
increase(glidekv_aerospike_connection_failures_total[5m])

# 按slot统计miss率
increase(glidekv_aerospike_slot_id_failed_keys[5m]) / increase(glidekv_aerospike_slot_id_num_keys[5m])

topk(5, increase(glidekv_aerospike_slot_id_failed_keys[5m]) / increase(glidekv_aerospike_slot_id_num_keys[5m])
)

# 总体miss率 (基于slot统计)
sum(increase(glidekv_aerospike_slot_id_failed_keys[5m])) / sum(increase(glidekv_aerospike_slot_id_num_keys[5m]))
```

### 5. 数据一致性监控

```promql
# 数据大小不匹配率
increase(glidekv_aerospike_value_size_not_equal_to_value_dim[5m]) / sum(increase(glidekv_aerospike_slot_id_num_keys[5m]))

```

## 环境变量配置

```bash
# 启用指标收集
export GLIDEKV_ENABLE_METRICS=true

# 设置采样率 (0.0-1.0)
export GLIDEKV_METRIC_SAMPLE_RATE=1.0

```
