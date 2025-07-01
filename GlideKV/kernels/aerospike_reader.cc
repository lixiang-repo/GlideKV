#include "aerospike_reader.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <type_traits>

template<typename K, typename V>
AerospikeReader<K, V>::AerospikeReader() {
    aerospike_init(&as_, NULL);
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
    
    // 初始化缓存的字符串常量
    namespace_cstr_ = namespace_.c_str();
    set_cstr_ = set_.c_str();
    field_name_cstr_ = field_name_.c_str();
    
    init();
}

template<typename K, typename V>
AerospikeReader<K, V>::AerospikeReader(const std::string& host, int port, const std::string& namespace_name, const std::string& set, const std::string& field_name) {
    aerospike_init(&as_, NULL);
    // loadConfigFromEnv(); // 从环境变量加载配置
    
    host_ = host;
    port_ = port;
    namespace_ = namespace_name;
    set_ = set;
    field_name_ = field_name;

    // 初始化缓存的字符串常量
    namespace_cstr_ = namespace_.c_str();
    set_cstr_ = set_.c_str();
    field_name_cstr_ = field_name_.c_str();
    
    init();
}

template<typename K, typename V>
AerospikeReader<K, V>::~AerospikeReader() {
    close();
    // 清理aerospike实例
    aerospike_destroy(&as_);
}

template<typename K, typename V>
void AerospikeReader<K, V>::init() {
    std::cout << "Configuration loaded from environment variables:" << std::endl;
    std::cout << "  Host: " << host_ << std::endl;
    std::cout << "  Port: " << port_ << std::endl;
    std::cout << "  Namespace: " << namespace_ << std::endl;
    std::cout << "  Set: " << set_ << std::endl;
    std::cout << "  Field: " << field_name_ << std::endl;
    if (!connect(host_, port_)) {
        std::cerr << "Failed to connect to Aerospike at " << host_ << ":" << port_ << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "  1. Aerospike server is running" << std::endl;
        std::cerr << "  2. Network connectivity" << std::endl;
        std::cerr << "  3. Environment variables are set correctly" << std::endl;
        return;
    }
    std::cout << "Successfully connected to Aerospike at " << host_ << ":" << port_ << std::endl;
}

template<typename K, typename V>
bool AerospikeReader<K, V>::connect(const std::string& host, int port) {
    as_config config;
    as_config_init(&config);
    as_config_add_hosts(&config, host.c_str(), port);
    aerospike_init(&as_, &config);
    as_error err;
    if (aerospike_connect(&as_, &err) != AEROSPIKE_OK) {
        std::cerr << "Failed to connect to Aerospike: " << err.message << std::endl;
        return false;
    }
    connected_ = true;
    return true;
}

template<typename K, typename V>
void AerospikeReader<K, V>::close() {
    if (connected_) {
        as_error err;
        aerospike_close(&as_, &err);
        connected_ = false;
    }
}

// 公共工具函数 - 提取Aerospike值
template<typename V>
inline V extract_aerospike_value(as_val* v) {
    if (!v) return static_cast<V>(0);
    
    switch (as_val_type(v)) {
        case AS_DOUBLE:
            return static_cast<V>(as_double_getorelse((as_double*)v, 0.0));
        case AS_INTEGER:
            return static_cast<V>(as_integer_getorelse((as_integer*)v, 0));
        default:
            return static_cast<V>(0);
    }
}

template<typename K, typename V>
void AerospikeReader<K, V>::extract_vector_from_record(as_record* record, int idx, int dim, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
    if (!record) return;  // 如果record为空，保持默认值不变
    
    uint16_t bin_count = record->bins.size;
    as_bin* bins = record->bins.entries;
    if (!bins) return;  // 如果bins为空，保持默认值不变
    
    // 边界检查
    if (idx < 0) {
        std::cerr << "Invalid index: " << idx << std::endl;
        return;
    }
    
    // 使用缓存的字符串常量，避免重复调用c_str()
    for (uint16_t i = 0; i < bin_count; ++i) {
        as_bin* bin = &bins[i];
        if (bin && bin->name && strcmp(bin->name, field_name_cstr_) == 0) {
            as_val* val = (as_val*)as_bin_get_value(bin);
            if (val && as_val_type(val) == AS_LIST) {
                as_list* list = as_list_fromval(val);
                if (list) {
                    uint32_t size = as_list_size(list);
                    if (static_cast<int>(size) == dim) {
                        for (uint32_t j = 0; j < size; ++j) {
                            value_flat(idx, j) = extract_aerospike_value<V>(as_list_get(list, j));
                        }
                    } else {
                        std::cerr << "Invalid dimension: " << dim << " for record " << idx << std::endl;
                        return;
                    }
                }
                return;  // 找到并处理了字段，退出
            }
        }
    }
    // 如果没有找到指定字段，保持默认值不变
}

// 显式实例化
template class AerospikeReader<int64_t, float>;
template class AerospikeReader<int64_t, double>; 