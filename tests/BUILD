load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@zsync3//:zsyncfile.bzl", "zsyncfile")

zsyncfile(
    name = "empty.zsync",
    file = "files/empty",
)

diff_test(
    name = "empty_test",
    file1 = ":empty.zsync",
    file2 = "files/empty.zsync",
)

zsyncfile(
    name = "loremipsum.zsync",
    blocksize = 8,
    file = "files/loremipsum",
)

diff_test(
    name = "loremipsum_test",
    file1 = ":loremipsum.zsync",
    file2 = "files/loremipsum.zsync",
)

zsyncfile(
    name = "url.zsync",
    file = "files/empty",
    url = "http://example.com/empty",
)

diff_test(
    name = "url_test",
    file1 = ":url.zsync",
    file2 = "files/url.zsync",
)

sh_test(
    name = "zsyncranges_test",
    timeout = "short",
    srcs = ["zsyncranges_test.sh"],
    data = [
        "files/loremipsum",
        ":loremipsum.zsync",
        "//:zsyncranges",
    ],
)
