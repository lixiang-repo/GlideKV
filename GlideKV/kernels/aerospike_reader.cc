#include "aerospike_reader.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/tstring.h"
#include <type_traits>
#include "tensorflow/core/util/work_sharder.h"

template<typename K, typename V>
AerospikeReader<K, V>::AerospikeReader() {
    aerospike_init(&as_, NULL);
    loadConfigFromEnv(); // 从环境变量加载配置
    
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
    std::cout << "  Host: " << host_ << " (from AEROSPIKE_HOST)" << std::endl;
    std::cout << "  Port: " << port_ << " (from AEROSPIKE_PORT)" << std::endl;
    std::cout << "  Namespace: " << namespace_ << " (from AEROSPIKE_NAMESPACE)" << std::endl;
    std::cout << "  Set: " << set_ << " (from AEROSPIKE_SET)" << std::endl;
    std::cout << "  Field: " << field_name_ << " (from AEROSPIKE_FIELD)" << std::endl;
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
void AerospikeReader<K, V>::extract_vector_from_list(as_list* list, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
    uint32_t size = as_list_size(list);
    if (size == 0) return;
    for (uint32_t i = 0; i < size; ++i) {
        value_flat(idx, i) = extract_aerospike_value<V>(as_list_get(list, i));
    }
}

template<typename K, typename V>
void AerospikeReader<K, V>::extract_vector_from_record(as_record* record, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
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
                    extract_vector_from_list(list, idx, value_flat);
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