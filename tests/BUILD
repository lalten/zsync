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
    name = "not_empty.zsync",
    blocksize = 8,
    file = "files/not empty",
)

diff_test(
    name = "not_empty_test",
    file1 = ":not_empty.zsync",
    file2 = "files/not_empty.zsync",
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
