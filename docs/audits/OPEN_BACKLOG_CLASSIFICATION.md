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
| **#16 – Konfigurationsebenen, Validierung und atomare Revisionen**: Tracking fuer #54–#57 | zwingend, aber nicht selbst implementierbar | `CUSTOM_APPLICATION`; sekundaer `ADAPTER_EXISTING_LIBRARY` | #54/#55 gemergt: Storeport, Wireformat, Slots, Dokumente; NVS/Preferences gemaess ADR-016 | Tracking, End-to-End-Vertrag und Abnahme der verbleibenden Graph-/Recoveryarbeit | NVS-Budget und Power-Cut real, Logik nativ | als Tracking behalten, nicht direkt implementieren; vor #56 Umfang entscheiden; Phase 1/2 | **OD-01:** volle #56/#57-Root-/Pending-/Secret-Komplexitaet fuer R1 bestaetigen oder per separatem Entscheid verkleinern |
| **#17 – Laufpersistenz und Kontrollpunkte** | zwingend | `CUSTOM_APPLICATION` | Laufschnappschuss, Laufrevisionen, Zustandsmaschine, `IStateStore`, Envelope/Slots | Kontrollpunktmodell, wichtige Ereignisse, Rueckfall und typisierte Korruption | reales NVS-/Cut-Verhalten spaeter | moeglichst nach gemergter Persistenzbasis entkoppeln und vor #24/#18 umsetzen; Phase 2 | Darf #17 auf #54/#55 plus schmalen Aktivierungsport statt Abschluss des gesamten #16 bauen? |
| **#18 – Wiederanlauf und temperaturgewichteter Fortschritt** | zwingend | `CUSTOM_APPLICATION`; sekundaer `CUSTOM_SAFETY_CORE` | virtuelle Zeit, Zustandsmaschine, Laufmodelle; #17 und #20 fehlen | phasenbezogener Recoveryentscheid, Unsicherheitsintervall und Fortschrittskorrektur | echte Unterbrechungsdaten/Temperaturverlaeufe spaeter | nach #17 und #20, vor Endintegration; Phase 2 | keine Bibliotheksentscheidung; Scope als mehrere kleine PRs schneiden |
| **#19 – Journale, Aufbewahrung, Bereinigung, Backup und Import** | R1-Mindestumfang ja, heutiger Umfang sehr breit | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | `IEventJournal`, typisierte Konfiguration, Wireformat; ArduinoJson als Kandidat | Journal-/Retentionpolicy, begrenzte Historie, Export, portables Backup, Importpreview | reale NVS-/Flash-/Heapbudgets | in Journal/Retention, Export und Backup/Import teilen; erst nach #17 und #56-Minimalentscheid; Phase 2/7 | **OD-07:** Mindestumfang und PR-Schnitt bestaetigen; keine unbegrenzte Komplettloesung |
| **#56 – Konfigurationsmanifeste, Preview und Runtimeaktivierung** | Konfigurationsaktivierung zwingend; konkrete Mechanik auditbeduerftig | `CUSTOM_APPLICATION` | #54/#55, 8-Slot-Limit, typisierte Dokumente, Resolverport | Graphen, Roots, Pending, Intent, Preview, Konflikte und Publish | keine reale Hardware fuer Kern; NVS-Budget spaeter | `BLOCKED_DEPENDENCY` unveraendert lassen; erst nach OD-01; Phase 1/2 | **OD-01:** volle Spezifikation oder kleinere R1-Transaktionsgrenze; bestehende ADR nur in separatem Prozess aendern |
| **#57 – Bootstrap, Secret-Manifeste und Recovery** | Bootstrap/Recovery zwingend, reale Secrets teilweise erst #27 | `CUSTOM_APPLICATION` | #54/#55; #56 fehlt; sicherer Zufallsport | Bootstrap, Epochen, NotProvisioned-Manifeste, Reset und End-to-End-Cuts | NVS/Flashschutz real; Logik nativ | weiterhin blockiert; nach #56 und OD-01; reale WLAN-/Auth-Secrets nicht vorziehen; Phase 2/7 | **OD-01/OD-09:** R1-Minimum, Secret-Grenze und Reihenfolge bestaetigen |

## E3: Sensor-, Regel- und Sicherheitskern

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#20 – Sensorqualitaet, Filterung und Plausibilitaet** | zwingend | `CUSTOM_SAFETY_CORE` | `ITemperatureSource`, Mock, virtuelle Zeit, zentrale Limitmuster | CRC-/Busstatusmodell, `VALID/STALE/FAILED`, Median/Tiefpass, Sprung-/Alterpruefung | reale Timingparameter spaeter | naechstes unabhaengiges Safety-Issue; Treiberdetails nicht einbauen; Phase 2 | keine vor Implementierung; Parameter bleiben commissioning |
| **#21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehr** | zwingend | `CUSTOM_SAFETY_CORE` | Programmmodell, Zustandsmaschine, #20-Ergebnis | optionaler verwendbarer Produktfuehler als primaerer Regelsensor; Raum-/Luftsensor als regulaerer Ersatz; stabile Rueckkehr und Ereignisse; Kuehlkoerper-/Peltier-Schutzsignal getrennt als Pflichtfreigabe | reale Sensorreaktion spaeter | direkt nach #20; Rollenentscheid nicht in den Treiber verlagern; Phase 2 | keine Bibliotheksentscheidung; Rollenprioritaet ist verbindlich |
| **#22 – Zeitproportionale PI-Regelung und Luftbegrenzung** | zwingend | `CUSTOM_SAFETY_CORE` | virtuelle Zeit, Programmlimits; Arduino PID/QuickPID nur Referenz | deterministischer PI, Anti-Windup, Luftbegrenzung und abstrakter Ausgang | Parameter #35 | eigene kleine PI-Implementierung, keine PID-Bibliothek; nach #21; Phase 2 | keine; Autotuning bleibt ausser R1 |
| **#23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik** | zwingend | `CUSTOM_SAFETY_CORE` | bidirektionaler/binaerer Port, Mocks, virtuelle Zeit | Planer, Impulsakkumulator, Richtungswechsel, Watchdog, Nachlauf | reale Zeiten #35 | nach #22, weiterhin ohne GPIO; Phase 2 | keine Bibliotheksentscheidung |
| **#24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion** | zwingendes Gate vor Aktoren | `CUSTOM_SAFETY_CORE` | Zustands-/Kommandokern, Ports/Mocks, Persistenzbasis; #17/#20–#23 fehlen | systemweite Fehlercodes, Latches, Bootprioritaet, Freigaben und Matrix; keine Peltierfreigabe ohne gueltiges, ausreichend vertrauenswuerdiges Kuehlkoerper-/Peltier-Schutzsignal | Hardwareinjektionen spaeter, Kern nativ | vor #29 und vor produktiver Bedienintegration abschliessen; Phase 2 | #17-Abhaengigkeit und minimaler Persistenzpfad bestaetigen |

## E4: Bedienung, Web und Diagnose

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#25 – gemeinsame UI-Modelle, Navigation, Mehrsprachigkeit** | zwingend | `CUSTOM_APPLICATION` | Programme, Zustaende, Kommandos und Konfigurationssprachen vorhanden | gemeinsame View-Modelle und Texte fuer das Touchdisplay als einzige lokale Oberflaeche sowie die sekundaere Weboberflaeche; Navigation und Formatierung | Darstellung spaeter real | nach stabilem Safety-/Fehlermodell; keine Modelle fuer Encoder, Taster oder Status-LED; in View-Modelle und Ressourcen teilbar; Phase 2 | Umfang der gemeinsamen UI-Basis klein halten |
| **#26 – lokale Touchoberflaeche** | zwingend; einzige lokale Bedien- und Anzeigeoberflaeche | `CUSTOM_APPLICATION`; sekundaer `ADAPTER_EXISTING_LIBRARY` | UI-Spezifikation, #25; Displaybibliotheken als Kandidaten | konkrete Screens, Bedienfluss, Kalibrierungs-UI; Treiberadapter #31 | Display/Touch hoch, Logik simulierbar | UI-Logik nativ; keine parallelen lokalen Eingabeports; produktives Rendering erst nach Display-Spike/#31; Phase 3/7 | **OD-02/OD-05:** Displaystack und schlanke Views versus LVGL |
| **#27 – Web-API, Weboberflaeche, Anmeldung und Konflikte** | R1 als sekundaere Bedienoberflaeche | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | Kommandos, Konfigurationsmodelle, `INetworkStatus`; Frameworkserver, ESPAsync, ArduinoJson, WiFiManager Kandidaten | API, Webviews, Authpolicy, Sessions, CSRF, Konflikte, Redaction | WLAN-/Browser-/Lasttest | in Transport/API, Webassets, Auth und Onboarding teilen; der lokale Betrieb darf nicht vom Web abhaengen; erst nach OD-09/#57-Minimum; Phase 7 | **OD-04/OD-06/OD-09:** Server, Onboarding und Authvertrag |
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

## Empfohlene Abhaengigkeitskorrekturen zur Ownerpruefung

1. #20–#23 koennen ohne Hardware und ohne Abschluss von #16 fortgesetzt werden.
2. #17 sollte nur von den tatsaechlich benoetigten gemergten Persistenzbausteinen
   abhaengen, nicht pauschal vom Abschluss des gesamten Tracking-Issues #16.
3. #24 benoetigt einen minimalen persistierten Fehler-/Bootvertrag, aber nicht
   zwingend jede Komfortfunktion aus #56/#57.
4. #30/#31 benoetigen vor der Produktivadapterwahl die dokumentierten Spikes;
   die UI-/Sensorfachlogik bleibt davon getrennt.
5. #19, #25–#28 sind fuer kleine PRs zu breit und sollten nach Ownerfreigabe
   innerhalb ihres Scopes in pruefbare Scheiben geteilt werden.

Diese Vorschlaege veraendern die Live-Issues nicht. Ihre Umsetzung benoetigt
separate Ownerentscheidungen und gegebenenfalls spaetere Issue-/Roadmapupdates.
