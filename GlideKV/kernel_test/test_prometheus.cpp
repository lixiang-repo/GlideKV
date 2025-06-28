#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <thread>
#include <chrono>
#include <iostream>

int main() {
  // 1. 创建 HTTP 暴露器 (默认端口 8080)
  prometheus::Exposer exposer{"localhost:8080"};
  
  // 2. 创建指标注册中心
  auto registry = std::make_shared<prometheus::Registry>();
  
  // 3. 添加计数器指标
  auto& counter = prometheus::BuildCounter()
      .Name("requests_total")
      .Help("Total HTTP requests")
      .Register(*registry)
      .Add({});

  // 4. 注册中心绑定到暴露器
  exposer.RegisterCollectable(registry);

  // 5. 模拟业务逻辑 (指标递增)
  while (true) {
    counter.Increment();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "counter: " << counter.Value() << std::endl;
  }
  
  return 0;
}
