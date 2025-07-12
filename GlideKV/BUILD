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
    "kernels/tbb_cache.h",
    "kernels/lookup_interface_stub.h",
    "kernels/lookup_table_op.cc",
    "kernels/prometheus_metrics.h",
    "kernels/metric_configs.h",
    "kernels/thread_local_random.h",
    "kernels/thread_local_random.cc",
    "kernels/aerospike_reader.cc",
    "kernels/aerospike_reader.h",
    "ops/lookup_ops.cc",
    "kernels/data_loader.h",
    "kernels/version_utils.h",
]

deps = [
        "@org_tensorflow//tensorflow/core/kernels:lookup_headers_lib",
        "@com_github_jupp0r_prometheus_cpp//core:core",
        "@com_github_jupp0r_prometheus_cpp//pull:pull",
        "@com_github_jupp0r_prometheus_cpp//push:push",
        "@aerospike_client_c//:aerospike_client_c",
        "@tbb//:tbb",
]

copts = [
    # "-D_GLIBCXX_USE_CXX11_ABI=1",
    "-DEIGEN_MAX_ALIGN_BYTES=64",
]

linkopts = [
    "-lpthread",
    "-lssl", 
    "-lcrypto",
    "-lz",
    "-llua5.1",  # 使用系统安装的Lua 5.1
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

tf_cc_binary(
    name = "test_tbb_cache",
    srcs = ["kernel_test/test_tbb_cache.cc"] + srcs,
    deps = ["@local_tf//:tensorflow_framework"] + deps,
    copts = ["-D_GLIBCXX_USE_CXX11_ABI=1"] + copts,
    linkopts = linkopts
)


tf_cc_binary(
    name = "app",
    srcs = ["kernel_test/test_main.cc"] + srcs,
    deps = ["@local_tf//:tensorflow_framework"] + deps,
    copts = ["-D_GLIBCXX_USE_CXX11_ABI=1"] + copts,
    linkopts = linkopts
)

