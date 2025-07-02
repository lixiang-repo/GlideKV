// tbb_lru_cache.h
#ifndef TBB_LRU_CACHE_H
#define TBB_LRU_CACHE_H

#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_vector.h>
#include <tbb/task_group.h>
#include <list>
#include <shared_mutex>
#include <mutex>
#include <set>
#include <functional>
#include <vector>
#include <optional>
#include <atomic>
#include <memory>
#include <filesystem>
#include "tensorflow/core/framework/tensor.h"
#include "data_loader.h"

/**
 * 基于 TBB 的线程安全 缓存实现
 * 支持并发访问
 * ============================================================================
 * Key个数与内存占用关系表 (适用于 K=uint64_t, V=std::vector<float>)
 * ============================================================================
 * | Key个数    | 基础结构(B) | slot_index(B) | 缓存数据(MB) | 总内存(MB) | 总内存(GB) |
 * |-----------|------------|--------------|-------------|-----------|-----------|
 * | 1,000,000 | 32         | 88           | 80.0        | 80.12     | 0.0783    |
 * | 5,000,000 | 32         | 88           | 400.0       | 400.12    | 0.3907    |
 * | 10,000,000| 32         | 88           | 800.0       | 800.12    | 0.7814    |
 * | 12,797,542| 32         | 88           | 800.0       | 800.12    | 1.0000    |
 * | 50,000,000| 32         | 88           | 4000.0      | 4000.12   | 3.9064    |
 * | 100,000,000| 32        | 88           | 8000.0      | 8000.12   | 7.8126    |
 * ============================================================================
 * 
 * 计算公式：
 * - 每个元素大小 = 8(key) + 8(智能指针) + 24(vector对象) + 32(vector数据) + 16(哈希节点) = 88字节
 * - 缓存数据大小 = Key个数 × 88字节
 * - 总内存 = 基础结构 + slot_index_ + 缓存数据大小
 * 
 * 注意事项：
 * - 实际内存使用可能略高于理论值（包含TBB内部管理开销）
 * - 内存计算基于vector<float>大小为8个元素
 * - 不同vector大小会影响内存占用
 * - 建议预留20%额外内存用于系统开销
 */

template<typename K, typename V>
class TBBCache {
protected:
    
    tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>> cache_;         // 主缓存：并发哈希表，存储智能指针
    
    size_t dim_;
    std::string version_;
    std::atomic<bool> initialized_{false};

    void init() {
        cache_ = tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>();
        std::thread load_thread([this]() {
            load_from_file();
        });
        load_thread.detach();
    }

    void load_from_file() {
        const std::string& model_base_path = std::string(getenv("MODEL_BASE_PATH"));
        // std::string model_base_path = "/data";
        if (model_base_path.empty()) {
            std::cerr << "MODEL_BASE_PATH is not set" << std::endl;
            return;
        }
        std::vector<std::string> files = get_files(model_base_path + "/" + version_, "variables/tbb_cache/sparse_*.gz");
        for (const auto& file : files) {
            bool success = load_from_gz_file<K, V>(file, cache_, dim_);
            if (!success) {
                cache_.clear();
                std::cout << "failed to load " << file << std::endl;
                return;
            }
        }
        initialized_.store(true, std::memory_order_relaxed);
    }

public:
    TBBCache(const std::string& version, size_t dim) : version_(version), dim_(dim) {
        init();
    }
    
    ~TBBCache() {
        cache_.clear();
    }

    std::vector<V>* get(const K& key) {
        if (!initialized_.load(std::memory_order_relaxed)) {
            return nullptr;
        }
        typename tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>::const_accessor accessor;
        if (cache_.find(accessor, key)) {
            return accessor->second.get();  // 返回原始指针，缓存保持所有权
        }
        return nullptr;
    }

};


#endif // TBB_LRU_CACHE_H

template class TBBCache<int64_t, float>;
template class TBBCache<int64_t, double>; 