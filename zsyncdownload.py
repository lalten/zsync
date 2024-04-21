#!/usr/bin/env python3

"""Example Python implementation of a zsyncranges-based downloader.

This one has no dependencies other than zsyncranges itself.

Run it like
```sh
bazel build //:zsyncranges
PATH="$PATH:$(bazel info bazel-bin)" ./zsyncdownload.py https://example.com/file.zsync file
```
"""

import hashlib
import json
import os
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path
from typing import NamedTuple


class ReuseableRange(NamedTuple):
    dst: int
    src: int
    len: int


class DownloadRange(NamedTuple):
    start: int
    end: int


class ZsyncRanges(NamedTuple):
    length: int
    reuse: list[ReuseableRange]
    download: list[DownloadRange]


def parse_zsyncfile(zsyncfile_path: str) -> dict[str, int | str]:
    headers = {}
    with Path(zsyncfile_path).open("rb") as f:
        while line := f.readline().strip().decode():
            k, v = line.strip().split(": ", 1)
            if k in {"Blocksize", "Length"}:
                v = int(v)
            headers[k] = v
    return headers


def update_file(
    file_path: str, file_url: str, ranges: ZsyncRanges, outfile: str
) -> None:
    downloaded_bytes = 0
    with open(outfile, "wb") as out:
        out.truncate(ranges.length)

        with open(file_path, "rb") as seed:
            for reuse in ranges.reuse:
                out.seek(reuse.dst, os.SEEK_SET)
                seed.seek(reuse.src, os.SEEK_SET)
                out.write(seed.read(reuse.len))

        for start, end in ranges.download:
            req = urllib.request.Request(file_url)
            req.remove_header("Range")
            req.add_header("Range", f"bytes={start}-{end}")
            out.seek(start, os.SEEK_SET)
            try:
                with urllib.request.urlopen(req) as data:
                    while chunk := data.read(1024):
                        out.write(chunk)
                        downloaded_bytes += len(chunk)
            except urllib.error.HTTPError as e:
                raise RuntimeError(
                    f"HTTP Error {e.code} for {req.get_header('Range')} (length {ranges.length})"
                ) from e


def make_url(zsyncurl: str, header_url: str) -> str:
    parsed_url = urllib.parse.urlparse(header_url)
    if parsed_url.netloc:
        return urllib.parse.urlunparse(parsed_url)
    return "/".join(zsyncurl.split("/")[:-1] + [parsed_url.path])


def sha1sum(file_path: str) -> str:
    return hashlib.sha1(Path(file_path).read_bytes()).hexdigest()


def parse_json(json_str: str) -> ZsyncRanges:
    data = json.loads(json_str)
    reuse = [ReuseableRange(*r) for r in data["reuse"]]
    download = [DownloadRange(*r) for r in data["download"]]
    return ZsyncRanges(data["length"], reuse, download)


def main(zsyncurl: str, seedfile: str, outfile: str) -> int:
    zsyncfile, _ = urllib.request.urlretrieve(zsyncurl)

    cmd = ["zsyncranges", zsyncfile, seedfile]  # assume zsyncranges is on $PATH
    out = subprocess.check_output(cmd, text=True)
    ranges = parse_json(out)

    zsync_headers = parse_zsyncfile(zsyncfile)
    fileurl = make_url(zsyncurl, zsync_headers["URL"])
    update_file(seedfile, fileurl, ranges, outfile)

    local_hash = sha1sum(outfile)
    if local_hash != zsync_headers["SHA-1"]:
        raise RuntimeError(f"SHA-1 mismatch: {local_hash} != {zsync_headers['SHA-1']}")
    return os.EX_OK


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} https://example.com/file.zsync infile outfile")
        sys.exit(1)
    sys.exit(main(*sys.argv[1:]))
