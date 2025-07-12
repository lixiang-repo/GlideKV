package(default_visibility = ["//visibility:public"])

# Aerospike C Client library
cc_library(
    name = "aerospike_client_c",
    srcs = glob([
        "src/main/aerospike/*.c",
        "src/main/aerospike/*.h",
        "modules/common/src/main/aerospike/*.c",
        "modules/common/src/main/aerospike/*.h", 
        "modules/common/src/main/citrusleaf/*.c",
        "modules/common/src/main/citrusleaf/*.h",
        # Lua modules
        "modules/mod-lua/src/main/*.c",
        "modules/mod-lua/src/main/*.h",
    ]),
    hdrs = glob([
        "src/include/aerospike/*.h",
        "modules/common/src/include/aerospike/*.h",
        "modules/common/src/include/citrusleaf/*.h",
        # Lua headers
        "modules/mod-lua/src/include/*.h",
    ]),
    includes = [
        "src/include",
        "modules/common/src/include",
        # Lua includes
        "modules/mod-lua/src/include",
    ],
    copts = [
        "-I/usr/include/lua5.1",  # Add Lua 5.1 include path
        "-D_GNU_SOURCE",  # 确保 CPU_ZERO 宏可用
    ],
    linkopts = [
        "-lpthread",
        "-lssl", 
        "-lcrypto",
        "-lz",
        "-llua5.1",  # Lua 5.1 library
    ],
    deps = [],
)

# Header files only
cc_library(
    name = "aerospike_headers",
    hdrs = glob([
        "src/include/aerospike/*.h",
        "modules/common/src/include/aerospike/*.h",
        "modules/common/src/include/citrusleaf/*.h",
        # Remove Lua headers
        # "modules/mod-lua/src/include/*.h",
        # "modules/lua/*.h",
    ]),
    includes = [
        "src/include",
        "modules/common/src/include",
        # Remove Lua includes
        # "modules/mod-lua/src/include",
        # "modules/lua",
    ],
    deps = [],
) 