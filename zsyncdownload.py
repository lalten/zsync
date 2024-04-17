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
import urllib.request
import urllib.parse


def parse_zsyncfile(zsyncfile_path: str) -> dict[str, int | str]:
    headers = {}
    with Path(zsyncfile_path).open("rb") as f:
        while line := f.readline().strip().decode():
            k, v = line.strip().split(": ", 1)
            if k in {"Blocksize", "Length"}:
                v = int(v)
            headers[k] = v
    return headers


def update_file(file_path: str, file_url: str, ranges: list[tuple[int, int]]) -> None:
    if not ranges:
        return
    req = urllib.request.Request(file_url)
    total_bytes = sum(end - start + 1 for start, end in ranges)
    downloaded_bytes = 0
    with open(file_path, "wb") as f:
        for start, end in ranges:
            req.remove_header("Range")
            req.add_header("Range", f"bytes={start}-{end}")
            f.seek(start)
            with urllib.request.urlopen(req) as data:
                while chunk := data.read(1024):
                    f.write(chunk)
                    downloaded_bytes += len(chunk)
                    progress = round(100 * downloaded_bytes / total_bytes)
                    print(f"{progress}%", end="\r")


def make_url(zsyncurl: str, header_url: str) -> str:
    parsed_url = urllib.parse.urlparse(header_url)
    if parsed_url.netloc:
        return urllib.parse.urlunparse(parsed_url)
    return "/".join(zsyncurl.split("/")[:-1] + [parsed_url.path])


def sha1sum(file_path: str) -> str:
    return hashlib.sha1(Path(file_path).read_bytes()).hexdigest()


def main(zsyncurl: str, seedfile: str) -> int:
    zsyncfile, _ = urllib.request.urlretrieve(zsyncurl)

    cmd = ["zsyncranges", zsyncfile, seedfile]
    out = subprocess.check_output(cmd, text=True)
    ranges = json.loads(out)["ranges"]

    headers = parse_zsyncfile(zsyncfile)
    fileurl = make_url(zsyncurl, headers["URL"])
    update_file(seedfile, fileurl, ranges)

    if sha1sum(seedfile) == headers["SHA-1"]:
        return os.EX_OK
    return 1


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} https://example.com/file.zsync file")
        sys.exit(1)
    sys.exit(main(sys.argv[1], sys.argv[2]))
