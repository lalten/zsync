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

#----------------------------------------------------------------
echo Need full file
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" /dev/null)"
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[],"download":[[0,1056]]}'
separator

#----------------------------------------------------------------
echo File is complete, need nothing
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$(pwd)/tests/files/loremipsum")"
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[0,0,1057]],"download":[]}'
separator

#----------------------------------------------------------------
echo Changed file, need partial update
cp tests/files/loremipsum "$TEST_TMPDIR/seed"
sed -i 's/massa/xxxxx/g' "$TEST_TMPDIR/seed"
# changed locations are at "grep -ob massa tests/files/loremipsum | cut -d: -f1": 172, 451, 566, 803
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# Ranges fall at change locations
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[0,0,168],[184,184,264],[456,456,104],[576,576,224],[808,808,249]],"download":[[168,183],[448,455],[560,575],[800,807]]}'
separator

#----------------------------------------------------------------
echo Read zsync from stdin
ranges="$(./zsyncranges - "$TEST_TMPDIR/seed" <"$(pwd)/tests/loremipsum.zsync")"
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[0,0,168],[184,184,264],[456,456,104],[576,576,224],[808,808,249]],"download":[[168,183],[448,455],[560,575],[800,807]]}'
separator

#----------------------------------------------------------------
echo Removed some blocks from the front, need partial update
dd if=tests/files/loremipsum of="$TEST_TMPDIR/seed" bs=1 skip=81
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# 81B gone, need 11 blocks (blocksize 8B - 88B) replaced
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[88,7,969]],"download":[[0,87]]}'
separator

#----------------------------------------------------------------
echo Added some blocks to the front
cat - tests/files/loremipsum <<<"This is extra data to be removed" >"$TEST_TMPDIR/seed"
cat "$TEST_TMPDIR/seed"
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[0,33,1057]],"download":[]}'
separator

#----------------------------------------------------------------
echo Added some blocks to the back
cat tests/files/loremipsum - <<<"This is extra data to be removed" >"$TEST_TMPDIR/seed"
cat "$TEST_TMPDIR/seed"
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
test "$ranges" == '{"length":1057,"checksum":{"SHA-1":"b1b8d0c324b78a99abfd2eec90260b1c2ba02eb8"},"reuse":[[0,0,1057]],"download":[]}'
separator
