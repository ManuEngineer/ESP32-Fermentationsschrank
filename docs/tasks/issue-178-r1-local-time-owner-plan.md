# Issue #178 – R1 lokale Zeitdarstellung und IANA-Zeitzonenaufloesung

## Planstatus und Baseline

```text
ISSUE=178
SCOPE=R1_LOCAL_TIME_OWNER
BASE_BRANCH=main
BASE_SHA=8bf48ccc28e0286e4550c7e778711e5682b72f42
PLAN_REVISION=1
PLAN_STATUS=AWAITING_OWNER_DECISION_O1_O2_AND_PLAN_APPROVAL
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCTION_CODE_CHANGED=NO
OWNER_APPROVED_PLAN_SHA=NONE
BASIS=ISSUE_126_PR_127_MERGED
S4_OF_ISSUE_172_GATED_ON_THIS_ISSUE=YES
PR179=SEPARATE_NO_S4_IMPLEMENTATION_HERE
ACTUATOR_RELEASE=NO
```

Plan-only. Dieser Plan fuehrt keine Produktionslogik, keine produktiven Tests,
keine Abhaengigkeit und keine #172-/S4-Aenderung ein.

## 1. Ziel und Nicht-Ziele

Ziel: der kleinste robuste, renderer- und transportneutrale R1-Owner fuer

```text
trusted UTC (ITimeSource, #126) + kanonische Zeitzone (UserConfiguration)
    -> lokale Zeit inkl. Sommerzeit-Offset
```

den #172/S4 (Touch) und Web spaeter unveraendert konsumieren.

Nicht-Ziele (Issue + Auftrag): keine neue UTC-/RTC-/NTP-Quelle, keine manuelle
Uhrzeit, keine UI-Navigation/Screen `Zeit / Zeitzone`, keine Web-/Touch-
Implementation, keine allgemeine Welt-Zeitzonenloesung, keine neue Dependency,
keine Aenderung der Run-/Recovery-Zeitsemantik aus #126, keine Aenderung an
Zeitzonenkatalog oder `UserConfiguration`-Schema.

## 2. Verifizierte Ausgangslage

| # | Befund | Beleg |
|---|---|---|
| F1 | `ITimeSource::unixTimeSeconds()` liefert trusted UTC oder `nullopt`; reine UTC-Quelle, kennt keine Zone. | `lib/device_platform/src/time_source.hpp` |
| F2 | `ITimeZoneResolver::prepare()` ist `const`, validiert nur den Bezeichner und liefert `PreparedTimeZone{canonicalIdentifier}` ohne Offset/Regel. Der Port-Kommentar schliesst ausdruecklich eine Zeitzonendatenbank aus. | `time_zone_resolver.hpp` |
| F3 | `EspTimeZoneResolver` akzeptiert nur den hartkodierten String `Europe/Zurich`; `MockTimeZoneResolver` akzeptiert jeden Bezeichner. | `esp_time_zone_resolver.cpp`, `mock_time_zone_resolver.cpp` |
| F4 | Der Firmwarekatalog enthaelt genau `Europe/Zurich`. **`Europe/Zurich` steht damit heute doppelt** (Katalog in `fermentation_app`, Adapter in `device_platform_esp_idf`) ohne Konsistenztest. | `firmware_configuration_catalog.cpp:9`, F3 |
| F5 | Die aktive Konfiguration traegt die vorbereitete Zone: `RuntimeConfigurationSnapshot::preparedTimeZone()`. `FermentationUiPresentationSource` kopiert daraus nur `canonicalTimeZoneId`; sein Kommentar bestimmt sie als einzige Quelle, die die vorbereitete Zone nicht erneut liest. | `runtime_configuration_snapshot.hpp:41`, `fermentation_application.cpp:1217`, `fermentation_ui_models.hpp:140-150` |
| F6 | Der einzige heutige Konsument ist `formatClockText` im Renderer: `gmtime_r` auf trusted UTC, ausdruecklich ohne Zonenanwendung. S4 ersetzt das (Issue #172); #178 aendert den Renderer nicht. | `main/fermentation_ui_renderer.cpp:199-215` |
| F7 | Toolchain: `LIBC_NEWLIB=y` (nicht Picolibc), `time_t` 64 Bit (statisch: `_Static_assert(sizeof(time_t)==8)` mit `xtensa-esp-elf-gcc` esp-15.2.0 uebersetzt). | `sdkconfig`, Toolchain-Probe |
| F8 | newlib besitzt `tzset`, `_tzset_r`, `localtime_r`, `__tzcalc_limits`, `__tz_lock` und den globalen Zustand `prev_tzenv`/`_daylight`; **keine** zoneinfo-/IANA-Datei in der Toolchain (`find ... zoneinfo` leer). `tm_gmtoff` ist nur bei `__TM_GMTOFF` vorhanden und darf nicht Vertragsbestandteil sein. | `libc.a`-Symbole, `time.h:48` |
| F9 | ESP-IDF-Doku (v6.1, `system_time`): Zeitzone nur ueber `setenv("TZ", ...)` + `tzset()` im POSIX/GNU-TZ-Format; keine IANA-Datenbank erwaehnt; keine Aussage zu Thread-Safety von `tzset`/`localtime_r`. Espressif nennt keine Komponente fuer IANA-Zonen. | docs.espressif.com/.../v6.1/.../system_time.html |
| F10 | Die vorhandenen `esp_idf_*_host`-Tests laufen mit dem ESP-IDF-Linux-Target gegen Host-libc. Ein Host-Lauf belegt daher newlib-Verhalten **nicht**. | `test/esp_idf_*_host/` (Annahme, vor Umsetzung zu bestaetigen, siehe 7) |
| F11 | Referenzprobe (Host-glibc, nicht Ziel-libc): `CET-1CEST,M3.5.0,M10.5.0/3` reproduziert fuer 2026 die IANA-Grenzen von `Europe/Zurich` (03-29 01:00 UTC, 10-25 01:00 UTC) sekundengenau. | lokale Probe, Abschnitt 8 |

## 3. Reuse-before-build-Bewertung

Reihenfolge laut Auftrag:

1. **ESP-IDF 6.1 / newlib (Option A).** POSIX-TZ (`CET-1CEST,M3.5.0,M10.5.0/3`)
   per `setenv`/`tzset`/`localtime_r`. Es existiert keine IANA-Datenbank; die
   IANA-ID muesste ohnehin auf einen POSIX-String abgebildet werden (kleine
   Tabelle, im R1-Katalog ein Eintrag). Kosten und Risiken:
   - **Globaler Prozesszustand:** `TZ` ist Umgebungsvariable; `setenv` ist nicht
     thread-sicher gegenueber `getenv` aus anderen Tasks (httpd, SNTP). `prepare()`
     ist `const` und liegt im Validierungspfad (`ConfigurationGraphStore`,
     `ConfigurationService`); Seiteneffekte dort sind unzulaessig. Es braeuchte
     genau einen Setz-Punkt im Composition Root und eine Regel fuer
     Zonenwechsel zur Laufzeit.
   - **Nicht ueber Host testbar (F10):** Native Tests und Host-Adaptertests
     laufen gegen glibc. DST-Grenzen auf newlib liessen sich nur auf
     Hardware/Emulation belegen.
   - **Geteilte Wahrheit:** Ein Regel-String plus ein Resolver plus ein Katalog.
2. **Offizielle Espressif-Komponenten/-Repos:** keine IANA-/Zeitzonenkomponente
   bekannt (F9). Wird beim Umsetzungsstart nicht erneut geprueft, wenn O1=B.
3. **Gepflegte Drittkomponente:** nicht erforderlich; eine allgemeine tzdata-/
   Kalenderbibliothek waere fuer eine Zone unverhaeltnismaessig (YAGNI, Flash,
   Lizenz-/Pflegeaufwand). Keine Aufnahme.
4. **Kleine Eigenloesung (Option B)** nur bei belegter Luecke. Die belegte Luecke
   ist F8/F10 plus Abschnitt 3.1: kein portabel testbarer, zustandsfreier
   Weg ueber den libc-Pfad.

Keine eigene DST-/IANA-**Datenbank** wird geplant. Option B ist eine einzelne
parametrierte Regel (Standardoffset + EU-Sommerzeitregel) und eine reine
Funktion.

### 3.1 Option B im Ueberblick

- reine, zustandsfreie Funktion in `device_platform`, identisch auf Host und
  Ziel; keine `TZ`, kein `tzset`, kein `localtime`, kein Lock;
- vollstaendig mit den vorhandenen nativen Tests und den IANA-Referenzvektoren
  aus Abschnitt 8 pruefbar;
- Wartungsrisiko: Aendert die Schweiz/EU die Sommerzeitregel, muss die Regel in
  der Firmware geaendert werden. Option A hat dasselbe Risiko (Regel-String in
  der Firmware); OTA ist nicht Release 1. Das Risiko wird dokumentiert, nicht
  beseitigt.

## 4. Architekturentscheidungen (aus dem Bestand abgeleitet)

**Owner.** Der einzige UTC->Local-Owner ist eine reine Funktion in
`device_platform` (ADR-013: portabel, anwendungsneutral, im Profil `native`
testbar). Sie liest keine Uhr, keine Konfiguration und kein Persistenzobjekt.

```cpp
namespace device_platform {

// Regelwerk einer Zone. Keine Zeitzonendatenbank: ein Standardoffset und
// hoechstens eine Sommerzeitregel. kUnavailable ist der fail-closed Default.
enum class DaylightSavingRule : std::uint8_t { Unavailable, None, EuropeanUnion };

struct TimeZoneRule {
    DaylightSavingRule dst{DaylightSavingRule::Unavailable};
    std::int16_t standardOffsetMinutes{0};  // Europe/Zurich: 60
};

struct LocalTime {            // renderer-/transportneutral, reine Werte
    std::int32_t year;        // Kalenderfelder der lokalen Zeit
    std::uint8_t month;       // 1..12
    std::uint8_t day;         // 1..31
    std::uint8_t hour;        // 0..23
    std::uint8_t minute;      // 0..59
    std::uint8_t second;      // 0..59
    std::int16_t utcOffsetMinutes;  // inkl. Sommerzeit, eindeutig auch im
                                    // doppelten Herbst-Bereich
    bool daylightSaving;
};

[[nodiscard]] std::optional<LocalTime> toLocalTime(
    std::optional<std::int64_t> trustedUtc, const TimeZoneRule& rule) noexcept;

}  // namespace device_platform
```

(Namen und Feldbreiten sind der Planvorschlag; sie werden in Commit 1 gegen die
vorhandene Namens-/Typkonvention finalisiert, ohne den Vertrag zu aendern.)

**Port.** `ITimeZoneResolver` bleibt ein schmaler Validierungs-/Vorbereitungs-
port. Er wird **minimal erweitert**, nicht durch einen zweiten Port ergaenzt:
`PreparedTimeZone` erhaelt zusaetzlich das Feld `TimeZoneRule rule`, das der
Resolver bei erfolgreichem `prepare()` fuellt. Eine getrennte Konvertierungs-
grenze ist nicht noetig, weil die Konvertierung eine reine Funktion ist und
keine Plattform-, Zeit- oder Zustandsabhaengigkeit hat. Der Port-Kommentar
("keine Zeitzonendatenbank") bleibt sinngemaess wahr und wird um die Aussage
ergaenzt, dass `rule` die einzige Regelquelle ist.

**Eine Zonenwahrheit.** Die Abbildung `kanonische IANA-ID -> TimeZoneRule` liegt
an genau einer Stelle: einer kleinen portablen Lookup-Funktion in
`device_platform` (`findTimeZoneRule(identifier)`), die fuer den R1-Katalog
`Europe/Zurich -> {EuropeanUnion, 60}` kennt. `EspTimeZoneResolver::prepare()`
ruft sie auf und ersetzt damit seinen eigenen hartkodierten String; es entsteht
**kein dritter Ort**. Die vorhandene Doppelung Katalog/Adapter (F4) wird nicht
still umgebaut, sondern durch einen nativen Konsistenztest abgesichert: jede
Katalogzone muss `findTimeZoneRule` aufloesen koennen (Drift wird ein Testfehler).
`MockTimeZoneResolver` bleibt Testhilfe und liefert fuer `Success` die Regel
ueber denselben Lookup bzw. eine setzbare Regel.

**`ITimeSource` bleibt reine UTC-Quelle.** Sie wird nicht veraendert.
Konsumenten reichen `timeSource.unixTimeSeconds()` an `toLocalTime` durch.

**Konsum ohne Duplikat von Konfiguration/Persistenz.** Die Regel kommt aus
`RuntimeConfigurationSnapshot::preparedTimeZone()` (F5), nicht aus dem
UI-String in `ClockViewInput`; es wird nichts erneut aufgeloest, nichts
persistiert. Damit Touch und Web denselben Owner konsumieren koennen, erhaelt
`FermentationUiPresentationSource` (einzige Presentation-Quelle, F5) das Feld
`TimeZoneRule timeZoneRule`, kopiert aus der vorbereiteten Zone (siehe O2).
Weiterreichen in den Presentation-Cache, `ClockViewInput` und Renderer ist
**S4/#172** bzw. spaeter Web, nicht #178.

**Fail-closed.**

| Eingang | Ergebnis |
|---|---|
| `trustedUtc == nullopt` | `nullopt` (keine Lokalzeit, kein UTC-Ersatz) |
| `rule.dst == Unavailable` (Default, nicht vorbereitete/unsupported Zone) | `nullopt` |
| `trustedUtc` ausserhalb des Gueltigkeitsfensters der Regel | `nullopt` |
| unbekannter/ungueltiger Bezeichner | `prepare()` liefert `UnsupportedIdentifier`; es gibt keine `PreparedTimeZone`, die Konfiguration wird wie bisher abgelehnt (bestehender Pfad, unveraendert) |

Gueltigkeitsfenster der EU-Regel: UTC 2000-01-01 .. 2099-12-31 (Jahre, fuer die
die Regel "letzter Sonntag Maerz/Oktober, 01:00 UTC" gilt und vollstaendig
getestet wird). Die Untergrenze wird in Commit 1 gegen die in #126 verwendete
Plausibilitaetsuntergrenze der trusted UTC abgeglichen; eine Abweichung ist
Befund, keine stille Wahl.

**Algorithmus (Option B).** Ganzzahlarithmetik, checked: Tage-seit-Epoche ->
buergerliches Datum (bekannter Civil-from-days-Algorithmus), Jahr der UTC-
Instanz, Sommerzeitbeginn/-ende als UTC-Instanz `letzter Sonntag im Maerz/
Oktober 01:00 UTC`; `daylightSaving = start <= utc < end`; Offset =
`standardOffsetMinutes + (dst ? 60 : 0)`; Kalenderfelder aus `utc + offset`.
Da der Wechsel in UTC definiert ist, ist die Abbildung UTC->lokal eindeutig; der
doppelte lokale 02:00-02:59-Bereich im Herbst unterscheidet sich ueber
`daylightSaving`/`utcOffsetMinutes`. Keine Allokation, kein Heap, keine Zeit-
abfrage, `noexcept`.

## 5. Ressourcen-, Lizenz- und Sicherheitswirkung

- Neue Bibliothek/Dependency: **nein**; `THIRD_PARTY_COMPONENTS.md` unveraendert,
  `dependencies.lock`/`idf_component.yml` unveraendert, keine Lizenzwirkung.
- Flash: geschaetzt wenige hundert Byte; RAM: `PreparedTimeZone` waechst um eine
  3-Byte-Struktur, `FermentationUiPresentationSource` um dieselbe; kein Heap, keine
  neue Task, kein Lock, keine neue Langzeitallokation. Messung: statischer
  Groessenvergleich (`sizeof`-Assertions in den Tests); ein Ressourcen-/Hardware-
  nachweis ist nur erforderlich, wenn der Build eine unerwartete Abweichung
  zeigt (laut Auftrag nur soweit die Loesung ihn erfordert).
- Safety: keine Aktorwirkung, kein Safety-Pfad, keine Persistenzaenderung; die
  Regelung bleibt von Netzwerk/Web/Anzeige unabhaengig. `ACTUATOR_RELEASE=NO`.
- Keine Aenderung von Run-/Recovery-Zeitsemantik (#126): Run, Checkpoints und
  Recovery nutzen weiter nur UTC/monotone Zeit; Lokalzeit ist reine Anzeige.

## 6. Owner-Entscheidungen (konkrete Optionen mit Empfehlung)

**O1 – Umrechnungsweg.**

| Option | Inhalt | Bewertung |
|---|---|---|
| **B (Empfehlung)** | Reine Funktion + eine EU-Regel in `device_platform` (Abschnitt 4) | portabel, zustandsfrei, auf Host und Ziel identisch testbar; Eigenanteil klein; Regelrisiko dokumentiert |
| A | newlib POSIX-TZ (`setenv`/`tzset`/`localtime_r`) im ESP-Adapter | maximale Wiederverwendung, aber globaler Prozesszustand, nicht thread-sicher dokumentiert, newlib-DST-Grenzen nur auf Hardware pruefbar, Setz-Punkt im Composition Root und Zonenwechsel-Regel noetig |

Empfehlung B, weil die belegte Luecke (F8/F10) genau den Wert von A (Reuse)
aufhebt: Ohne portablen Test waere der Reuse-Gewinn nicht verifizierbar.
Bei Wahl von A wird dieser Plan als neue Revision (Adapter-Pfad, Setz-Punkt,
Hardware-/Emulationsnachweis der DST-Grenzen auf newlib) vollstaendig
konsolidiert und erneut freigegeben.

**O2 – Konsumentenzugang.** #178 liefert zusaetzlich das Feld
`FermentationUiPresentationSource::timeZoneRule` (Empfehlung: ja), damit
"Touch und Web konsumieren denselben Owner" bei Merge von #178 tatsaechlich
gilt und S4 nur noch Cache/`ClockViewInput`/Renderer verdrahtet. Alternative:
nur Funktion + `PreparedTimeZone::rule`, Zugang zur Presentation erst in S4
(kleiner #178-Diff, aber S4 beruehrt dann die Application-Facade).

Keine weiteren Entscheidungen offen. Kein neues ADR noetig: ADR-013 wird
eingehalten, kein Modul wird neu geschnitten; die Entscheidung wird in
`docs/ADOPT_OR_BUILD.md` (neuer kurzer Abschnitt "Lokale Zeit" vor
"Entscheidungsnachweis") festgehalten.

## 7. Umsetzungs- und Commit-Schnitte (nach Freigabe der exakten Plan-SHA)

Nach jedem Commit wird angehalten (Ownerfreigabe je Commit). Vor Commit 1 wird
F10 (Host-Adaptertests laufen gegen Host-libc) und die #126-Untergrenze
bestaetigt.

1. **Domain:** `device_platform/src/local_time.{hpp,cpp}` mit
   `TimeZoneRule`, `LocalTime`, `toLocalTime`, Lookup `findTimeZoneRule`; neues
   Testsuite `test/test_local_time` (Abschnitt 8). Pruefung der Architektur-
   grenzen (`scripts/check_architecture_boundaries.py`).
2. **Resolver:** `PreparedTimeZone::rule`; `EspTimeZoneResolver` nutzt
   `findTimeZoneRule` (hartkodierter String entfaellt); `MockTimeZoneResolver`
   liefert die Regel; Konsistenztest Katalog <-> Lookup; direkt betroffene
   Konsumententests (`test_configuration_graph_store`,
   `test_configuration_service`, `test_configuration_codecs`,
   `test_configuration_documents`, `test_fermentation_ui_presentation_cache`,
   `test_device_ui_contracts`) und der Adapter-Host-Test des Resolvers, falls
   vorhanden.
3. **Presentation-Zugang (O2):** `FermentationUiPresentationSource::timeZoneRule`
   aus `preparedTimeZone()`; Tests der Quelle.
4. **Doku/Status:** `ADOPT_OR_BUILD.md`, Port-Kommentar, `ROADMAP.md`,
   `LOCAL_UI.md` nur falls ein kanonischer Absatz die UTC-Anzeige beschreibt;
   `CHANGELOG.md` nach Repositoryregel.

Nicht Teil von #178: Renderer, `formatClockText`, `ClockViewInput`,
Presentation-Cache, Screen, Web, Hardware.

## 8. Tests und Nachweise

Deterministisch nativ, ohne NTP/RTC. Referenzvektoren wurden unabhaengig mit
IANA-Daten (Python `zoneinfo`, `Europe/Zurich`) berechnet:

| Fall | UTC-Sekunden | erwartete Lokalzeit | Offset | DST |
|---|---|---|---|---|
| Winter 2026-01-01 00:00Z | 1767225600 | 2026-01-01 01:00:00 | +60 | nein |
| Sommer 2026-07-01 00:00Z | 1782864000 | 2026-07-01 02:00:00 | +120 | ja |
| Fruehjahr 2026 -1 s | 1774745999 | 2026-03-29 01:59:59 | +60 | nein |
| Fruehjahr 2026 0 | 1774746000 | 2026-03-29 03:00:00 | +120 | ja |
| Herbst 2026 -1 s | 1792889999 | 2026-10-25 02:59:59 | +120 | ja |
| Herbst 2026 0 | 1792890000 | 2026-10-25 02:00:00 | +60 | nein |
| Fruehjahr 2027 -1 s / 0 | 1806195599 / 1806195600 | 2027-03-28 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2027 -1 s / 0 | 1824944399 / 1824944400 | 2027-10-31 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2024 (Schaltjahr) -1 s / 0 | 1711846799 / 1711846800 | 2024-03-31 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2024 -1 s / 0 | 1729990799 / 1729990800 | 2024-10-27 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2040 (> int32-Zeit) -1 s / 0 | 2216249999 / 2216250000 | 2040-03-25 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2040 -1 s / 0 | 2234998799 / 2234998800 | 2040-10-28 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Jahreswechsel 2026-12-31 23:30Z | 1798759800 | 2027-01-01 00:30:00 | +60 | nein |
| Fenstergrenze unten 2000-01-01 00:00Z | 946684800 | 2000-01-01 01:00:00 | +60 | nein |
| Fenstergrenze oben 2099-12-31 23:59:59Z | 4102444799 | 2100-01-01 00:59:59 | +60 | nein |

Zusaetzlich:

- fehlende trusted UTC (`nullopt`) -> `nullopt`, kein UTC-Ersatz;
- `TimeZoneRule{}` (Default `Unavailable`) -> `nullopt`; Werte ausserhalb des
  Fensters (1999-12-31T23:59:59Z, 2100-01-01T00:00:00Z) -> `nullopt`; negative
  UTC -> `nullopt`;
- doppelter lokaler Herbstbereich: zwei verschiedene UTC-Instanzen mit
  identischer lokaler Uhrzeit sind ueber `daylightSaving` unterscheidbar;
- `findTimeZoneRule`: `Europe/Zurich` -> Regel; unbekannter/leer/falsche
  Schreibweise -> nicht gefunden;
- Resolver: `EspTimeZoneResolver::prepare("Europe/Zurich")` traegt die Regel,
  `UnsupportedIdentifier` fuer alles andere (bestehender kanonischer
  Validierungspfad, `ConfigurationGraphStore`/`ConfigurationService` lehnen
  unveraendert ab); Konsistenztest Katalog <-> Lookup;
- `sizeof(TimeZoneRule)`/`sizeof(LocalTime)`-Assertions als Ressourcennachweis;
- Konsumententests laut Commit 2/3 unveraendert gruen.

**Bekannte Nicht-Nachweise (bewusst):** Option B nutzt kein libc-TZ, daher ist
kein newlib-/Hardwarenachweis der DST-Grenzen erforderlich. Ein Gesamtbuild
beider ESP-IDF-Profile und alle uebrigen Gates laufen erst im Self-Check bzw.
Pre-Ready-Lauf nach Ownerfreigabe; nicht ausgefuehrt gilt als nicht bestanden.

## 9. Risiken

| Risiko | Behandlung |
|---|---|
| Regelaenderung (EU/CH Sommerzeitabschaffung) | dokumentiertes Wartungsrisiko; Regel an einer Stelle; Firmwareupdate noetig (OTA ist nicht Release 1) |
| Zweite Zonenwahrheit | Lookup ist einzige Abbildung; Konsistenztest Katalog <-> Lookup (F4) |
| Zeitgrenzen/Overflow | Gueltigkeitsfenster, checked Arithmetik, Grenztests 2000/2099/2040 |
| Verwechslung Anzeige und Run-Zeit | Lokalzeit nur Anzeige; Run/Recovery bleiben UTC/monoton |
| Option A nachtraeglich gewuenscht | O1; erfordert vollstaendig neue Planrevision |

## 10. Dokumentationswirkung

`docs/ADOPT_OR_BUILD.md` (neuer Abschnitt "Lokale Zeit": Entscheidung und Evidenz F7-F11),
`docs/ROADMAP.md` (Zeile #178 zu Beginn dieses PR; spaeter Merge-Status),
Kommentar in `time_zone_resolver.hpp`, `CHANGELOG.md`. Keine Requirements
werden in der Roadmap kopiert.

## 11. Stop-Block

```text
PLAN_PATH=docs/tasks/issue-178-r1-local-time-owner-plan.md
IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```
