# Issue #29 Build- und Ressourcenbericht

Dieser Bericht gehört zu Issue #29 und ist dem Implementierungs-HEAD
`764759f8b307be75ce0acc8042b8fdef79b19ea7` zugeordnet. Die ESP-IDF-Profile
wurden mit ESP-IDF `v6.0.2` / Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` gebaut.

## `esp32_bringup`

- `size.json total_size`: 192519 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 192640 Bytes
- ELF: 6958632 Bytes
- Mapfile: 5021101 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `2b1de6d6a368794932df27e4bdc9e7e4d3d0b709c788db2bf67bb2c487d07961`

## `esp32_release`

- `size.json total_size`: 126475 Bytes
- DRAM: 12618 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 126592 Bytes
- ELF: 3132344 Bytes
- Mapfile: 2722263 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `788bf5fda2065bdf0bcff4775021498a264d05f768ca9a08c23aef0bb53dfa78`

## Geltungsgrenze

Die Werte sind Build-/Artefaktwerte, keine Messung eines angeschlossenen
Boards. Beide Profile verwenden im generierten Build die aktuelle
ESP-IDF-Single-App-Tabelle:

```text
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,1M,
```

Das ist eine erfasste aktuelle Buildbaseline und keine Festlegung der
finalen Produktionspartitionierung. Reale Flashgröße, Boardrevision und
PSRAM-Status bleiben bis zur Hardwaremessung offen.
