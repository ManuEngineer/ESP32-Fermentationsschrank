# Agent-Auftrag fuer Issue #16

## Issue

**[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen**

Aktueller Snapshot-Status: `READY`

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/16

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das
> Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung,
> kein Ersatz fuer das Issue oder die Spezifikationsquellen.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich
an Issue #16:
"[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen"

Ich uebernehme Review, Merge und Branch-Loeschung. Merge oder loesche weder PR
noch Branch selbst.

1. Repository vorbereiten

   git checkout main
   git pull --ff-only
   git fetch --prune
   git status --short
   git branch --show-current

   Ist der Arbeitsbaum nicht sauber oder laeuft Merge, Rebase oder Cherry-Pick:
   nichts veraendern oder verwerfen, keinen Branch erstellen, BLOCKED melden und
   anhalten. Arbeite niemals direkt auf main.

2. Vor jeder Aenderung vollstaendig lesen

   - aktuelles GitHub-Issue #16 inklusive Kommentare
   - AGENTS.md
   - lib/device_platform/AGENTS.md
   - lib/device_platform_test_support/AGENTS.md
   - lib/fermentation_app/AGENTS.md
   - docs/SPECIFICATION_REVIEW.md, insbesondere Dokumentationsprioritaet
   - docs/DECISIONS.md
   - docs/SETTINGS_AND_STORAGE.md
   - docs/CONFIGURATION_PERSISTENCE.md
   - docs/BACKUP_SECURITY_RETENTION.md
   - docs/PR38_REVIEW_CORRECTIONS.md
   - docs/CI_AND_QUALITY_GATES.md
   - aktuellen Speicher-, Zeit-, Programm-, Lauf- und Test-Support-Code

3. Freigabe pruefen

   Beginne nur, wenn das Live-Issue READY ist, #9, #10 und #12 geschlossen sind
   und kein offener PR oder Remote-Branch Issue #16 bereits bearbeitet. Bei
   Widerspruch nicht raten, sondern BLOCKED mit Beleg melden.

4. Plan vor Codeaenderungen berichten

   Berichte vorhandenen Stand, fehlende Akzeptanzkriterien, geplante Module und
   Tests, Migrations- und Ressourcenrisiken sowie die Trennung zu #17, #19 und
   #27. Normale Implementierungsdetails selbst entscheiden. Nur bei echter
   fachlicher Ownerentscheidung, Sicherheitswiderspruch, widerspruechlicher
   Spezifikation oder nachweislich nicht implementierbarer Grenze anhalten.

5. Branch

   Erstelle vom aktuellen main:
   feat/issue-16-konfigurationsebenen-validierung-und-atomare-revisio

6. Verbindlicher produktiver Scope

   Implementiere den vollstaendigen Vertrag aus
   docs/CONFIGURATION_PERSISTENCE.md, insbesondere:

   - anwendungsneutrale binaersichere Speicher-, Wire-, CRC-, Envelope-, Slot-,
     Random-, Zeit- und Zeitzonenbausteine in device_platform
   - ausschliesslich anwendungsneutrale Fault-Injection-Adapter in
     device_platform_test_support
   - konkrete Dokumente, Schemas, Manifeste, Roots, IDs, Validierung,
     Orchestrierung und Recovery in fermentation_app
   - UserConfiguration Schema 1 mit Displaysprache, IANA-Zeitzone und sichtbarem
     Geraetenamen
   - leere, aber verpflichtend referenzierte ServiceConfiguration Schema 1
   - ProgramCatalog Schema 1 mit vier Factory-Arbeitskopien und bis zu zwoelf
     Benutzerprogrammen, bestehendem ProgramDocument-Schema und allen Text-, ID-
     und Payloadgrenzen
   - hybride Dokumentrevisionen mit genau einer gemeinsam aktivierten
     Manifestgeneration und letzter gueltiger Rueckfallgeneration
   - genau einen Pending-Zweig; sobald Pending existiert, keine parallelen
     dauerhaft gespeicherten Active-Aenderungen
   - gebundene Aktivierungsabsicht und idempotente Aktivierung durch
     `Anwenden und neu starten`
   - zentralen generationen- und ownergebundenen ConfigurationPreview-Pfad mit
     genau einem Previewplatz, 15 Minuten monotoner Lebensdauer und sicheren
     16-Byte-Zufallswerten
   - erneute Basis-, Gesamtvalidierungs- und Wirkungspruefung vor Commit
   - RuntimeConfigurationSnapshot mit einzigem persistentem Linearisierungspunkt
     am gueltigen ConfigurationRootRecord
   - nicht allokierenden, nicht fehlschlagenden Publish nach Root-Commit
   - Bootstrap, StorageEpoch, wiederaufnehmbaren Werksreset und sichere
     Behandlung beschaedigter bestehender Daten
   - feste Revisions-, Manifest-, Root-, Pending-, Intent- und Secret-Slots mit
     Referenzanalyse statt Referenzzaehlern
   - ConnectivitySecretSetManifest und AuthenticationManifest Schema 1
     ausschliesslich als NotProvisioned
   - vorwaertsgerichtete Authentication-Root-Semantik mit Prepared/Committed
     und eigener CredentialEpoch
   - Copy-Migration, Schemaablehnung und ProgramDocument-Migration im Katalog
   - kanonisches Big-Endian-Wireformat, Envelope-Version 1 und
     CRC-32/ISO-HDLC mit Golden-Vektoren
   - globale monotone MutationSequence je StorageEpoch mit dauerhafter
     Reservierung, Luecken, Ueberlaufschutz und getrennten starken Typen
   - typisierte Commit-, Validierungs-, Persistenz-, Konflikt-, Aktivierungs-
     und Migrationsergebnisse ohne Secret-Leaks
   - zentrale Softwaregrenzen: 32.768 Byte Payload, 32.817 Byte Record,
     49.152 Byte Preview-Dynamik, 256 zusammengefasste Aenderungseintraege und
     hoechstens ein vollstaendiger Record-Arbeitspuffer

7. Verbindliche Architektur- und Sicherheitsregeln

   - device_platform kennt keine Fermentationsdokumente, Factorywerte,
     Manifestbedeutungen, Pending-, Authentication-, Preview- oder Laufsemantik.
   - Nur fermentation_app validiert den vollstaendigen Referenzgraphen und
     entscheidet fachliche Aktivierung.
   - src/main.cpp bleibt reine Composition Root.
   - Ein hoher Sequenzwert allein aktiviert niemals einen Record.
   - NotFound ist kein Lesefehler; beschaedigte Daten sind kein leerer Speicher.
   - Keine Mischung verschiedener Dokumentgenerationen darf sichtbar werden.
   - Der aktive oder wiederherzustellende Lauf und sein Snapshot bleiben durch
     jede Konfigurationsoperation unveraendert.
   - Vor Root-Commit darf jeder Fehler ohne Wirkung abbrechen. Nach Root-Commit
     erfolgt kein stiller Rollback; eine Publish-Vertragsverletzung sperrt
     Aktoren und fuehrt zur sicheren Konfigurationsstoerung.
   - Secrets, Tokens, Nonces und daraus abgeleitete Werte erscheinen nie in
     Logs, Diagnosen, Zusammenfassungen oder Exporten.
   - Keine C++-Exceptions als Voraussetzung fuer Fault Injection oder sichere
     Speicherfehlerbehandlung.
   - Keine PSRAM-Abhaengigkeit und keine grosse neue Abhaengigkeit ohne belegte
     Notwendigkeit und Ressourcenvergleich.

8. Ausdruecklicher Nicht-Scope

   Nicht implementieren:

   - Laufpersistenz, Kontrollpunkte oder Laufjournal aus #17
   - portables Backupformat, Importexport und Aufbewahrung aus #19, ausser dem
     fuer #16 erforderlichen generischen Preview-/Transaktionsvertrag
   - reale WLAN-Credential-Dokumente und Netzwerkvalidierung
   - Passwort-/PIN-Pruefnachweise, KDF, Salt, Work-Factor, Pepper, Anmeldung,
     Sessions, Tokens, CSRF oder Sperrzeiten aus #27
   - noch nicht fachlich definierte Display-, Ton-, Sensor-, Regel-, Hardware-
     oder Sicherheitsfelder
   - freie Secret-Blob- oder Reservefelder
   - physische Recovery-Geste, reale GPIO-/Aktorlogik oder behauptete reale
     Flash-Atomizitaet
   - zweites ProgramDocument-Schema oder stille Factory-Programmaenderungen

9. Verbindliche Tests

   Implementiere die vollstaendige Testmatrix aus
   docs/CONFIGURATION_PERSISTENCE.md. Mindestens:

   - gueltige und ungueltige Dokumente, Schemas, Referenzgraphen und Grenzen
   - minimaler und maximaler ProgramCatalog sowie UTF-8-Grenzfaelle
   - Envelope-/CRC-/Endian-/Bool-/Optional-/binary64-Golden-Vektoren
   - unbekannte Metadaten erhalten, unbekannte fachliche Typen ablehnen
   - Bootstrap, NotFound, Lesefehler, Korruption, Fallback und SAFE_BOOT
   - Active-, Pending-, Intent-, Connectivity-, Authentication-, Migrations-
     und Reset-Workflows an jedem Write-Cut unmittelbar vor und nach Commit
   - workflowspezifische Recovery-Orakel statt pauschalem SAFE_BOOT
   - Prepared erlaubt keine Anmeldung; widerrufene Credentials kehren nicht
     zurueck
   - Pending aktiviert nie ohne passende Absicht und erzeugt keinen Active-Zweig
   - prepare-, Root-, Allocation-, Zeitzonen- und Publish-Fehler
   - gleichzeitige Leser sehen nur vollstaendig alte oder neue Runtimegeneration
   - aktiver Laufschnappschuss bleibt wert- beziehungsweise bytegleich
   - maximaler Preview innerhalb 49.152 Bytes; Recordpuffer innerhalb 32.817
     Bytes; vollstaendige Freigabe nach jedem Ende
   - Test-Support ist in keinem Produktionsprofil enthalten

10. Pruefungen und Ressourcenbericht

   Fuehre mindestens aus:

   - pio test -e native
   - pio run -e native
   - pio run -e esp32_bringup
   - pio run -e esp32_release
   - vollstaendige Format-, clang-tidy-, Architektur-, Geheimnis- und
     Quality-Gate-Pruefungen gemaess docs/CI_AND_QUALITY_GATES.md
   - git diff --check

   Erzeuge mit identischer Toolchain und sauberen Builds fuer Base-SHA und
   PR-Head einen Bericht mit statischem RAM, Flash, firmware.bin, firmware.elf
   und Delta. Werte sind informativ, bis reale Budgets bestaetigt sind.

   Dokumentiere als nicht ausgefuehrt beziehungsweise offen:

   - reale Heapreserve unter Web-/Display-/Regellast
   - reale Replace-Atomizitaet des ESP32-Flashadapters
   - reale Flashlebensdauer
   - hardwarebezogene Reset- und Brownoutpruefungen

11. Dokumentation und Pull Request

   Aktualisiere mindestens CHANGELOG.md und bei technischen Abweichungen die
   kanonischen Spezifikationsquellen. Pruefe vor Commit:

   git status
   git diff --check
   git diff --stat
   git diff

   Committe ausschliesslich Issue-#16-Scope, pushe den Branch und erstelle einen
   PR gegen main. Der PR nennt Scope, Architekturgrenzen, ausgefuehrte und nicht
   ausgefuehrte Tests, Base/Head-Ressourcenwerte, bekannte Einschraenkungen und
   `Closes #16`.

   Nicht mergen, kein Auto-Merge, Branch nicht loeschen, Issue nicht manuell
   schliessen und Issue #17 nicht beginnen. Danach vollstaendig anhalten.
```

## Spezifikationsquellen

- `docs/SETTINGS_AND_STORAGE.md`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`

## Definition of Done

- produktiver Scope und Nicht-Scope eingehalten
- vollstaendige native Test- und Cut-Point-Matrix bestanden
- `native`, `esp32_bringup` und `esp32_release` erfolgreich gebaut
- Quality Gates bestanden
- statische Ressourcenwirkung gegen Base-SHA dokumentiert
- Test-Support aus Produktionsprofilen ausgeschlossen
- reale Hardware-/Adaptermessungen eindeutig als spaetere Gates verknuepft
- Implementierung und Dokumentation abgeschlossen

## Vorgeschlagener Branch

`feat/issue-16-konfigurationsebenen-validierung-und-atomare-revisio`
