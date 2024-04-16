#!/bin/bash

set -euxo pipefail

# File doesn't even exist yet
if out="$(2>&1 ./zsyncranges "$(pwd)/tests/loremipsum.zsync" /invalid)"; then
    echo "Expected failure, got: $out"
    exit 1
fi
test "$out" == "/invalid: No such file or directory"

# Need full file
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" /dev/null)"
test "$ranges" == "[[0,1063]]"

# File is complete, need nothing
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$(pwd)/tests/files/loremipsum")"
test "$ranges" == "[]"

# Changed file, need partial update
cp tests/files/loremipsum "$TEST_TMPDIR/seed"
sed -i 's/massa/xxxxx/g' "$TEST_TMPDIR/seed"
# changed locations are at "grep -ob massa tests/files/loremipsum | cut -d: -f1": 172, 451, 566, 803
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# Ranges fall at change locations
test "$ranges" == "[[168,183],[448,455],[560,575],[800,807]]"

# Removed some blocks from the front, need partial update
dd if=tests/files/loremipsum of="$TEST_TMPDIR/seed" bs=1 skip=81
cat "$TEST_TMPDIR/seed"
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# 81B gone, need 11 blocks (blocksize 8B - 88B) replaced
test "$ranges" == "[[0,87]]"
