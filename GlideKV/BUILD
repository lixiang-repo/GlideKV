# Build simple_hash_table custom ops example, which is similar to,
# but simpler than, tf.lookup.experimental.MutableHashTable
load("@org_tensorflow//tensorflow:tensorflow.bzl", "tf_cc_binary", "tf_custom_op_library")

licenses(["notice"])  # Apache 2.0
# tf.sysconfig.get_compile_flags()

package(default_visibility = ["//visibility:public"])

py_library(
    name = "py_ops",
    srcs = glob(["*.py"]),
    data = [
        ":_lookup_ops.so",
    ],
    srcs_version = "PY2AND3",
)

srcs = [
    "kernels/lookup_interface_stub.h",
    "kernels/lookup_table_op.cc",
    "kernels/glidekv_prometheus_metrics.h",
    "kernels/system_monitor.h",
    "kernels/metric_configs.h",
    "kernels/thread_local_random.h",
    "kernels/thread_local_random.cc",
    "kernels/system_monitor.cc",
    "kernels/aerospike_reader.cc",
    "kernels/aerospike_reader.h",
    "ops/lookup_ops.cc",
]

deps = [
        "@org_tensorflow//tensorflow/core/kernels:lookup_headers_lib",
        "@com_github_jupp0r_prometheus_cpp//core:core",
        "@com_github_jupp0r_prometheus_cpp//pull:pull",
        "@com_github_jupp0r_prometheus_cpp//push:push",
        "@aerospike_client_c//:aerospike_client_c",
]

copts = [
    # "-D_GLIBCXX_USE_CXX11_ABI=1",
    "-DEIGEN_MAX_ALIGN_BYTES=64",
    # 原子操作支持
    # "-std=c++17",
    # SIMD优化支持
    "-mavx2",           # 启用AVX2指令集
    "-msse2",           # 启用SSE2指令集  
    "-msse3",           # 启用SSE3指令集
    "-msse4.1",         # 启用SSE4.1指令集
    "-msse4.2",         # 启用SSE4.2指令集
    "-mfma",            # 启用FMA指令集
    "-march=native",    # 针对当前CPU架构优化
    "-mtune=native",    # 针对当前CPU微架构优化
    # 内存对齐优化
    "-falign-functions=32",
    "-falign-loops=32",
    "-falign-jumps=32",
    # 其他性能优化
    "-O3",              # 最高级别优化
    "-ffast-math",      # 快速数学运算
    "-funroll-loops",   # 循环展开
    "-ftree-vectorize", # 自动向量化
]

linkopts = [
    "-lpthread",
    "-lssl", 
    "-lcrypto",
    "-lz",
    "-llua5.3",  # 使用系统安装的Lua 5.3
    "-latomic",  # 添加原子操作库
]

tf_custom_op_library(
    name = "_lookup_ops.so",
    srcs = srcs,
    deps = ["@local_tf//:tensorflow_framework"] + deps,
    copts = ["-D_GLIBCXX_USE_CXX11_ABI=1"] + copts,
    linkopts = linkopts
)

############################aerospike ops############################
cc_library(
    name = 'glidekv_lookup_ops',
    srcs = srcs,
    alwayslink = 1,
    deps = deps,
    copts = copts,
    linkopts = linkopts
)

######################test######################

cc_binary(
    name = "test_json",
    srcs = ["test_json.cc"]
)

cc_binary(
    name = "test_shell",
    srcs = ["test_shell.cc"]
)
