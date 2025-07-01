package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tbb",
    hdrs = glob([
        "include/tbb/*.h",
        "include/tbb/*/*.h",
    ]),
    includes = ["include"],
    linkopts = ["-ltbb"],
    visibility = ["//visibility:public"],
) 
