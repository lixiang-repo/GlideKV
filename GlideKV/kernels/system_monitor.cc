#include "system_monitor.h"
#include "glidekv_prometheus_metrics.h"

namespace tensorflow {
namespace lookup {

void SystemMonitor::monitor_loop() {
    while (monitoring_) {
        double cpu_usage = get_cpu_usage();
        double memory_usage = get_memory_usage();
        double memory_available = get_memory_available_gb();
        // Prometheus记录
        GLIDEKV_METRIC_SET(SYSTEM_CPU_USAGE_PERCENT, cpu_usage, 1);
        GLIDEKV_METRIC_SET(SYSTEM_MEMORY_USAGE_PERCENT, memory_usage, 1);
        GLIDEKV_METRIC_SET(SYSTEM_MEMORY_AVAILABLE_GB, memory_available, 1);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void InitializeGlobalSystemMonitor() {
    SystemMonitor::Initialize();
}

} // namespace lookup
} // namespace tensorflow 