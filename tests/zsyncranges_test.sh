#!/bin/bash

set -euxo pipefail
function separator() { echo -e "\n\n"; }

# File doesn't even exist yet
if out="$(./zsyncranges 2>&1 "$(pwd)/tests/loremipsum.zsync" /invalid)"; then
    echo "Expected failure, got: $out"
    exit 1
fi
test "$out" == "/invalid: No such file or directory"
separator

# Need full file
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" /dev/null)"
test "$ranges" == '{"ranges":[[0,1063]]}'
separator

# File is complete, need nothing
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$(pwd)/tests/files/loremipsum")"
test "$ranges" == '{"ranges":[]}'
separator

# Changed file, need partial update
cp tests/files/loremipsum "$TEST_TMPDIR/seed"
sed -i 's/massa/xxxxx/g' "$TEST_TMPDIR/seed"
# changed locations are at "grep -ob massa tests/files/loremipsum | cut -d: -f1": 172, 451, 566, 803
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# Ranges fall at change locations
test "$ranges" == '{"ranges":[[168,183],[448,455],[560,575],[800,807]]}'
separator

# Removed some blocks from the front, need partial update
dd if=tests/files/loremipsum of="$TEST_TMPDIR/seed" bs=1 skip=81
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# 81B gone, need 11 blocks (blocksize 8B - 88B) replaced
test "$ranges" == '{"ranges":[[0,87]]}'
separator

# Added some blocks to the front
dd if=tests/files/loremipsum of="$TEST_TMPDIR/seed" bs=1 skip=81
cat - tests/files/loremipsum <<<"This is extra data to be removed" >"$TEST_TMPDIR/seed"
cat "$TEST_TMPDIR/seed"
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
test "$ranges" == '{"ranges":[]}' # TODO: fix!
separator

# Added some blocks to the back
dd if=tests/files/loremipsum of="$TEST_TMPDIR/seed" bs=1 skip=81
cat tests/files/loremipsum - <<<"This is extra data to be removed" >"$TEST_TMPDIR/seed"
cat "$TEST_TMPDIR/seed"
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
test "$ranges" == '{"ranges":[]}' # TODO: fix!
separator
