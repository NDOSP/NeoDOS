#!/bin/bash
set -e

BUILD_DIR=build
OVMF_DIR=$BUILD_DIR/OVMF
DISK_IMG=$BUILD_DIR/disk.img

OVMF_SOURCE=/usr/share/ovmf/OVMF.fd

if [[ ! -f "$OVMF_SOURCE" ]]; then
    echo "OVMF.fd not found at $OVMF_SOURCE"
    exit 1
fi

mkdir -p $OVMF_DIR

cp "$OVMF_SOURCE" "$OVMF_DIR/OVMF_CODE.fd"

cp "$OVMF_SOURCE" "$OVMF_DIR/OVMF_VARS.fd"

if [[ ! -f "$DISK_IMG" ]]; then
    echo "Disk image $DISK_IMG not found."
    exit 1
fi

rm -rf debug/
mkdir debug/

qemu-system-x86_64 \
    -m 60m \
    -drive if=pflash,format=raw,readonly=on,file=$OVMF_DIR/OVMF_CODE.fd \
    -drive if=pflash,format=raw,file=$OVMF_DIR/OVMF_VARS.fd \
    -drive file=$DISK_IMG,format=raw \
    -boot menu=on \
    -net none \
    -d int \
    -serial file:debug/serial.log
