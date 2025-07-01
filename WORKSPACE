workspace(name = "GlideKV")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//:tf_framework_repo.bzl", "tf_framework_repo")

tf_framework_repo(name = "local_tf")

# git clone git@github.com:tensorflow/serving.git
git_repository(
    name = "tf_serving",
    remote = "https://github.com/tensorflow/serving.git",
    commit = "a0be2bf6ebf690a01dbe9747910952422be55945",  # 用 r2.15 的 commit hash 替换
)

load("@tf_serving//tensorflow_serving:workspace.bzl", "tf_serving_workspace")
tf_serving_workspace()

# com_github_jupp0r_prometheus_cpp
git_repository(
    name = "com_github_jupp0r_prometheus_cpp",
    commit = "4bd38da318ec54af8e2d8d5d0bdbd5eb9bc0784f",  # 或特定 commit ID
    remote = "https://github.com/jupp0r/prometheus-cpp.git",
)

load("@com_github_jupp0r_prometheus_cpp//bazel:repositories.bzl", "prometheus_cpp_repositories")
prometheus_cpp_repositories()

# Aerospike C Client
git_repository(
    name = "aerospike_client_c",
    remote = "https://github.com/aerospike/aerospike-client-c.git",
    commit = "354a1283",  # 使用7.0.4版本的commit hash
    init_submodules = True,  # 初始化子模块
    build_file = "//third_party_glidekv:aerospike_client_c.BUILD",
)

# ===== TBB (Intel Threading Building Blocks) 2020.3 =====
http_archive(
    name = "tbb",
    sha256 = "ebc4f6aa47972daed1f7bf71d100ae5bf6931c2e3144cf299c8cc7d041dca2f3",
    strip_prefix = "oneTBB-2020.3",
    urls = [
        "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2020.3.tar.gz",
    ],
    build_file = "//third_party_glidekv:tbb.BUILD",
)
