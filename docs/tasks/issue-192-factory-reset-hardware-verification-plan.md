# Issue #192 – Hardwareverifikation lokaler Werksreset: Testablauf (Revision 2)

Nur Evidence/Dokumentation. Keine Produktcodeaenderung, keine neue Reset-/
Recovery-Implementierung. `ACTUATOR_RELEASE=NO`; Aktoren bleiben physisch
getrennt/deaktiviert. PR #194 (Revision 1 auf Baseline `df5a3dd`) wurde
geschlossen und nicht gemergt; dieser Plan setzt dieselbe Arbeit in einem neuen
Draft-PR auf aktuellem `main` fort. PR #193 (#172/D10) bleibt unberuehrt.

Revision 2 ersetzt Revision 1 vollstaendig (kein paralleler Plan). Die
Ownerfreigabe fuer Revision 1 (G1 fuer `df5a3dd`, G4 = `BLOCKED` belassen) war
an den damaligen Stand gebunden und gilt fuer den neuen Stand **nicht**; alle
Gates in Abschnitt 4 sind neu zu erteilen.

**Ausfuehrungsstand (2026-10-10):** G1 erteilt; P1 und P2/R01-N (N1–N8) ohne
Hold/Vollreset ausgefuehrt, Befund in
`docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`. G2–G4 offen.

## 1. Ausgangslage (geprueft am 2026-10-10)

```text
BASELINE=21082de766a0b7b52108448c85ae51ba8d84b3f1 (main, Merge von PR #199; enthaelt PR #191 inkl. S1 ResetEligibleNoRuntime und PR #199 Service-PIN-Einstieg)
QUELLUNTERSCHIED_ZU_f859ef6=nur docs/ (4393d27 docs: HW-188-A01 Evidence + Merge-Commit); Produktquellen identisch zum in HW-188-A01 geflashten Stand f859ef6
GERAET_AKTUELL=f859ef6 (UART-Boot-Log 2026-10-10, quellgleich zu main 21082de, kein Flash; Provenienz in der Evidence 20261010)
ISSUE_STAND=#192 OPEN / BLOCKED_HARDWARE; PR #191 und PR #199 gemergt; HW-19-R01..R03 auf dem aktuellen main NOT_RUN
```

Build-Provenienz der Baseline (sauberer Build, `--require-clean-source-tree`,
Profil `release`, ESP-IDF v6.1 `fff9895c82d744c7237be8847347bdd1b07c6643`):

```text
SOURCE_HEAD=21082de766a0b7b52108448c85ae51ba8d84b3f1 (detached Worktree, git status sauber, SOURCE_TREE_CLEAN=YES)
BUILD=python3 scripts/build_esp_idf_profiles.py release --require-clean-source-tree  -> PASS (Profil esp32_release)
APP_VERSION_STRING=21082de (im Image enthalten)
APP_BIN_SHA256=9416e64013d37503257f1818614889767eae39cb9f51668f0f910d8ac5599ec7  (1 740 064 Byte)
APP_ELF_SHA256=d3196493f3b8f734a0d7c1fd24e6fbbaef0b34fc7e8c971921ad6823c7e2af10
PARTITION_TABLE_SHA256=d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d  (identisch zu df5a3dd und f859ef6 -> nur App flashbar, kein Erase)
BOOTLOADER_SHA256=d70a1e164a87b2950cb569fba92ab123fab45f770b8814010f2926968416902f  (weicht vom in HW-188-A01 genannten Wert 2c1b4979... ab; Ursache nicht untersucht; Bootloader wird nicht geflasht)
FLASH_ARGS=0x1000 bootloader, 0x8000 Partitionstabelle, 0x10000 App; geplant ist ausschliesslich 0x10000
FLASH_UND_GERAETEZUGRIFF=NICHT AUSGEFUEHRT (kein Flash, kein Port geoeffnet)
```

Bisherige Hashes/Images von `df5a3dd` (App `06e08f0c...`) gelten **nicht** fuer
den aktuellen `main` und duerfen nicht fuer ihn herangezogen werden.

Hardwarebezogene Befunde aus dem Quellstand (nicht am Geraet nachgewiesen):

- Das Produkt verdrahtet den Ablauf `Warning -> Confirm -> Hold`
  (`FactoryResetStage`): Hold 5000 ms (`kApprovedFactoryResetHoldMillis`),
  Release-Pfad der Hauptschleife, Press-Dispatcher.
- **Zugang 1 "PIN vergessen?"** ist mit PR #199 im Produktcode regulaer
  erreichbar: `Einstellungen -> Service (PIN) -> PIN-Seite -> "PIN vergessen?"`
  (Slot 3, auch waehrend einer PIN-Sperre). Der in Revision 1 dokumentierte
  Zugangsblocker (Settings-Zeile `Service (PIN)` gesperrt) ist damit
  **im Produktcode behoben**. Am Geraet belegt HW-188-A01 (f859ef6) den Weg bis
  "PIN vergessen?" -> Warnung -> Abbrechen (kein Reset). Das ist **kein**
  HW-19-R01-Nachweis; R01 bleibt `NOT_RUN`. Die historischen Ergebnisse
  (R01 `BLOCKED`, R03 `BLOCKED` auf `df5a3dd`) werden nicht rueckwirkend
  geaendert.
- **Zugang 2 `SAFE_BOOT`-Eintrag und `ResetEligibleNoRuntime`:** erscheint nur in
  `SafeBoot` bzw. wenn der Recoverykern `ResetEligibleNoRuntime` zugelassen hat.
  Beides setzt einen defekten/unaufloesbaren Datenzustand voraus. Ohne rohe
  NVS-/Flash-Manipulation oder kuenstliche Korruption (im Auftrag
  ausgeschlossen) ist kein sicherer Eintrittspfad bekannt -> R03 bleibt
  `BLOCKED`, bis der Owner einen sicheren Pfad benennt (G4).
- **Persistenzrisiko beim Firmwarewechsel:** Ein Flash des aktuellen `main` auf
  ein Geraet mit aelterem Image (`3918c50`/`df5a3dd`) kann Schema-Migrationen
  ausloesen (u. a. `ServiceConfiguration` Schema 3); ein Zurueckflashen ist fuer
  diese Daten nicht gesichert. Fuer den Werksreset (loescht die Konfiguration
  ohnehin) unerheblich, fuer den Rueckweg relevant. Laeuft das Geraet bereits
  mit `f859ef6`, entfaellt dieses Risiko (Quellen identisch).

## 2. Ablauf

Nichtdestruktive Schritte sind von destruktiven Schritten, Netzwerkstopp/
Powercut und `SAFE_BOOT`-/NoRuntime-Sonderfaellen getrennt. Nur die Phasen P0
und die Vorbereitung von P2 sind in diesem Auftrag erledigt; **keine** Phase
mit Hardwareeingriff wurde ausgefuehrt.

| Phase | Inhalt | Destruktiv | Gate |
|---|---|---|---|
| P0 | Baseline `21082de`, sauberer Build, Hashes (erledigt, Abschnitt 1) | nein | – |
| P1 | Geraetestand per UART-Boot-Log feststellen; nur wenn er nicht dem Build entspricht: Flash nur der App (0x10000), kein Erase, kein NVS-/State-Store-Erase; Boot, `application: ready`, Touchkalibrierung vorhanden | nein (Schema-Migration moeglich, Abschnitt 1) | **Owner-Gate G1** |
| P2 / R01-N | **nichtdestruktive** R01-Schritte (Abschnitt 2.1) | nein | – (nach P1) |
| P2b / R01-H | Hold-Fortschritt/-Abbruch (Druck auf das Halteziel, Loslassen vor 5000 ms) | nein **nur bei korrektem Loslassen**; ein versehentliches Halten >= 5000 ms loest den vollstaendigen Reset aus | **Owner-Gate G2** (vor dem ersten Druck auf das Halteziel) |
| P3 / R01+R02 destruktiv | erster vollstaendiger 5000-ms-Hold; Beobachtung Reset, Neustart, Ersteinrichtung, Kalibrierungserhalt, Aktoren AUS; R02: AP-/Client-Trennung, `esp_wifi_stop()`, `httpd_stop()` anhand UART, Client, HTTP | **ja** | **Owner-Gate G2 (vorher konkrete Datenliste)** |
| P4 / R03 | `SAFE_BOOT`/`ResetEligibleNoRuntime`: nur vorhandener sicherer Eintrittspfad; sonst `BLOCKED` mit konkreter Luecke fuer den Owner | nein / ja | **G4**, bei Ausfuehrung zusaetzlich G2 |
| P5 | Evidence-Bericht, Roadmap/Issue #192 nur anhand tatsaechlich durchgefuehrter Tests | nein | – |

Stromunterbrechung waehrend der Reset-Schreibschritte (R02-Cutpoints) wird
**nicht** durchgefuehrt, solange der Owner kein reproduzierbares Verfahren
freigegeben hat (G3). Ohne G3 bleibt dieser Teil `NOT_RUN`.

### 2.1 Nichtdestruktive R01-Schritte (nach P1, ohne G2)

Alle Schritte laufen mit Aktoren physisch getrennt, ohne Druck auf das Halteziel
und enden im `Abbrechen`-Pfad. Reihenfolge und erwartetes Ergebnis:

| # | Schritt am Geraet | Erwartung / Beleg |
|---|---|---|
| N1 | Startseite -> `Einstellungen` -> `Service (PIN)` antippen | PIN-Seite oeffnet sich (Zeile nicht gesperrt); Owner-Beobachtung, UART: keine Fehlersignatur |
| N2 | PIN-Seite: "PIN vergessen?" sichtbar und antippbar (ohne PIN-Eingabe) | Wechsel in `Warning`; Owner-Beobachtung |
| N3 | Warnung: Datenverlusttext vollstaendig lesbar, keine abgeschnittenen Texte | Sprachen DE, EN, ES nacheinander (Textbreiten); Owner-Beobachtung |
| N4 | Warnung -> `Abbrechen` | zurueck zur PIN-Seite, kein Reset-/Panic-Signal, Zustand unveraendert |
| N5 | Warnung -> weiter zu `Confirm`; Text pruefen; `Abbrechen` | wie N3/N4 fuer die Bestaetigungsseite |
| N6 | Warnung -> `Confirm` -> weiter zu `Hold`; **nur Anzeige**, Halteziel **nicht beruehren**; `Abbrechen` | Halteseite zeigt Fortschritt 0, Texte DE/EN/ES; kein Reset |
| N7 | N2 waehrend aktiver PIN-Sperre (3 Fehlversuche) wiederholen | "PIN vergessen?" bleibt erreichbar; entspricht L2 aus HW-188-A01, hier fuer R01 neu zu belegen |
| N8 | Vorher/Nachher-Vergleich: Programme, Grenzen, Sprache, Geraetename, WLAN-Verbindung; UART: `touch press dispatch`-Zeilen | Daten unveraendert, kein Reset-Header, kein Panic/WDT/Brownout/OOM |

Erst nach G2 (Abschnitt 4) folgt P2b: Druck auf das Halteziel und Loslassen
bzw. Verschieben **vor** 5000 ms (Fortschritt setzt zurueck), sonst nur mit
Reset-Folge.

## 3. Messung und Abbruch

- UART-Aufnahme mit Zeitstempeln durchgehend, **eine** Aufnahme (Port nicht
  mitten im Test neu oeffnen; ein Oeffnen kann das Board zuruecksetzen); Port
  bei `--before default-reset` nur zu Beginn; Marker ueber Datei. Client-/HTTP-
  Belege fuer R02 vom Host aus.
- Abbruch ohne Fix bei: Crash, WDT, OOM, nicht bestaetigtem Netzwerk-/HTTP-Stopp,
  unerwartetem Speicherzustand, nicht erreichbarem Einstieg. Beleg sichern,
  Befund dem Owner vorlegen, keine stille Korrektur.
- Rohlogs lokal; im Repository nur Hashes und bereinigte Werte (keine SSID, MAC,
  Passwoerter, keine Webpasswoerter im Chat).
- Ergebnis je HW-19-R01/R02/R03 getrennt `PASS`/`FAIL`/`NOT_RUN`/`BLOCKED` mit
  Ausgangszustand, exakter Firmware-/Build-SHA, Interaktion, UART-/Client-Belegen,
  Resetursache, Einschraenkungen. Ergebnisse des Stands `df5a3dd` bleiben als
  historische Evidence erhalten und werden nicht uebernommen.

## 4. Konkrete Ownerentscheide (neu zu erteilen)

- **G1** Flash des aktuellen `main` (`21082de`, nur App, ohne Erase) freigeben,
  falls der festgestellte Geraetestand nicht bereits dem Build entspricht;
  Kenntnisnahme Schema-Migration und unsicherer Rueckweg auf aeltere Images.
  Fuer die nichtdestruktiven Schritte P2/R01-N ist nur G1 noetig.
- **G2** Vor dem ersten Druck auf das Halteziel (P2b) und vor dem ersten
  vollstaendigen Hold (P3): Bestaetigung der konkreten Daten (Programme,
  Grenzen, Konfiguration, Netzwerk-/Zugangsdaten, Authentifizierungszustand der
  Epoche) werden auf Werkszustand zurueckgesetzt; Touchkalibrierung (`tc0`/`tc1`)
  bleibt; danach Ersteinrichtung. Wiederherstellbarkeit: nur ueber
  Neueinrichtung; die Liste wird vor G2 gegen den Quellstand konkretisiert
  vorgelegt.
- **G3** Powercut-Verfahren (optional; sonst `NOT_RUN`).
- **G4** R03: Umgang mit der fehlenden sicheren `SAFE_BOOT`-Vorbedingung
  (Optionen: `BLOCKED` belassen / Owner nennt vorhandenen sicheren Pfad).
  Bis dahin `BLOCKED`.

Dieser Plan und der zugehoerige Auftrag erteilen **keine** Flash-,
NVS-Erase-, Werksreset-, 5000-ms-Hold-, Powercut- oder `SAFE_BOOT`-
Manipulationsfreigabe.
