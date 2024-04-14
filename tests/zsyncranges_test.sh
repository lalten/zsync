#!/bin/bash

set -euxo pipefail

cp tests/files/loremipsum "$TEST_TMPDIR/seed"
sed -i 's/Lorem/Hello/g' "$TEST_TMPDIR/seed"
# changed locations are at "grep -ob Lorem tests/files/loremipsum | cut -d: -f1": 0, 268, 296, 564

ranges="$(./zsyncranges "$(pwd)/tests/loremipsum.zsync" "$TEST_TMPDIR/seed")"

# Ranges fall at change locations
test "$ranges" == "[[0,7],[264,279],[296,303],[560,575]]"
