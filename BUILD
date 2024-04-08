load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

local_defines = [
    "_XOPEN_SOURCE=700",
    'VERSION=\\"0.6.2\\"',
    "HAVE_GETADDRINFO",
    'VERSION=\\"0.6.2\\"',
    'PACKAGE=\\"zsync\\"',
]

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
    deps = [":zsglobal"],
)

cc_library(
    name = "libzsync",
    srcs = [
        "libzsync/sha1.c",
        "libzsync/sha1.h",
        "libzsync/zmap.c",
        "libzsync/zmap.h",
        "libzsync/zsync.c",
    ],
    hdrs = ["libzsync/zsync.h"],
    local_defines = local_defines,
    deps = [
        ":format_string",
        ":librcksum",
        ":zsglobal",
        "@zlib",
    ],
)

cc_test(
    name = "sha1test",
    srcs = [
        "libzsync/sha1.c",
        "libzsync/sha1.h",
        "libzsync/sha1test.c",
    ],
    local_defines = local_defines,
    deps = [":zsglobal"],
)

cc_binary(
    name = "zsyncmake",
    srcs = [
        "make.c",
        "makegz.c",
        "makegz.h",
    ],
    local_defines = local_defines,
    deps = [
        ":format_string",
        ":librcksum",
        ":libzsync",
        ":zsglobal",
        "@zlib",
    ],
)

cc_binary(
    name = "zsync",
    srcs = [
        "base64.c",
        "client.c",
        "http.c",
        "http.h",
        "url.c",
        "url.h",
    ],
    local_defines = local_defines,
    deps = [
        ":format_string",
        ":librcksum",
        ":libzsync",
        ":progress",
        ":zsglobal",
        "@zlib",
    ],
)
