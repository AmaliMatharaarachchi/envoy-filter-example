package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":http_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

api_proto_package()

envoy_cc_library(
    name = "http_filter_lib",
    srcs = ["http_filter.cc"],
    hdrs = ["http_filter.h"],
    repository = "@envoy",
    deps = [
        ":helloworld_cc_grpc",
        ":greeter_client",
        "@envoy//source/exe:envoy_common_lib",
    ],
)


# Following is for the GRPC service
# The following three rules - the usage of the cc_grpc_library rule in
# in a mode compatible with the native proto_library and cc_proto_library rules.
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

proto_library(
    name = "helloworld_proto",
    srcs = ["helloworld.proto"],
)

cc_proto_library(
    name = "helloworld_cc_proto",
    deps = [":helloworld_proto"],
)

cc_grpc_library(
    name = "helloworld_cc_grpc",
    srcs = [":helloworld_proto"],
    grpc_only = True,
    deps = [":helloworld_cc_proto"],
)

cc_library(
    name = "greeter_client",
    srcs = ["greeter_client.cc"],
    deps = [
        ":helloworld_cc_grpc",
        # http_archive made this label available for binding
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "greeter_server",
    srcs = ["greeter_server.cc"],
    defines = ["BAZEL_BUILD"],
    deps = [
        ":helloworld_cc_grpc",
        # http_archive made this label available for binding
        "@com_github_grpc_grpc//:grpc++",
    ],
)

envoy_cc_library(
    name = "http_filter_config",
    srcs = ["http_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":helloworld_cc_grpc",
        # http_archive made this label available for binding
        "@com_github_grpc_grpc//:grpc++",
        ":http_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)