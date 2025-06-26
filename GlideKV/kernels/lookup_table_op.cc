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

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/variant.h"
#include "tensorflow/core/kernels/initializable_lookup_table.h"
#include "tensorflow/core/lib/gtl/inlined_vector.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/platform/random.h"
#include "tensorflow/core/framework/tensor.h"
#include <iostream>
#include "tensorflow/core/util/work_sharder.h"
#include "aerospike_reader.h"
#include "glidekv_prometheus_metrics.h"

namespace tensorflow {
namespace lookup {

// Lookup table that wraps an unordered_map. Behaves identical to
// HashTableOfTensors except that each value must be a vector.
template <class K, class V>
class HashTableOfTensors final : public LookupInterface {
 public:
  HashTableOfTensors(OpKernelContext* ctx, OpKernel* kernel) {
    OP_REQUIRES_OK(ctx, GetNodeAttr(kernel->def(), "value_shape", &value_shape_));
    OP_REQUIRES(
        ctx, TensorShapeUtils::IsVector(value_shape_),
        errors::InvalidArgument("Default value must be a vector, got shape ",
                                value_shape_.DebugString()));

    std::cout << "=== Aerospike Vector Reader ===" << std::endl;

    // 初始化 GlideKV Prometheus metrics
    InitializeGlideKVPrometheusMetrics("127.0.0.1:8080");
    PrintPrometheusMetricsConfig();

    // AerospikeReader 初始化 - 使用智能指针
    reader_ = std::make_unique<AerospikeReader<K, V>>();
    reader_->init();
    
  }

  Status Find(OpKernelContext* ctx, const Tensor& key, Tensor* value,
              const Tensor& default_value) override {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 增加操作计数
    GLIDEKV_METRIC_INCREMENT(LOOKUP_OPS_TOTAL);
    
    if (!reader_->connected_) {
        std::cerr << "Not connected to Aerospike" << std::endl;
        // 记录连接失败计数
        GLIDEKV_METRIC_INCREMENT(CONNECTION_FAILURES_TOTAL);
        return OkStatus();
    }
    
    const auto key_flat = key.flat<K>();
    const size_t num_keys = key_flat.size();
    if (num_keys == 0) {
        return OkStatus();
    }
    
    // 记录批量大小
    GLIDEKV_METRIC_OBSERVE(BATCH_SIZE, num_keys);
    
    // 创建批量读取记录
    as_batch_records records;
    as_batch_records_inita(&records, num_keys);
    
    // 创建key到index的映射
    std::unordered_map<int64_t, size_t> key_to_idx;
    key_to_idx.reserve(num_keys);
    
    // 填充批量读取请求
    for (size_t i = 0; i < num_keys; ++i) {
        const int64_t key = key_flat(i);
        key_to_idx[key] = i;
        
        as_batch_read_record* record = as_batch_read_reserve(&records);
        as_key_init_int64(&record->key, reader_->namespace_.c_str(), reader_->set_.c_str(), key);
        record->read_all_bins = true;
    }
    
    // 执行批量读取
    as_error err;
    auto batch_read_start = std::chrono::high_resolution_clock::now();
    if (aerospike_batch_read(&reader_->as_, &err, NULL, &records) != AEROSPIKE_OK) {
        std::cerr << "Batch read failed: " << err.message << std::endl;
        as_batch_records_destroy(&records);
        // 记录批量读取失败
        GLIDEKV_METRIC_INCREMENT(BATCH_READ_FAILURES_TOTAL);
        return OkStatus();
    }
    
    // 记录批量读取延迟
    auto batch_read_end = std::chrono::high_resolution_clock::now();
    auto batch_read_duration = std::chrono::duration_cast<std::chrono::microseconds>(batch_read_end - batch_read_start);
    GLIDEKV_METRIC_OBSERVE(BATCH_READ_LATENCY_US, batch_read_duration.count());
    
    // 处理批量读取结果
    as_vector* list = &records.list;
    
    // 统计成功和失败的记录数
    int64_t success_count = 0;
    int64_t failure_count = 0;

    auto& worker_threads = *ctx->device()->tensorflow_cpu_worker_threads();
    uint32_t num_worker_threads = worker_threads.num_threads;
    auto shard = [&](uint32_t begin, uint32_t end) {
      for (uint32_t i = begin; i < end; ++i) {
        as_batch_read_record* record = (as_batch_read_record*)as_vector_get(list, i);
        const int64_t key_val = as_integer_getorelse((as_integer*)record->key.valuep, -1);
        
        if (record->result == AEROSPIKE_OK) {
            auto it = key_to_idx.find(key_val);
            if (it != key_to_idx.end()) {
                const size_t idx = it->second;
                reader_->extract_vector_from_record(&record->record, idx, value);
                // 原子递增成功计数
                __sync_fetch_and_add(&success_count, 1);
            }
        } else {
            // 原子递增失败计数
            __sync_fetch_and_add(&failure_count, 1);
        }
      }
    };
    int64 slices = static_cast<int64>(num_keys / worker_threads.num_threads) + 1;
    Shard(num_worker_threads, worker_threads.workers, num_keys, slices, shard);
    
    // 记录成功和失败计数
    GLIDEKV_METRIC_INCREMENT_BY(KEYS_FOUND_TOTAL, success_count);
    GLIDEKV_METRIC_INCREMENT_BY(KEYS_NOT_FOUND_TOTAL, failure_count);
    
    // 计算并记录成功率
    if (num_keys > 0) {
        double success_rate = static_cast<double>(success_count) / num_keys;
        GLIDEKV_METRIC_OBSERVE(SUCCESS_RATE, success_rate);
    }
    
    // 清理资源
    as_batch_records_destroy(&records);
    
    // 记录总操作延迟
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    GLIDEKV_METRIC_OBSERVE(TOTAL_LATENCY_US, total_duration.count());
    
    return OkStatus();
  }

  size_t size() const override {return 0;}

  Status DoInsert(bool clear, const Tensor& keys, const Tensor& values) {
    return OkStatus();
  }

  Status Insert(OpKernelContext* ctx, const Tensor& keys,
                const Tensor& values) override {
    return DoInsert(false, keys, values);
  }

  Status Remove(OpKernelContext* ctx, const Tensor& keys) override {
    return OkStatus();
  }

  Status ImportValues(OpKernelContext* ctx, const Tensor& keys,
                      const Tensor& values) override {
    return DoInsert(true, keys, values);
  }

  Status ExportValues(OpKernelContext* ctx) override {
    return OkStatus();
  }

  DataType key_dtype() const override { return DataTypeToEnum<K>::v(); }

  DataType value_dtype() const override { return DataTypeToEnum<V>::v(); }

  TensorShape key_shape() const final { return TensorShape(); }

  TensorShape value_shape() const override { return value_shape_; }

  int64_t MemoryUsed() const override {
    return 0;
  }

 private:
  // Writes all keys and values into `keys` and `values`. `keys` and `values`
  // must point to tensors of size `table_.size()`.
  
  TensorShape value_shape_;
  // typedef gtl::InlinedVector<V, 4> ValueArray;
  // 使用智能指针管理AerospikeReader，避免内存泄漏
  std::unique_ptr<AerospikeReader<K, V>> reader_;

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
