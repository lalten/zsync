load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

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
    defines = ["HAVE_INTTYPES_H"],
    deps = [
        ":progress",
        ":zsglobal",
    ],
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
    defines = ['VERSION=\\"0.6.2\\"'],
    deps = [
        ":format_string",
        ":librcksum",
        ":zsglobal",
        "@zlib",
    ],
)

cc_binary(
    name = "zsyncmake",
    srcs = [
        "make.c",
        "makegz.c",
        "makegz.h",
    ],
    defines = [
        'VERSION=\\"0.6.2\\"',
        'PACKAGE=\\"zsyncmake\\"',
    ],
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
    defines = [
        "HAVE_GETADDRINFO",
        'VERSION=\\"0.6.2\\"',
        'PACKAGE=\\"zsync\\"',
    ],
    deps = [
        ":format_string",
        ":librcksum",
        ":libzsync",
        ":progress",
        ":zsglobal",
        "@zlib",
    ],
)
