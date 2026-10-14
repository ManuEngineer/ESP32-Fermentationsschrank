# G1 IMAGE01 – Flash- und Imageidentität

- Gate: `PR203_RES01_G1_20261014_IMAGE01`.
- Produktbasis: `d35ee85edf4f93f20e9edbe13f8851e406d1b82b`.
- Diagnosequelle: `bb4f43a69740f234d26a951707991f792cc3ffde`.
- App SHA-256: `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0`.
- ELF SHA-256: `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`.
- Ein App-only-Schreibvorgang an `0x10000`, 1,760,560 B; esptool meldete erfolgreiche Hashverifikation.
- Postflash-Readback: App-Prefix stimmt exakt mit dem gepinnten Image überein.
- Vergleich: Bootloader-/Partition-/NVS-/PHY-Region `0x000000–0x00FFFF` und `state_store` `0x300000–0x3FFFFF` byteidentisch; Partitionsbereich ebenfalls identisch; 0 Nicht-App-Byteunterschiede.
- Kandidaten-UART bestätigte Quellcommit, erwartetes Board, Release-Profil, `application: ready`, `HomeConnected` und deaktivierte Aktoren.

Die Original-Readbacks und vollständigen esptool-Leseprotokolle bleiben lokal. Öffentliche SHA256SUMS bindet nur das bereinigte Review-Paket.
