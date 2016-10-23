cc_binary(
    name = "_jsonnet.so",
    srcs = ["_jsonnet.c"],
    copts = ["-Iexternal/cpython/Include"],
    deps = [
        "@cpython//:python_h",
        "//core:libjsonnet",
    ],
    linkshared = 1,
    visibility = ["//visibility:public"],
)
