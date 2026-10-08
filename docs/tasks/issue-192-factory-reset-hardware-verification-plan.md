# Issue #192 – Hardwareverifikation lokaler Werksreset: Testablauf (Revision 1)

Nur Evidence/Dokumentation. Keine Produktcodeaenderung, keine neue Reset-/
Recovery-Implementierung. `ACTUATOR_RELEASE=NO`; Aktoren bleiben physisch
getrennt/deaktiviert. PR #193 (#172/D10) bleibt unberuehrt.

## 1. Ausgangslage (geprueft)

```text
BASELINE=df5a3ddf41889b46f9b4d3c64085092a88e25a0a (main, enthaelt PR #191 inkl. ResetEligibleNoRuntime-Slice S1)
GERAET_AKTUELL=sauberes Produkt-Image 3918c50 (ohne Werksreset-Implementierung)
CLEAN_BUILD_BASELINE=esp32_release, --require-clean-source-tree, ESP-IDF v6.1 (fff9895...), App version df5a3dd
  app sha256 06e08f0caa80b9cb949c49009732a492290f5a949b8ead150e00435f45428bd8
  partition-table sha256 d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d (identisch zum Geraet -> nur App flashbar, kein Erase)
```

Hardwarebezogene Befunde aus dem Quellstand (nicht am Geraet nachgewiesen):

- Das Produkt verdrahtet den Ablauf: Hold 5000 ms (`kApprovedFactoryResetHoldMillis`),
  Release-Pfad der Hauptschleife, Press-Dispatcher.
- Zugang 1 "PIN vergessen?": Seite `Pin`, Slot 3. Die Seite `Service` ist (anders als der
  Settings-Eintrag "Service (PIN)", der im Smoke 07.10. nicht oeffenbar war) laut Quelle
  ueber Diagnostics -> Service -> PIN erreichbar; ob dieser Weg am Geraet funktioniert,
  ist offen und wird in R01 geprueft.
- Zugang 2 `SAFE_BOOT`-Eintrag und `ResetEligibleNoRuntime`: erscheint nur in
  `SafeBoot` bzw. wenn der Recoverykern `ResetEligibleNoRuntime` zugelassen hat. Beides
  setzt einen defekten/unaufloesbaren Datenzustand voraus. Ohne rohe NVS-/Flash-
  Manipulation oder kuenstliche Korruption (im Auftrag ausgeschlossen) ist kein
  sicherer Eintrittspfad bekannt -> R03 ist voraussichtlich `BLOCKED`.
- **Persistenzrisiko beim Firmwarewechsel:** zwischen `3918c50` und `df5a3dd` liegt
  Issue #30 (C2): `ServiceConfiguration` Schema 3 (Payload 81 -> 179 Byte), Migration von
  Schema 1/2 vorhanden. Nach einem Schreibvorgang unter der neuen Firmware ist ein
  Zurueckflashen auf `3918c50` fuer diese Daten nicht gesichert (Downgrade nicht
  geprueft). Fuer den Werksreset (loescht ohnehin die Konfiguration) ist das
  unerheblich; fuer den Rueckweg auf `3918c50` relevant.

## 2. Ablauf

| Phase | Inhalt | Destruktiv | Gate |
|---|---|---|---|
| P0 | Baseline, sauberer Build `df5a3dd`, Hashes (erledigt, siehe oben) | nein | – |
| P1 | Flash nur der App (0x10000), kein Erase, kein NVS-/State-Store-Erase; Boot, `application: ready`, Touchkalibrierung vorhanden, Datenmigration sichtbar | nein (Schema-Migration moeglich, siehe oben) | **Owner-Gate G1** |
| P2 / R01 nicht destruktiv | Erreichbarkeit "PIN vergessen?" (Diagnostics -> Service -> PIN) und `SAFE_BOOT`-Eintrag; Warnung, Bestaetigung; Textbreiten DE/EN/ES; Loslassen/Verschieben/Abbruch vor 5000 ms; Daten unveraendert (Vorher/Nachher-Vergleich ueber Anzeige und UART) | nein | – |
| P3 / R01+R02 destruktiv | erster vollstaendiger 5000-ms-Hold; Beobachtung Reset, Neustart, Ersteinrichtung, Kalibrierungserhalt, Aktoren AUS; R02: AP-/Client-Trennung, `esp_wifi_stop()`, `httpd_stop()` anhand UART, Client, HTTP | **ja** | **Owner-Gate G2 (vorher konkrete Datenliste)** |
| P4 / R03 | `SAFE_BOOT`/`ResetEligibleNoRuntime`: nur vorhandener sicherer Eintrittspfad; sonst `BLOCKED` mit konkreter Luecke fuer den Owner | nein / ja | **G2 bzw. Ownerentscheid zur Luecke** |
| P5 | Evidence-Bericht, Roadmap/Issue #192 nur anhand tatsaechlich durchgefuehrter Tests | nein | – |

Stromunterbrechung waehrend der Reset-Schreibschritte (R02-Cutpoints) wird **nicht**
durchgefuehrt, solange der Owner kein reproduzierbares Verfahren freigegeben hat (G3).
Ohne G3 bleibt dieser Teil `NOT_RUN`.

## 3. Messung und Abbruch

- UART-Aufnahme mit Zeitstempeln durchgehend (Port bei `--before default-reset`-Reset);
  Client-/HTTP-Belege fuer R02 vom Host aus.
- Abbruch ohne Fix bei: Crash, WDT, OOM, nicht bestaetigtem Netzwerk-/HTTP-Stopp,
  unerwartetem Speicherzustand, nicht erreichbarem Einstieg. Beleg sichern, Befund dem Owner
  vorlegen, keine stille Korrektur.
- Rohlogs lokal; im Repository nur Hashes und bereinigte Werte (keine SSID, MAC,
  Passwoerter).
- Ergebnis je HW-19-R01/R02/R03 getrennt `PASS`/`FAIL`/`NOT_RUN`/`BLOCKED` mit
  Ausgangszustand, exakter Firmware-/Build-SHA, Interaktion, UART-/Client-Belegen,
  Resetursache, Einschraenkungen.

## 4. Konkrete Ownerentscheide

- **G1** Flash von `df5a3dd` (nur App, ohne Erase) freigeben; Kenntnisnahme Schema-3-
  Migration und unsicherer Rueckweg auf `3918c50`.
- **G2** Vor dem ersten vollstaendigen Hold: Bestaetigung der konkreten Daten (Programme,
  Grenzen, Konfiguration, Netzwerk-/Zugangsdaten, Authentifizierungszustand der
  Epoche) werden auf Werkszustand zurueckgesetzt; Touchkalibrierung (`tc0`/`tc1`)
  bleibt; danach Ersteinrichtung. Wiederherstellbarkeit: nur ueber Neueinrichtung; die
  Liste wird vor G2 gegen den Quellstand konkretisiert vorgelegt.
- **G3** Powercut-Verfahren (optional; sonst `NOT_RUN`).
- **G4** R03: Umgang mit der fehlenden sicheren `SAFE_BOOT`-Vorbedingung (Optionen:
  `BLOCKED` belassen / Owner nennt vorhandenen sicheren Pfad).
