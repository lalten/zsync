load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

local_defines = ["_XOPEN_SOURCE=700"]

cc_library(
    name = "zsglobal",
    hdrs = ["zsglobal.h"],
)

cc_library(
    name = "format_string",
    hdrs = ["format_string.h"],
)

cc_library(
    name = "progress",
    srcs = ["progress.c"],
    hdrs = ["progress.h"],
    deps = [":zsglobal"],
)

cc_library(
    name = "librcksum",
    srcs = [
        "librcksum/hash.c",
        "librcksum/internal.h",
        "librcksum/md4.c",
        "librcksum/md4.h",
        "librcksum/range.c",
        "librcksum/rsum.c",
        "librcksum/state.c",
    ],
    hdrs = ["librcksum/rcksum.h"],
    local_defines = local_defines,
    deps = [
        ":progress",
        ":zsglobal",
    ],
)

cc_test(
    name = "md4test",
    srcs = [
        "librcksum/md4.c",
        "librcksum/md4.h",
        "librcksum/md4test.c",
    ],
    copts = ["-Wno-overflow"],
    deps = [":zsglobal"],
)

cc_library(
    name = "libzsync",
    srcs = [
        "libzsync/sha1.c",
        "libzsync/sha1.h",
        "libzsync/zsync.c",
    ],
    hdrs = ["libzsync/zsync.h"],
    local_defines = local_defines,
    deps = [
        ":format_string",
        ":librcksum",
        ":zsglobal",
    ],
)

cc_test(
    name = "sha1test",
    srcs = [
        "libzsync/sha1.c",
        "libzsync/sha1.h",
        "libzsync/sha1test.c",
    ],
    copts = ["-Wno-overflow"],
    local_defines = local_defines,
    deps = [":zsglobal"],
)

cc_binary(
    name = "zsyncmake",
    srcs = ["make.c"],
    local_defines = local_defines,
    visibility = ["//visibility:public"],
    deps = [
        ":format_string",
        ":librcksum",
        ":libzsync",
        ":zsglobal",
    ],
)

cc_binary(
    name = "zsync",
    srcs = [
        "client.c",
        "url.c",
        "url.h",
    ],
    local_defines = local_defines,
    visibility = ["//visibility:public"],
    deps = [
        ":curl",
        ":format_string",
        ":librcksum",
        ":libzsync",
        ":progress",
        ":zsglobal",
    ],
)

cc_binary(
    name = "zsyncranges",
    srcs = ["zsyncranges.c"],
    visibility = ["//visibility:public"],
    deps = [
        ":librcksum",
        ":libzsync",
    ],
)

cc_library(
    name = "curl",
    srcs = ["curl.c"],
    hdrs = ["curl.h"],
)

cc_test(
    name = "curl_test",
    timeout = "short",
    srcs = ["curl_test.c"],
    data = ["curl_test.txt"],
    deps = [":curl"],
)

py_binary(
    name = "zsyncdownload",
    testonly = True,
    srcs = ["zsyncdownload.py"],
    data = [":zsyncranges"],
    visibility = ["//tests:__subpackages__"],
)
