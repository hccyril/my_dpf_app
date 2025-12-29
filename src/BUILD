cc_binary(
    name = "my_dpf_demo",
    srcs = ["main.cc"],
    deps = [
        # 依赖 fork 仓库中的 DPF 主库。
        # "@google_DPF" 对应 WORKSPACE 中 local_repository 的 name。
        # "//dpf:distributed_point_function" 来自你刚刚的 bazel query 结果。
        "@distributed_point_functions//dpf:distributed_point_function",
    ],
)
