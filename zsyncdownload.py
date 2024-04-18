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
import os
from pathlib import Path
import sys
import subprocess
import json
import tempfile
from typing import NamedTuple
import urllib.request
import urllib.parse
import shutil


class ReuseableRange(NamedTuple):
    dst: int
    src: int
    len: int


class DownloadRange(NamedTuple):
    start: int
    end: int


class ZsyncRanges(NamedTuple):
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
    new_file_size: int, file_path: str, file_url: str, ranges: ZsyncRanges
) -> None:
    req = urllib.request.Request(file_url)
    downloaded_bytes_total = sum(end - start + 1 for start, end in ranges.download)
    downloaded_bytes = 0
    with tempfile.SpooledTemporaryFile() as temp:
        temp.write(b"\0" * new_file_size)

        with open(file_path, "rb") as seed:
            for reuse in ranges.reuse:
                temp.seek(reuse.dst, os.SEEK_SET)
                seed.seek(reuse.src, os.SEEK_SET)
                temp.write(seed.read(reuse.len))

        for start, end in ranges.download:
            req.remove_header("Range")
            req.add_header("Range", f"bytes={start}-{end}")
            temp.seek(start, os.SEEK_SET)
            with urllib.request.urlopen(req) as data:
                while chunk := data.read(1024):
                    temp.write(chunk)
                    downloaded_bytes += len(chunk)
                    progress = round(100 * downloaded_bytes / downloaded_bytes_total)
                    print(f"{progress}%", end="\r")

        temp.seek(0, os.SEEK_SET)
        with open(file_path, "wb") as out:
            shutil.copyfileobj(temp, out)


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
    return ZsyncRanges(reuse, download)


def main(zsyncurl: str, seedfile: str) -> int:
    zsyncfile, _ = urllib.request.urlretrieve(zsyncurl)

    cmd = ["zsyncranges", zsyncfile, seedfile]
    out = subprocess.check_output(cmd, text=True)
    ranges = parse_json(out)

    zsync_headers = parse_zsyncfile(zsyncfile)
    fileurl = make_url(zsyncurl, zsync_headers["URL"])
    update_file(zsync_headers["Length"], seedfile, fileurl, ranges)

    if sha1sum(seedfile) == zsync_headers["SHA-1"]:
        return os.EX_OK
    return 1


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} https://example.com/file.zsync file")
        sys.exit(1)
    sys.exit(main(sys.argv[1], sys.argv[2]))
