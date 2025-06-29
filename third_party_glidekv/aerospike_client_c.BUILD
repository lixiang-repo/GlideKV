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
        "modules/mod-lua/src/main/*.c",
        "modules/mod-lua/src/main/*.h",
    ]) + [
        # 只包含Aerospike实际需要的lua源文件，排除onelua.c和lua.c
        "modules/lua/lapi.c",
        "modules/lua/lauxlib.c",
        "modules/lua/lbaselib.c",
        "modules/lua/lcode.c",
        "modules/lua/lcorolib.c",
        "modules/lua/lctype.c",
        "modules/lua/ldblib.c",
        "modules/lua/ldebug.c",
        "modules/lua/ldo.c",
        "modules/lua/ldump.c",
        "modules/lua/lfunc.c",
        "modules/lua/lgc.c",
        "modules/lua/llex.c",
        "modules/lua/lmem.c",
        "modules/lua/lobject.c",
        "modules/lua/lopcodes.c",
        "modules/lua/lparser.c",
        "modules/lua/lstate.c",
        "modules/lua/lstring.c",
        "modules/lua/ltable.c",
        "modules/lua/ltm.c",
        "modules/lua/lundump.c",
        "modules/lua/lvm.c",
        "modules/lua/lzio.c",
    ],
    hdrs = glob([
        "src/include/aerospike/*.h",
        "modules/common/src/include/aerospike/*.h",
        "modules/common/src/include/citrusleaf/*.h",
        "modules/mod-lua/src/include/*.h",
        "modules/mod-lua/src/include/aerospike/*.h",
        "modules/lua/*.h",
    ]),
    includes = [
        "src/include",
        "modules/common/src/include",
        "modules/mod-lua/src/include",
        "modules/lua",
    ],
    copts = [
        "-D_GNU_SOURCE",
        "-DEIGEN_MAX_ALIGN_BYTES=64",
        "-std=gnu99",
    ],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
        "-lz",
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
        "modules/mod-lua/src/include/*.h",
        "modules/lua/*.h",
    ]),
    includes = [
        "src/include",
        "modules/common/src/include",
        "modules/mod-lua/src/include",
        "modules/lua",
    ],
    deps = [],
) 