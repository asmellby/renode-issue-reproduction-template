#!/usr/bin/env bash
set -x
set -e

# Builds src/test.c into artifacts/test.elf. CROSS selects the toolchain prefix.
CROSS=${CROSS:-arm-none-eabi-}
if ! command -v "${CROSS}gcc" > /dev/null; then
    sudo apt-get update
    sudo apt-get install -y gcc-arm-none-eabi
fi

make -C src CROSS="$CROSS"
mkdir -p artifacts
cp src/test.elf artifacts/test.elf
