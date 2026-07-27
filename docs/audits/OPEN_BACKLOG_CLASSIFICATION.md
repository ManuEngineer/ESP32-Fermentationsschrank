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
| **#19 – Journale, Aufbewahrung, Bereinigung, Backup und Import** | R1-relevant; OD-07-Teilentscheid bindet den spaeteren Schnitt, Issue selbst bleibt im Audit unveraendert | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | `IEventJournal`, typisierte Konfiguration, binaeres Wireformat, Laufmodelle und OD-01-Active-/Fallback-Kern; ArduinoJson `7.4.3` als bevorzugter `SPIKE_REQUIRED`-Kandidat nur fuer externe Vertraege | vier geordnete Bereiche: A typisiertes Ereignisjournal/Retention ohne periodische Rohmessungen oder Secrets; B begrenzte verdichtete Laufhistorie/stromausfallsichere Bereinigung; C nur lesender Laufexport/secret-freies Backup; D vollvalidierte Importvorschau/atomare Aktivierung | reale NVS-/Partitions-/Flash-/Heapbudgets, Fuellen bis zur Bereinigung, Cut-/Recoverytests sowie JSON-Grenz-/Fuzz-/Ressourcennachweis; 5 detaillierte Laeufe/50 Zusammenfassungen sind Messziel | Reihenfolge A Journal/Retention, B Historie/Bereinigung, C Export/Backup, D Import/Aktivierung; erst nach #17 und fuer D nach stabiler neu geschnittener #56-R1-Basis; Werksreset bleibt bei #57, Touchkalibrierung in zentraler Resetpolicy; keine neuen Issues in diesem Audit | **OD-07 teilweise entschieden:** #19, #25 und #26 sind verbindlich geschnitten; #27/#28 bleiben offen, endgueltige Codec-Uebernahme bleibt Spike-Gate |
| **#56 – Konfigurationsmanifeste, Preview und Runtimeaktivierung** | schlanke Konfigurationsaktivierung zwingend | `CUSTOM_APPLICATION` | #54/#55, 8-Slot-Limit, typisierte Dokumente, Resolverport | vollstaendig validiertes `ActiveConfigurationManifest`; kanonischer Root; genau eine Fallbackgeneration; Rotation fuer Active/Fallback/laufende Mutation; Graphvalidierung; fluechtige Vorschau und Konfliktschutz; vorbereiteter Runtime-Snapshot; atomarer Publish und typisierte Fehler | keine reale Hardware fuer Kern; NVS-Budget/Cut-Points spaeter | nicht unveraendert implementieren; zuerst separater Planungs-/ADR-Neuschnitt auf Variante B; persistentes Pending, Pending-Root, Intent und Pending-Abschlusslogik in spaetere eigene Arbeit verschieben; Phase 1/2 | OD-01 entschieden; separat pruefen, ob Dokumentrevisionen und Rootsequenz die Funktion einer `MutationSequence` vollstaendig abdecken |
| **#57 – Bootstrap, Secret-Manifeste und Recovery** | Bootstrap und Reset zwingend; vorbereitete Secret-Domaenen nicht Teil des schlanken Kerns | `CUSTOM_APPLICATION` | #54/#55; neu geschnittener #56-R1-Kern erforderlich | sicherer Bootstrap mit `Initializing`/`Initialized`/`Resetting`; Factory-Erkennung nur bei fehlerfrei lesbarem fabrikneuem Speicher; `StorageEpoch`; Korruptionssperre; idempotent wiederaufnehmbarer Werksreset und logische Unerreichbarkeit alter Epochen | NVS/Cut-Verhalten real; keine physische Loeschgarantie | nicht unveraendert implementieren; zuerst separater Planungs-/ADR-Neuschnitt; vorbereitete Connectivity-/Authentication-Manifeste, Authentication-Roots, `CredentialEpoch` und kombinierte Secret-Transaktionen in spaetere konsumentennahe Issues verschieben; Phase 1/2/7 | OD-01 entschieden; OD-09 bleibt erst fuer reale Authentication-Nachweise relevant |

## E3: Sensor-, Regel- und Sicherheitskern

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#20 – Sensorqualitaet, Filterung und Plausibilitaet** | zwingend | `CUSTOM_SAFETY_CORE` | `ITemperatureSource`, Mock, virtuelle Zeit, zentrale Limitmuster | CRC-/Busstatusmodell, `VALID/STALE/FAILED`, Median/Tiefpass, Sprung-/Alterpruefung | reale Timingparameter spaeter | naechstes unabhaengiges Safety-Issue; Treiberdetails nicht einbauen; Phase 2 | keine vor Implementierung; Parameter bleiben commissioning |
| **#21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehr** | zwingend | `CUSTOM_SAFETY_CORE` | Programmmodell, Zustandsmaschine, #20-Ergebnis | optionaler verwendbarer Produktfuehler als primaerer Regelsensor, der in Stillstand und zulaessigem Lauf fehlen darf; Raum-/Luftsensor als regulaerer Ersatz; stabile Rueckkehr und Ereignisse; Kuehlkoerper-/Peltier-Schutzsignal getrennt als Pflichtfreigabe, auch bei veraltetem oder nicht vertrauenswuerdigem Wert | reale Sensorreaktion spaeter | direkt nach #20; Rollenentscheid nicht in den Treiber verlagern; technische Anwesenheits-/Wiederanschlussereignisse konsumieren; Phase 2 | keine Bibliotheksentscheidung; Rollenprioritaet ist verbindlich |
| **#22 – Zeitproportionale PI-Regelung und Luftbegrenzung** | zwingend | `CUSTOM_SAFETY_CORE` | virtuelle Zeit, Programmlimits; Arduino PID/QuickPID nur Referenz | deterministischer PI, Anti-Windup, Luftbegrenzung und abstrakter Ausgang | Parameter #35 | eigene kleine PI-Implementierung, keine PID-Bibliothek; nach #21; Phase 2 | keine; Autotuning bleibt ausser R1 |
| **#23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik** | zwingend | `CUSTOM_SAFETY_CORE` | bidirektionaler/binaerer Port, Mocks, virtuelle Zeit | Planer, Impulsakkumulator, Richtungswechsel, Watchdog, Nachlauf | reale Zeiten #35 | nach #22, weiterhin ohne GPIO; Phase 2 | keine Bibliotheksentscheidung |
| **#24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion** | zwingendes Gate vor produktiven Aktoren, nicht vor aktorfreien Spikes | `CUSTOM_SAFETY_CORE` | Zustands-/Kommandokern, Ports/Mocks, Persistenzbasis; #17/#20–#23 fehlen | systemweite Fehlercodes, Latches, Bootprioritaet, Freigaben und Matrix; Publish-Vertragsfehler aus Variante B; keine Peltierfreigabe ohne gueltiges, ausreichend vertrauenswuerdiges Kuehlkoerper-/Peltier-Schutzsignal | Hardwareinjektionen spaeter, Kern nativ | parallel zur minimalen Hardwarebaseline und den aktorfreien Spikes entwickeln; fuer produktive Aktoradapter/-tests zwingend, aber keine Vorbedingung fuer Baseline oder Bibliotheksevaluation; Phase 2/3/6 | minimalen Persistenzpfad im separaten Planungs-/ADR-Schritt festlegen |

## E4: Bedienung, Web und Diagnose

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#25 – gemeinsame UI-Modelle, Navigation, Mehrsprachigkeit** | R1-relevant; OD-07-Teilentscheid bindet den spaeteren Zweierschnitt, Issue selbst bleibt im Audit unveraendert | `CUSTOM_APPLICATION` | Programme, Zustaende, Kommandos, Sensorqualitaet, Meldungen, Berechtigungen und Konfigurationssprachen vorhanden | A kleine oberflaechenneutrale Praesentationsmodelle fuer Fachfakten, Qualitaet/Alter, Regelsensor, Meldungen, Aktionen/Sperrgruende und Revisionen; B stabile Textschluessel, DE/ES/EN-Kataloge, deutscher Fallback und semantische Formatierung | Modelle, Kataloge und Tests nativ; reale Pixel-/Textpassung spaeter | nach stabilen Fach-/Safetyzustaenden; Touchnavigation/Layout nach #26, Webnavigation/responsives Layout nach #27, Adapter nach #31, LVGL-Frage nach OD-02 unter OD-05; keine Mega-View und keine Treiber-, Widget-, HTML-, CSS-, ArduinoJson- oder Webservertypen | **OD-07 teilweise entschieden:** #19, #25 und #26 sind verbindlich geschnitten; #27/#28 bleiben offen; UI projiziert kanonische Entscheidungen und leitet Safety, Sensorrolle, Berechtigung oder Konfliktfreiheit nicht neu ab |
| **#26 – lokale Touchoberflaeche** | vollstaendig R1-relevant; einzige lokale Bedien- und Anzeigeoberflaeche; OD-07-Teilentscheid bindet den spaeteren Fuenferschnitt, Issue selbst bleibt unveraendert | `CUSTOM_APPLICATION`; sekundaer `ADAPTER_EXISTING_LIBRARY` | lokale UI-Spezifikation, #25-Praesentationsmodelle/Sprachressourcen und fachliche Kommandos; Display-/Touchkandidaten nur fuer #31/OD-02 | A Navigation/Interaktion; B Standby/Programmauswahl/Start; C Programmverwaltung/Editor; D Lauf/Meldungen/Stop/Wiederanlauf; E Einstellungen/Diagnose/Service/Recovery-UI; keine Fach-, Safety-, Auth-, Persistenz- oder Aktorentscheidungen neu herleiten | Screen-/Dialog-/Aktionslogik nativ und mit simulierten Touchereignissen testbar; reales Rendering, Touchrohwerte, Kalibrierungsmessung und Pixel-/Pufferabnahme bei #31 | in A–E umsetzen: grosse Ziele, Rueckweg, Abbruch, Bestaetigung, Aufwecktouch ohne Kommando, keine notwendige Wischgeste; #25 verwenden, kein allgemeines Widget-/Layout-/UI-Framework; Rendering nach #31/OD-02, LVGL-Vergleich unter OD-05, Auth nach OD-09, Resetmechanik bei #57; microSD-/SD-Slot ohne R1-Menue, Adapter, Persistenz oder Spike | **OD-07-Teilentscheid fuer #26 getroffen:** #19/#25/#26 entschieden, #27/#28 offen; Service-PIN hebt Safety nicht auf, Geraet bleibt ohne WLAN und SD voll bedienbar |
| **#27 – Web-API, Weboberflaeche, Anmeldung und Konflikte** | R1 als sekundaere lokale Bedienoberflaeche; sicherer Betrieb ohne WLAN/Browser | `CUSTOM_APPLICATION`; sekundaer `CONFIGURE_FRAMEWORK`, konditional `ADOPT_LIBRARY` | Kommandos, Konfigurationsmodelle, `INetworkStatus`; Arduino-ESP32 `WebServer` als R1-Baseline, ESPAsync nur Rueckfall; ArduinoJson `7.4.3` bevorzugt und `SPIKE_REQUIRED`; WiFiManager `2.0.17` als bevorzugter Onboarding-Spikekandidat | serverunabhaengige begrenzte DTO-/Endpunktlogik, kleiner JSON-Codec, stabile Projektfehler, API, Webviews, Authpolicy, Sessions, CSRF, Konflikte und Redaction; keine Server-, Portal- oder ArduinoJson-Typen im Fach-/View-Kern; fuer Onboarding projektspezifische Start-/Touch-, Kandidatenpruefungs-, Commit-, Secret-, Recovery- und Safetylogik | WLAN-/Browser-/Client-/Cut-/Abbruch-/Jitter-/Last- und Ressourcentest sowie JSON-Build-/Grenz-/Fuzz-/Heapnachweis auf realem ESP32 | in kleine PRs fuer DTO/JSON-Codec, Lese-API/Transport, mutierende Kommandos, Webassets, Auth und Onboarding schneiden; JSON-Profile 1/4/16 KiB und Tiefe 6 zuerst pruefen; Frameworkserver zuerst; WiFiManager-Onboardingspike zuerst, Frameworkadapter nur bei dokumentiertem Ausloeser; keine produktive Doppelimplementierung oder Provider-/Pluginarchitektur; spaetere Server-/Codecwechsel ueber Composition Root/konkrete Integrationsgrenzen; reale Secret-Domaene erst mit diesem Konsumenten und nach OD-09; Phase 3/7 | **OD-04 entschieden:** Frameworkserver-Baseline; **OD-06-Richtung entschieden:** WiFiManager bevorzugt, endgueltige Uebernahme nach Spike; JSON-Richtung beschlossen, endgueltige Codec-Uebernahme nach Spike; **OD-09:** Authvertrag |
| **#28 – Diagnose, Diagramme, Service und Exporte** | R1-Mindestumfang ja, heutiger Umfang breit | `CUSTOM_APPLICATION`; sekundaer `ADOPT_LIBRARY` | strukturierte Kernmodelle, Ports/Mocks; ArduinoJson `7.4.3` als bevorzugter spikepflichtiger Codec fuer externe Antworten | begrenzte Diagnose-DTOs, Serviceablauf, Charts, Redaction und Export; grosse Verlaeufe paginieren oder streamen, keine unbeschraenkten Gesamtdokumente | reale Ressourcen-, Jitter- und Aktortests | in passive Diagnose/Export und aktiven Serviceablauf teilen; kleinen Codec nur an der Ausgabegrenze verwenden, keine Bibliothekstypen im Kern; Phase 3/7, Hardwareaktionen erst nach Gates | **OD-07:** Mindestcharts/-historie und PR-Schnitt; Codec-Uebernahme bleibt Spike-Gate |

## E5: ESP32- und Hardwareintegration

| Issue und bisheriges Ziel | R1-Relevanz | Kategorie | Vorhandene Repositorybausteine / externe Kandidaten | Verbleibender eigener Code | Hardwareabhaengigkeit | Empfehlung, Abhaengigkeiten und neue Reihenfolge | Ownerentscheidung |
|---|---|---|---|---|---|---|---|
| **#29 – ESP32-Bring-up, Partition, Ressourcen, sichere Ausgaenge** | minimaler Baseline-Anteil zwingend vor aktorfreien Spikes; produktiver Anteil spaeter | `BLOCKED_HARDWARE`; sekundaer `CONFIGURE_FRAMEWORK` | fixierte PlatformIO-/Arduino-Toolchain, sichere Profile, UART/esptool, ADR-016 NVS | Baseline: Boardrevision, UART, Flash/Boot/Reset, Versorgung, Toolchain, kein PSRAM, Firmware-/RAM-/Heapwerte, GPIO-/Businventar und physisch getrennte oder inaktive Aktorpfade; spaeter: finale Partitionierung, Pins, Adapter, Verbraucher und Abnahmen | vollstaendig | spaetere Aufteilung empfehlen, Issue im Audit nicht aendern; Baseline nach Audit-/Planungsbereinigung ohne #24-Abschluss, produktiver Anteil hinter Safety-Gates; keine GPIO-Reserve fuer ausgeschlossene Bedienelemente; Phase 3/6 | Boardrevision, Hardwarezugang und Messfreigabe |
| **#30 – DS18B20-Busse und reale Sensoradapter** | zwingend | `BLOCKED_HARDWARE`; sekundaer `ADAPTER_EXISTING_LIBRARY` | Sensorport; Dallas+OneWire und Espressif-Kandidaten; #20/#21 laufen parallel | duennner technischer Adapter fuer Bus-ID, ROM, Mess-/Zeit-/Aufloesungs-, CRC-, Anwesenheits-, Timeout- und Fehlerstatus; keine Rollen- oder Safety-Semantik | vollstaendig | nach minimaler Baseline und parallel zu #20–#24: beide Stacks durch Build-Stufe 1, Sensorsmoke-Stufe 2 und bei Erfolg identische Topologie-/Fehlermatrix Stufe 3; Software und Topologie getrennt entscheiden; Produktbus immer separat, A bevorzugt, B pinabhaengiger Rueckfall, C nur negativer Referenztest; TRS `Tip=VDD`, `Ring=DQ`, `Sleeve=GND` an konkreter Buchse und Schutzbeschaltung real pruefen; Phase 3–6 | **OD-03a:** Softwarestack; **OD-03b:** A oder begruendeter Rueckfall B; Toolchain-Scheitern ist keine generelle Untauglichkeit |
| **#31 – Display- und Touchadapter** | zwingend fuer die einzige lokale Bedien- und Anzeigeoberflaeche | `BLOCKED_HARDWARE`; sekundaer `ADAPTER_EXISTING_LIBRARY` | #25/#26-Modelle koennen parallel reifen; drei Haupt- und zwei bedingte Reservekandidaten erfasst | Display-/Touchadapter, Touchrohwerte, Kalibrierungsmessung und Ressourcenprofil; Kalibrierungspersistenz und Resetpolicy bleiben im Persistenz-/Recoveryvertrag | vollstaendig | nach minimaler Baseline und ohne #24-Abschluss: Stufe 0 Hardwareidentifikation, alle Hauptkandidaten durch Stufe 1, ausreichend erfolgreiche Kandidaten durch Smoke-Test, nur dessen Erfolge durch Vollmatrix, danach Stufe-4-Ownerwahl; Reserven nur bei dokumentiertem Ausloeser; genau eine Bibliothek produktiv anbinden; Phase 3–6 | **OD-02:** Treiber in Stufe 4; **OD-05:** UI-Framework erst nach Adapter und repraesentativem Screen |
| **#32 – Luefter, Summer, MOSFET-Ausgaenge** | zwingend | `BLOCKED_HARDWARE`; sekundaer `CONFIGURE_FRAMEWORK` | binaerer Port/Mock, #23/#24 | GPIO-/eventuell PWM-Adapter und reale Rollenbindung fuer Luefter und den Summer als einziges zusaetzliches lokales Ausgabeelement | vollstaendig | keine externe Luefterbibliothek und keine Status-LED; unbelastet vor Verbraucher; Phase 6 | reale Kanaele, Pegel, Verbraucherwerte |
| **#33 – BTS7960 und begrenzte Peltierpruefung** | zwingend | `BLOCKED_HARDWARE`; sekundaer `CUSTOM_SAFETY_CORE` | bidirektionaler Port, #23/#24, Infineon-Datenblatt | GPIO-/Enable-Adapter, Hardwareprofil und sichere Testintegration | vollstaendig und sicherheitskritisch | erst nach #29/#30/#32 und dem produktiven Aktor-Safety-Gate 2B; keine generische BTS7960-Library; Phase 6 | Pulldowns, Polaritaet, Temperatursicherung, R_IS/L_IS |

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
5. Der minimale Baseline-Anteil von #29 und die aktorfreien Display-/Touch- und
   DS18B20-/1-Wire-Spikes duerfen parallel zu #20–#24 laufen; #24 ist dafuer
   keine Vorbedingung.
6. #30 benoetigt vor der Produktivadapterwahl die Sensorstufen 1 bis 3 mit
   identischem Test von Topologie A und B; Software- und Bustopologiewahl
   bleiben getrennt, der Produktbus separat und Topologie C ausgeschlossen. #31
   benoetigt den gestuften Display-/Touchvergleich von Hardwareidentifikation
   bis Stufe-4-Ownerwahl; Reservekandidaten durchlaufen die Vollmatrix nicht
   automatisch. Die UI-/Sensorfachlogik bleibt davon getrennt, und LVGL wird
   erst nach Displaytreiber und Adaptervertrag bewertet.
7. Produktive Aktoradapter und reale Aktortests bleiben von #23/#24 und den
   jeweiligen Hardware-/Commissioning-Gates abhaengig.
8. #19 ist im OD-07-Teilentscheid verbindlich in vier geordnete Bereiche, #25
   in zwei gemeinsame UI-Basisbereiche und #26 in fuenf lokale UI-Bereiche
   geschnitten; #27 und #28 bleiben fuer ihre separaten Ownerentscheide offen.
   Erst ein ownerfreigegebener
   Planungsschritt aendert oder erstellt Issues.
9. Der Onboardingteil von #27 prueft WiFiManager zuerst begrenzt. Ein eigener
   Frameworkgegenprototyp entsteht nur bei dokumentiertem Ausloeser; ein
   temporaerer WLAN-Ausfall startet kein Portal, und ein fehlgeschlagener
   Credential-Kandidat veraendert den bisher funktionierenden Stand nicht.

Der Audit veraendert die Live-Issues und ADRs nicht. Der verbindlich notwendige
Neuschnitt und alle spaeteren Issues fuer Pending oder reale Secret-Domaenen
erfolgen erst in einem separaten ownerfreigegebenen Planungsschritt.
