#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "glidekv_prometheus_metrics.h"

namespace tensorflow {
namespace lookup {

// 系统监控类 - 单例模式
class SystemMonitor {
private:
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_{false};  // 默认不在监控
    std::condition_variable cv_;
    
    // 上次CPU统计
    uint64_t last_cpu_total_ = 0;
    uint64_t last_cpu_idle_ = 0;
    uint64_t last_timestamp_ = 0;
    
    // 获取CPU使用率
    double get_cpu_usage() {
        std::ifstream file("/proc/stat");
        if (!file.is_open()) return 0.0;
        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string cpu;
            uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
            uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
            uint64_t idle_total = idle + iowait;
            if (last_cpu_total_ > 0) {
                uint64_t total_diff = total - last_cpu_total_;
                uint64_t idle_diff = idle_total - last_cpu_idle_;
                if (total_diff > 0) {
                    double cpu_usage = 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
                    last_cpu_total_ = total;
                    last_cpu_idle_ = idle_total;
                    return cpu_usage;
                }
            }
            last_cpu_total_ = total;
            last_cpu_idle_ = idle_total;
        }
        return 0.0;
    }
    
    // 获取内存使用率
    double get_memory_usage() {
        std::ifstream file("/proc/meminfo");
        if (!file.is_open()) return 0.0;
        uint64_t total = 0, free = 0, available = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("MemTotal:") == 0) {
                sscanf(line.c_str(), "MemTotal: %lu", &total);
            } else if (line.find("MemFree:") == 0) {
                sscanf(line.c_str(), "MemFree: %lu", &free);
            } else if (line.find("MemAvailable:") == 0) {
                sscanf(line.c_str(), "MemAvailable: %lu", &available);
            }
        }
        if (total > 0) {
            uint64_t used = total - available;
            return 100.0 * static_cast<double>(used) / total;
        }
        return 0.0;
    }
    
    // 获取可用内存(GB)
    double get_memory_available_gb() {
        std::ifstream file("/proc/meminfo");
        if (!file.is_open()) return 0.0;
        uint64_t available = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("MemAvailable:") == 0) {
                sscanf(line.c_str(), "MemAvailable: %lu", &available);
                break;
            }
        }
        return static_cast<double>(available) / (1024 * 1024);
    }
    
    // 获取总内存(GB)
    double get_memory_total_gb() {
        struct sysinfo info;
        if (sysinfo(&info) != 0) return 0.0;
        uint64_t total_mem = info.totalram * info.mem_unit;
        return static_cast<double>(total_mem) / (1024 * 1024 * 1024);
    }
    
    // 监控线程函数
    void monitor_loop();

public:
    // 构造函数
    SystemMonitor() {
        // 不自动启动，由 Initialize() 方法控制启动
    }
    // 析构函数
    ~SystemMonitor() {
        stop();
    }
    // 禁用拷贝构造和赋值
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;

    // 启动监控
    void start() {
        if (monitoring_.load(std::memory_order_acquire)) return;  // 已在监控直接返回
        monitoring_.store(true, std::memory_order_release);  // 设置为true表示开始监控
        std::thread(&SystemMonitor::monitor_loop, this).detach();
    }

    // 停止监控
    void stop() {
        if (!monitoring_.load(std::memory_order_acquire)) return;  // 已停止直接返回
        monitoring_.store(false, std::memory_order_release);  // 设置为false表示停止监控
        std::cout << "[SystemMonitor] Stopped system monitoring thread" << std::endl;
    }

    // 单例获取实例
    static SystemMonitor& getInstance() {
        static SystemMonitor instance;
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            std::cout << "🖥️  SystemMonitor singleton initialized!" << std::endl;
        });
        return instance;
    }
    
    // 初始化函数 - 确保只初始化一次
    static void Initialize() {
        auto& instance = getInstance();
        if (instance.monitoring_.load(std::memory_order_acquire)) {
            std::cout << "🖥️  SystemMonitor: Already monitoring, skipping..." << std::endl;
            return;
        }
        std::cout << "🖥️  SystemMonitor: Starting system monitoring..." << std::endl;
        instance.start();
        std::cout << "🖥️  SystemMonitor: System monitoring started successfully!" << std::endl;
    }
};

// 声明全局初始化函数，供 Prometheus 相关代码调用
void InitializeGlobalSystemMonitor();

} // namespace lookup
} // namespace tensorflow 