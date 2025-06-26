#pragma once

#include <string>
#include <memory>
#include <cstdlib>
#include "tensorflow/core/framework/tensor.h"

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

    aerospike as_;
    
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
    
    // 简化的read，只接受keys和values参数
    bool read(const tensorflow::Tensor& key, tensorflow::Tensor* value);

    void extract_vector_from_record(as_record* record, int idx, tensorflow::Tensor* vector_data);
    
}; 
