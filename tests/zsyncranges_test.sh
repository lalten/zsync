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
test "$ranges" == "[[0,591]]"

# File is complete, need nothin
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$(pwd)/tests/files/loremipsum")"
test "$ranges" == "[]"

# Changed file, need partial update
cp tests/files/loremipsum "$TEST_TMPDIR/seed"
sed -i 's/Lorem/Hello/g' "$TEST_TMPDIR/seed"
# changed locations are at "grep -ob Lorem tests/files/loremipsum | cut -d: -f1": 0, 268, 296, 564
ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"
# Ranges fall at change locations
test "$ranges" == "[[0,7],[264,279],[296,303],[560,575]]"
