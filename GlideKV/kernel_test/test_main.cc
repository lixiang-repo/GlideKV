#include <iostream>
#include "GlideKV/kernels/version_utils.h"

int main() {
    int max_version = get_max_version("/data/model/dnn_winr_v1/export_dir/dense_model");
    std::cout << "max_version: " << max_version << std::endl;
    return 0;
}