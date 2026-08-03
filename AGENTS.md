# AGENTS.md

Diese Datei gilt fuer das gesamte Repository.

## Projektziel

Eine sichere, lokal bedienbare ESP32-Firmware fuer einen Fermentationsschrank
entwickeln. Das Geraet regelt mit einem Peltier sowohl Heizen als auch Kuehlen,
bleibt ohne Netzwerk funktionsfaehig und darf bei Fehlern keine unbeabsichtigte
Aktorfreigabe erzeugen.

## Verbindlicher Entwicklungsstand

- Die Release-1-Spezifikation wurde mit PR #38 nach `main` uebernommen.
- Epics #2 bis #8 und Issues #9 bis #37 bilden die geplante Arbeitsstruktur.
- #9 ist das erste Implementierungs-Issue.
- Pro Implementierungs-Issue wird ein eigener Branch und ein kleiner PR verwendet.

## Zielhardware und Grenzen

- ESP32-32E mit 4 MB Flash
- keine PSRAM-Abhaengigkeit
- Peltier 12 V / etwa 60 W ueber BTS7960
- drei DS18B20: Schrankluft, abnehmbares Produkt und Kuehlkoerper
- Innen- und Aussenluefter
- 320-x-240-Touchdisplay
- einmalige Temperatursicherung und 7,5-A-Ueberstromsicherung
- UART/FT232RL als Update- und Wiederherstellungsweg fuer Release 1

Keine GPIO-Zuordnung, kein aktiver Pegel, kein Display-/Touchcontroller und keine
BTS7960-Diagnose darf vor realer Verifikation als bestaetigt behandelt werden.

## Dokumentationsprioritaet

Die verbindliche und vollstaendige Reihenfolge bei Dokumentationswiderspruechen
steht ausschliesslich in `docs/SPECIFICATION_REVIEW.md` im Abschnitt
`Dokumentationsprioritaet`. Kurzfassungen in Einstiegs- oder Agentendokumenten
duerfen diese Reihenfolge nicht ersetzen.

Akzeptierte ADRs werden im zentralen Register `docs/DECISIONS.md` gefuehrt.
Ausfuehrliche ADR-Einzeldokumente duerfen das Register ergaenzen, aber nicht
ersetzen.

Zentrale Einstiege:

- `docs/SPECIFICATION_PLAN.md`
- `docs/IMPLEMENTATION_PLAN.md`
- `docs/IMPLEMENTATION_ISSUES.md`
- `docs/ACCEPTANCE_TESTS.md`
- `docs/OPEN_POINTS.md`
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`
- `docs/CI_AND_QUALITY_GATES.md`
- `docs/ESP_IDF_UPGRADE_CONTRACT.md`
- `docs/ENGINEERING_PRINCIPLES.md`
- `lib/README.md`
- `references/LINKS.md`

## Verbindliche Software-Engineering-Grundsaetze

SOLID, DRY und KISS sind verbindliche Grundsaetze fuer alle Agenten und fuer
jede Planung, Architekturentscheidung, Bibliotheksbewertung, Implementierung,
Testaenderung und jedes Review in diesem Repository. Die ausfuehrliche
Erlaeuterung steht in `docs/ENGINEERING_PRINCIPLES.md`; die Verbindlichkeit wird
durch diesen Abschnitt festgelegt.

- **Single Responsibility:** Funktionen, Klassen und Module besitzen eine klar
  abgegrenzte Verantwortung.
- **Open/Closed:** Stabile Kernlogik wird bevorzugt ueber vorhandene
  Abstraktionen erweitert, statt fuer jede Erweiterung wiederholt veraendert zu
  werden.
- **Liskov Substitution:** Implementierungen einer Schnittstelle muessen ohne
  Verletzung ihrer dokumentierten Vertraege austauschbar sein.
- **Interface Segregation:** Kleine, zweckgebundene Schnittstellen sind grossen
  universellen Schnittstellen vorzuziehen.
- **Dependency Inversion:** Fachlogik haengt von Abstraktionen ab, nicht direkt
  von Hardware-, Framework-, Bibliotheks- oder Infrastrukturklassen.
- **DRY:** Fachliche Regeln, Validierungen und Vertraege besitzen eine eindeutige
  Quelle. Copy-and-paste-Logik und parallel divergierende Implementierungen sind
  zu vermeiden.
- **KISS:** Die einfachste verstaendliche, testbare und wartbare Loesung, welche
  die Anforderungen sowie Security- und Safetygrenzen erfuellt, ist zu
  bevorzugen.

Die Grundsaetze duerfen nicht mechanisch angewendet werden:

- SOLID rechtfertigt keine vorsorgliche Ueberabstraktion oder ungenutzte
  Erweiterungspunkte.
- DRY verlangt keine gemeinsame Abstraktion fuer nur oberflaechlich aehnlichen
  Code mit unterschiedlichen fachlichen Verantwortungen.
- KISS rechtfertigt keine Vereinfachung, welche Safety, Security, Recovery,
  Testbarkeit oder dokumentierte Vertraege schwaecht.

Eine bewusste Abweichung muss im freigegebenen Plan oder, bei einer ausdruecklich
freigegebenen trivialen Aenderung, in der PR-Beschreibung konkret begruendet
werden. Reviews pruefen diese Grundsaetze ausdruecklich; reine Behauptungen wie
"SOLID eingehalten" ohne Bezug auf den tatsaechlichen Diff gelten nicht als
Nachweis.

## Architekturregeln

Der fachliche Kern wird soweit sinnvoll ohne Arduino-Abhaengigkeit umgesetzt.
Direkte Hardwarezugriffe gehoeren in Adapter.

Der Kern darf insbesondere nicht direkt verwenden:

- `digitalWrite` oder `analogRead`
- 1-Wire-, Display- oder WLANbibliotheken
- konkrete Dateisystemaufrufe
- globale reale Zeit ohne abstrahierte Zeitquelle

Vorgesehene Profile:

```text
native
esp32_bringup
esp32_release
```

`native` ist der ausschliessliche PlatformIO-Hosttestpfad. Die beiden
ESP32-Produktionsprofile werden ausschliesslich mit dem festgelegten
ESP-IDF-6.0.2-Pfad gebaut: `src/main.cpp` ist die native-only Composition
Root, `main/app_main.cpp` die ESP-IDF Composition Root. Ein Arduino-
Produktionspfad besteht nicht.

`esp32_bringup` startet mit gesperrten Aktoren und dem sichtbaren Zustand
`HARDWARE_UNVERIFIED`. Ein Wechsel auf `esp32_release` darf unbekannte Hardware
nicht automatisch freigeben.

## Modul- und Wiederverwendungsregeln

ADR-013 ist verbindlich. Die Firmware trennt:

```text
src/main.cpp                      native-only Composition Root
main/app_main.cpp                 ESP-IDF Composition Root
lib/device_platform/              anwendungsneutrale Geraetedienste (Produktion)
lib/device_platform_esp_idf/      konkrete ESP-IDF-Produktionsadapter
lib/device_platform_test_support/ Mockadapter und Simulation fuer native Tests
lib/fermentation_app/             konkrete Fermentationsanwendung
```

- `src/main.cpp` und `main/app_main.cpp` verbinden jeweils nur ihre passende
  Plattform mit der Anwendung und enthalten keine Prozess-, Regel-, Persistenz-
  oder Aktorlogik.
- `fermentation_app` darf nur schmale Plattform-Schnittstellen verwenden und
  kennt weder Arduino noch die konkrete Klasse `DevicePlatform`.
- `device_platform` darf keine Fermentationsbegriffe, Fermentationszustaende oder
  Abhaengigkeit auf `fermentation_app` enthalten.
- `device_platform_esp_idf` haengt nur in Richtung `device_platform` und darf
  weder von `fermentation_app` noch von `device_platform_test_support`
  abhaengen.
- `device_platform_test_support` darf von `device_platform` abhaengen, nicht
  umgekehrt; weder `fermentation_app` noch `main.cpp` noch ein
  ESP32-Produktionsbuild duerfen davon abhaengen.
- Die projektspezifische `app_config.hpp` bleibt ausserhalb der
  wiederverwendbaren Plattform.
- Allgemeine Module muessen im Profil `native` testbar sein.
- Keine vorschnelle Auslagerung oder Universalplattform: Ein separates
  Plattform-Repository entsteht erst bei einem zweiten realen Anwendungsfall
  oder einem klaren unabhaengigen Wartungsvorteil.

Fuer Dateien innerhalb der drei Modulverzeichnisse gelten zusaetzlich die dort
liegenden `AGENTS.md`.

## Sicherheitsregeln

- Peltier und H-Bruecke bleiben bei Boot, Reset, Fehler und unklarer Lage AUS.
- Heizen und Kuehlen koennen nie gleichzeitig freigegeben werden.
- Richtungswechsel erzwingen Abschaltung, Mindest-Auszeit und Totzeit.
- Sicherheitsabschaltungen ueberstimmen Mindest-Einschaltzeiten.
- Schrankluft- und Kuehlkoerpersensor sind fuer jede Peltierfreigabe erforderlich.
- Direkte GPIO- oder Aktorzustaende werden nie als Wiederanlaufzustand gespeichert.
- Ein Neustart ist kein Fehlerreset.
- `Quittieren` und `Fehler zuruecksetzen` bleiben getrennt.
- Service-PIN oder Webzugang umgehen keine firmwarefesten Grenzen.
- Web, WLAN, Display und Exporte duerfen Regel- und Sicherheitsaufgaben nicht
  blockieren.

## Lauf- und Konfigurationsregeln

- Ein Lauf verwendet einen unveraenderlichen Programmschnappschuss.
- Das Quellprogramm wird durch einen laufenden Prozess nicht still veraendert.
- Zieltemperatur und verbleibende Dauer duerfen nur ueber die ausdruecklich
  spezifizierte Laufaktion mit Vorschau, Bestaetigung, Validierung und
  protokollierter Laufrevision geaendert werden.
- Andere normale Einstellungen veraendern einen aktiven Lauf nicht.
- Konfigurationen und Kontrollpunkte werden atomar, versioniert und mit
  Rueckfallrevision gespeichert.
- Es wird nicht in jedem Sensorzyklus in Flash geschrieben.
- Historien und Puffer sind fest begrenzt.

## Werte und Parametrierung

- Entwicklerseitige Validierungsgrenzen werden pro fachlichem Bereich in einer
  zentralen `*_limits.hpp` definiert und nicht als Magic Numbers verteilt.
- Benutzer- und programmspezifische Werte bleiben versionierte Laufzeitdaten und
  werden nicht in Limits-Headern gespeichert.
- Hardwarebelegung, Inbetriebnahmewerte und Regelparameter werden ueber
  validierte, versionierte Profile bereitgestellt.
- Firmwarefeste Sicherheitsgrenzen duerfen durch Benutzer-, Programm- oder
  Inbetriebnahmekonfiguration nur verschaerft, niemals gelockert werden.

## Release-1-Abgrenzung

Nicht als Release-1-Funktion implementieren:

- Web-OTA oder duale OTA-Slots
- automatischer Firmwaredownload
- benutzeraktivierbare UART-Diagnose
- Cloud- oder Pushpflicht
- eigener WireGuard-Client
- Tuerkontakt
- verpflichtende RTC
- verpflichtende 12-V-ADC-Messung
- Kaskadenregelung oder PID-Autotuning
- automatische Wartungserinnerungen

Schnittstellen fuer spaetere Erweiterungen duerfen vorbereitet werden, solange
keine ungenutzten grossen Bibliotheken, Speicherpuffer oder aktorfaehigen
Zukunftsfunktionen eingebaut werden.

## Umgang mit offenen Werten

- `TBD_HARDWARE`: reale Komponente, Pin, Pegel oder Verdrahtung fehlt
- `TBD_COMMISSIONING`: thermischer, regelungstechnischer oder prozessbezogener
  Wert wird am realen Schrank bestimmt
- `TBD_IMPLEMENTATION_BUDGET`: reale Build-, Heap- oder Flashmessung fehlt
- `FUTURE_RELEASE`: bewusst nicht Bestandteil von Release 1

Kein solcher Platzhalter darf in der fertigen Firmware unbemerkt als gueltiger
Laufzeitwert verwendet werden.

## Tests und Definition of Done

Jedes Issue erfuellt alle zutreffenden Punkte:

- Implementierung vollstaendig
- native, simulierte oder Hardwaretests vorhanden und bestanden
- ESP32-Zielbuild erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar als budgetabhaengig markiert
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse eingecheckt
- keine unbestaetigte Hardwareannahme als Tatsache implementiert
- Akzeptanzkriterien des Issues erfuellt
- SOLID, DRY und KISS gegen den tatsaechlichen Diff geprueft; Abweichungen sind
  konkret begruendet

Hardwareunabhaengige Logik darf vor Hardwareankunft abgeschlossen werden. Die
reale Verifikation bleibt in einem verknuepften `BLOCKED_HARDWARE`-Issue sichtbar.

## Plan-first-Workflow fuer Implementierungsarbeit

Fuer jede nicht triviale Implementierungs-, Architektur-, Persistenz-,
Security-, Safety-, Hardware-, Bibliotheks- oder moduluebergreifende Aufgabe
gilt ein zweistufiger Draft-PR-Workflow.

### 1. Planungsphase

Der Agent erstellt zunaechst einen Branch und einen Draft-PR.

In der Planungsphase darf der Agent ausschliesslich:

- den Live-Stand des Repositorys, des Issues und der Abhaengigkeiten
  analysieren;
- geltende Anweisungen, Spezifikationen, ADRs und Reviewkommentare lesen;
- eine versionierte Plan-Datei erstellen oder aktualisieren;
- die Draft-PR-Beschreibung aktualisieren;
- Fragen, Widersprueche, Risiken und offene Gates dokumentieren.

Die Plan-Datei liegt unter:

```text
docs/tasks/issue-<issue-number>-implementation-plan.md
```

Bei Aufgaben ohne einzelnes Issue wird ein eindeutiger beschreibender Dateiname
verwendet.

In dieser Phase sind ohne ausdrueckliche Ownerfreigabe nicht erlaubt:

- Produktionscode;
- produktive Tests;
- neue Abhaengigkeiten;
- Build-, Toolchain- oder Konfigurationsaenderungen;
- Issueaenderungen;
- ADRs;
- Hardware-, GPIO- oder Pinentscheidungen;
- Bibliotheksauswahl;
- produktive Spikes.

Der Plan muss mindestens enthalten:

- Ziel;
- Nicht-Ziele;
- verbindliche Quellen und Entscheidungen;
- aktuelle Ausgangslage;
- betroffene Module und voraussichtlich betroffene Dateien;
- Abhaengigkeiten und Gates;
- geplanter kleiner PR-/Commit-Schnitt;
- Daten-, Zustands- und Schnittstellenvertraege;
- Fehler-, Recovery-, Security- und Safetygrenzen;
- Teststrategie;
- Dokumentationsaenderungen;
- offene Entscheidungen;
- Bewertung der geplanten Loesung gegen SOLID, DRY und KISS einschliesslich
  begruendeter Abweichungen;
- als `SPIKE_REQUIRED`, `MEASUREMENT_REQUIRED`, `TBD_HARDWARE`,
  `TBD_COMMISSIONING`, `EVALUATE_BEFORE_RELEASE` oder
  `FINAL_SELECTION_PENDING` verbleibende Punkte;
- ausdruecklich verbotene Vorwegnahmen;
- Abnahmekriterien.

Die PR-Beschreibung nennt:

- Plan-Datei;
- Plan-Commit-SHA;
- Planstatus;
- offene Ownerentscheidungen;
- ausdruecklich:

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Der Agent haelt nach Commit und Push des Plans an.

### 2. Planfreigabe

Die Implementierung darf erst nach einem eindeutigen Ownerkommentar beginnen:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Die Freigabe gilt ausschliesslich fuer die Planversion des genannten Commits.

Eine allgemeine Zustimmung ohne Plan-Commit-SHA gilt nicht als
Implementierungsfreigabe.

### 3. Umsetzung im selben Draft-PR

Nach der Planfreigabe erfolgt die Umsetzung grundsaetzlich im selben Draft-PR.

Der Agent muss:

- innerhalb des freigegebenen Scopes bleiben;
- Planpunkte und tatsaechliche Aenderungen nachvollziehbar korrelieren;
- nur die freigegebenen Module und Verantwortungen aendern;
- keine neue Produkt-, Architektur-, Persistenz-, Security-, Safety-, Hardware-
  oder Bibliotheksentscheidung selbststaendig treffen;
- offene Evaluations-, Mess- und Hardwareentscheidungen offen lassen;
- keine zusaetzlichen Issues oder ADRs ohne ausdrueckliche Freigabe erstellen;
- PR und Branch nicht selbst mergen oder loeschen.

Kleine technische Detailentscheidungen sind erlaubt, wenn:

- sie keine sichtbare Produktwirkung besitzen;
- sie keine oeffentliche oder persistente Schnittstelle veraendern;
- sie keine neue Abhaengigkeit erzeugen;
- sie keine Security-, Safety-, Recovery- oder Hardwaregrenze veraendern;
- sie eindeutig innerhalb des freigegebenen Plans liegen.

### 4. Materielle Planabweichungen

Eine materielle Planabweichung liegt insbesondere vor bei:

- veraendertem Scope;
- neuen Dateien oder Modulen ausserhalb des Plans;
- neuer Abhaengigkeit;
- geaendertem Daten-, Wire-, Persistenz- oder API-Vertrag;
- neuer Architekturentscheidung;
- neuer Security-, Safety-, Recovery- oder Hardwareannahme;
- vorgezogener Evaluation oder Produktivauswahl;
- geaenderter Issue- oder PR-Struktur;
- einem neuen Risiko, das die Abnahmekriterien beeinflusst.

Bei einer materiellen Abweichung muss der Agent:

1. die Umsetzung anhalten;
2. die Plan-Datei aktualisieren;
3. einen neuen Plan-Commit pushen;
4. die bisherige Freigabe als ueberholt kennzeichnen;
5. erneut auf Ownerfreigabe warten.

Die bestehende Planfreigabe gilt nach einer materiellen Planaenderung nicht
weiter.

Der Agent darf den Plan nicht nachtraeglich still an eine bereits vorgenommene
Implementierung anpassen.

### 5. Abschlussphase

Vor Ready for Review dokumentiert der Agent:

- freigegebenen Plan-Commit;
- umgesetzte Planpunkte;
- zugehoerige Commits;
- tatsaechliche Abweichungen;
- ausgefuehrte Tests und Nachweise;
- verbleibende Hardware-, Mess-, Security- oder Release-Gates;
- offene Reviewthreads;
- geaenderte Dateien;
- Bewertung des tatsaechlichen Diffs gegen SOLID, DRY und KISS;
- Bestaetigung, dass keine nicht freigegebene Entscheidung getroffen wurde.

Der Agent fuehrt mindestens aus:

- projektspezifische Tests;
- Format- und Konsistenzpruefungen;
- `git diff --check`;
- Secretpruefung;
- Pruefung gegen `AGENTS.md`;
- Pruefung des tatsaechlichen Diffs gegen den freigegebenen Plan;
- konkrete Pruefung des tatsaechlichen Diffs gegen SOLID, DRY und KISS.

Der PR bleibt Draft, bis ein unabhaengiges Abschlussreview erfolgt ist.

Der Agent:

- setzt den PR nicht selbst auf Ready for Review;
- mergt nicht;
- aktiviert kein Auto-Merge;
- verwendet keinen Force-Push;
- loescht den Branch nicht.

### 6. Ausnahme fuer triviale Aenderungen

Ein separater Plan-Commit ist nur dann nicht erforderlich, wenn alle folgenden
Bedingungen erfuellt sind:

- genau eine klar begrenzte mechanische Aenderung;
- keine Verhaltensaenderung;
- keine neue oder veraenderte Schnittstelle;
- keine Persistenz-, Security-, Safety-, Recovery- oder Hardwarewirkung;
- keine neue Abhaengigkeit;
- keine moduluebergreifende Aenderung;
- kein ungeklaerter Interpretationsspielraum;
- der Owner hat die direkte Umsetzung ausdruecklich freigegeben.

Typische Beispiele:

- Tippfehler;
- reine Formatierung;
- eindeutig falscher Link;
- mechanische Umbenennung ohne Vertragsaenderung;
- kleine Dokumentkorrektur ohne neue Entscheidung.

Sobald Zweifel bestehen, gilt der Plan-first-Workflow.

### 7. Plan-Datei beim Abschluss

Die Plan-Datei bleibt mindestens bis zum Abschlussreview im Branch.

Vor dem Merge entscheidet der Owner:

- Plan als Projektnachweis behalten;
- Plan in eine dauerhafte Entscheidungs- oder Implementierungsdokumentation
  ueberfuehren;
- oder Plan aus dem finalen Dateibaum entfernen.

Auch bei einer spaeteren Entfernung bleiben der freigegebene Plan-Commit-SHA
und die Planhistorie im PR nachvollziehbar.
