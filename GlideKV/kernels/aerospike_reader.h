#pragma once

#include <string>
#include <atomic>
#include "tensorflow/core/framework/tensor.h"

// SIMD优化支持
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX2__
#include <immintrin.h>
#endif
#ifdef __AVX512F__
#include <immintrin.h>
#endif

// Aerospike C客户端头文件
extern "C" {
#include <aerospike/aerospike.h>
#include <aerospike/aerospike_key.h>
#include <aerospike/as_record.h>
#include <aerospike/as_error.h>
#include <aerospike/as_list.h>
#include <aerospike/as_val.h>
#include <aerospike/as_integer.h>
#include <aerospike/as_double.h>
#include <aerospike/aerospike_batch.h>
#include <aerospike/as_vector.h>
}

template<typename K, typename V>
class AerospikeReader {
public:
    // 配置参数 - 从环境变量读取，带默认值
    std::string host_;
    int port_;
    std::string namespace_;
    std::string set_;
    std::string field_name_;
    bool connected_ = false;
    
    // 缓存字符串常量，避免重复调用c_str()
    const char* namespace_cstr_ = nullptr;
    const char* set_cstr_ = nullptr;
    const char* field_name_cstr_ = nullptr;
    
    aerospike as_;
    
    AerospikeReader();
    AerospikeReader(const std::string& host, int port, const std::string& namespace_name, const std::string& set, const std::string& field_name);
    ~AerospikeReader();
    
    void init();
    
    bool connect(const std::string& host, int port);

    void close();

    void extract_vector_from_record(as_record* record, int idx, int dim, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat);
    
}; 
