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

// SIMD优化的向量提取函数 - 泛型版本（普通展开）
template<typename K, typename V>
void AerospikeReader<K, V>::extract_vector_simd(as_list* list, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
    if (!list) return;
    uint32_t size = as_list_size(list);
    if (size == 0) return;  // 如果list为空，保持默认值不变
    
    uint32_t j = 0;
    uint32_t unrolled_size = size & ~(UNROLL_FACTOR - 1);
    for (; j < unrolled_size; j += UNROLL_FACTOR) {
        __builtin_prefetch(&list, 0, 3);
        for (uint32_t k = 0; k < UNROLL_FACTOR && (j + k) < size; ++k) {
            uint32_t pos = j + k;
            as_val* v = as_list_get(list, pos);
            if (v && as_val_type(v) == AS_DOUBLE) {
                value_flat(idx, pos) = static_cast<V>(as_double_getorelse((as_double*)v, 0.0));
            }
        }
    }
    for (; j < size; ++j) {
        as_val* v = as_list_get(list, j);
        if (v && as_val_type(v) == AS_DOUBLE) {
            value_flat(idx, j) = static_cast<V>(as_double_getorelse((as_double*)v, 0.0));
        }
    }
}

// float特化：SIMD极致优化
template<>
void AerospikeReader<int64_t, float>::extract_vector_simd(as_list* list, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<float, 2>())& value_flat) {
    if (!list) return;
    uint32_t size = as_list_size(list);
    if (size == 0) return;  // 如果list为空，保持默认值不变
    
#ifdef __AVX2__
    uint32_t j = 0;
    uint32_t avx_size = size & ~7;
    for (; j < avx_size; j += 8) {
        __builtin_prefetch(&list, 0, 3);
        float values[8] __attribute__((aligned(32)));  // 栈上分配，避免频繁malloc/free
        
        for (uint32_t k = 0; k < 8; ++k) {
            as_val* v = as_list_get(list, j + k);
            if (v && as_val_type(v) == AS_DOUBLE) {
                values[k] = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
            } else {
                values[k] = 0.0f;  // 默认值
            }
        }
        __m256 vec = _mm256_load_ps(values);
        _mm256_storeu_ps(&value_flat(idx, j), vec);
    }
    for (; j < size; ++j) {
        as_val* v = as_list_get(list, j);
        if (v && as_val_type(v) == AS_DOUBLE) {
            value_flat(idx, j) = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
        }
    }
#elif defined(__SSE2__)
    uint32_t j = 0;
    uint32_t sse_size = size & ~3;
    for (; j < sse_size; j += 4) {
        __builtin_prefetch(&list, 0, 3);
        float values[4] __attribute__((aligned(16)));  // 栈上分配，避免频繁malloc/free
        
        for (uint32_t k = 0; k < 4; ++k) {
            as_val* v = as_list_get(list, j + k);
            if (v && as_val_type(v) == AS_DOUBLE) {
                values[k] = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
            } else {
                values[k] = 0.0f;  // 默认值
            }
        }
        __m128 vec = _mm_load_ps(values);
        _mm_storeu_ps(&value_flat(idx, j), vec);
    }
    for (; j < size; ++j) {
        as_val* v = as_list_get(list, j);
        if (v && as_val_type(v) == AS_DOUBLE) {
            value_flat(idx, j) = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
        }
    }
#else
    uint32_t j = 0;
    uint32_t unrolled_size = size & ~(UNROLL_FACTOR - 1);
    for (; j < unrolled_size; j += UNROLL_FACTOR) {
        __builtin_prefetch(&list, 0, 3);
        for (uint32_t k = 0; k < UNROLL_FACTOR; ++k) {
            uint32_t pos = j + k;
            if (pos >= size) break;  // 边界检查
            as_val* v = as_list_get(list, pos);
            if (v && as_val_type(v) == AS_DOUBLE) {
                value_flat(idx, pos) = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
            }
        }
    }
    for (; j < size; ++j) {
        as_val* v = as_list_get(list, j);
        if (v && as_val_type(v) == AS_DOUBLE) {
            value_flat(idx, j) = static_cast<float>(as_double_getorelse((as_double*)v, 0.0));
        }
    }
#endif
}

template<typename K, typename V>
void AerospikeReader<K, V>::extract_vector_from_record(as_record* record, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
    if (!record) return;  // 如果record为空，保持默认值不变
    
    uint16_t bin_count = record->bins.size;
    as_bin* bins = record->bins.entries;
    if (!bins) return;  // 如果bins为空，保持默认值不变
    
    for (uint16_t i = 0; i < bin_count; ++i) {
        as_bin* bin = &bins[i];
        if (bin && bin->name && strcmp(bin->name, field_name_.c_str()) == 0) {
            as_val* val = (as_val*)as_bin_get_value(bin);
            if (val && as_val_type(val) == AS_LIST) {
                as_list* list = as_list_fromval(val);
                if (list) {
                    extract_vector_simd(list, idx, value_flat);
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