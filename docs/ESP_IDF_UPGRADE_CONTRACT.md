# ESP-IDF-Upgradevertrag

## Ziel und Geltung

ESP-IDF `v6.0.2` mit Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` ist die fixierte
ESP32-Produktionsgrundlage. Dieser Vertrag regelt Upgrades der Toolchain und
gilt fuer die getrennten Profile `esp32_bringup` und `esp32_release`.
PlatformIO bleibt ausschliesslich der native Hosttestpfad.

Der kanonische lokale und CI-Build erfolgt ueber
`scripts/build_esp_idf_profiles.py`; `scripts/check_build_profiles.py` prueft
Herkunft, Profilisolation, generierte `sdkconfig`-Dateien und die wirksamen
Compile-Definitionen. ESP-IDF-Static-Analysis erfolgt getrennt mit dem
offiziellen `esp-clang`-Pfad ueber `scripts/run_esp_idf_static_analysis.py`.

## Gemeinsame Regeln

- Der Produktionsstand verwendet keinen unfixierten ESP-IDF-Tag oder lokalen
  Fork ohne ausdruecklichen Ownerentscheid.
- Private ESP-IDF-Header, globale Versionszweige im Fachkern und eine
  generische Wrapper-Schatten-API sind ausgeschlossen.
- Erforderliche Versionszweige werden hier mit Begruendung und expliziter
  Entfernungsbedingung erfasst. Derzeit gibt es keine.
- Ein Upgrade aendert nicht still die Hardware-, GPIO-, Partitions- oder
  Aktorfreigabepolitik.
- Die reale Hardwarepruefung erfolgt nur auf dem exakten finalen
  Implementierungs-Head. Aendert sich dieser Head, sind alle Smoke-Nachweise
  zu wiederholen.

## Komponenten- und Lockfilevertrag

Der aktuelle Produktionsstand verwendet keine externe ESP-IDF-Komponente.
Deshalb gibt es weder ein `idf_component.yml` noch ein `dependencies.lock`;
eine leere Manifest- oder Lockdatei als Scheinstruktur ist nicht zulaessig.

Bei einem spaeteren echten Bedarf werden zuerst offizielle Espressif-
Component-Manager-Komponenten geprueft. Jede uebernommene Abhaengigkeit muss
auf eine feste Version gebunden sein. Direkte Git-Downloads, unfixierte
Versionsbereiche und unbestimmte Quell-URLs sind ausgeschlossen. Sobald die
erste reale Component-Manager-Abhaengigkeit eingebunden wird, ist das erzeugte
`dependencies.lock` zu versionieren und darf nicht manuell bearbeitet werden.
Komponenten- und Lockfile-Diffs werden ab diesem Zeitpunkt Teil der
Upgrade-Nachweise.

## Bugfix-Upgrade

Ein Bugfix-Upgrade verlangt vor Merge:

1. getrennten Vollbuild von Bring-up und Release;
2. alle nativen Tests und Quality Gates;
3. Ressourcenvergleich gegen die letzte Baseline;
4. Diff der generierten `sdkconfig`-Dateien gegen die Overlays;
5. beide Hardware-Smoke-Tests.

## Minor-Upgrade

Zusaetzlich zu den Bugfix-Gates sind offizielle Espressif-
Migrationshinweise, Deprecated-/Removed-API-Funde, ein `sdkconfig`-Diff je
Profil und – sobald vorhanden – ein Komponenten-/Lockfile-Diff zu pruefen. Ein
vollstaendiger Hardware-Paritaetstest ist Pflicht.

## Major-Upgrade

Ein Major-Upgrade ist immer ein eigenes Plan-first-Issue. Es verlangt einen
Parallelbuild bis zur Paritaet, eine Adapter- und Portpruefung fuer
`device_platform_esp_idf`, eine eigene Ressourcen- und Hardwarefreigabe und
darf die stabile Toolchain nicht direkt ueberschreiben.

## Hardware-Smoke-Gate

Vor Merge sind je mindestens 35 Sekunden auf demselben unbelasteten ESP32-
Board ohne externe 12-V-Versorgung, BTS7960, Display, Sensoren, Luefter oder
Peltier zu beobachten. Beide Laeufe verwenden UART/USB und MOSFET-Ausgaenge
ohne Last.

| Profil | Erwarteter Hardwarezustand | Aktorpolicy | Weitere Kriterien |
|---|---|---|---|
| `esp32_bringup` | `HARDWARE_UNVERIFIED` | `LOCKED_FOR_BRINGUP` | reale Aktoren deaktiviert, Anwendung bereit |
| `esp32_release` | `HARDWARE_UNVERIFIED` | `REQUIRE_VERIFIED_HARDWARE` | reale Aktoren deaktiviert, Anwendung bereit |

Fuer beide gelten: erster Heartbeat etwa nach 1000 ms, weitere Abstaende etwa
1000 ms, monotone Uptime, genau zwei Ressourcenmessungen sowie kein Reset,
Watchdog, Panic, Brownout oder unerwartete Hardwareaktivitaet. Der Nachweis
nennt finalen Implementierungs- und Firmware-Build-SHA, die identisch sein
muessen, Profil, seriellen Port, Beobachtungsdauer, gemessene Heartbeats und
Ressourcenwerte sowie PASS/FAIL je Kriterium.

## Verbleibende Messgates

Verbindliche Byte-Schwellen bleiben `TBD_IMPLEMENTATION_BUDGET`. Reale Heap-,
Jitter-, Watchdog-, Flashatomizitaets- und Hardwaremessungen werden nicht durch
einen erfolgreichen CI-Build ersetzt.
