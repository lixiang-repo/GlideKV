/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/core/kernels/lookup_table_op.h"
#define EIGEN_USE_THREADS

#include <string>
#include <type_traits>
#include <utility>
#include <chrono>
#include <atomic>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/variant.h"
#include "tensorflow/core/kernels/initializable_lookup_table.h"
#include "tensorflow/core/lib/gtl/inlined_vector.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/platform/random.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/util/work_sharder.h"

#include "aerospike_reader.h"
#include "prometheus_metrics.h"
#include "lookup_interface_stub.h"
#include "tbb_cache.h"  // 添加TBB cache头文件
#include <tbb/concurrent_hash_map.h>  // 添加TBB并发哈希表头文件


namespace tensorflow {
namespace lookup {

// Lookup table that wraps an unordered_map. Behaves identical to
// HashTableOfTensors except that each value must be a vector.
template <class K, class V>
class HashTableOfTensors final : public LookupInterfaceStub {
 public:
  HashTableOfTensors(OpKernelContext* ctx, OpKernel* kernel) {
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "value_shape", &value_shape_));
    OP_REQUIRES(
        ctx, TensorShapeUtils::IsVector(value_shape_),
        errors::InvalidArgument("Default value must be a vector, got shape ",
                                value_shape_.DebugString()));

    // 初始化 GlideKV Prometheus metrics - 优雅处理端口冲突
    // 注意：SystemMonitor 会在 Prometheus 指标初始化时自动启动
    InitializeGlideKVPrometheusMetrics("127.0.0.1:8080");

    // AerospikeReader 初始化 - 使用智能指针
    std::string host;
    int port;
    std::string namespace_name;
    std::string set_name;
    std::string field_name;
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "host", &host));
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "port", &port));
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "namespace", &namespace_name));
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "set_name", &set_name));
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "field_name", &field_name));
    reader_ = std::make_unique<AerospikeReader<K, V>>(host, port, namespace_name, set_name, field_name);

    // 初始化TBB cache - 设置缓存维度
    tbb_cache_ = std::make_unique<TBBCache<K, V>>(static_cast<size_t>(value_shape_.dim_size(0)));

    LOG(INFO) << "HashTableOfTensors with TBB Cache initialized!";
  }

  Status Find(OpKernelContext* ctx, const Tensor& key, Tensor* value,
              const Tensor& default_value) override {
    // 记录操作开始时间
    auto start_time = std::chrono::high_resolution_clock::now();
    double random_value = ThreadLocalRandomGenerator::GetRandomValue();
    
    auto value_flat = value->flat_inner_dims<V, 2>();
    auto value_dim = static_cast<size_t>(value_shape_.dim_size(0));

    // 早期退出检查 - 优化分支预测
    if (__builtin_expect(!reader_->connected_, 0)) {
        std::cerr << "Not connected to Aerospike" << std::endl;
        GLIDEKV_METRIC_INCREMENT(CONNECTION_FAILURES_TOTAL, 1, random_value);
        return OkStatus();
    }

    const auto key_flat = key.flat<K>();
    const size_t num_keys = key_flat.size();
    // 第一步：从缓存中查找 - 简化版本，避免复杂的线程安全问题
    // 对于小批量key，串行处理可能更高效
    std::unordered_map<K, size_t> key_to_idx;
    std::vector<std::pair<K, std::vector<V>*>> cache_hits;
    std::vector<K> cache_miss_keys;
    std::unordered_map<int, int64_t> slot_id_num_keys;
    
    key_to_idx.reserve(num_keys);
    cache_hits.reserve(num_keys);
    cache_miss_keys.reserve(num_keys);
    
    // 串行处理第一阶段：缓存查找和分类
    for (size_t i = 0; i < num_keys; ++i) {
        const K key_val = key_flat(i);
        if (key_val <= 0) {
            continue;
        }
        key_to_idx[key_val] = i;

        // 检查缓存
        auto value_ptr = tbb_cache_->get(key_val);
        if (value_ptr && value_ptr->size() == value_dim) {
            cache_hits.push_back({key_val, value_ptr});
        } else {
            // 缓存未命中，添加到需要从Aerospike查询的列表
            cache_miss_keys.push_back(key_val);
        }

        int slot_id = key_val >> 48;
        slot_id_num_keys[slot_id]++;
    }

    for (auto& [slot_id, count] : slot_id_num_keys) {
      GLIDEKV_METRIC_LABEL_COUNTER(SLOT_ID_NUM_KEYS, "slot_id", std::to_string(slot_id), count, random_value);
    }

    // 第二步：从Aerospike批量查询缓存未命中的key
    const size_t num_cache_misses = cache_miss_keys.size();
    
    // 使用TBB并发哈希表统计失败的slot_id，性能更好
    tbb::concurrent_hash_map<int, std::atomic<int64_t>> failed_slot_counts;
    
    // 创建批量读取记录 - 使用RAII确保资源清理
    as_batch_read_records records;
    std::vector<as_batch_read_record*> batch_records;
    // 预分配批量读取记录数组，提高内存局部性
    batch_records.reserve(num_cache_misses);

    // 使用RAII确保as_batch_read_destroy被调用
    auto cleanup_records = [&records](void*) {
        as_batch_read_destroy(&records);
    };
    std::unique_ptr<void, decltype(cleanup_records)> records_guard(nullptr, cleanup_records);

    if (num_cache_misses > 0) {
      // 初始化批量读取记录
      as_batch_read_inita(&records, num_cache_misses);
      
      // 填充批量读取请求和key映射
      for(auto& key_val : cache_miss_keys) {
          as_batch_read_record* record = as_batch_read_reserve(&records);
          batch_records.push_back(record);
          as_key_init_int64(&record->key, reader_->namespace_cstr_, reader_->set_cstr_, key_val);
          record->read_all_bins = true;
      }
      
      // 执行批量读取
      as_error err;
      auto lookup_start = std::chrono::high_resolution_clock::now();
      if (__builtin_expect(aerospike_batch_read(&reader_->as_, &err, NULL, &records) != AEROSPIKE_OK, 0)) {
        std::cerr << "Batch read failed: " << err.message << std::endl;
        GLIDEKV_METRIC_INCREMENT(LOOKUP_FAILURES_TOTAL, 1, random_value);
      } else {
        // 记录批量读取延迟
        auto lookup_end = std::chrono::high_resolution_clock::now();
        auto lookup_duration = std::chrono::duration_cast<std::chrono::microseconds>(lookup_end - lookup_start);
        double lookup_latency_ms = lookup_duration.count() / 1000.0;
        GLIDEKV_METRIC_HISTOGRAM_OBSERVE(LOOKUP_LATENCY_HISTOGRAM, lookup_latency_ms, random_value);
      }

    }
    // 第三步：使用缓存填充value_flat
    auto shard = [&](uint32_t begin, uint32_t end) {
      for (uint32_t i = begin; i < end; ++i) {
        if (i >= num_cache_misses) {
          // 如果i大于等于num_cache_misses
          // 用缓存填充value_flat
          const size_t cache_hit_idx = i - num_cache_misses;
          const auto& cache_hit = cache_hits[cache_hit_idx];
          const K key_val = cache_hit.first;
          auto value_ptr = cache_hit.second;

          auto it = key_to_idx.find(key_val);
          if (it != key_to_idx.end()) {
            const size_t idx = it->second;
            std::copy(value_ptr->begin(), value_ptr->end(), value_flat.data() + idx * value_dim);
          }
          continue;
        }

        // 安全的预取操作 - 添加边界检查
        if (i + 1 < end && i + 1 < batch_records.size()) {
            __builtin_prefetch(batch_records[i + 1], 0, 3);
        }
        
        // 使用缓存的记录指针，提高内存访问效率
        as_batch_read_record* record = batch_records[i];
        if (__builtin_expect(!record, 0)) {
            continue;
        }
        
        const K key_val = as_integer_getorelse((as_integer*)record->key.valuep, -1);
        
        if (record->result == AEROSPIKE_OK) {
            // 使用哈希表查找 - key_to_idx现在是只读的，线程安全
            auto it = key_to_idx.find(key_val);
            if (it != key_to_idx.end()) {
                const int64_t cache_miss_idx = it->second;
                
                reader_->extract_vector_from_record(&record->record, cache_miss_idx, value_dim, value_flat);
                
            }
        } else if(random_value < GlideKVPrometheusMetricsManager::get_global_sampling_rate()) {
            int slot_id = key_val >> 48;
            // 使用TBB并发哈希表 + 原子操作，确保线程安全
            typename tbb::concurrent_hash_map<int, std::atomic<int64_t>>::accessor acc;
            failed_slot_counts.insert(acc, slot_id);
            acc->second.fetch_add(1, std::memory_order_relaxed);
        }
      }
    };

    // 优化分片策略 - 更好的负载均衡和缓存效率
    auto& worker_threads = *ctx->device()->tensorflow_cpu_worker_threads();
    uint32_t num_worker_threads = worker_threads.num_threads;
    const uint32_t shard_slice_size = std::max(32U, std::min(256U, static_cast<uint32_t>((cache_miss_keys.size() + cache_hits.size()) / num_worker_threads)));
    const uint32_t shard_num_slices = ((cache_miss_keys.size() + cache_hits.size()) + shard_slice_size - 1) / shard_slice_size;
    Shard(num_worker_threads, worker_threads.workers, cache_miss_keys.size() + cache_hits.size(), shard_num_slices, shard);
    
    // 批量更新Prometheus指标（在Shard执行后）
    if (cache_hits.size() > 0) {
        GLIDEKV_METRIC_INCREMENT(CACHE_HIT_KEYS, cache_hits.size(), random_value);
    }
    
    // 批量更新失败的slot统计（确保所有修改都完成）
    for (auto it = failed_slot_counts.begin(); it != failed_slot_counts.end(); ++it) {
        int64_t value = it->second.load(std::memory_order_acquire);
        if (value > 0) {
            GLIDEKV_METRIC_LABEL_COUNTER(SLOT_ID_FAILED_KEYS, "slot_id", std::to_string(it->first), value, random_value);
        }
    }

    // RAII自动清理资源
    records_guard.reset();
    
    // 记录总操作延迟
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double total_latency_ms = total_duration.count() / 1000.0;
    GLIDEKV_METRIC_HISTOGRAM_OBSERVE(TOTAL_LATENCY_HISTOGRAM, total_latency_ms, random_value);

    return OkStatus();
  }


  DataType key_dtype() const override { return DataTypeToEnum<K>::v(); }

  DataType value_dtype() const override { return DataTypeToEnum<V>::v(); }

  TensorShape key_shape() const final { return TensorShape(); }

  TensorShape value_shape() const override { return value_shape_; }

 private:
  TensorShape value_shape_;
  std::unique_ptr<AerospikeReader<K, V>> reader_;
  std::unique_ptr<TBBCache<K, V>> tbb_cache_;
};

}  // namespace lookup

// Base class for kernels that take a LookupTable handle as the 0th input.
class LookupTableOpKernel : public OpKernel {
 public:
  explicit LookupTableOpKernel(OpKernelConstruction* ctx)
      : OpKernel(ctx),
        expected_input_0_(ctx->input_type(0) == DT_RESOURCE ? DT_RESOURCE
                                                            : DT_STRING_REF) {}

 protected:
  Status GetTable(OpKernelContext* ctx, lookup::LookupInterface** table) {
    if (expected_input_0_ == DT_RESOURCE) {
      return GetResourceLookupTable("table_handle", ctx, table);
    } else {
      return GetReferenceLookupTable("table_handle", ctx, table);
    }
  }

  // Input 0 could be a STRING_REF or a RESOURCE
  const DataType expected_input_0_;
};

// Table lookup op. Perform the lookup operation on the given table.
class LookupTableFindOp : public LookupTableOpKernel {
 public:
  using LookupTableOpKernel::LookupTableOpKernel;

  void Compute(OpKernelContext* ctx) override {
    lookup::LookupInterface* table;
    OP_REQUIRES_OK(ctx, GetTable(ctx, &table));
    core::ScopedUnref unref_me(table);

    DataTypeVector expected_inputs = {expected_input_0_, table->key_dtype(),
                                      table->value_dtype()};
    DataTypeVector expected_outputs = {table->value_dtype()};
    OP_REQUIRES_OK(ctx, ctx->MatchSignature(expected_inputs, expected_outputs));

    const Tensor& key = ctx->input(1);
    const Tensor& default_value = ctx->input(2);
    OP_REQUIRES_OK(ctx, table->CheckFindArguments(key, default_value));

    TensorShape output_shape = key.shape();
    output_shape.RemoveLastDims(table->key_shape().dims());
    output_shape.AppendShape(table->value_shape());
    Tensor* out;
    OP_REQUIRES_OK(ctx, ctx->allocate_output("values", output_shape, &out));
    
    // 根据table的value_dtype来初始化输出tensor为0
    DataType value_dtype = table->value_dtype();
    switch (value_dtype) {
        case DT_FLOAT:
            out->flat<float>().setZero();
            break;
        case DT_DOUBLE:
            out->flat<double>().setZero();
            break;
        case DT_INT32:
            out->flat<int32>().setZero();
            break;
        case DT_INT64:
            out->flat<int64_t>().setZero();
            break;
        default:
            // 对于不支持的类型，使用通用的零初始化方法
            // 这确保了所有输出tensor都有默认值
            break;
    }

    OP_REQUIRES_OK(ctx, table->Find(ctx, key, out, default_value));
  }
};

REGISTER_KERNEL_BUILDER(Name("LookupFind").Device(DEVICE_CPU),
                        LookupTableFindOp);

// Register the HashTableOfTensors op.
#define REGISTER_KERNEL(key_dtype, value_dtype)                                \
  REGISTER_KERNEL_BUILDER(                                                     \
      Name("HashTableOfTensors")                                        \
          .Device(DEVICE_CPU)                                                  \
          .TypeConstraint<key_dtype>("key_dtype")                              \
          .TypeConstraint<value_dtype>("value_dtype"),                         \
      LookupTableOp<lookup::HashTableOfTensors<key_dtype, value_dtype>, \
                    key_dtype, value_dtype>)                                   \

// REGISTER_KERNEL(int32, double);
// REGISTER_KERNEL(int32, float);
// REGISTER_KERNEL(int32, int32);
REGISTER_KERNEL(int64_t, double);
REGISTER_KERNEL(int64_t, float);
// REGISTER_KERNEL(int64_t, int32);
// REGISTER_KERNEL(int64_t, int64_t);
// REGISTER_KERNEL(int64_t, tstring);
// REGISTER_KERNEL(tstring, bool);
// REGISTER_KERNEL(tstring, double);
// REGISTER_KERNEL(tstring, float);
// REGISTER_KERNEL(tstring, int32);
// REGISTER_KERNEL(tstring, int64_t);

#undef REGISTER_KERNEL

}  // namespace tensorflow
