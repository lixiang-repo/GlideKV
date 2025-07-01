def _tf_framework_repo_impl(ctx):
    tf_path = ctx.os.environ.get("TF_SO_PATH")
    if not tf_path:
        fail("TF_SO_PATH environment variable not set! Run 'python3 configure.py' first.")
    ctx.symlink(tf_path, "tf_so_dir")
    ctx.file("BUILD", """
cc_library(
    name = "tensorflow_framework",
    srcs = ["tf_so_dir/libtensorflow_framework.so.2"],
    hdrs = glob(["tf_so_dir/include/**/*.h"]),
    visibility = ["//visibility:public"],
)
""")

tf_framework_repo = repository_rule(
    implementation = _tf_framework_repo_impl,
    local = True,
    environ = ["TF_SO_PATH"],
) 