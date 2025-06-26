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

    // 连接到 Aerospike
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
    // 不需要手动清理配置，aerospike_destroy会处理
}

template<typename K, typename V>
void AerospikeReader<K, V>::extract_vector_from_record(as_record* record, int idx, tensorflow::Tensor* value) {
    if (!record) return;
    auto value_flat = value->flat_inner_dims<V, 2>();
    for (uint16_t i = 0; i < record->bins.size; i++) {
        as_bin* bin = &record->bins.entries[i];
        if (bin && strcmp(bin->name, field_name_.c_str()) == 0) {
            as_val* val = (as_val*)as_bin_get_value(bin);
            if (val && as_val_type(val) == AS_LIST) {
                as_list* list = as_list_fromval(val);
                if (list) {
                    uint32_t size = as_list_size(list);
                    for (uint32_t j = 0; j < size; ++j) {
                        as_val* v = as_list_get(list, j);
                        if (v) {
                            switch (as_val_type(v)) {
                                case AS_DOUBLE:
                                    value_flat(idx, j) = static_cast<V>(as_double_getorelse((as_double*)v, 0.0));
                                    break;
                            }
                        }
                    }
                    
                    // 调整容量以节省内存
                    return; // 找到指定字段后直接返回
                }
            }
        }
    }
}

// Explicit template instantiations for common types
template class AerospikeReader<int64_t, float>;
template class AerospikeReader<int64_t, double>;
