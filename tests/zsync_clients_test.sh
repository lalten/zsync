#!/bin/bash

set -euxo pipefail

function separator() { echo -e "\n\n"; }

# Fetch prerequisites
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20220117/zsync2-37-c679907-x86_64.AppImage
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20220117/zsync2-37-c679907-x86_64.AppImage.zsync
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage
curl -fsSLO https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync
separator

echo zsyncranges: Update from 37 to 63
./zsyncranges \
    "$(pwd)/zsync2-37-c679907-x86_64.AppImage.zsync" \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    >out
test "$(cat out)" == "{\"length\":11363520,\
\"checksum\":{\"SHA-1\":\"259214c9238ecfa3cac38c647fe4c328516bf5de\"},\
\"reuse\":[[112640,116815,4096],[141312,145888,14336],[178176,181209,8192],[11360256,181209,3264],\
[3659776,4100882,14336],[3676160,4117266,14336],[3692544,4133650,14336],[3708928,4150034,14336],\
[3725312,4166418,14336],[3741696,4182802,14336],[3758080,4199186,8192],[3772416,4213522,10240],\
[4554752,5021518,391168],[11347968,11909463,6144]],\
\"download\":[[0,112639],[116736,141311],[155648,178175],[186368,3659775],[3674112,3676159],[3690496,3692543],\
[3706880,3708927],[3723264,3725311],[3739648,3741695],[3756032,3758079],[3766272,3772415],[3782656,4554751],\
[4945920,11347967],[11354112,11360255]]}"
separator

echo zsync: Update from 37 to 63
./zsync \
    -i "$(pwd)/zsync2-37-c679907-x86_64.AppImage" \
    -u https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage \
    -o "$(pwd)/out" \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-63-1608115-x86_64.AppImage | cut -d' ' -f1)"
rm out*
separator

echo zsyncdownload.py: Update from 37 to 63
./zsyncdownload \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20230304/zsync2-63-1608115-x86_64.AppImage.zsync \
    "$(pwd)/zsync2-37-c679907-x86_64.AppImage" \
    "$(pwd)/out"
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-63-1608115-x86_64.AppImage | cut -d' ' -f1)"
separator

echo zsyncranges: Update from 63 to 37
./zsyncranges \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage.zsync" \
    "$(pwd)/zsync2-37-c679907-x86_64.AppImage" \
    >out
test "$(cat out)" == "{\"length\":11924672,\
\"checksum\":{\"SHA-1\":\"53ba3c59d868390faf22af8455a0076555bfaecc\"},\
\"reuse\":[[10240,10128,4096],[116736,112561,4096],[145408,140832,14336],[182272,177113,8192],[11921408,177113,3264],\
[4100096,3658990,14336],[4116480,3675374,14336],[4132864,3691758,14336],[4149248,3708142,14336],\
[4165632,3724526,14336],[4182016,3740910,14336],[4198400,3757294,8192],[4212736,3771630,10240],\
[5021696,4554930,391168],[11909120,11347625,8192]],\
\"download\":[[0,10239],[14336,116735],[120832,145407],[159744,182271],[190464,4100095],[4114432,4116479],\
[4130816,4132863],[4147200,4149247],[4163584,4165631],[4179968,4182015],[4196352,4198399],[4206592,4212735],\
[4222976,5021695],[5412864,11909119],[11917312,11921407]]}"
separator

echo zsync: Update from 63 to 37
./zsync \
    -i "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    -u https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20220117/zsync2-37-c679907-x86_64.AppImage \
    -o "$(pwd)/out" \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20220117/zsync2-37-c679907-x86_64.AppImage.zsync
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-37-c679907-x86_64.AppImage | cut -d' ' -f1)"
rm out*
separator

echo zsyncdownload.py: Update from 37 to 63
./zsyncdownload \
    https://github.com/AppImageCommunity/zsync2/releases/download/2.0.0-alpha-1-20220117/zsync2-37-c679907-x86_64.AppImage.zsync \
    "$(pwd)/zsync2-63-1608115-x86_64.AppImage" \
    "$(pwd)/out"
test "$(sha1sum out | cut -d' ' -f1)" == "$(sha1sum zsync2-37-c679907-x86_64.AppImage | cut -d' ' -f1)"
separator
