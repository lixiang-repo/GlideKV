

#include <vector>
#include <iostream>

int main() {
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};

    if (!vec.empty()) {
        float* ptr = vec.data();  // 推荐方式
        std::cout << "第一个元素: " << *ptr << std::endl;
        std::cout << "第一个元素地址: " << ptr << std::endl;
    }
}