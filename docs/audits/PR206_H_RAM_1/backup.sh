#!/bin/bash
set -e
export IDF_PATH=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1
. $IDF_PATH/export.sh >/dev/null 2>&1
P="--port /dev/ttyUSB0 --baud 460800"
esptool $P --before default-reset --after no-reset read-flash 0x0 0x10000 pre_0x0_0x10000.bin
esptool $P --before default-reset --after no-reset read-flash 0x10000 0x2F0000 backup_factory_A.bin
esptool $P --before default-reset --after no-reset read-flash 0x10000 0x2F0000 backup_factory_B.bin
sha256sum pre_0x0_0x10000.bin backup_factory_A.bin backup_factory_B.bin
