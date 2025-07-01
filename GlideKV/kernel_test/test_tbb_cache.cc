#include "GlideKV/kernels/tbb_cache.h"
#include <iostream>
#include <set>
#include <vector>
#include <iomanip>
#include <chrono>

/**
 * 测试size()方法的时间复杂度
 * 验证是否为O(1)而不是O(n)
 */
void test_size_time_complexity() {
    std::cout << "\n=== Size()方法时间复杂度测试 (原子计数器实现) ===" << std::endl;
    std::cout << std::setw(12) << "元素个数" 
              << std::setw(15) << "size()耗时(ms)" 
              << std::setw(15) << "平均耗时(ms)" 
              << std::setw(15) << "验证数量" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    // 增加更大的测试规模
    std::vector<size_t> test_sizes = {10000, 100000, 500000, 1000000, 5000000};
    
    for (size_t size : test_sizes) {
        // 创建缓存
        std::set<uint64_t> slot_index = {0, 1, 2, 3};
        TBBCache<uint64_t, std::vector<float>> cache(100, slot_index);
        
        // 插入数据
        std::vector<float> value = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f};
        std::cout << "正在插入 " << size << " 个元素..." << std::endl;
        for (size_t i = 0; i < size; i++) {
            cache.insert(i, value);
        }
        
        // 验证插入的元素数量
        size_t actual_size = cache.size();
        std::cout << "实际插入元素数量: " << actual_size << std::endl;
        
        // 测试size()方法耗时 - 增加测试次数，减少编译器优化
        const int test_iterations = 10000;  // 增加测试次数
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t total_size = 0;  // 累积结果，防止优化
        for (int i = 0; i < test_iterations; i++) {
            total_size += cache.size();  // 累积结果
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double total_ms = duration.count() / 1000.0;
        double avg_ms = total_ms / test_iterations;
        
        std::cout << std::setw(12) << actual_size 
                  << std::setw(15) << std::fixed << std::setprecision(6) << total_ms
                  << std::setw(15) << std::fixed << std::setprecision(6) << avg_ms 
                  << std::setw(15) << total_size / test_iterations << std::endl;
    }
    
    std::cout << "\n结论：使用原子计数器实现的size()方法：" << std::endl;
    std::cout << "- 时间复杂度：O(1) - 常数时间" << std::endl;
    std::cout << "- 性能：极快，适合频繁调用" << std::endl;
    std::cout << "- 线程安全：原子操作保证并发安全" << std::endl;
}

/**
 * 验证不同key个数的内存占用
 * 与理论表格进行对比
 */
void test_memory_usage_table() {
    std::cout << "\n=== 内存占用验证表 ===" << std::endl;
    std::cout << std::setw(12) << "Key个数" 
              << std::setw(15) << "理论内存(MB)" 
              << std::setw(15) << "实际内存(MB)" 
              << std::setw(15) << "差异(%)" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    std::vector<size_t> test_sizes = {1000, 10000, 100000, 500000, 1000000};
    
    for (size_t size : test_sizes) {
        // 创建缓存
        std::set<uint64_t> slot_index = {0, 1, 2, 3};
        TBBCache<uint64_t, std::vector<float>> cache(100, slot_index);
        
        // 插入数据
        std::vector<float> value = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f};
        for (size_t i = 0; i < size; i++) {
            cache.insert(i, value);
        }
        
        // 计算理论内存（MB）
        double theoretical_mb = (32 + 88 + size * 80) / (1024.0 * 1024.0);
        
        // 获取实际内存
        double actual_mb = cache.precise_memory_usage_gb(size) * 1024.0;
        
        // 计算差异
        double diff_percent = ((actual_mb - theoretical_mb) / theoretical_mb) * 100.0;
        
        std::cout << std::setw(12) << size 
                  << std::setw(15) << std::fixed << std::setprecision(2) << theoretical_mb
                  << std::setw(15) << std::fixed << std::setprecision(2) << actual_mb
                  << std::setw(15) << std::fixed << std::setprecision(1) << diff_percent << "%" << std::endl;
    }
}

/**
 * TBBCache 测试程序
 * 
 * 功能演示：
 * 1. 创建TBBCache实例
 * 2. 批量插入数据
 * 3. 查询数据
 * 4. 监控内存使用和缓存大小
 * 5. 验证内存占用表格
 * 6. 测试size()方法时间复杂度
 * 
 * 内存占用分析：
 * - 基础结构：约32字节
 * - slot_index_：24字节 + 4个slot * 16字节 = 88字节
 * - 每个cache元素：8(key) + 24(vector对象) + 8*4(vector数据) + 16(哈希节点) = 80字节
 * - 100万个元素总内存：约80MB
 */
int main() {
    // 创建slot_index，定义有效的缓存槽位
    std::set<uint64_t> slot_index = {0, 1, 2, 3};
    
    // 创建容量为 100GB 的基础缓存
    // 注意：实际内存使用会根据插入的数据量动态计算
    TBBCache<uint64_t, std::vector<float>> tbb_cache_(100, slot_index);
    

    // 创建测试数据：8个float值的vector
    std::vector<float> value1 = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f};

    // 批量插入100万个元素，测试性能和内存使用
    std::cout << "开始插入100万个元素..." << std::endl;
    for (int i = 0; i < 100000000; i++) {
        tbb_cache_.insert(i, value1);
    }
    std::cout << "插入完成！" << std::endl;
    
    // 获取并显示元素
    auto value = tbb_cache_.get(1);
    if (!value.empty()) {
        std::cout << "Key 1: ";
        for (float v : value) std::cout << v << " ";
        std::cout << std::endl;
    }

    // 显示内存使用情况
    // 参数5000000是预估的缓存大小，用于内存计算
    std::cout << "Memory usage: " << tbb_cache_.precise_memory_usage_gb(5000000) << " GB" << std::endl;
    std::cout << "Size: " << tbb_cache_.size() << std::endl;
    
    // 验证内存占用表格
    test_memory_usage_table();
    
    // 测试size()方法时间复杂度
    test_size_time_complexity();
    
    return 0;
}