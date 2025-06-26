workspace(name = "GlideKV")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# 定义 Aerospike C 客户端外部依赖
new_local_repository(
    name = "aerospike_client_c",
    path = "/usr/local",  # 假设 Aerospike C 客户端安装在此路径
    build_file = "//third_party:aerospike.BUILD",
)

# 下载并配置 Google Test
http_archive(
    name = "com_google_googletest",
    sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
    strip_prefix = "googletest-1.14.0",
    urls = ["https://github.com/google/googletest/archive/1.14.0.tar.gz"],
)

