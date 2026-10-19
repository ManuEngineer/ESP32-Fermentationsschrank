#!/bin/bash
set -e
export IDF_PATH=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1
. $IDF_PATH/export.sh >/dev/null 2>&1
P="--port /dev/ttyUSB0 --baud 460800"
esptool $P --before default-reset --after no-reset write-flash --flash-mode keep --flash-size keep --flash-freq keep 0x10000 ../artifacts/overlay.bin
SZ=$(stat -c %s ../artifacts/overlay.bin)
esptool $P --before default-reset --after no-reset read-flash 0x10000 $SZ post_app_overlay_readback.bin
esptool $P --before default-reset --after no-reset read-flash 0x0 0x10000 post_0x0_0x10000.bin
sha256sum ../artifacts/overlay.bin post_app_overlay_readback.bin pre_0x0_0x10000.bin post_0x0_0x10000.bin
