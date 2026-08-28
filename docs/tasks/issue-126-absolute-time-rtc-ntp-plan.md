# [E3.8] Plan: R1-Absolute-Zeitplattform mit DS3231 und ESP-IDF-SNTP

## Status und Owner-Gate

```text
PLAN_STATUS=PLAN_ONLY
IMPLEMENTATION=NOT_STARTED
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
OWNER_PLAN_REVIEW_SCOPE=EXACT_PLAN_COMMIT
```

Dieser Plan ist die vollstaendige, eigenstaendige Planfassung fuer Issue #126.
Er wird nach der bestaetigten Owner-Freigabe des exakten Plan-Commits
umgesetzt. In dieser Planrunde werden keine Produktionsdateien, Testdateien,
Abhaengigkeiten, ADRs oder Hardwareprofile mit produktiven Werten geaendert.

## 1. Live-Baseline und Ausfuehrungsgate

Der Auftrag wurde erst nach der Live-Pruefung von PR #125 ausgefuehrt:

```text
PR125_STATE=MERGED
PR125_MERGE_COMMIT=5b8b86b99347bb0bb104dd1c2968040656119440
R1_DEVELOPMENT_BASE_BRANCH=integration/r1-development
R1_DEVELOPMENT_BASE_SHA=5b8b86b99347bb0bb104dd1c2968040656119440
CURRENT_PLAN_BRANCH=agent/issue-126-absolute-time-rtc-ntp-plan
ISSUE=126
```

Die Merge-Ancestor-Pruefung des PR-125-Merge-Commits gegen
`origin/integration/r1-development` war `PASS`. Der alte #124-Featurebranch
wurde nicht fortgesetzt; die neue Arbeit basiert auf dem aktuellen
Integrationsbranch. Vor Erstellung dieses Draft-PR bestanden keine offenen
PRs.

Issue #124 bleibt fachlich unveraendert und ist nach PR #125 geschlossen. Der
neue Zeitbaustein liefert nur die trusted UTC, die #124 bereits als Eingang
verwendet:

```text
trusted current UTC available   -> #124 exact recovery evaluation
trusted current UTC unavailable -> #124 WaitingForTrustedTime
```

## 2. Ownerentscheidungen und Ziel

Die folgenden Entscheidungen sind fuer diesen Plan vorgegeben:

```text
ABSOLUTE_TIME_ARCHITECTURE=RTC_DS3231_PLUS_NTP
RTC=DS3231
NTP=ESP_IDF_SNTP
APPLICATION_TIME_PORT=device_platform::ITimeSource
PROVISIONAL_OPERATOR_RESUME_BEFORE_TRUSTED_TIME=NO
RTC_STORAGE_TIMEZONE=UTC
NTP_REAL_NETWORK_OPERATION_DEPENDS_ON_CONNECTIVITY=YES
```

Der Datenfluss ist:

```text
DS3231 RTC --------------------\
                                -> ESP-Systemzeit / UTC
ESP-IDF SNTP ------------------/             |
                                             v
                              device_platform::ITimeSource
                                             |
                                             v
                                      fermentation_app
```

`fermentation_app` kennt weder DS3231 noch SNTP. `device_platform` bleibt
app-neutral und enthaelt keine ESP-IDF- oder Fermentationsbegriffe.

Zielsemantik des bestehenden Ports:

```text
monotonicMillis()
  -> weiterhin monotone, bootlokale Laufzeit
  -> von RTC-/NTP-Korrekturen unberuehrt

unixTimeSeconds()
  -> trusted UTC aus der ESP-Systemzeit
  -> std::nullopt, solange keine Quelle trusted ist
```

## 3. Nicht-Ziele und harte Grenzen

- Kein zweiter Zeit-Port, kein Zeitqualitaets-Enum und keine parallele
  `fermentation`-Zeit-API.
- Keine WLAN-Provisionierung, Credentials, SoftAP-/Captive-Portal-,
  Connectivity- oder Connection-Manager-Logik. Das bleibt Issue #89.
- Kein eigener NTP-Client.
- Kein eigener DS3231-Registertreiber, solange der Pflichtkandidat den Vertrag
  erfuellt.
- Keine Aenderung an `priorBootPhaseElapsed`, `observedRunSeconds`,
  `wall_clock_since_checkpoint_seconds`, `RecoveryDisposition` oder der
  bestehenden FSM-Recoverytopologie.
- Kein provisionaler Operator-Resume und keine Aktorfreigabe ohne trusted
  absolute Zeit, wenn #124 sie fuer Current-`FERMENTING` benoetigt.
- Keine geratenen GPIOs, I2C-Ports, Pegel, Batterieannahmen oder
  Hardwarefreigaben.
- Keine SQW-, INT- oder 32-kHz-Nutzung in R1, solange der Audit keinen echten
  Bedarf nachweist.
- Kein Hardwarelauf in diesem PR. `TBD_HARDWARE`, `TBD_COMMISSIONING` und
  `TBD_IMPLEMENTATION_BUDGET` werden niemals produktive Laufzeitwerte.

## 4. Verifizierter Ist-Audit

### 4.1 Dokumente und Entscheidungen

| Quelle | Verifizierter Iststand | Konsequenz fuer Issue #126 |
|---|---|---|
| `docs/AGENT_WORKFLOW.md` | Plan-first; versionierter vollstaendiger Plan, exakte Plan-SHA, Draft-PR und Handover | Nur Dokumentations- und Reihenfolgesynchronisierung in dieser Runde |
| `docs/ENGINEERING_PRINCIPLES.md` | Repository-first, Espressif-first, Adopt-or-build vor Eigenbau, keine Parallelvertraege | Erst ESP-IDF-Bestand und Registry-Kandidat bewerten; Adapter hinter bestehendem Port |
| `docs/DECISIONS.md`, ADR-013 | `device_platform` fuer app-neutrale Ports, `device_platform_esp_idf` fuer konkrete Adapter, `fermentation_app` nur gegen Ports | DS3231/SNTP in `device_platform_esp_idf`; `ITimeSource` bleibt einziger App-Port |
| `docs/ARCHITECTURE.md` | `ITimeSource` und optionale UTC sind vorhanden; NTP ist als absolute Zeitquelle vorgesehen; RTC war bisher Erweiterungspunkt | Der Isttext benoetigt nach Ownerfreigabe eine konsistente RTC-Ergaenzung; in dieser Planrunde nicht umschreiben |
| `docs/RECOVERY_AND_INTERRUPTION.md` | #124 verlangt trusted aktuelle UTC fuer den exakten Current-`FERMENTING`-Pfad; fehlende UTC bleibt RAM-only `WaitingForTrustedTime` und all-off | Zeitplattform liefert nur den bestehenden Eingang; keine Recoverylogik duplizieren |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Fail-closed, keine Aktorfreigabe aus unbekannter Lage, frische Safety-Evidenz bleibt erforderlich | RTC/NTP-Trust darf keine Safety-Grenze umgehen |
| `docs/HARDWARE.md` | ESP32-32E, 4 MB, keine PSRAM-Abhaengigkeit; Pins/Boardrevision bleiben offen; RTC ist dort noch als nicht verpflichtende Zukunftshardware dokumentiert | Ownerentscheidung ist gegen den alten optionalen Status zu synchronisieren, aber nicht still in dieser Planrunde |
| `docs/HARDWARE_REVISIONS.md` | NTP primaer, RTC optional, spaetere Zeitkorrektur historisch beschrieben | Historischer Konflikt wird sichtbar gehalten; Umsetzung benoetigt eine neue konsistente normative Fassung |
| `docs/OPEN_POINTS.md` | Boardrevision, GPIOs, Bootpegel, Flash/RAM und reale Hardwaremessungen sind offen | RTC-Software darf digital geplant werden; RTC-Hardware bleibt blockiert |
| `docs/ESP_IDF_UPGRADE_CONTRACT.md` | ESP-IDF `v6.0.2` am Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`; Komponenten werden erst bei echtem Bedarf mit Lockfile gebunden | `idf_component.yml` und generiertes `dependencies.lock` erst nach Planfreigabe |
| `docs/CI_AND_QUALITY_GATES.md` | Planung fuehrt keine Builds oder vollstaendigen Testlaeufe aus; native und beide ESP-IDF-Profile sind spaetere Gates | Alle Planungsnachweise bleiben `NOT_RUN` |

Die veralteten optionalen RTC-Aussagen in den Hardware- und Architekturquellen
sind kein stiller Gegenentscheid gegen die explizite Ownerentscheidung dieses
Issues. Sie werden erst in der Umsetzung oder einem dafuer freigegebenen
Dokumentationsschnitt aufloesbar geaendert. Bis dahin sind sie als
`SOURCE_OF_TRUTH_CONFLICT=OWNER_DECISION_OVERRIDES_OLD_OPTIONAL_RTC_TEXT`
dokumentiert.

### 4.2 Bestehende Zeit- und Recovery-Implementierung

Der relevante Produktionsstand ist:

- `lib/device_platform/src/time_source.hpp` definiert den einzigen
  app-neutralen `ITimeSource`-Port mit `uint64_t monotonicMillis()` und
  `std::optional<int64_t> unixTimeSeconds()`.
- `VirtualTimeSource` trennt monotone Testzeit und optionale simulierte UTC;
  `setUnixTimeSeconds()` beeinflusst die monotone Zeit nicht.
- `lib/device_platform_esp_idf/src/esp_timer_time_source.*` verwendet
  `esp_timer_get_time()` mit einem Instanz-Baselinewert und liefert fuer UTC
  derzeit immer `std::nullopt`.
- `main/app_main.cpp` erstellt `EspTimerTimeSource`, injiziert sie vor dem
  Application-Boot und verwendet danach denselben Port fuer die Laufzeit.
- `FermentationApplication::currentCheckpointTime()` liest ausschliesslich
  `ITimeSource`. `update()` ruft bereits die bestehende
  `reevaluateWaitingForTrustedTime()` auf.
- `RunPersistenceCoordinator` behandelt fehlende aktuelle UTC nach validiertem
  Checkpoint-UTC als `WaitingForTrustedTime`; negative oder unklare UTC-Deltas
  werden nicht geraten, sondern abgelehnt.
- `ProcessStateMachine` kennt weder Persistenz, RTC, NTP noch Systemzeit und
  bleibt unveraendert.
- Das Repository besitzt derzeit keine ESP-IDF-Komponentendependency, kein
  `idf_component.yml`, kein `dependencies.lock`, keinen produktiven I2C-
  Adapter und keinen SNTP-Lifecycle.

Damit ist keine zweite Zeit-API erforderlich. Die kleinste Erweiterung besteht
aus einer konkreten Systemzeit-/Trust-Implementierung hinter demselben Port,
einem DS3231-Adapter und einem kleinen ESP-IDF-SNTP-/Arbitrationskoordinator.

### 4.3 Board- und I2C-Audit

Es gibt aktuell kein bestaetigtes Boardprofil mit RTC-Konfiguration:

- `config/hardware.example.yaml` beschreibt RTC bisher als
  `required: false`, `status: FUTURE_RELEASE`.
- `config/pins.example.yaml` enthaelt nur Rollen und `TBD_HARDWARE`; es gibt
  weder RTC-SDA/SCL noch einen bestaetigten I2C-Bus.
- `docs/HARDWARE.md` bestaetigt weder konkrete GPIOs noch eine Boardrevision.

Der Plan darf daher keine Zahlenwerte erfinden. Die Umsetzung muss ein
semantisches Profil mit dieser Form unterstuetzen, sobald das kanonische
Boardprofil und seine Pin-/Busnachweise vorliegen:

```yaml
rtc:
  present: <verified boolean>
  type: DS3231
  bus: <verified I2C bus>
  sda: <verified board-profile pin>
  scl: <verified board-profile pin>
  address: 0x68
```

`0x68` ist die DS3231-Adressentscheidung; Bus, SDA und SCL bleiben bis zum
Owner-Hardwareprofil offen. Der I2C-Bus wird als geteilter Bus mit einem
einzigen Lebensdauer-/Initialisierungsowner geplant, nicht als DS3231-
Sonderbus. Der DS3231-Adapter darf keine bereits belegten Busressourcen
ueberschreiben.

## 5. Zielvertrag der Zeitplattform

### 5.1 Trust-Gate fuer RTC

Der DS3231-Wert darf nur dann als trusted absolute Zeit in die Systemzeit und
`ITimeSource` gelangen, wenn alle folgenden Nachweise in derselben validierten
Leseoperation vorhanden sind:

```text
I2C_DEVICE_REACHABLE=YES
REGISTER_READ_VALID=YES
CALENDAR_VALUE_VALID=YES
OSF_OSCILLATOR_STOP_FLAG=0
```

`CALENDAR_VALUE_VALID` umfasst mindestens BCD-/Statusbitpruefung, gueltige
Sekunde/Minute/Stunde, Monat/Tag, Schaltjahr-/Monatslaengenpruefung und den
vom DS3231 unterstuetzten Kalenderbereich. Die UTC-Konvertierung muss
timezonefrei und checked erfolgen. Der Adapter darf die von der Bibliothek
gelieferte `struct tm` nicht ungeprueft als Kalenderbeweis uebernehmen.

Weitere notwendige Plausibilitaetspruefungen werden im Umsetzungsschnitt
gegen Datenblatt und konkrete Bibliothekssemantik festgelegt. Es werden keine
willkuerlichen Produktgrenzen wie eine lokale Sommerzeit oder eine
Benutzerzeitzone in den RTC-Trust eingebaut.

`OSF=1` bedeutet zwingend `RTC_TIME_TRUSTED=NO`. Das Flag wird nicht nur
geloescht, um einen vorhandenen Wert nachtraeglich vertrauenswuerdig aussehen
zu lassen.

### 5.2 Systemzeit und bestehender Port

Die konkrete ESP32-Zeitquelle behaelt die bisherige monotone Baseline. Sie
fuehrt intern nur einen kleinen Trust-Zustand fuer die absolute Systemzeit:

```text
untrusted boot / keine gueltige Quelle -> unixTimeSeconds() == nullopt
trusted RTC seed                    -> gettimeofday() als UTC publizierbar
trusted NTP sync                    -> System-UTC publizierbar
retrograder/inkonsistenter Zustand  -> Trust verlieren, nullopt, fail closed
```

Der Trust-Zustand ist kein neuer App-Port und wird nicht durch die Anwendung
gesetzt. Nur der konkrete Plattformkoordinator darf ihn nach abgeschlossenem
RTC-/NTP-Nachweis aktualisieren. Zeitzone/DST bleiben im vorhandenen
`ITimeZoneResolver` und in der Anzeigeebene; die RTC speichert und liest UTC.

### 5.3 Bootsequenz

Nach Ownerfreigabe wird folgende Reihenfolge implementiert:

1. ESP-IDF-/I2C-Bus und DS3231-Adapter gemaess validiertem Boardprofil
   vorbereiten.
2. DS3231 erreichen, Register lesen, Kalender validieren und OSF pruefen.
3. Bei vollstaendig trusted RTC: UTC in die ESP-Systemzeit setzen und den
   bestehenden `ITimeSource`-Trust setzen.
4. Bei fehlender, nicht erreichbarer oder untrusted RTC: keine absolute Zeit
   publizieren; `unixTimeSeconds()` bleibt `nullopt`.
5. Die Anwendung wird mit demselben `ITimeSource` gestartet. #124 bleibt bei
   fehlender trusted UTC in `WaitingForTrustedTime`.
6. Ein spaeteres NTP-Sync-Ereignis setzt beziehungsweise korrigiert die
   Systemzeit nach der freigegebenen Arbitration und markiert UTC erst danach
   trusted.
7. Das bestehende `FermentationApplication::update()` reevaluert #124
   automatisch; der Zeitadapter startet keinen zweiten Recoverypfad.

Ein untrusted Boot erzeugt weder einen provisionalen Resume noch einen
Operator-Resume vor trusted Zeit. Ohne RTC und ohne NTP bleibt #124
fail-closed in `WaitingForTrustedTime`.

### 5.4 NTP-Vertrag und #89-Grenze

Es wird die offizielle ESP-IDF-SNTP-Huelle verwendet, bevorzugt
`esp_netif_sntp_*`. Der Zeitbaustein besitzt:

- keinen WLAN-Treiber, kein Credentialmanagement und keine Provisionierung;
- keine eigene Server-/Retry-/Connectivity-Policy ausserhalb der vom
  `esp_netif_sntp_*`-Vertrag benoetigten Konfiguration;
- einen klaren Start-/Sync-Hook, den der spaetere Connectivity-Besitzer
  aufrufen kann;
- einen Sync-Callback oder ein Sync-Ereignis, das die Systemzeit-/Trust- und
  RTC-Nachfuehrung ueber den konkreten Plattformkoordinator ausloest.

NTP darf:

```text
- die ESP-Systemzeit setzen oder korrigieren;
- untrusted RTC nach erfolgreicher Synchronisierung initialisieren;
- eine trusted RTC periodisch nachfuehren.
```

NTP darf nicht direkt `fermentation_app` aufrufen, Recoveryzustande
entscheiden oder Aktoren freigeben. Ein echter Netzwerk-/NTP-Lauf bleibt
abhängig von #89 und wird in diesem PR nicht ausgefuehrt.

## 6. Vollstaendige RTC/NTP-Arbitration

Die folgenden Punkte sind der verpflichtende Entscheidungs- und Testumfang.
Die Empfehlungen sind bewusst explizit; nicht vorweggenommene
Ownerentscheidungen bleiben offen und werden nicht durch Implementierungsdetail
versteckt.

### 6.1 Gueltige RTC und spaeteres NTP

```text
CURRENT_STATE=
  Kein RTC-/NTP-Producer im aktuellen Repository. #124 besitzt bereits den
  einzigen fachlichen Re-Evaluationspfad ueber ITimeSource.

MINIMAL_KISS_OPTION=
  Nach einem erfolgreichen NTP-Sync die NTP-UTC als Netzwerkreferenz fuer die
  Systemzeit behandeln und die RTC in einem serialisierten Plattformpfad
  nachfuehren. MonotonicMillis bleibt unberuehrt.

RECOMMENDATION=
  NTP wird nach dem explizit freigegebenen Sync-Ereignis Referenz; der bereits
  trusted RTC-Wert wird nicht als zweiter fachlicher Zeitpfad weitergereicht.

WHY=
  Es gibt genau eine publizierte System-UTC und genau einen App-Port. Damit
  werden Quelle und Anwendung entkoppelt und kein Zeit-Consensus-Modell
  eingefuehrt.

OWNER_DECISION_REQUIRED=NO
```

### 6.2 Kleine RTC-NTP-Abweichung

```text
CURRENT_STATE=
  Keine Toleranz, kein Synchronisationsmodus und keine Write-Schwelle sind im
  Projekt festgelegt. ESP-IDF-SNTP unterstuetzt unmittelbare und smooth
  Korrektur; smooth kann grosse Abweichungen nach ESP-IDF-Regel unmittelbar
  setzen.

MINIMAL_KISS_OPTION=
  Keine eigene numerische Konsensus-Toleranz. Den von ESP-IDF angebotenen
  smooth-Sync als bevorzugte Korrektur fuer eine kleine Abweichung pruefen und
  den RTC-Wert nach dem akzeptierten Sync wieder angleichen.

RECOMMENDATION=
  Kleine Abweichungen nicht als Recovery- oder Persistenzereignis behandeln.
  Die konkrete Definition von klein bleibt an die bestaetigte ESP-IDF-
  Synchronisationssemantik gebunden, nicht an eine neue Fachkonstante.

WHY=
  Der monotone Laufzeitpfad bleibt stabil und #124 wird nicht wegen jeder
  normalen NTP-Korrektur neu modelliert. Eine willkuerliche Sekunden- oder
  Minuten-Toleranz waere eine neue Ownerentscheidung.

OWNER_DECISION_REQUIRED=YES
```

### 6.3 Grosse RTC-NTP-Abweichung

```text
CURRENT_STATE=
  Es gibt weder eine Erkennung noch eine Policy fuer grosse Abweichungen.
  ESP-IDF dokumentiert bei smooth sync eine unmittelbare Korrektur oberhalb
  seiner Schwelle.

MINIMAL_KISS_OPTION=
  NTP-Ergebnis nur nach der offiziellen SNTP-Synchronisierung akzeptieren,
  System-UTC nicht aus Checkpointdaten umschreiben und den RTC-Write sowie die
  Folgevalidierung getrennt behandeln.

RECOMMENDATION=
  Eine grosse Vorwaertskorrektur darf die trusted System-UTC aktualisieren,
  loest aber selbst keinen direkten Recovery- oder Aktorpfad aus. Eine grosse
  Rueckwaertskorrektur darf nicht als trusted Ruecksprung publiziert werden;
  sie fuehrt zu Trustverlust beziehungsweise fail-closed, bis eine Quelle
  erneut sauber validiert ist.

WHY=
  #124 kann nur den bestehenden UTC-Term gegen den unveraenderten Checkpoint
  bewerten. Direkte Sonderlogik im SNTP-Callback wuerde eine zweite Recovery-
  Semantik erzeugen. Der Rueckwaertsschutz verhindert negative oder falsch
  fortgesetzte Zeitanker.

OWNER_DECISION_REQUIRED=YES
```

### 6.4 Darf die Systemzeit rueckwaerts springen?

```text
CURRENT_STATE=
  POSIX-/ESP-Systemzeit kann durch settimeofday beziehungsweise SNTP
  korrigiert werden. Der bestehende Recoverycode verwirft negative UTC-Deltas;
  der vorhandene Snapshotpfad erkennt rueckwaerts laufende Unix-Zeit.

MINIMAL_KISS_OPTION=
  MonotonicMillis niemals korrigieren. Absolute Zeit nur publizieren, solange
  der konkrete Trustzustand und die zuletzt beobachtete UTC konsistent sind;
  bei beobachtetem Ruecksprung Trust verlieren und nullopt liefern.

RECOMMENDATION=
  Kein retrograder Wert im trusted ITimeSource-Vertrag. Der Umsetzungsschnitt
  muss vor der Produktintegration pruefen, ob esp_netif_sntp_* mit smooth sync
  und dem vorhandenen Hook diese Grenze vor dem Systemzeit-Setzen einhalten
  kann. Falls nicht, ist der Owner zwischen einem begrenzten ESP-IDF-
  Sofortsprung und einem engeren SNTP-Gate zu entscheiden.

WHY=
  Ein kurzfristiger Ruecksprung darf weder monotone Timer noch den naechsten
  UTC-Checkpoint semantisch veraendern. Nachtraegliches Korrigieren eines
  bereits persistierten Checkpoints ist nicht zulaessig.

OWNER_DECISION_REQUIRED=YES
```

### 6.5 Schutz vor falscher Recovery-/Checkpoint-Semantik bei grosser Korrektur

```text
CURRENT_STATE=
  #124 berechnet nur aus dem validierten Checkpoint-UTC und aktueller trusted
  UTC. Ein negativer Abstand wird abgelehnt; ein grosser positiver Abstand
  kann gemaess dem bereits freigegebenen Vertrag eine normale Abschluss- oder
  Recoveryentscheidung ergeben.

MINIMAL_KISS_OPTION=
  Checkpoint-UTC, Revision, monotone Werte und Persistenzgraph unveraendert
  lassen. Ein SNTP-Ereignis darf nur den Zeitquellenstatus aktualisieren;
  #124 reevaluert ueber application.update() mit derselben Quelle.

RECOMMENDATION=
  Keine neue grosse-Delta-Recoveryengine, keine Checkpointkorrektur und kein
  direkter Callback-Handoff an die FSM. Bei trusted Vorwaertskorrektur gilt
  die bestehende #124-Rechnung; bei unklarer oder rueckwaertiger Lage bleibt
  der Pfad fail-closed.

WHY=
  Die exakte R1-Wall-Clock-Semantik bleibt an einer Stelle. Damit werden
  `priorBootPhaseElapsed`, `observedRunSeconds` und die bestehende FSM-
  Topologie nicht doppelt oder indirekt veraendert.

OWNER_DECISION_REQUIRED=YES
```

### 6.6 Zeitpunkt des RTC-Schreibens nach NTP

```text
CURRENT_STATE=
  Kein RTC-Write und keine NTP-Eventverarbeitung vorhanden. Die Kandidaten-
  komponentenoperationen sind einzeln threadgeschuetzt, ein projektweiter
  Synchronisations- und Lebensdauerowner fehlt.

MINIMAL_KISS_OPTION=
  Nach jedem akzeptierten NTP-Sync die validierte UTC in einem serialisierten
  Plattformpfad in den DS3231 schreiben, danach lesen und validieren. Keine
  zusaetzliche Flashpersistenz und keine eigene Periodik neben SNTP.

RECOMMENDATION=
  RTC nach jedem von ESP-IDF gemeldeten erfolgreichen Sync angleichen, sofern
  die Arbitration den Sync akzeptiert. Die NTP-Periodik bleibt bei
  esp_netif_sntp_*; eine separate RTC-Wear-/Consensus-Engine wird nicht gebaut.

WHY=
  Ein einzelner Write pro Netzwerk-Sync ist nachvollziehbar und haelt den
  Batteriezeitanker aktuell, ohne die Recovery- oder Flashvertraege zu
  erweitern.

OWNER_DECISION_REQUIRED=YES
```

### 6.7 Fehlgeschlagener RTC-Write

```text
CURRENT_STATE=
  Es gibt keine Fehlerabbildung fuer RTC-/I2C-Fehler. #124 unterscheidet
  trusted UTC nur ueber ITimeSource und besitzt keinen RTC-Status.

MINIMAL_KISS_OPTION=
  NTP-Systemzeit bleibt fuer den aktuellen Boot trusted, wenn der NTP-Sync
  selbst erfolgreich und die UTC validiert ist. Ein fehlgeschlagener RTC-Write
  macht nur den RTC-Stand fuer kuenftige Boots untrusted; OSF wird nicht
  geloescht. Der Write wird beim naechsten erfolgreichen NTP-Sync erneut
  versucht.

RECOMMENDATION=
  RTC-Fehler sichtbar diagnostizieren, aber nicht in einen zweiten
  Application-Recoverystatus umdeuten. Ohne spaeteren NTP bleibt der naechste
  Boot wegen fehlender trusted RTC in WaitingForTrustedTime.

WHY=
  Die aktuell verfuegbare NTP-Referenz soll nicht durch einen unabhaengigen
  RTC-Schreibfehler verloren gehen; zugleich darf ein unbestaetigter RTC-Wert
  nicht fuer den naechsten Boot freigegeben werden.

OWNER_DECISION_REQUIRED=NO
```

### 6.8 OSF-Vertrag nach erster NTP-Initialisierung

```text
CURRENT_STATE=
  Die geplante DS3231-Komponente kann OSF lesen und loeschen. Im Repository
  gibt es noch keinen Ablauf, der Setzen, Readback und OSF in einer
  vertrauenswuerdigen Transaktion verbindet.

MINIMAL_KISS_OPTION=
  NTP setzt eine UTC in den RTC, liest die Zeit zurueck und validiert sie.
  Erst nach erfolgreichem Readback wird OSF geloescht. Danach wird OSF erneut
  gelesen; nur OSF=0 schliesst den RTC-Trust ab.

RECOMMENDATION=
  Genau diese Reihenfolge implementieren. Fehler bei Setzen, Readback,
  Validierung oder OSF-Clear lassen OSF beziehungsweise RTC-Trust untrusted;
  kein "Clear first" und kein stiller Erfolg.

WHY=
  OSF=1 ist laut Ownervertrag ein Beweis gegen RTC-Trust. Das bewusste Setzen
  und Validieren einer NTP-Zeit ist die erforderliche Voraussetzung fuer das
  Zuruecksetzen des Flags.

OWNER_DECISION_REQUIRED=NO
```

### 6.9 Vorhandene, aber untrusted RTC

```text
CURRENT_STATE=
  Ein RTC-Modul ist im aktuellen Profil weder konfiguriert noch real
  bestaetigt. `EspTimerTimeSource` publiziert deshalb derzeit gar keine UTC.

MINIMAL_KISS_OPTION=
  Fehlende RTC, nicht erreichbare RTC, OSF=1 oder ungueltiger Kalender liefern
  keinen UTC-Wert. Auf NTP warten; ohne NTP in #124 WaitingForTrustedTime und
  all-off bleiben.

RECOMMENDATION=
  Keine automatische RTC-Reparatur, kein OSF-Loeschen ohne NTP-Setzen und
  keinen provisionalen Operator-Resume. Eine vorhandene Batterie-/RTC-
  Hardware ist nur nach dem vollstaendigen Trust-Gate verwendbar.

WHY=
  Batterie, Versorgung und Oszillatorzustand sind ohne OSF-/Kalendernachweis
  keine vertrauenswuerdige absolute Zeit. Die Semantik bleibt fail-closed.

OWNER_DECISION_REQUIRED=NO
```

### 6.10 Ehrliches `nullopt` im ITimeSource

```text
CURRENT_STATE=
  Der Port hat die gewuenschte optionale Rueckgabe bereits; der ESP-IDF-
  Adapter liefert aktuell jedoch immer nullopt und besitzt keinen Trust-Latch.

MINIMAL_KISS_OPTION=
  Internes boolesches Trust-Latch im konkreten ESP-IDF-Zeitadapter. Das Latch
  wird ausschliesslich nach erfolgreicher RTC- oder NTP-Validierung gesetzt
  und bei Ruecksprung, Fehler oder Verlust der Quelle geloescht.

RECOMMENDATION=
  `unixTimeSeconds()` gibt exakt dann eine checked UTC zurueck, wenn die
  konkrete Systemzeitquelle trusted ist; andernfalls immer `std::nullopt`.
  `monotonicMillis()` bleibt unabhaengig. Die Anwendung erhaelt keine
  Quellinformationen und keine zweite API.

WHY=
  Das haelt #124 und alle kuenftigen Konsumenten ehrlich und erlaubt native
  Tests mit `VirtualTimeSource`, ohne reale Uhr oder Netzwerkabhaengigkeit.

OWNER_DECISION_REQUIRED=NO
```

### 6.11 Offene Ownerentscheidungen aus der Arbitration

Vor Umsetzung dieses Plans muss der Owner insbesondere bestaetigen:

```text
OWNER_DECISIONS_REQUIRED=
  SMALL_RTC_NTP_DELTA_POLICY;
  BACKWARD_TIME_POLICY;
  LARGE_TIME_DELTA_POLICY;
  LARGE_DELTA_RECOVERY_POLICY;
  RTC_SYNC_WRITE_FREQUENCY
```

Die Empfehlungen oben bilden den KISS-Vorschlag. Eine abweichende Entscheidung
ist materiell und erfordert vor Umsetzung eine neue exakte Plan-SHA.

## 7. Bibliotheks- und Lizenz-Audit

### 7.1 Pflichtkandidat DS3231

Der am 2026-08-28 live gepruefte Pflichtkandidat ist:

```text
DS3231_COMPONENT_CANDIDATE=esp-idf-lib/ds3231
DS3231_COMPONENT_VERSION=1.1.7
DS3231_COMPONENT_REGISTRY_COMMIT=cbe14063d3f2bf39489e18d896c725b8111b5cc4
DS3231_COMPONENT_LICENSE=MIT
DS3231_COMPONENT_TARGETS=esp32,...,esp32s3
```

Der Registry-Eintrag beschreibt einen DS1337-/DS3231-Treiber, nennt die
Installation ueber den ESP Component Manager und fuehrt `esp32` sowie `esp32s3`
als Targets. Die gepruefte API besitzt unter anderem:

- `DS3231_ADDR` mit `0x68`;
- `ds3231_init_desc()`/`ds3231_free_desc()`;
- `ds3231_get_time()` und `ds3231_set_time()`;
- `ds3231_get_oscillator_stop_flag()`;
- `ds3231_clear_oscillator_stop_flag()`.

Der Treiber dokumentiert die `struct tm`-Zeit als timezoneagnostisch und
empfiehlt GMT/UTC. Das passt zum RTC-UTC-Vertrag, ersetzt aber nicht die
projektseitige Kalender- und Trustvalidierung.

### 7.2 Direkte Abhaengigkeiten

Die Version 1.1.7 deklariert:

```text
esp-idf-lib/i2cdev             version="*"  -> beim Einbinden exakt locken
esp-idf-lib/esp_idf_lib_helpers version="*" -> beim Einbinden exakt locken
```

Der am Planungsdatum aufgeloeste Registry-Stand ist:

```text
esp-idf-lib/i2cdev=2.1.2
LICENSE=MIT
CURRENT_IMPLEMENTATION=driver/i2c_master.h plus FreeRTOS mutexes

esp-idf-lib/esp_idf_lib_helpers=1.4.0
LICENSE=ISC

ESP-IDF esp_netif / I2C master components
LICENSE=Apache-2.0 (ESP-IDF component source)
```

`i2cdev` v2.1.2 verwendet den modernen `i2c_master`-Treiber und besitzt
Port-/Device-Mutexe. Der Header enthaelt zusaetzlich Legacy-Kompatibilitaet;
die ESP-IDF-6.0-Linie kennzeichnet den alten `driver/i2c.h`-Treiber als EOL.
Das ist ein konkretes Compile-/Warnungsrisiko und kein bereits bestaetigter
Kompatibilitaetsnachweis.

### 7.3 Adopt-or-build-Entscheidung

```text
REUSE_EXISTING_COMPONENT=YES_PROVISIONAL
OWN_DS3231_REGISTER_DRIVER=NO_UNLESS_COMPONENT_FAILS_EXPLICITLY
ESP_IDF_6_0_2_COMPATIBILITY=UNVERIFIED_PLAN_GATE
ESP32_TARGET_COMPATIBILITY=REGISTRY_DECLARED_NOT_BUILT
ESP32S3_TARGET_COMPATIBILITY=REGISTRY_DECLARED_NOT_BUILT
I2C_API_COMPATIBILITY=MODERN_I2C_MASTER_DECLARED_BY_RESOLVED_I2CDEV_NOT_BUILT
OSF_ACCESS_CAPABILITY=API_PRESENT_NOT_RUN
THREAD_SAFETY=DEVICE/PORT_MUTEXES_DECLARED_NOT_INTEGRATED
FLASH_RAM_COST=NOT_MEASURED
LICENSE_NOTICE_REQUIREMENT=ALL_RESOLVED_COMPONENT_LICENSES_AND_NOTICES_RECORD
MAINTENANCE_STATUS=REGISTRY_VERSION_LATEST_AT_AUDIT_DATE_MAINTAINER_DECLARED
```

Die Auswahl wird erst nach dem festen ESP-IDF-6.0.2-Compile-/Static-Test,
Lockfilepruefung, Lizenzabdeckung und einem gezielten Adaptertest endgueltig.
Die Registry-Lizenz- und Abhaengigkeitsangaben sind in diesem Plan referenziert:

- [DS3231 Registry v1.1.7](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/readme?language=en)
- [DS3231 v1.1.7 Manifest und Commit](https://github.com/esp-idf-lib/ds3231/blob/cbe14063d3f2bf39489e18d896c725b8111b5cc4/idf_component.yml)
- [DS3231 API-Dokumentation](https://esp-idf-lib.github.io/ds3231/)
- [DS3231-Abhaengigkeiten](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/dependencies?language=en)
- [i2cdev Registry](https://components.espressif.com/components/esp-idf-lib/i2cdev/versions/2.1.2/readme)
- [ESP-IDF Systemzeit und SNTP](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/system/system_time.html)
- [ESP-NETIF-SNTP-API](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/network/esp_netif_programming.html)
- [ESP-IDF-I2C-API](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/peripherals/i2c.html)

Die ESP-IDF-Dokumentation empfiehlt fuer SNTP die `esp_netif`-Wrapper wegen
Thread-Safety. Sie dokumentiert unmittelbare und smooth Korrektur sowie die
explizite Sync-Warte-/Event-/Callback-Semantik. Der Umsetzungsschnitt muss
gegen die exakt installierte v6.0.2-API kompilieren; eine Dokumentationsseite
allein ist kein Buildnachweis.

## 8. Geplante Modul- und Implementierungsschnitte

Alle folgenden Schnitte beginnen erst nach Owner-Freigabe dieser Plan-SHA.

### Schnitt A – Komponenten und ESP-IDF-Basis

- Root-`idf_component.yml` mit fester `esp-idf-lib/ds3231`-Version anlegen
  oder die nach dem Repository-Standard passende Manifestposition verwenden.
- Mit dem ESP Component Manager aufloesen; das erzeugte
  `dependencies.lock` versionieren und nicht manuell editieren.
- Direkte und transitive Lizenzdateien/Notices fuer DS3231, `i2cdev`,
  `esp_idf_lib_helpers`, `esp_netif` und I2C-Bestand erfassen.
- `device_platform_esp_idf` um nur die konkret benoetigten ESP-IDF-
  Komponentenanforderungen erweitern.
- Beide Produktionsprofile bleiben ESP-IDF `v6.0.2`, 4 MB, ohne PSRAM und
  mit gesperrten Aktoren.

Abbruchsignal: Die Komponente baut nicht sauber gegen v6.0.2, benoetigt eine
ungepruefte Legacy-I2C-API, verletzt den Lockfilevertrag oder kann OSF nicht
nachweisen. Dann Implementation anhalten, Befund dokumentieren und einen neuen
Plan fuer einen begruendeten Alternativkandidaten vorlegen.

### Schnitt B – Schmale konkrete Platform-Adapter

Voraussichtlich in `lib/device_platform_esp_idf`:

- DS3231-Adapter fuer Erreichbarkeit, Register-/Kalenderread, UTC-Konvertierung,
  `set_time`, Readback und OSF-Zugriff;
- geteilter I2C-Bus-/Device-Lebensdauerowner, der nur verifizierte
  Boardprofilwerte akzeptiert;
- Erweiterung oder klare Umbenennung von `EspTimerTimeSource`, ohne den
  `ITimeSource`-Port zu duplizieren;
- ESP-IDF-Systemzeit-/Trust-Koordinator fuer RTC-Seed, NTP-Event,
  Ruecksprungsschutz, RTC-Resync und Fehlerstatus;
- `esp_netif_sntp_*`-Start-/Sync-Hook ohne Connectivity- oder Credentialbesitz.

Der Koordinator bleibt klein und linear. Keine allgemeine Zeit-Consensus-
Engine, kein globaler Service-Locator und keine Fermentationskenntnis.
Callback-Arbeit, RTC-Schreiben und Systemzeit-Trust werden gegen die konkrete
ESP-IDF-/FreeRTOS-Lebensdauer und Thread-Sicherheit serialisiert. Ein NTP-
Callback darf die FSM nicht direkt aufrufen.

### Schnitt C – Boardprofil und Composition Root

- Das kanonische Boardprofil um `rtc.present`, `rtc.type`, `rtc.bus`,
  `rtc.sda`, `rtc.scl` und `rtc.address` erweitern.
- Keine `TBD_HARDWARE`-Zahl in eine produktive Konfiguration uebernehmen.
- `main/app_main.cpp` bleibt der Composition Root fuer Erstellung, Besitz,
  Lebensdauer und Injektion der konkreten Plattformobjekte.
- `src/main.cpp` bleibt native Composition Root; native Tests verwenden
  `VirtualTimeSource` und Fake-/Host-Transporte.
- `fermentation_app` wird nur dann geaendert, wenn der Audit einen konkreten
  Integrationsfehler findet; der bestehende `ITimeSource`-Aufruf und die
  automatische #124-Reevaluation bleiben der Vertrag.

### Schnitt D – Host/Fake-I2C und Arbitration

Die digitale Testschicht muss ohne reale Uhr, Netzwerk oder DS3231 auskommen:

- Fake-I2C-Registertransporte fuer erreichbares/nicht erreichbares Geraet,
  Read-/Write-Fehler und konkurrierende Zugriffe;
- gueltige/ungueltige BCD- und Kalenderwerte;
- OSF=0/1, OSF-Clear erst nach erfolgreichem Setzen und Readback;
- RTC-Bootseed, fehlender RTC, NTP-Initialisierung und periodische Nachfuehrung;
- kleine Vorwaerts-/Rueckwaertskorrektur und grosse Deltas gemaess finaler
  Ownerentscheidung;
- Ruecksprung-/Trustverlust und ehrliches `nullopt`;
- fehlgeschlagener RTC-Write bei weiter trusted NTP-Systemzeit;
- unveraenderte monotone Zeit trotz Systemzeitkorrektur;
- `FermentationApplication`-/#124-Test: `WaitingForTrustedTime` wird nach
  einem spaeter gelieferten trusted Wert automatisch ueber `update()` erneut
  bewertet, ohne neue Persistenzmutation nur durch das Warten.

Die Fake-I2C-Schicht ist ein technischer Testseam und keine neue oeffentliche
Zeit-API. Produktionscode haengt nicht von `device_platform_test_support` ab.

## 9. Test-, Build- und Nachweisplan

### 9.1 Digitale Nachweise

Nach Umsetzung werden gezielt und reproduzierbar ausgefuehrt:

```text
native:
  pio test -e native --filter <betroffene Zeit-/RTC-/Arbitrationstests>

ESP-IDF:
  . "$IDF_PATH/export.sh"
  python scripts/build_esp_idf_profiles.py all
  python scripts/run_esp_idf_static_analysis.py all

Architektur/Qualitaet:
  python scripts/check_architecture_boundaries.py
  python scripts/check_secrets.py
  git diff --check
  clang-format --dry-run --Werror <geaenderte C/C++-Dateien>
```

Der konkrete Testfilter wird im Umsetzungs-PR nach den tatsaechlich geaenderten
Dateien festgelegt. Ein vollstaendiger nativer Lauf erfolgt erst nach einem
befundfreien Review und ausdruecklicher Owner-Anweisung. Nicht ausgefuehrte
Tests bleiben `NOT_RUN`.

### 9.2 Profil- und Architekturtests

Der Nachweis muss fuer `esp32_bringup` und `esp32_release` pruefen:

- ESP-IDF exakt v6.0.2 und kein Arduino-Produktionspfad;
- ESP32- und, soweit die Komponente dies behauptet, ESP32-S3-Kompatibilitaet;
- moderne I2C-Master-API ohne unkontrollierte Legacy-Driver-Annahme;
- korrekte `REQUIRES`-/Lockfile-/Lizenzabdeckung;
- keine `fermentation_app`-Abhaengigkeit aus `device_platform_esp_idf`;
- keine Zeit-/RTC-/NTP-Typen in der Application ausser `ITimeSource`;
- Aktoren bleiben in beiden Profilen gesperrt.

### 9.3 Hardware- und Netzwerkgrenzen

Diese Nachweise sind in diesem Plan ausdruecklich nicht ausfuehrbar:

```text
RTC_SOFTWARE=may PASS digitally after implementation
RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING
RTC_REAL_DETECTION=NOT_RUN
RTC_REAL_I2C_TRANSACTION=NOT_RUN
RTC_REAL_OSF_BEHAVIOR=NOT_RUN
RTC_BATTERY_BACKUP=NOT_RUN
RTC_POWER_CYCLE_RETENTION=NOT_RUN
RTC_BOOT_SEED_ON_BOARD=NOT_RUN
NTP_TO_RTC_REAL_RESYNC=NOT_RUN
LONG_DURATION_DRIFT=NOT_RUN
NTP_REAL_NETWORK_RUN=NOT_RUN
```

Die spaetere Hardwarekampagne benoetigt Boardrevision, exakte Pins/Bus,
Verdrahtung, Versorgung, Pull-ups, Batterie, Power-Cycle- und UART-/Log-
Provenienz. Sie ersetzt keinen digitalen Test und wird erst auf dem finalen
Implementierungs-Head ausgefuehrt.

## 10. Dokumentations- und Roadmapwirkung

### In dieser Planrunde

- dieser vollstaendige Plan unter
  `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md`;
- `docs/ROADMAP.md` minimal auf den gemergten PR #125 und die Einordnung
  `#124 -> #126 -> #25` synchronisieren;
- Issue #126 mit Ziel, Grenzen, Ownerentscheidungen, Hardware-/Netzwerkstatus
  und Planverweis synchronisieren;
- Draft-PR mit Basis, Planpfad, exakter Plan-SHA, `IMPLEMENTATION=NOT_STARTED`,
  `MERGE=NO` und offenen Ownerentscheidungen synchronisieren;
- genau einen aktuellen `SESSION HANDOVER`-Kommentar veroeffentlichen;
- keine normative Recovery-, Hardware-, Architektur- oder Lizenzregister-
  Umschreibung, keine Produktions-/Testimplementation.

### Nach Ownerfreigabe und Umsetzung

- normative Architecture-/Hardware-/Zeitquellen gegen die neue RTC-Owner-
  entscheidung widerspruchsfrei aktualisieren;
- bei erster Komponenteneinbindung Lockfile, Lizenz-/Notice-Nachweis und
  `docs/THIRD_PARTY_COMPONENTS.md` nach Projektvertrag aktualisieren;
- nur bei einer neuen, ueber die vorgegebenen Entscheidungen hinausgehenden
  Architekturentscheidung ADR-Register ergaenzen;
- #124 fachlich unveraendert lassen und nur den vorhandenen
  `ITimeSource`-Eingang verifizieren;
- Roadmap nach dem live verifizierten Umsetzungsstatus aktualisieren.

## 11. Definition of Done fuer die spaetere Umsetzung

- [ ] DS3231-Komponentenkandidat gegen ESP-IDF 6.0.2 gebaut oder begruendet
      verworfen; keine ungepruefte Eigenimplementierung.
- [ ] Direkte Dependencies, feste Lockfile-Versionen, Lizenzen und Notices
      nachgewiesen.
- [ ] `ITimeSource` bleibt einziger Application-Zeitvertrag.
- [ ] monotone Zeit bleibt von RTC-/NTP-Korrekturen unberuehrt.
- [ ] `unixTimeSeconds()` liefert nur bei trusted System-UTC und sonst
      `nullopt`.
- [ ] RTC-Trust prueft Reachability, Register, Kalender und OSF=0.
- [ ] OSF wird erst nach bewusstem validiertem Setzen/Readback/Clear geschlossen.
- [ ] RTC-Seed, NTP-Systemzeit und NTP-zu-RTC-Sync sind getestet.
- [ ] Rueckwaerts- und grosse Delta-Policy entspricht der Ownerentscheidung;
      keine falsche Recovery-/Checkpointsemantik.
- [ ] Shared-I2C-Bus und semantisches Boardprofil verwenden keine geratenen
      Pins oder `TBD_HARDWARE` als Laufzeitwerte.
- [ ] #89-Grenze und #124-Vertrag bleiben erhalten.
- [ ] Native Fake-I2C-/Arbitration-/#124-Reevaluationstests bestehen.
- [ ] Beide ESP-IDF-Profile und Architekturgrenzen bestehen.
- [ ] RTC-Hardware und NTP-Netzwerkstatus sind separat als PASS/BLOCKED/
      NOT_RUN belegt; kein digitaler Build wird als Hardwarefreigabe dargestellt.

## 12. Planabschluss und Owner-Review

```text
R1_DEVELOPMENT_BASE_SHA=5b8b86b99347bb0bb104dd1c2968040656119440
NEW_TIME_ISSUE=126
PLAN_BRANCH=agent/issue-126-absolute-time-rtc-ntp-plan
PLAN_PATH=docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md
PLAN_SHA=<nach Plan-Commit einzutragen>

TIME_ARCHITECTURE=RTC_DS3231_PLUS_NTP
RTC_DEVICE=DS3231
RTC_STORAGE_TIMEZONE=UTC
RTC_TRUST_OSF=OSF_1_MEANS_RTC_TIME_TRUSTED_NO
NTP_IMPLEMENTATION=ESP_IDF_SNTP
ITIME_SOURCE_REUSED=YES

DS3231_COMPONENT_CANDIDATE=esp-idf-lib/ds3231
DS3231_COMPONENT_VERSION=1.1.7
DS3231_COMPONENT_LICENSE=MIT
ESP_IDF_6_0_2_COMPATIBILITY=UNVERIFIED_PLAN_GATE

RTC_TO_SYSTEM_TIME=PLANNED_AFTER_VALIDATED_RTC_TRUST
NTP_TO_SYSTEM_TIME=PLANNED_VIA_ESP_NETIF_SNTP_SYNC
NTP_TO_RTC_SYNC=PLANNED_AFTER_ACCEPTED_NTP_SYNC_AND_READBACK

RTC_NTP_ARBITRATION=OWNER_REVIEW_REQUIRED
BACKWARD_TIME_POLICY=OWNER_REVIEW_REQUIRED_NO_TRUSTED_RETROGRADE
LARGE_TIME_DELTA_POLICY=OWNER_REVIEW_REQUIRED

ISSUE89_BOUNDARY=CONNECTIVITY_AND_WLAN_OWNED_BY_ISSUE89
ISSUE124_CONTRACT_CHANGE=NO

RTC_SOFTWARE_IMPLEMENTATION=NOT_STARTED
RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING
NTP_REAL_NETWORK_RUN=NOT_RUN

OWNER_DECISIONS_REQUIRED=SMALL_DELTA,BACKWARD_TIME,LARGE_DELTA,LARGE_DELTA_RECOVERY,RTC_WRITE_FREQUENCY

MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
```

STOP – Owner Review des Absolute-Time-Plans.
