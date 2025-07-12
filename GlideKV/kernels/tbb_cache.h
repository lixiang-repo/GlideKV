#ifndef TBB_CACHE_H
#define TBB_CACHE_H

#include <tbb/concurrent_hash_map.h>
#include <tbb/parallel_for.h>
#include <tbb/flow_graph.h>
#include <atomic>
#include <memory>
#include <thread>
#include <string>
#include <vector>
#include <future>
#include <algorithm>

#include "data_loader.h"
#include "version_utils.h"

/**
 * 基于 TBB 的线程安全 缓存实现
 * 支持并发访问
 * ============================================================================
 * Key个数与内存占用关系表 (适用于 K=uint64_t, V=std::vector<float>)
 * ============================================================================
 * | Key个数    | 基础结构(B) | slot_index(B) | 缓存数据(MB) | 总内存(MB) | 总内存(GB) |
 * |-----------|------------|--------------|-------------|-----------|-----------|
 * | 10,000,000| 32         | 88           | 800.0       | 800.12    | 0.7814    |
 * | 12,201,611| 32         | 88           | 1024.0      | 1024.0    | 1.0000    |
 * | 50,000,000| 32         | 88           | 4000.0      | 4000.12   | 3.9064    |
 * ============================================================================
 * 
 * 计算公式：
 * - 每个元素大小 = 8(key) + 8(智能指针) + 24(vector对象) + 32(vector数据) + 16(哈希节点) = 88字节
 * - 缓存数据大小 = Key个数 × 88字节
 * - 总内存 = 基础结构 + 缓存数据大小
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
    std::atomic<bool> initialized_{false};
    std::atomic<bool> stop_flag_{false};  // 停止标志

    void load_from_file() {
        LOG(INFO) << "loading tbb cache from file...";

        std::filesystem::path fs_model_path = get_model_path();
        std::string model_path = fs_model_path.string();
        int max_version = get_max_version(fs_model_path.parent_path().string());
        
        // 等待模型文件就绪
        int wait_count = 0;
        const int max_wait_count = 60; // 5分钟 = 60 * 5秒 
        while (wait_count < max_wait_count && (model_path.find(std::to_string(max_version)) == std::string::npos || 
                !fs::exists(model_path + "/variables/tbb_cache"))) {
            if (stop_flag_.load(std::memory_order_relaxed)) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
            wait_count++;
            if (wait_count % 12 == 0) { // 每分钟打印一次日志 
                LOG(INFO) << "Still waiting for tbb cache file: " << model_path << " (waited " << (wait_count * 5) << " seconds)"; 
            }
        
            fs_model_path = get_model_path();
            model_path = fs_model_path.string();
            max_version = get_max_version(fs_model_path.parent_path().string());
        }

        LOG(INFO) << "successfully loaded model: " << model_path << " with max_version: " << max_version;

        std::vector<std::string> cache_files = get_files(model_path, "variables/tbb_cache/sparse_*.gz");
        if (cache_files.empty()) {
            LOG(INFO) << "no tbb cache_files found in model path: " << model_path;
            return;
        }
        
        // 使用TBB线程池处理文件
        std::atomic<int64_t> total_count{0};
        
        tbb::parallel_for(tbb::blocked_range<size_t>(0, cache_files.size()),
            [&](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i != range.end(); ++i) {
                    if (stop_flag_.load(std::memory_order_relaxed)) {
                        return;
                    }
                    
                    const auto& file = cache_files[i];
                    int64_t count = load_from_gz_file<K, V>(file, cache_, dim_);
                    if (count >= 0) {
                        total_count.fetch_add(count, std::memory_order_relaxed);
                    }
                }
            });

        if (!stop_flag_.load(std::memory_order_relaxed) && cache_.size() == total_count.load(std::memory_order_relaxed)) {
            initialized_.store(total_count.load(std::memory_order_relaxed) > 0, std::memory_order_relaxed);
            LOG(INFO) << "successfully initialized tbb cache from model path: " << model_path;
        } else {
            cache_.clear();
            LOG(INFO) << "failed to initialize tbb cache from model path: " << model_path;
        }
    }

public:
    TBBCache(size_t dim) : dim_(dim) {
        cache_ = tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>();
        stop_flag_.store(false, std::memory_order_relaxed);
        
        // 使用shared_ptr确保对象生命周期
        auto self_ptr = std::shared_ptr<TBBCache>(this, [](TBBCache*){}); // 自定义删除器，防止重复删除
        
        // 启动加载线程并detach，传递shared_ptr
        std::thread([self_ptr]() {
            self_ptr->load_from_file();
        }).detach();
    }
    
    ~TBBCache() {
        // 设置停止标志
        stop_flag_.store(true, std::memory_order_relaxed);
        
        // 由于使用了detach，不需要join，线程会在后台自动结束
        initialized_.store(false, std::memory_order_relaxed);
        cache_.clear();
    }

    // 删除拷贝构造函数和赋值操作符
    TBBCache(const TBBCache&) = delete;
    TBBCache& operator=(const TBBCache&) = delete;

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


#endif // TBB_CACHE_H

template class TBBCache<int64_t, float>;
template class TBBCache<int64_t, double>; 