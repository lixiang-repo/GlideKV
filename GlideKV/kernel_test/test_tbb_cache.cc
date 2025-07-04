#include <iostream>
#include <fstream>
#include <string>
#include <atomic>
#include <set>
#include "GlideKV/kernels/tbb_cache.h"

int main() {
    std::string path = "/data/model/dnn_winr_v1/export_dir/dense_model/1751475006/assets.extra/tf_serving_warmup_requests";
    setenv("WARMUP_PATH", path.c_str(), 1);

    // Create TBBCache with dimension 8
    TBBCache<int64_t, double> cache_(8);

    // std::cout << "cache size: " << cache_.get(1)->size() << std::endl;

    auto cache_ptr = cache_.get(1);
    while (cache_ptr == nullptr) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cache_ptr = cache_.get(1);
    }
    std::cout << "cache size: " << cache_ptr->size() << std::endl;

    std::cout << "cache[1]: " << (*cache_ptr)[0] << std::endl;


    

    return 0;
}