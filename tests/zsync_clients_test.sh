#!/bin/bash

set -euxo pipefail

function separator() { echo -e "\n\n"; }

# Fetch prerequisites
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230303/zsync2-61-dc43891-x86_64.AppImage
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230303/zsync2-61-dc43891-x86_64.AppImage.zsync
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync
separator

echo zsyncranges: Update from 61 to 63
./zsyncranges \
    "$(pwd)/zsync2-61-dc43891-x86_64.AppImage.zsync" \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    >out
test "$(cat out)" == '{"length":11392192,"reuse":[[0,0,180224],[182272,181209,8192],[2717696,3662195,1306624],[4313088,4969843,466944],[7923712,5439359,231424]],"download":[[180224,182271],[190464,2717695],[4024320,4313087],[4780032,7923711],[8155136,11392191]]}'
separator

echo zsync: Update from 61 to 63
./zsync \
    -i "$(pwd)/zsync2-61-dc43891-x86_64.AppImage" \
    -u https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage \
    -o "$(pwd)/out" \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-63-1608115-x86_64.AppImage | cut -d' ' -f1)"
rm out*
separator

echo zsyncdownload.py: Update from 61 to 63
./zsyncdownload \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync \
    "$(pwd)/zsync2-61-dc43891-x86_64.AppImage" \
    "$(pwd)/out"
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-63-1608115-x86_64.AppImage | cut -d' ' -f1)"
separator

echo zsyncranges: Update from 63 to 61
./zsyncranges \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage.zsync" \
    "$(pwd)/zsync2-61-dc43891-x86_64.AppImage" \
    >out
test "$(cat out)" == '{"length":11924672,"reuse":[[0,0,180224],[182272,181209,8192],[11921408,181209,3264],[3661824,2717325,1306624],[4970496,4313741,466944],[5439488,7923841,231424]],"download":[[180224,182271],[190464,3661823],[4968448,4970495],[5437440,5439487],[5670912,11921407]]}'
separator

echo zsync: Update from 63 to 61
./zsync \
    -i "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    -u https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230303/zsync2-61-dc43891-x86_64.AppImage \
    -o "$(pwd)/out" \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230303/zsync2-61-dc43891-x86_64.AppImage.zsync
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-61-dc43891-x86_64.AppImage | cut -d' ' -f1)"
rm out*
separator

echo zsyncdownload.py: Update from 61 to 63
./zsyncdownload \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230303/zsync2-61-dc43891-x86_64.AppImage.zsync \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    "$(pwd)/out"
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-61-dc43891-x86_64.AppImage | cut -d' ' -f1)"
separator
