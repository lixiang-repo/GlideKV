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
    "kernels/lookup_table_op.cc",
    "kernels/aerospike_reader.h",
    "kernels/aerospike_reader.cpp",
    "kernels/glidekv_prometheus_metrics.h",
    "ops/lookup_ops.cc",
]

copts = [
    # "-D_GLIBCXX_USE_CXX11_ABI=1",
    "-DEIGEN_MAX_ALIGN_BYTES=64"
]

linkopts = [
    "-L/usr/lib",
    "-laerospike",
    "-L/usr/local/lib",
    "-lprometheus-cpp-core",
    "-lprometheus-cpp-pull",
    "-lprometheus-cpp-push",
    "-lcivetweb",
    # "-lnuma",
    "-latomic",
    "-lpthread",
    "-lm",
    "-ldl",
    "-lssl",
    "-lcrypto",
    "-lz",
    "-Wl,--as-needed",
    "-Wl,--strip-all",
    "-Wl,--gc-sections",
    "-Wl,--build-id=none",
]

tf_custom_op_library(
    name = "_lookup_ops.so",
    srcs = srcs,
    deps = [
        "@org_tensorflow//tensorflow/core/kernels:lookup_headers_lib",
        "@local_tf//:tensorflow_framework",
    ],
    copts = copts,
    linkopts = linkopts
)

############################aerospike ops############################
cc_library(
    name = 'glidekv_lookup_ops',
    srcs = srcs,
    alwayslink = 1,
    deps = [
     "@org_tensorflow//tensorflow/core/kernels:lookup_headers_lib",
    ],
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
