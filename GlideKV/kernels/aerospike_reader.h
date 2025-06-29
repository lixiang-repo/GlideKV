#pragma once

#include <string>
#include <memory>
#include <cstdlib>
#include "tensorflow/core/framework/tensor.h"
#include <atomic>
#include <vector>

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
    std::atomic<bool> _on{true};
    
    // 缓存字符串常量，避免重复调用c_str()
    const char* namespace_cstr_ = nullptr;
    const char* set_cstr_ = nullptr;
    const char* field_name_cstr_ = nullptr;
    
    aerospike as_;
    
    // 性能优化配置
    static constexpr uint32_t UNROLL_FACTOR = 8;  // 循环展开因子
    
    AerospikeReader();
    ~AerospikeReader();
    
    // 从环境变量读取配置参数
    void loadConfigFromEnv() {
        // 从环境变量读取配置参数，如果不存在则使用默认值
        const char* host_env = std::getenv("AEROSPIKE_HOST");
        host_ = host_env ? std::string(host_env) : "localhost";
        
        const char* port_env = std::getenv("AEROSPIKE_PORT");
        port_ = port_env ? std::atoi(port_env) : 3000;
        
        const char* namespace_env = std::getenv("AEROSPIKE_NAMESPACE");
        namespace_ = namespace_env ? std::string(namespace_env) : "test";
        
        const char* set_env = std::getenv("AEROSPIKE_SET");
        set_ = set_env ? std::string(set_env) : "vectors";
        
        const char* field_env = std::getenv("AEROSPIKE_FIELD");
        field_name_ = field_env ? std::string(field_env) : "vector";
    }
    
    void init();
    
    bool connect(const std::string& host, int port);

    void close();

    void extract_vector_from_record(as_record* record, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat);
    
    // 向量提取函数
    void extract_vector_from_list(as_list* list, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat);
    
}; 
