#!/usr/bin/env python3
"""
GlideKV Metrics 监控脚本
监控和分析GlideKV的性能指标
"""

import time
import json
import requests
import psutil
import os
from datetime import datetime
from collections import defaultdict, deque
import matplotlib.pyplot as plt
import numpy as np

class GlideKVMetricsMonitor:
    """GlideKV性能指标监控器"""
    
    def __init__(self, tfserving_url="http://localhost:8501", 
                 metrics_endpoint="/v1/models/glidekv/metrics"):
        self.tfserving_url = tfserving_url
        self.metrics_endpoint = metrics_endpoint
        self.metrics_history = defaultdict(lambda: deque(maxlen=1000))
        self.start_time = time.time()
        
    def collect_system_metrics(self):
        """收集系统指标"""
        cpu_percent = psutil.cpu_percent(interval=1)
        memory = psutil.virtual_memory()
        disk = psutil.disk_usage('/')
        
        return {
            'timestamp': time.time(),
            'cpu_percent': cpu_percent,
            'memory_percent': memory.percent,
            'memory_used_gb': memory.used / (1024**3),
            'memory_available_gb': memory.available / (1024**3),
            'disk_percent': disk.percent,
            'disk_used_gb': disk.used / (1024**3),
            'disk_free_gb': disk.free / (1024**3)
        }
    
    def collect_tfserving_metrics(self):
        """收集TensorFlow Serving指标"""
        try:
            response = requests.get(f"{self.tfserving_url}{self.metrics_endpoint}", 
                                  timeout=5)
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Failed to get metrics: {response.status_code}")
                return None
        except Exception as e:
            print(f"Error collecting TF Serving metrics: {e}")
            return None
    
    def parse_glidekv_metrics(self, tf_metrics):
        """解析GlideKV特定的指标"""
        if not tf_metrics:
            return {}
        
        glidekv_metrics = {}
        
        # 解析Prometheus格式的指标
        for metric in tf_metrics.get('metrics', []):
            metric_name = metric.get('name', '')
            metric_value = metric.get('value', 0)
            
            if 'glidekv' in metric_name.lower():
                glidekv_metrics[metric_name] = metric_value
        
        return glidekv_metrics
    
    def calculate_performance_stats(self, metrics_data):
        """计算性能统计"""
        if not metrics_data:
            return {}
        
        stats = {}
        
        # 计算吞吐量
        if 'total_requests' in metrics_data:
            current_time = time.time()
            elapsed_time = current_time - self.start_time
            stats['throughput_rps'] = metrics_data['total_requests'] / elapsed_time
        
        # 计算成功率
        if 'total_requests' in metrics_data and 'successful_requests' in metrics_data:
            total_req = metrics_data['total_requests']
            success_req = metrics_data['successful_requests']
            if total_req > 0:
                stats['success_rate'] = (success_req / total_req) * 100
        
        # 计算平均延迟
        if 'total_latency_ns' in metrics_data and 'total_requests' in metrics_data:
            total_latency = metrics_data['total_latency_ns']
            total_req = metrics_data['total_requests']
            if total_req > 0:
                stats['avg_latency_us'] = (total_latency / total_req) / 1000
        
        # 计算命中率
        if 'total_keys_processed' in metrics_data and 'total_keys_found' in metrics_data:
            total_keys = metrics_data['total_keys_processed']
            found_keys = metrics_data['total_keys_found']
            if total_keys > 0:
                stats['hit_rate'] = (found_keys / total_keys) * 100
        
        return stats
    
    def update_metrics_history(self, metrics_data):
        """更新指标历史"""
        timestamp = time.time()
        for metric_name, metric_value in metrics_data.items():
            self.metrics_history[metric_name].append((timestamp, metric_value))
    
    def generate_metrics_report(self):
        """生成指标报告"""
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # 获取最新的系统指标
        system_metrics = self.collect_system_metrics()
        
        # 获取最新的TF Serving指标
        tf_metrics = self.collect_tfserving_metrics()
        glidekv_metrics = self.parse_glidekv_metrics(tf_metrics)
        
        # 计算性能统计
        performance_stats = self.calculate_performance_stats(glidekv_metrics)
        
        # 更新历史数据
        all_metrics = {**system_metrics, **glidekv_metrics, **performance_stats}
        self.update_metrics_history(all_metrics)
        
        # 生成报告
        report = {
            'timestamp': current_time,
            'system_metrics': system_metrics,
            'glidekv_metrics': glidekv_metrics,
            'performance_stats': performance_stats
        }
        
        return report
    
    def print_metrics_report(self, report):
        """打印指标报告"""
        print(f"\n=== GlideKV Metrics Report - {report['timestamp']} ===")
        
        # 系统指标
        sys_metrics = report['system_metrics']
        print(f"System Metrics:")
        print(f"  CPU Usage: {sys_metrics.get('cpu_percent', 0):.1f}%")
        print(f"  Memory Usage: {sys_metrics.get('memory_percent', 0):.1f}%")
        print(f"  Memory Used: {sys_metrics.get('memory_used_gb', 0):.2f} GB")
        print(f"  Disk Usage: {sys_metrics.get('disk_percent', 0):.1f}%")
        
        # GlideKV指标
        glidekv_metrics = report['glidekv_metrics']
        print(f"\nGlideKV Metrics:")
        print(f"  Total Requests: {glidekv_metrics.get('total_requests', 0)}")
        print(f"  Successful Requests: {glidekv_metrics.get('successful_requests', 0)}")
        print(f"  Failed Requests: {glidekv_metrics.get('failed_requests', 0)}")
        print(f"  Total Keys Processed: {glidekv_metrics.get('total_keys_processed', 0)}")
        print(f"  Total Keys Found: {glidekv_metrics.get('total_keys_found', 0)}")
        print(f"  Batch Operations: {glidekv_metrics.get('total_batch_operations', 0)}")
        print(f"  Aerospike Errors: {glidekv_metrics.get('total_aerospike_errors', 0)}")
        
        # 性能统计
        perf_stats = report['performance_stats']
        print(f"\nPerformance Stats:")
        print(f"  Throughput: {perf_stats.get('throughput_rps', 0):.2f} requests/sec")
        print(f"  Success Rate: {perf_stats.get('success_rate', 0):.2f}%")
        print(f"  Hit Rate: {perf_stats.get('hit_rate', 0):.2f}%")
        print(f"  Average Latency: {perf_stats.get('avg_latency_us', 0):.2f} μs")
        
        # 延迟统计
        if 'min_latency_ns' in glidekv_metrics and 'max_latency_ns' in glidekv_metrics:
            min_latency = glidekv_metrics['min_latency_ns'] / 1000  # 转换为微秒
            max_latency = glidekv_metrics['max_latency_ns'] / 1000
            print(f"  Min Latency: {min_latency:.2f} μs")
            print(f"  Max Latency: {max_latency:.2f} μs")
        
        print("=" * 60)
    
    def save_metrics_to_file(self, report, filename="glidekv_metrics.json"):
        """保存指标到文件"""
        try:
            with open(filename, 'w') as f:
                json.dump(report, f, indent=2)
        except Exception as e:
            print(f"Error saving metrics to file: {e}")
    
    def plot_metrics_trends(self, save_plot=True):
        """绘制指标趋势图"""
        if not self.metrics_history:
            print("No metrics data available for plotting")
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('GlideKV Performance Metrics Trends', fontsize=16)
        
        # 吞吐量趋势
        if 'throughput_rps' in self.metrics_history:
            timestamps, values = zip(*self.metrics_history['throughput_rps'])
            axes[0, 0].plot(timestamps, values, 'b-', label='Throughput (RPS)')
            axes[0, 0].set_title('Throughput Over Time')
            axes[0, 0].set_ylabel('Requests/sec')
            axes[0, 0].legend()
            axes[0, 0].grid(True)
        
        # 延迟趋势
        if 'avg_latency_us' in self.metrics_history:
            timestamps, values = zip(*self.metrics_history['avg_latency_us'])
            axes[0, 1].plot(timestamps, values, 'r-', label='Avg Latency (μs)')
            axes[0, 1].set_title('Average Latency Over Time')
            axes[0, 1].set_ylabel('Latency (μs)')
            axes[0, 1].legend()
            axes[0, 1].grid(True)
        
        # 成功率趋势
        if 'success_rate' in self.metrics_history:
            timestamps, values = zip(*self.metrics_history['success_rate'])
            axes[1, 0].plot(timestamps, values, 'g-', label='Success Rate (%)')
            axes[1, 0].set_title('Success Rate Over Time')
            axes[1, 0].set_ylabel('Success Rate (%)')
            axes[1, 0].legend()
            axes[1, 0].grid(True)
        
        # 命中率趋势
        if 'hit_rate' in self.metrics_history:
            timestamps, values = zip(*self.metrics_history['hit_rate'])
            axes[1, 1].plot(timestamps, values, 'm-', label='Hit Rate (%)')
            axes[1, 1].set_title('Cache Hit Rate Over Time')
            axes[1, 1].set_ylabel('Hit Rate (%)')
            axes[1, 1].legend()
            axes[1, 1].grid(True)
        
        plt.tight_layout()
        
        if save_plot:
            plt.savefig('glidekv_metrics_trends.png', dpi=300, bbox_inches='tight')
            print("Metrics trends plot saved as 'glidekv_metrics_trends.png'")
        
        plt.show()
    
    def monitor_continuously(self, interval=5, duration=None):
        """持续监控指标"""
        print(f"Starting GlideKV metrics monitoring...")
        print(f"Monitoring interval: {interval} seconds")
        if duration:
            print(f"Monitoring duration: {duration} seconds")
        
        start_time = time.time()
        report_count = 0
        
        try:
            while True:
                # 检查是否达到监控时长
                if duration and (time.time() - start_time) >= duration:
                    print(f"\nMonitoring completed after {duration} seconds")
                    break
                
                # 生成报告
                report = self.generate_metrics_report()
                self.print_metrics_report(report)
                
                # 保存报告
                self.save_metrics_to_file(report, f"glidekv_metrics_{report_count}.json")
                
                report_count += 1
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print(f"\nMonitoring stopped by user after {report_count} reports")
        
        # 生成最终的趋势图
        if self.metrics_history:
            self.plot_metrics_trends()

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='GlideKV Metrics Monitor')
    parser.add_argument('--tfserving-url', default='http://localhost:8501',
                       help='TensorFlow Serving URL')
    parser.add_argument('--interval', type=int, default=5,
                       help='Monitoring interval in seconds')
    parser.add_argument('--duration', type=int, default=None,
                       help='Monitoring duration in seconds')
    parser.add_argument('--plot-only', action='store_true',
                       help='Only generate plots from existing data')
    
    args = parser.parse_args()
    
    monitor = GlideKVMetricsMonitor(args.tfserving_url)
    
    if args.plot_only:
        monitor.plot_metrics_trends()
    else:
        monitor.monitor_continuously(args.interval, args.duration)

if __name__ == "__main__":
    main() 