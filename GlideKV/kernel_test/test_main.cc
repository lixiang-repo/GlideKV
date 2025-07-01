#include <iostream>
#include <random>

int main() {
    // 随机数引擎（默认为 mt19937，梅森旋转算法）
    std::random_device rd;  // 用于获取种子
    std::mt19937 gen(rd()); // 随机数生成器
    
    // 分布范围：[1, 100]
    std::uniform_int_distribution<> dis(1, 100);
    
    // 生成随机数
    for (int i = 0; i < 5; ++i) {
        std::cout << dis(gen) << " ";
    }
    
    return 0;
}