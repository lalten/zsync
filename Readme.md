# zsync3

Zsync enables partial updates of local files, think **client-side rsync over HTTP**.
The only server-side requirement is support for [HTTP range requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests).

## zync3
zsync3 is an opinionated fork of the original [zsync](https://github.com/cph6/zsync).

The main differences are
* removal of compression awareness
* zsync3 uses the host's `curl` to download the ranges, which means it adds support for https.
* Bazel build system and Bazel rules support

## Contents

This repository contains
* `zsync`, a program to update a local file using ranged downloads
* `zsyncmake`, a program to create .zsync files
* `zsyncfile`, a Bazel rule to generate a .zsync file for a given file
* `zsyncranges`, a program to tell which ranges of a file need updating

### zsync

Compared to the original `zsync`, zsync3:
* Does not support any compression.
  It will not look inside .gz files.
  In fact zsync3 does not depend on zlib anymore.
* Uses `curl` subprocess calls under the hood to download the http ranges.
  This means it supports https, which the original zsync did not support.

Flag changes:
* No `-V` to print the version (to improve Bazel caching)
* No `-s` which was a synomym for `-q` (quiet).
* No `-A` flag or `http_proxy` env var to supply http username/password. (See note on `ZSYNC_CURL` below)

zsync3 uses the `ZSYNC_CURL` environment variable to specify the curl command to use (default: `curl`).
This can be used to specify a proxy or other curl options:
```sh
ZSYNC_CURL='curl --user "$USER:$PASS"' zsync3 http://example.com/file.zsync
```
The value of the ZSYNC_CURL environment variable is passed as is to `sh -c` in the curl subprocess `popen` so this variable expansion is possible.
The curl options `--fail-with-body`, `--silent`, `--show-error`, `--location`, `--netrc` are always added to the curl command.

### zsyncmake

Compared to the original `zsyncmake`, zsync3:
* Does not support any compression.
  It will not look inside .gz files.
  In fact zsync3 does not depend on zlib anymore.
* Behaves like original zsync zsyncmake with the flags `-e` `-Z` set.
* The `-e` (do_exact), `-C` (do_recompress), `-U` (URL to decompressed content), `-z` (do_compress), `-Z` (no_look_inside) flags are removed.
* No `-V` to print the version (to improve Bazel caching)
* A new `-M` flag to disable the MTime header being added to the zsync file (to enable reproducible builds).

### zsyncfile

`zsyncfile` is a Bazel rule to generate a .zsync file for a given file.
Use it like this:
```Starlark
load("@rules_appimage//appimage:appimage.bzl", "appimage") # example
load("@zsync3//:zsyncfile.bzl", "zsyncfile")

# example
appimage(
    name = "program.appimage",
    binary = ":program",
)

zsyncfile(
    name = "program.appimage.zsync",
    file = ":program.appimage",
)
```

```sh
❯ bazel build //path/to/program:program.appimage.zsync
❯ head -n7 bazel-bin/path/to/program/program.appimage.zsync
zsync: 0.6.2
Filename: path/to/program/program.appimage.zsync
Blocksize: 2048
Length: 123456
Hash-Lengths: 1,2,4
URL: program.appimage
SHA-1: da39a3ee5e6b4b0d3255bfef95601890afd80709
```

The rules supports optional attributes to set the `URL`, `Filename`, and `Blocksize`.

### zsyncranges

`zsyncranges` is a program to tell which ranges of a file need updating without downloading anything.
Compared to `zsync`, it only supports exactly one source (seed) file and requries the zsyncfile to be a local file.

`zsyncranges` prints a json list of the start and end byte offsets of the ranges that need updating in the source (seed) file.

```sh
❯ bazel run @zsync3//:zsyncranges -- "$(pwd)/file.zsync" "$(pwd)/file"
[[0,7],[264,279],[296,303],[560,575]]
```
(The need for $(pwd) is a Bazel thing.)

The intended use case of zsyncranges is integration with a download manager that supports ranged downloads.


## Alternatives

Zsync's combination of popularity and abandonment has led to a number of forks and reimplementations.

| Repository                                      | Language | Maintained | Notes                                                                                   |
| ----------------------------------------------- | -------- | ---------- | --------------------------------------------------------------------------------------- |
| https://github.com/cph6/zsync                   | C        | 💀          | Available via most package managers, just `apt install zsync`                           |
| https://github.com/lalten/zsync <-- you're here | C        | ✅          | No compression support. Using `curl` http client. Bazel integration.                    |
| https://github.com/sisong/hsynz                 | C++      | ✅          | Supports zstd, stronger checksums, directories. Using forked `minihttp` http client lib |
| https://github.com/AppImageCommunity/zsync2     | C++      | ✅          | C++ wrapper for zsync. Using `libcpr` http client lib                                   |
| https://github.com/probonopd/zsync-curl         | C        | 💀          | Mostly original zsync. Using `libcurl` http client lib                                  |
| https://github.com/AppImageCommunity/zsync3     | C++      | 💀          |                                                                                         |
| https://github.com/salesforce/zsync4j           | Java     | 💀          |                                                                                         |
| https://github.com/rokups/zinc/                 | C++      | 💀          |                                                                                         |
| https://github.com/AppImageCrafters/libzsync-go | Go       | 🤷          |                                                                                         |
| https://github.com/Redundancy/go-sync           | Go       | 💀          |                                                                                         |
| https://github.com/Jsmuk/zsyncnet               | .Net     | 💀          |                                                                                         |
| https://github.com/disenone/zsync               | Python 2 | 💀          |                                                                                         |
| https://github.com/kayahr/zsync                 | Node.js  | 🤷          |                                                                                         |
| https://github.com/myml/msync                   | Go       | 💀          |                                                                                         |

