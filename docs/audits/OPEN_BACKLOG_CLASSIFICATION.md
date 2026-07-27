# Klassifikation des offenen Implementierungsbacklogs

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Umfang

Live-Stand: 2026-07-27. Erfasst sind alle offenen Implementierungs- und
Tracking-Issues #16–#37 sowie #56 und #57. Die offenen Epiccontainer #3–#8 und
das Audit-Issue #62 sind keine Implementierungsissues und werden deshalb nicht
als eigene Arbeitsbausteine klassifiziert. Kein Issue wurde durch diesen Audit
veraendert.

Primaerkategorien:

`ADOPT_LIBRARY`, `ADAPTER_EXISTING_LIBRARY`, `CUSTOM_SAFETY_CORE`,
`CUSTOM_APPLICATION`, `CONFIGURE_FRAMEWORK`, `DEFER_AFTER_R1`,
`BLOCKED_HARDWARE`, `REMOVE_OR_REPLACE`.

"Reihenfolge" verweist auf die Phasen der
[`vorgeschlagenen Roadmap`](PROPOSED_RELEASE_1_ROADMAP.md), nicht auf eine
bereits beschlossene Issueaenderung.

Verbindlicher Querschnitt fuer diese Klassifikation: Das Touchdisplay ist die
einzige lokale Bedien- und Anzeigeoberflaeche, die Weboberflaeche ist sekundaer
und der Summer das einzige zusaetzliche lokale Ausgabeelement. Encoder,
Programmwahlschalter, Start-/Stop-Taster und Status-LED sind dauerhaft kein
Bestandteil dieses Projekts; der 230-V-AC-Hauptschalter ist kein
Firmwareeingang. Beim Temperaturvertrag ist der optionale, verwendbare
Produktfuehler primaer, der Raum-/Luftsensor regulaerer Ersatz und der
Kuehlkoerper-/Peltier-Schutzsensor verpflichtende Freigabegrundlage.

## E2: Persistenz und Recovery

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#16 – Konfigurationsebenen, Validierung und atomare Revisionen**: Tracking fuer #54–#57 | zwingend, aber nicht selbst implementierbar | `CUSTOM_APPLICATION`; sekundaer `ADAPTER_EXISTING_LIBRARY` | #54/#55 gemergt: Storeport, Wireformat, Slots, Dokumente; NVS/Preferences gemaess ADR-016 | Tracking und Abnahme des entschiedenen Variante-B-Kerns; spaetere Variante-A-Arbeit getrennt halten | NVS-Budget und Power-Cut real, Logik nativ | nicht unveraendert fortsetzen; nach dem Audit in separatem ownerfreigegebenem Planungs-/ADR-Schritt auf R1-Kern und spaetere additive Arbeit neu schneiden; Phase 1/2 | OD-01 ist entschieden; Audit aendert Issue oder ADR nicht |
| **#17 – Laufpersistenz und Kontrollpunkte** | zwingend | `CUSTOM_APPLICATION` | Laufschnappschuss, Laufrevisionen, Zustandsmaschine, `IStateStore`, Envelope/Slots | Kontrollpunktmodell, wichtige Ereignisse, Rueckfall und typisierte Korruption | reales NVS-/Cut-Verhalten spaeter | auf #54/#55 und nur die tatsaechlich benoetigten schmalen Variante-B-Vertraege stuetzen; keine Abhaengigkeit von Pending-/Secret-Infrastruktur; vor #24/#18; Phase 2 | keine OD-01-Frage mehr; exakte schmale Abhaengigkeit im Planungs-/ADR-Schritt festlegen |
| **#18 – Wiederanlauf und temperaturgewichteter Fortschritt** | zwingend | `CUSTOM_APPLICATION`; sekundaer `CUSTOM_SAFETY_CORE` | virtuelle Zeit, Zustandsmaschine, Laufmodelle; #17 und #20 fehlen | phasenbezogener Recoveryentscheid, Unsicherheitsintervall und Fortschrittskorrektur | echte Unterbrechungsdaten/Temperaturverlaeufe spaeter | nach #17 und #20, vor Endintegration; Phase 2 | keine Bibliotheksentscheidung; Scope als mehrere kleine PRs schneiden |
| **#19 – Journale, Aufbewahrung, Bereinigung, Backup und Import** | R1-Mindestumfang ja, heutiger Umfang sehr breit | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | `IEventJournal`, typisierte Konfiguration, Wireformat; ArduinoJson als Kandidat | Journal-/Retentionpolicy, begrenzte Historie, Export, portables Backup, Importpreview | reale NVS-/Flash-/Heapbudgets | in Journal/Retention, Export und Backup/Import teilen; erst nach #17 und dem neu geschnittenen #56-R1-Kern; Phase 2/7 | **OD-07:** Mindestumfang und PR-Schnitt bestaetigen; keine unbegrenzte Komplettloesung |
| **#56 – Konfigurationsmanifeste, Preview und Runtimeaktivierung** | schlanke Konfigurationsaktivierung zwingend | `CUSTOM_APPLICATION` | #54/#55, 8-Slot-Limit, typisierte Dokumente, Resolverport | vollstaendig validiertes `ActiveConfigurationManifest`; kanonischer Root; genau eine Fallbackgeneration; Rotation fuer Active/Fallback/laufende Mutation; Graphvalidierung; fluechtige Vorschau und Konfliktschutz; vorbereiteter Runtime-Snapshot; atomarer Publish und typisierte Fehler | keine reale Hardware fuer Kern; NVS-Budget/Cut-Points spaeter | nicht unveraendert implementieren; zuerst separater Planungs-/ADR-Neuschnitt auf Variante B; persistentes Pending, Pending-Root, Intent und Pending-Abschlusslogik in spaetere eigene Arbeit verschieben; Phase 1/2 | OD-01 entschieden; separat pruefen, ob Dokumentrevisionen und Rootsequenz die Funktion einer `MutationSequence` vollstaendig abdecken |
| **#57 – Bootstrap, Secret-Manifeste und Recovery** | Bootstrap und Reset zwingend; vorbereitete Secret-Domaenen nicht Teil des schlanken Kerns | `CUSTOM_APPLICATION` | #54/#55; neu geschnittener #56-R1-Kern erforderlich | sicherer Bootstrap mit `Initializing`/`Initialized`/`Resetting`; Factory-Erkennung nur bei fehlerfrei lesbarem fabrikneuem Speicher; `StorageEpoch`; Korruptionssperre; idempotent wiederaufnehmbarer Werksreset und logische Unerreichbarkeit alter Epochen | NVS/Cut-Verhalten real; keine physische Loeschgarantie | nicht unveraendert implementieren; zuerst separater Planungs-/ADR-Neuschnitt; vorbereitete Connectivity-/Authentication-Manifeste, Authentication-Roots, `CredentialEpoch` und kombinierte Secret-Transaktionen in spaetere konsumentennahe Issues verschieben; Phase 1/2/7 | OD-01 entschieden; OD-09 bleibt erst fuer reale Authentication-Nachweise relevant |

## E3: Sensor-, Regel- und Sicherheitskern

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#20 – Sensorqualitaet, Filterung und Plausibilitaet** | zwingend | `CUSTOM_SAFETY_CORE` | `ITemperatureSource`, Mock, virtuelle Zeit, zentrale Limitmuster | CRC-/Busstatusmodell, `VALID/STALE/FAILED`, Median/Tiefpass, Sprung-/Alterpruefung | reale Timingparameter spaeter | naechstes unabhaengiges Safety-Issue; Treiberdetails nicht einbauen; Phase 2 | keine vor Implementierung; Parameter bleiben commissioning |
| **#21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehr** | zwingend | `CUSTOM_SAFETY_CORE` | Programmmodell, Zustandsmaschine, #20-Ergebnis | optionaler verwendbarer Produktfuehler als primaerer Regelsensor; Raum-/Luftsensor als regulaerer Ersatz; stabile Rueckkehr und Ereignisse; Kuehlkoerper-/Peltier-Schutzsignal getrennt als Pflichtfreigabe | reale Sensorreaktion spaeter | direkt nach #20; Rollenentscheid nicht in den Treiber verlagern; Phase 2 | keine Bibliotheksentscheidung; Rollenprioritaet ist verbindlich |
| **#22 – Zeitproportionale PI-Regelung und Luftbegrenzung** | zwingend | `CUSTOM_SAFETY_CORE` | virtuelle Zeit, Programmlimits; Arduino PID/QuickPID nur Referenz | deterministischer PI, Anti-Windup, Luftbegrenzung und abstrakter Ausgang | Parameter #35 | eigene kleine PI-Implementierung, keine PID-Bibliothek; nach #21; Phase 2 | keine; Autotuning bleibt ausser R1 |
| **#23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik** | zwingend | `CUSTOM_SAFETY_CORE` | bidirektionaler/binaerer Port, Mocks, virtuelle Zeit | Planer, Impulsakkumulator, Richtungswechsel, Watchdog, Nachlauf | reale Zeiten #35 | nach #22, weiterhin ohne GPIO; Phase 2 | keine Bibliotheksentscheidung |
| **#24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion** | zwingendes Gate vor Aktoren | `CUSTOM_SAFETY_CORE` | Zustands-/Kommandokern, Ports/Mocks, Persistenzbasis; #17/#20–#23 fehlen | systemweite Fehlercodes, Latches, Bootprioritaet, Freigaben und Matrix; Publish-Vertragsfehler aus Variante B; keine Peltierfreigabe ohne gueltiges, ausreichend vertrauenswuerdiges Kuehlkoerper-/Peltier-Schutzsignal | Hardwareinjektionen spaeter, Kern nativ | nur vom benoetigten schmalen Fehler-/Boot-/Publish-Vertrag abhaengig machen, nicht von Pending oder vorbereiteten Secret-Domaenen; vor #29 und Bedienintegration; Phase 2 | minimalen Persistenzpfad im separaten Planungs-/ADR-Schritt festlegen |

## E4: Bedienung, Web und Diagnose

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#25 – gemeinsame UI-Modelle, Navigation, Mehrsprachigkeit** | zwingend | `CUSTOM_APPLICATION` | Programme, Zustaende, Kommandos und Konfigurationssprachen vorhanden | gemeinsame View-Modelle und Texte fuer das Touchdisplay als einzige lokale Oberflaeche sowie die sekundaere Weboberflaeche; Navigation und Formatierung | Darstellung spaeter real | nach stabilem Safety-/Fehlermodell; keine Modelle fuer Encoder, Taster oder Status-LED; in View-Modelle und Ressourcen teilbar; Phase 2 | Umfang der gemeinsamen UI-Basis klein halten |
| **#26 – lokale Touchoberflaeche** | zwingend; einzige lokale Bedien- und Anzeigeoberflaeche | `CUSTOM_APPLICATION`; sekundaer `ADAPTER_EXISTING_LIBRARY` | UI-Spezifikation, #25; Displaybibliotheken als Kandidaten | konkrete Screens, Bedienfluss, Kalibrierungs-UI; Treiberadapter #31 | Display/Touch hoch, Logik simulierbar | UI-Logik nativ; keine parallelen lokalen Eingabeports; produktives Rendering erst nach Display-Spike/#31; Phase 3/7 | **OD-02/OD-05:** Displaystack und schlanke Views versus LVGL |
| **#27 – Web-API, Weboberflaeche, Anmeldung und Konflikte** | R1 als sekundaere Bedienoberflaeche | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | Kommandos, Konfigurationsmodelle, `INetworkStatus`; Frameworkserver, ESPAsync, ArduinoJson, WiFiManager Kandidaten | API, Webviews, Authpolicy, Sessions, CSRF, Konflikte, Redaction und erste reale Authentication-/Connectivity-Nachweise | WLAN-/Browser-/Lasttest | in Transport/API, Webassets, Auth und Onboarding teilen; der lokale Betrieb darf nicht vom Web abhaengen; reale Secret-Domaene erst mit diesem Konsumenten und nach OD-09 festlegen; Phase 7 | **OD-04/OD-06/OD-09:** Server, Onboarding und Authvertrag |
| **#28 – Diagnose, Diagramme, Service und Exporte** | R1-Mindestumfang ja, heutiger Umfang breit | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | strukturierte Kernmodelle, Ports/Mocks; ArduinoJson Kandidat | Diagnose-DTOs, Serviceablauf, Charts, Export/Redaction | reale Ressourcen und Aktortests | in passive Diagnose/Export und aktiven Serviceablauf teilen; Phase 7, Hardwareaktionen erst nach Gates | **OD-07:** Mindestcharts/-historie und PR-Schnitt |

## E5: ESP32- und Hardwareintegration

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#29 – ESP32-Bring-up, Partition, Ressourcen, sichere Ausgaenge** | zwingendes Hardwaregate | `BLOCKED_HARDWARE`; sekundaer `CONFIGURE_FRAMEWORK` | fixierte PlatformIO-/Arduino-Toolchain, sichere Profile, UART/esptool, ADR-016 NVS | reale Baseline, NVS-Adapter/Partition, sichere Boardkonfiguration und Protokoll; kein Firmwareeingang fuer den 230-V-AC-Hauptschalter | vollstaendig | erster Hardware-PR; keine Aktorfreigabe und keine GPIO-Reserve fuer Encoder, Programmwahlschalter, Start-/Stop-Taster oder Status-LED; Phase 3/6 | Boardrevision, Hardwarezugang und Messfreigabe |
| **#30 – DS18B20-Busse und reale Sensoradapter** | zwingend | `BLOCKED_HARDWARE`; sekundaer `ADAPTER_EXISTING_LIBRARY` | Sensorport, #20/#21; Dallas+OneWire und Espressif-Kandidaten | duennner, rollenunabhaengiger Adapter fuer Bus-/ROM-Konfiguration und Fehleruebersetzung | vollstaendig | DS18B20-Spike vor Auswahl, danach kleiner Adapter-PR; Rollenprioritaet und Safety verbleiben in #20/#21/#24; Phase 3–6 | **OD-03:** Kandidat anhand identischer Messung |
| **#31 – Display- und Touchadapter** | zwingend fuer die einzige lokale Bedien- und Anzeigeoberflaeche | `BLOCKED_HARDWARE`; sekundaer `ADAPTER_EXISTING_LIBRARY` | #25/#26-Modelle; fuenf Kandidaten evaluiert | Display-/Touchadapter, Kalibrierungspersistenz und Ressourcenprofil | vollstaendig | verbindlicher Display-Spike, danach genau eine Bibliothek; keine weiteren lokalen Eingabe-/Anzeigeadapter; Phase 3–6 | **OD-02/OD-05:** Treiber und UI-Framework nach Messung |
| **#32 – Luefter, Summer, MOSFET-Ausgaenge** | zwingend | `BLOCKED_HARDWARE`; sekundaer `CONFIGURE_FRAMEWORK` | binaerer Port/Mock, #23/#24 | GPIO-/eventuell PWM-Adapter und reale Rollenbindung fuer Luefter und den Summer als einziges zusaetzliches lokales Ausgabeelement | vollstaendig | keine externe Luefterbibliothek und keine Status-LED; unbelastet vor Verbraucher; Phase 6 | reale Kanaele, Pegel, Verbraucherwerte |
| **#33 – BTS7960 und begrenzte Peltierpruefung** | zwingend | `BLOCKED_HARDWARE`; sekundaer `CUSTOM_SAFETY_CORE` | bidirektionaler Port, #23/#24, Infineon-Datenblatt | GPIO-/Enable-Adapter, Hardwareprofil und sichere Testintegration | vollstaendig und sicherheitskritisch | erst nach #29/#30/#32 und Gate 2B; keine generische BTS7960-Library; Phase 6 | Pulldowns, Polaritaet, Temperatursicherung, R_IS/L_IS |

## E6: Inbetriebnahme und Abnahme

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#34 – Sensorvergleich, Offsets, thermische Grundvermessung** | zwingendes Commissioning-Gate | `BLOCKED_HARDWARE` | Diagnose-/Exportmodelle spaeter, drei reale Sensoren | Messplan ausfuehren, Offsets und Thermikdaten versionieren | vollstaendig | nach #29–#33; keine Treiberwahl mehr; Phase 8 | Referenzmessgeraete, Massen und Aufbau bereitstellen |
| **#35 – PI-Parameter, Luftbegrenzung, Sicherheitsgrenzen** | zwingend | `BLOCKED_HARDWARE`; sekundaer `CUSTOM_SAFETY_CORE` | #22–#24 und Messdaten #34 | vier Parametersaetze und nachgewiesene Grenzen | vollstaendig | reine Parametrierung/Validierung, kein PID-Autotuning; Phase 8 | Ownerfreigabe der gemessenen Sicherheitsrevision |
| **#36 – Hardwareabnahme, Fehlerinjektionen, Standardprogramme** | zwingend | `BLOCKED_HARDWARE` | gesamte vorherige Software/Adapter, Acceptance-Testvertrag | reale Matrix, Programmdaten und Abweichungsmanagement | vollstaendig | Gate vor Releasebelastung; Phase 8 | Abnahmeverantwortung und sichere Testfreigaben |
| **#37 – siebentaegiger Belastungstest und R1-Abnahme** | zwingendes Schlussgate | `BLOCKED_HARDWARE` | Ressourcen-/Testinstrumentierung aus allen vorigen Issues | sieben Tage ausfuehren, Resultate bewerten, Releaseentscheid | vollstaendig | letzter Schritt; keine neue Funktion waehrend Test; Phase 8 | formelle Ownerentscheidung Release/Blockade |

## Verbindliche Folgen und weitere Abhaengigkeitskorrekturen

1. #20–#23 koennen ohne Hardware und ohne Abschluss von #16 fortgesetzt werden.
2. #16/#56/#57 muessen nach dem Audit in einem separaten ownerfreigegebenen
   Planungs-/ADR-Schritt auf Variante B zugeschnitten werden; sie duerfen nicht
   unveraendert implementiert werden.
3. #17 haengt nur von den tatsaechlich benoetigten gemergten
   Persistenzbausteinen und schmalen R1-Vertraegen ab, nicht pauschal vom
   gesamten Tracking-Issue #16 oder von Variante-A-Funktionen.
4. #24 benoetigt einen minimalen persistierten Fehler-/Boot- und
   Publish-Vertragsfehlerpfad, aber kein Pending, Intent oder vorbereitete
   Secret-Domaenen.
5. #30/#31 benoetigen vor der Produktivadapterwahl die dokumentierten Spikes;
   die UI-/Sensorfachlogik bleibt davon getrennt.
6. #19, #25–#28 sind fuer kleine PRs zu breit und sollten nach Ownerfreigabe
   innerhalb ihres Scopes in pruefbare Scheiben geteilt werden.

Der Audit veraendert die Live-Issues und ADRs nicht. Der verbindlich notwendige
Neuschnitt und alle spaeteren Issues fuer Pending oder reale Secret-Domaenen
erfolgen erst in einem separaten ownerfreigegebenen Planungsschritt.
