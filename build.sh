#!/usr/bin/env bash
set -x
set -e

# bin/zephyr.elf is committed, so CI does not build Zephyr; it only archives the ELF. To rebuild it,
# from the directory containing this repository:
#   west init -l <this repository> && west update
#   west build -b nucleo_h533re <this repository>/app && cp build/zephyr/zephyr.elf <this repository>/bin/
mkdir -p artifacts
cp bin/zephyr.elf artifacts/
