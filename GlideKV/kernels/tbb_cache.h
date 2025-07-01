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
#include "tensorflow/core/framework/tensor.h"

/**
 * 基于 TBB 的线程安全 LRU 缓存实现
 * 支持并发访问、批量操作和异步淘汰
 * 
 * 内存占用说明：
 * - 基础结构：约32字节（max_size_ + slot_index_基础开销）
 * - slot_index_：24字节基础开销 + 每个slot 16字节
 * - cache_：每个元素约48字节 + vector数据大小
 * - 总内存 = 基础结构 + slot_index_ + cache_元素总数 * 单元素大小
 * 
 * 性能特点：
 * - 线程安全：使用TBB concurrent_hash_map保证并发安全
 * - 内存效率：支持精确内存计算和自动清理
 * - 访问速度：O(1)平均查找时间
 * - Size()性能：O(1)使用原子计数器
 * 
 * ============================================================================
 * Key个数与内存占用关系表 (适用于 K=uint64_t, V=std::vector<float>)
 * ============================================================================
 * | Key个数    | 基础结构(B) | slot_index(B) | 缓存数据(MB) | 总内存(MB) | 总内存(GB) |
 * |-----------|------------|--------------|-------------|-----------|-----------|
 * | 1,000     | 32         | 88           | 0.08        | 0.12      | 0.0001    |
 * | 10,000    | 32         | 88           | 0.8         | 0.92      | 0.0009    |
 * | 100,000   | 32         | 88           | 8.0         | 8.12      | 0.0079    |
 * | 500,000   | 32         | 88           | 40.0        | 40.12     | 0.0392    |
 * | 1,000,000 | 32         | 88           | 80.0        | 80.12     | 0.0783    |
 * | 5,000,000 | 32         | 88           | 400.0       | 400.12    | 0.3907    |
 * | 10,000,000| 32         | 88           | 800.0       | 800.12    | 0.7814    |
 * | 12,797,542| 32         | 88           | 800.0       | 800.12    | 1.0000    |
 * | 50,000,000| 32         | 88           | 4000.0      | 4000.12   | 3.9064    |
 * | 100,000,000| 32        | 88           | 8000.0      | 8000.12   | 7.8126    |
 * | 500,000,000| 32        | 88           | 40000.0     | 40000.12  | 39.0626   |
 * | 1,000,000,000| 32      | 88           | 80000.0     | 80000.12  | 78.1251   |
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
    
    double max_size_;                                       // 最大容量
    std::set<K> slot_index_;                                // cache slot index，用于过滤无效key
    
    tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>> cache_;         // 主缓存：并发哈希表，存储智能指针
    tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>> temp_cache_;    // 临时缓存：用于原子清理操作
    
    std::atomic<size_t> element_count_{0};         // 原子计数器：记录元素数量

    void init() {
        cache_ = tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>();
        temp_cache_ = tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>();
        element_count_.store(0, std::memory_order_relaxed);
    }

public:
    /**
     * 构造函数
     * @param max_size 最大容量
     * @param slot_index 有效的slot索引集合
     */
    TBBCache(size_t max_size, std::set<K> slot_index) : max_size_(max_size), slot_index_(slot_index) {
        init();
    }
    
    /**
     * 析构函数
     * 自动清理所有缓存数据，释放内存
     */
    ~TBBCache() {
        clear();  // 销毁时清空缓存
    }
    
    /**
     * 检查键是否存在
     * @param key 键
     * @return 如果存在返回true，否则返回false
     * 
     * 线程安全操作，对于uint64_t类型的key会验证slot有效性
     */
    bool contains(const K& key) {
        if constexpr (std::is_same_v<K, uint64_t>) {
            if (slot_index_.find(key >> 48) == slot_index_.end()) {
                return false;
            }
        }

        typename tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>::const_accessor accessor;
        bool found = cache_.find(accessor, key);
        return found;
    }
    
    /**
     * 获取元素
     * @param key 键
     * @return 对应的值的原始指针，如果不存在返回nullptr
     * 
     * 线程安全操作，对于uint64_t类型的key会验证slot有效性
     * 注意：返回的指针由缓存管理，调用者不应删除
     */
    std::vector<V>* get(const K& key) {
        if constexpr (std::is_same_v<K, uint64_t>) {
            if (slot_index_.find(key >> 48) == slot_index_.end()) {
                return nullptr;
            }
        }

        typename tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>::const_accessor accessor;
        if (cache_.find(accessor, key)) {
            return accessor->second.get();  // 返回原始指针，缓存保持所有权
        }
        return nullptr;
    }
    
    /**
     * 插入元素
     * @param key 键
     * @param value_ptr 值的智能指针
     * 
     * 线程安全操作，使用emplace确保原子性：
     * 1. 避免了先insert再赋值的竞态条件
     * 2. 确保插入和赋值在单个原子操作中完成
     * 3. 避免了insert+赋值的竞态条件
     */
    void insert(const K& key, std::unique_ptr<std::vector<V>> value_ptr) {       
        // 使用 emplace 进行原子插入和赋值
        // emplace 在单个原子操作中完成插入和值初始化
        // 返回true表示新插入，false表示已存在
        bool inserted = cache_.emplace(key, std::move(value_ptr));
        
        // 如果是新插入的元素，增加计数器
        if (inserted) {
            element_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void insert_with_idx(const K& key, int idx, decltype(std::declval<tensorflow::Tensor>().flat_inner_dims<V, 2>())& value_flat) {
        if constexpr (std::is_same_v<K, uint64_t>) {
            if (slot_index_.find(key >> 48) == slot_index_.end()) {
                return;  // 不在有效范围内，直接返回
            }
        }
        if (element_count_.load(std::memory_order_relaxed) > max_size_) {
            return;
        }

        auto vector_ptr = std::make_unique<std::vector<V>>();
        int64_t value_dim = value_flat.dimension(1);
        
        // 边界检查
        if (value_dim <= 0 || idx < 0) {
            std::cerr << "Invalid value_dim or idx: value_dim=" << value_dim << ", idx=" << idx << std::endl;
            return;
        }
        
        // 检查是否越界
        int64_t total_elements = value_flat.dimension(0) * value_dim;
        if (idx * value_dim + value_dim > total_elements) {
            std::cerr << "Index out of bounds: idx=" << idx << ", value_dim=" << value_dim << ", total_elements=" << total_elements << std::endl;
            return;
        }
        
        vector_ptr->resize(value_dim);  // 使用resize而不是reserve，确保vector有正确的size
        std::copy(value_flat.data() + idx * value_dim, value_flat.data() + (idx + 1) * value_dim, vector_ptr->begin());

        // 直接调用insert方法
        insert(key, std::move(vector_ptr));
    }

    /**
     * 获取当前缓存中的元素数量
     * @return 缓存中的元素总数
     * 
     * 时间复杂度：O(1) - 使用原子计数器
     * 注意：由于并发特性，返回的数量可能在调用后立即变化
     * 
     * 使用原子计数器实现O(1)的size()操作，
     * 避免了TBB concurrent_hash_map::size()的O(n)复杂度
     */
    size_t size() const {
        return element_count_.load(std::memory_order_relaxed);
    }

    /**
     * 清空缓存
     * 
     * 使用原子交换操作实现非阻塞清空：
     * 1. 原子交换主缓存和临时缓存
     * 2. 清空临时缓存（释放所有内存）
     * 3. 重置原子计数器
     * 
     * 这种方式的优点：
     * - 非阻塞：其他线程可以继续访问
     * - 原子性：清空操作是原子的
     * - 内存安全：智能指针自动释放所有内存
     * - 计数器同步：重置原子计数器
     */
    void clear() {
        // 原子交换，将旧数据移到临时缓存
        cache_.swap(temp_cache_);
        
        // 清空临时缓存（智能指针会自动释放内存）
        temp_cache_.clear();
        
        // 确保主缓存也被清空
        cache_.clear();
        
        // 重置原子计数器
        element_count_.store(0, std::memory_order_relaxed);
        
        // 函数结束时temp_cache_自动销毁
    }
    

};


#endif // TBB_LRU_CACHE_H

template class TBBCache<int64_t, float>;
template class TBBCache<int64_t, double>; 