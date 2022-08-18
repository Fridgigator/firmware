#!/usr/bin/env sh

mkdir /dest
cp -r /src /dest
cd /dest/src

rm -rf ./build
mkdir ./build

set -e

cd ./build
cmake .. -DRust_CARGO_TARGET=thumbv6m-none-eabi -DRust_TOOLCHAIN=stable-x86_64-unknown-linux-gnu -DCMAKE_MAKE_PROGRAM=gmake -DCorrosion_DIR=/usr/local/lib64/cmake/Corrosion
make
cp ./firmware.uf2 /out/firmware.uf2
