# Issue #178 – R1 lokale Zeitdarstellung und IANA-Zeitzonenaufloesung

## Planstatus und Baseline

```text
ISSUE=178
SCOPE=R1_LOCAL_TIME_OWNER
BASE_BRANCH=main
BASE_SHA=8bf48ccc28e0286e4550c7e778711e5682b72f42
PLAN_REVISION=3
PLAN_STATUS=AWAITING_OWNER_DECISION_O1_AND_PLAN_APPROVAL
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCTION_CODE_CHANGED=NO
OWNER_APPROVED_PLAN_SHA=NONE
SUPERSEDES_REVISION=2_AT_e5b5eeff4e0a2c56eff79ab1e1cada6dd74a73ac
B1_REUSE_BEFORE_BUILD=PASS
B2_SINGLE_TIMEZONE_TRUTH=PASS
B3_RTC_2000_2099_LEAK=PASS
B4_HISTORICAL_RULE_VALIDITY=ADDRESSED_IN_REVISION_3
BASIS=ISSUE_126_PR_127_MERGED
S4_OF_ISSUE_172_GATED_ON_THIS_ISSUE=YES
PR179=SEPARATE_NO_S4_IMPLEMENTATION_HERE
ACTUATOR_RELEASE=NO
```

Plan-only. Diese Revision ist eigenstaendig und vollstaendig; fruehere Revisionen
muessen nicht herangezogen werden. Es gibt keine Produktionslogik, keine produktiven
Tests, keine Dependency und keine #172-/S4-Aenderung vor Freigabe der exakten
Plan-SHA.

## 1. Ziel und Nicht-Ziele

Ziel: der kleinste robuste, renderer- und transportneutrale R1-Owner fuer

```text
trusted UTC (ITimeSource, #126) + kanonische Zeitzone (UserConfiguration)
    -> lokale Zeit inkl. Sommerzeit-Offset
```

den #172/S4 (Touch) und Web spaeter unveraendert konsumieren.

Nicht-Ziele: keine neue UTC-/RTC-/NTP-Quelle, keine manuelle Uhrzeit, keine
UI-Navigation/Screen `Zeit / Zeitzone`, keine Web-/Touch-/Renderer-Aenderung
(keine UI-spezifische Vorverdrahtung), keine allgemeine Welt-Zeitzonenloesung,
keine neue Dependency, keine Aenderung der Run-/Recovery-Zeitsemantik aus #126,
keine Aenderung von `UserConfiguration`-Schema oder Persistenz.

## 2. Verifizierte Ausgangslage

| # | Befund | Beleg |
|---|---|---|
| F1 | `ITimeSource::unixTimeSeconds()` liefert trusted UTC oder `nullopt`; reine UTC-Quelle, kennt keine Zone und ist der allgemeine trusted-UTC-Port (auch NTP-only, #126). | `lib/device_platform/src/time_source.hpp` |
| F2 | `ITimeZoneResolver::prepare()` ist `const`, validiert nur den Bezeichner und liefert `PreparedTimeZone{canonicalIdentifier}` ohne Offset/Regel. Der Port-Kommentar schliesst eine Zeitzonendatenbank aus. | `time_zone_resolver.hpp` |
| F3 | `EspTimeZoneResolver` akzeptiert nur den hartkodierten String `Europe/Zurich`; `MockTimeZoneResolver` akzeptiert jeden Bezeichner (Testdouble). | `esp_time_zone_resolver.cpp`, `mock_time_zone_resolver.cpp` |
| F4 | Der Firmwarekatalog `kTimeZones` (`fermentation_app`) enthaelt genau `Europe/Zurich`; `containsTimeZoneId` wird von `configuration_documents.cpp:95` fuer die Dokumentvalidierung genutzt, `prepare()` danach im Graph-Store. **`Europe/Zurich` steht damit heute doppelt** (Katalog, Adapter). | `firmware_configuration_catalog.cpp:9`, F3 |
| F5 | Die aktive Konfiguration traegt die vorbereitete Zone: `RuntimeConfigurationSnapshot::preparedTimeZone()`. `FermentationUiPresentationSource` kopiert nur `canonicalTimeZoneId`. | `runtime_configuration_snapshot.hpp:41`, `fermentation_application.cpp:1217` |
| F6 | Einziger Zeitkonsument ist `formatClockText` im Renderer (`gmtime_r` auf trusted UTC, bewusst ohne Zone). S4 ersetzt das; #178 aendert den Renderer nicht. | `main/fermentation_ui_renderer.cpp:199-215` |
| F7 | Pinned Toolchain: `LIBC_NEWLIB=y` (nicht Picolibc), `time_t` 64 Bit (`_Static_assert(sizeof(time_t)==8)` mit `xtensa-esp-elf-gcc` esp-15.2.0 uebersetzbar). | `sdkconfig`, Toolchain-Probe |
| F8 | **Offizieller Pfad (ESP-IDF 6.1):** `setenv("TZ", <POSIX-TZ>)` + `tzset()` + `localtime[_r]()` inkl. DST; die ESP-IDF-Doku nennt keine IANA-Datenbank, Espressif nennt keine Zeitzonenkomponente. | docs.espressif.com `v6.1/.../system_time.html` |
| F9 | **newlib der gepinnten Toolchain, per `nm -u` auf `libc.a`:** `localtime_r` ruft `__tz_lock`, `_tzset_unlocked`, `__tzcalc_limits`, `gmtime_r`, `__tz_unlock`; `_tzset_r` ruft `_getenv_r`, `strcmp`, `_malloc_r`, `free`; `_setenv_r` und `_getenv_r` halten `__env_lock`; `__tz_lock` liegt auf `__retarget_lock_acquire` (ESP-IDF-Locks). Die Pfade sind damit **durch newlib-Locks gegen parallele Aufrufe geschuetzt**; die pauschale Aussage "nicht thread-sicher" war nicht belegt und wird nicht uebernommen. Reale Restrisiken sind andere (siehe 3). | `libc.a`-Symbolanalyse (`nm -A`), esp-15.2.0_20251204 |
| F10 | Im Repository nutzt kein Code `TZ`, `setenv`, `tzset`, `localtime`, `mktime` oder `strftime`; nur `gmtime_r` (Renderer, `absolute_time_internal.hpp:213`). Ein TZ-basierter Pfad stuende also nicht im Konflikt mit anderen TZ-Nutzern. | `grep` ueber `lib main src include` |
| F11 | **#126-Bestand:** `lib/device_platform_esp_idf/src/absolute_time_internal.hpp` enthaelt `leapYear`, `daysInMonth`, Howard-Hinnant-`daysFromCivil` und die Rueckrichtung ueber libc-`gmtime_r` (`unixToDs3231Tm`), samt nativen Tests (`test/test_absolute_time_internal`). Der Kommentar "restricted to 2000..2099" betrifft den DS3231SN-Kalender der Aufrufer, nicht die Funktion selbst. Das Modul liegt in `device_platform_esp_idf` und ist fuer `device_platform` (ADR-013) nicht erreichbar. | `absolute_time_internal.hpp:48-72, 211-218` |
| F12 | #126 begrenzt nur den **DS3231SN-RTC-Kalender** auf 2000..2099 (`R1_DS3231SN_SUPPORTED_UTC_YEAR_RANGE`); NTP-only ist gueltig, und `ITimeSource` hat keine Jahresgrenze. Es gibt keine kanonische produktweite Lokalzeitgrenze. | `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md` (Zeilen 107, 454, 1194) |
| F13 | Referenzprobe (Host-glibc, nicht Ziel-libc): `CET-1CEST,M3.5.0,M10.5.0/3` reproduziert fuer 2026 die IANA-Grenzen von `Europe/Zurich` sekundengenau. Die Probe belegt die Regel, **nicht** newlib. | lokale Probe |
| F14 | Die `esp_idf_*_host`-Tests laufen mit dem ESP-IDF-Linux-Target gegen Host-libc (vor Umsetzung zu bestaetigen, siehe 7). Es gibt weder QEMU noch einen Hardware-Schritt in den GitHub-Gates fuer Zeitvektoren. | `test/esp_idf_*_host/`, `docs/CI_AND_QUALITY_GATES.md` |
| F15 | **Historische Zonensemantik:** `Europe/Zurich` hatte 1970 keine Sommerzeit; von 1981 bis 1995 endete die Sommerzeit am letzten Sonntag im September. Die modellierte Regel (letzter Sonntag Maerz/Oktober, 01:00 UTC) trifft erst ab 1996 zu (IANA-Probe: 1995-10-01 +60/ohne DST, die moderne Regel haette dort DST; 1996-03-31 und 1996-10-27 stimmen). #126 begrenzt `ITimeSource::unixTimeSeconds()` nicht auf >= 1996; ein SNTP-Sync kann technisch einen aelteren Wert als trusted publizieren. | IANA-`zoneinfo`-Probe, F1, F12 |

## 3. Reuse-before-build: Bewertung O1

Pruefreihenfolge laut `ENGINEERING_PRINCIPLES.md`:

1. **ESP-IDF 6.1 / newlib** (F8, F9): vorhanden und funktionsfaehig; es fehlt nur
   die Abbildung IANA-ID -> POSIX-String (kleine Tabelle). Keine IANA-Datenbank.
2. **Offizielle Espressif-Komponenten/-Repos:** keine Zeitzonenkomponente bekannt
   (F8). Keine Aufnahme.
3. **Gepflegte Drittkomponente:** nicht erforderlich; eine tzdata-/Kalender-
   bibliothek waere fuer eine Zone unverhaeltnismaessig (Flash, Lizenz, Pflege).
4. **Kleine Eigenentwicklung** nur bei nachgewiesener Luecke.

### Option A – newlib-POSIX-TZ (offizieller Pfad)

Konsequenzen im Projekt (alle belegt, F9/F10/F14):

- Konvertierung kann nicht portabel sein: ein neuer Port (oder eine
  Erweiterung von `ITimeZoneResolver`) mit ESP-Adapter, der `setenv`/`tzset`/
  `localtime_r` kapselt; `PreparedTimeZone` traegt den POSIX-String.
- **Globaler Zustand:** `TZ` muss dem aktiven Snapshot entsprechen. Ohne
  Zustandsverwaltung im Adapter (aktive Regel merken, bei Abweichung neu setzen,
  unter eigenem Mutex) kann eine Konvertierung still die falsche Zone anwenden;
  das ist ein neuer Fail-closed-Pfad, der zusaetzlichen Code und Tests braucht.
  Thread-Safety selbst ist durch newlib-Locks gegeben (F9).
- **Heap:** `_setenv_r` (`malloc`/`realloc`) und `_tzset_r` (`malloc`/`free`)
  allozieren bei jeder Zonenaenderung; eine feste `TZ`-Zuweisung pro Aenderung
  ist klein und selten, aber nicht heapfrei. Das Projekt hat eine dokumentierte
  Heap-Knappheit (PR #174, `heap_alloc_failed`-Beobachtungen).
- **Nachweis:** Die DST-Grenzen der Ziel-libc sind weder nativ noch im
  Linux-Target-Hostlauf (Host-libc, F14) belegbar. Ein belastbarer Nachweis
  braucht einen Hardware-/Emulationsschritt, der in den bestehenden Gates nicht
  existiert; ohne ihn bliebe das Akzeptanzkriterium "DST korrekt" auf dem Ziel
  unbewiesen und nicht regressionsgeschuetzt.

### Option B – kleine reine Funktion in `device_platform` (Empfehlung)

Die Luecke gegenueber A ist nicht "auf dem Host besser testbar", sondern:
(1) das Akzeptanzkriterium ist auf dem Ziel durch die vorhandenen Gates
nachweisbar, weil die Regel keine Ziel-libc-Zeitzonenlogik benutzt; (2) kein
Prozesszustand, kein Mutex, keine Aktivierungsreihenfolge, keine Heap-Allokation;
(3) kein neuer Port/Adapter, sondern eine reine Funktion, die Touch und Web von
jedem Task direkt aufrufen; (4) der eigene Anteil bleibt klein, weil vorhandene
Bausteine wiederverwendet werden:

- `daysFromCivil` aus #126 wird **extrahiert**, nicht dupliziert (siehe 4);
- die Aufteilung der lokalen Epochensekunden in Kalenderfelder nutzt libc-
  `gmtime_r` wie bereits #126 (`unixToDs3231Tm`) und der Renderer; keine zweite
  Civil-from-days-Implementierung;
- neu ist nur: Sommerzeit-Beginn/-Ende (letzter Sonntag Maerz/Oktober 01:00 UTC)
  aus `daysFromCivil` und dem Wochentag, ca. 25 Zeilen.

Wartungsrisiko: Aendern CH/EU die Sommerzeitregel, muss die Firmware angepasst
werden. Option A traegt dasselbe Risiko (POSIX-String in der Firmware); OTA ist
nicht Release 1. Das Risiko ist dokumentiert, nicht beseitigt.

**Empfehlung B**, bei wesentlicher Gewichtung des Espressif-first-Grundsatzes ist
A zulaessig; O1 liegt beim Owner (Abschnitt 6).

## 4. Architekturentscheidungen (Option B, aus dem Bestand abgeleitet)

**Kalenderlogik (Reuse).** `daysFromCivil` wird unveraendert aus
`device_platform_esp_idf/src/absolute_time_internal.hpp` in einen neuen
portablen Header `device_platform/src/civil_calendar.hpp` (Namespace
`device_platform`) verschoben; `absolute_time_internal.hpp` bindet ihn ein.
`leapYear`/`daysInMonth` bleiben im DS3231-Header (nur dort benoetigt). Der
Kommentar wird korrigiert: die Funktion ist allgemein; der 2000..2099-Bereich
ist Vertrag der DS3231-Aufrufer. `test_absolute_time_internal` laeuft
unveraendert weiter.

**Einzige Zonenwahrheit.** Genau eine kanonische Tabelle der in diesem Build
unterstuetzten Zonen: eine `constexpr`-Tabelle in
`device_platform/src/time_zone_rule.hpp` (je Eintrag kanonische IANA-ID +
Regel inklusive Gueltigkeitsbeginn), R1: ein Eintrag `Europe/Zurich`. Keine Registry, keine Provider, keine
Laufzeitregistrierung. Abgeleitet davon, nicht erneut gepflegt:

- `EspTimeZoneResolver::prepare()` und der Lookup `findTimeZoneRule(id)`
  verwenden ausschliesslich diese Tabelle (hartkodierter String im Adapter
  entfaellt);
- `firmware_configuration_catalog::containsTimeZoneId` delegiert an
  `findTimeZoneRule` (`fermentation_app` darf `device_platform` nutzen,
  ADR-013); `kTimeZones` in `fermentation_app` entfaellt; die API und die
  Aufrufer (`configuration_documents.cpp`) bleiben unveraendert;
- `MockTimeZoneResolver` bleibt Testdouble (akzeptiert beliebige Bezeichner,
  keine Supportliste); fuer `Success` liefert er die Regel ueber denselben
  Lookup, sofern der Bezeichner bekannt ist, sonst eine setzbare Test-Regel.

Damit gibt es **keine zweite manuell gepflegte Supportliste** und keinen
Konsistenztest als Ersatz fuer eine Einzelquelle; der Test prueft nur, dass
`containsTimeZoneId` und `findTimeZoneRule` fuer Katalog und Fremd-IDs
uebereinstimmen. `CONFIGURATION_PERSISTENCE.md` (Zeilen 203-206) benennt den
Katalog; der Absatz wird auf "kanonische Tabelle in `device_platform`, vom
`fermentation_app`-Katalog abgeleitet" angepasst.

**Typen (Vorschlag, finale Namen in Commit 2 gegen die Konvention).**

```cpp
namespace device_platform {

enum class DaylightSavingRule : std::uint8_t { Unavailable, None, EuropeanUnion };

struct TimeZoneRule {                       // Default = fail-closed
    DaylightSavingRule dst{DaylightSavingRule::Unavailable};
    std::int16_t standardOffsetMinutes{0};  // Europe/Zurich: 60
    std::int64_t validFromUtcSeconds{0};    // Regel gilt erst ab dieser UTC;
                                            // Europe/Zurich: 820454400
                                            // (1996-01-01T00:00:00Z)
};

struct LocalTime {                          // reine Werte, renderer-/transportneutral
    std::int32_t year;
    std::uint8_t month, day, hour, minute, second;
    std::int16_t utcOffsetMinutes;          // inkl. Sommerzeit; eindeutig im
    bool daylightSaving;                    // doppelten Herbst-Bereich
};

[[nodiscard]] std::optional<LocalTime> toLocalTime(
    std::optional<std::int64_t> trustedUtc, const PreparedTimeZone& zone) noexcept;
[[nodiscard]] std::optional<TimeZoneRule> findTimeZoneRule(
    const std::string& canonicalIdentifier);
}
```

**Port.** `ITimeZoneResolver` bleibt der schmale Validierungs-/Vorbereitungsport
und wird **minimal erweitert**: `PreparedTimeZone` erhaelt `TimeZoneRule rule`,
vom Resolver bei `Success` aus der Tabelle gefuellt. Kein zweiter Port (die
Konvertierung ist eine reine Funktion ohne Plattform-/Zeit-/Zustandsabhaengigkeit).
Der Port-Kommentar wird angepasst ("keine Zeitzonendatenbank" bleibt wahr: eine
Regeltabelle, keine Datenbank).

**`ITimeSource` bleibt reine UTC-Quelle** und wird nicht veraendert; Konsumenten
reichen `timeSource.unixTimeSeconds()` durch.

**Konsumentenvertrag (ohne UI-Vorverdrahtung).** `toLocalTime` nimmt die
`PreparedTimeZone` aus `RuntimeConfigurationSnapshot::preparedTimeZone()`; es
wird weder erneut aufgeloest noch Konfiguration/Persistenz dupliziert. Wie der
jeweilige Konsument diese vorbereitete Zone erhaelt (Presentation-Quelle/Cache/
`ClockViewInput` fuer Touch, Application-Lesepfad fuer Web), liegt in S4 bzw.
im Web-Plan; #178 ergaenzt **kein** Feld in `FermentationUiPresentationSource`,
Cache oder Renderer. Beide Konsumenten rufen dieselbe Funktion; sie ist weder
renderer- noch transportspezifisch. S4 muss in seinem Plan die Weitergabe der
vorbereiteten Zone vorsehen (Hinweis fuer #172, kein #178-Scope).

**Fail-closed.**

| Eingang | Ergebnis |
|---|---|
| `trustedUtc == nullopt` | `nullopt` (keine Lokalzeit, kein UTC-Ersatz) |
| `zone.rule.dst == Unavailable` (Default, nicht vorbereitet) | `nullopt` |
| `trustedUtc < zone.rule.validFromUtcSeconds` (Europe/Zurich: vor 1996-01-01T00:00:00Z) | `nullopt`; keine Umrechnung mit der modernen Regel |
| libc-`gmtime_r` kann die Instanz nicht darstellen | `nullopt` |
| unbekannte/ungueltige Zone | `prepare()` liefert `UnsupportedIdentifier`, es entsteht keine `PreparedTimeZone`; Ablehnung ueber den bestehenden Pfad |

**Gueltigkeitsgrenze der Zonenregel (B4).** Die modellierte Regel ist die
heutige EU-Regel; fuer `Europe/Zurich` trifft sie ab 1996 zu (F15). Die Grenze
`validFromUtcSeconds = 820454400` (1996-01-01T00:00:00Z, mitten in der
Standardzeit, daher ohne DST-Randfall) ist Teil der **kanonischen Zonentabelle**
und gilt fuer beide Optionen O1=A und O1=B: UTC davor liefert `nullopt`
(fail-closed), nie eine still falsch umgerechnete Lokalzeit. Es wird keine
historische IANA-Semantik gebaut oder behauptet; Zeitpunkte vor 1996 zeigen
keine Lokalzeit, ein spaeterer Bedarf waere eine eigene Scope-Entscheidung. Nach
oben gibt es keine Grenze: die Regel gilt fort (POSIX-TZ-Semantik); eine
kuenftige Regelaenderung erfordert eine Firmwareaenderung (Abschnitt 9).
Die Grenze ist eine Eigenschaft **dieser Zonenregel**, keine allgemeine Produkt-
oder RTC-Zeitgrenze: der RTC-Bereich 2000..2099 aus #126 (F12) bleibt auf den
DS3231SN-Adapter beschraenkt, und `ITimeSource` bleibt ungegrenzt.

**Algorithmus.** Nach der Grenzpruefung Ganzzahlarithmetik: Jahr der UTC-Instanz aus `gmtime_r`;
Beginn/Ende = letzter Sonntag Maerz/Oktober 01:00 UTC aus `daysFromCivil` und
Wochentag `(tage + 4) mod 7`; `daylightSaving = start <= utc < ende`; Offset =
`standardOffsetMinutes + (dst ? 60 : 0)`; Felder aus `gmtime_r(utc + offset)`.
Da der Wechsel in UTC definiert ist, ist UTC->lokal eindeutig; der doppelte
lokale 02:00-02:59-Bereich im Herbst unterscheidet sich ueber `daylightSaving`.
Keine Allokation, kein Zustand, `noexcept`.

## 5. Ressourcen-, Lizenz- und Sicherheitswirkung

- Neue Dependency/Lizenz: **nein** (`THIRD_PARTY_COMPONENTS.md`,
  `dependencies.lock`, `idf_component.yml` unveraendert).
- Flash/RAM: keine Schaetzung als Fakt. `sizeof(TimeZoneRule)` und
  `sizeof(LocalTime)` werden im Plan **nicht** behauptet; sie werden in Commit 2
  per `static_assert`/Test inklusive Padding/Alignment ermittelt und im
  Commit-Nachweis ausgewiesen. `PreparedTimeZone` waechst um ein
  `TimeZoneRule`. Kein Heap, keine Task, kein Lock, keine Langzeitallokation.
  Ein ESP-Ressourcenvergleich folgt im Self-Check/Pre-Ready-Lauf, nicht als
  eigener Nachweis dieses Plans.
- Safety: keine Aktorwirkung, kein Safety-/Persistenzpfad; Regelung bleibt von
  Netzwerk/Web/Anzeige unabhaengig. `ACTUATOR_RELEASE=NO`.
- Keine Aenderung der Run-/Recovery-Zeitsemantik (#126): Lokalzeit ist reine
  Anzeige; Run, Checkpoints und Recovery nutzen weiter nur UTC/monotone Zeit.

## 6. Owner-Entscheidung

**O1 – Umrechnungsweg.**

| Option | Inhalt | Bewertung |
|---|---|---|
| **B (Empfehlung)** | Reine Funktion + eine Regel + Tabelle in `device_platform`; `daysFromCivil` aus #126 extrahiert, Felder ueber libc-`gmtime_r` (Abschnitt 3/4) | zustands-/heapfrei; DST-Grenzen auf dem Ziel durch bestehende Gates beweisbar; kein neuer Port/Adapter; kleiner Eigenanteil (~25 Zeilen Regel) |
| A | newlib-POSIX-TZ im ESP-Adapter (offizieller Pfad) | maximale Wiederverwendung; newlib ist per Lock thread-sicher (F9); aber neuer Port/Adapter, Zonen-Aktivierungsprotokoll, Heap bei Zonenaenderung, DST-Grenzen auf dem Ziel nur ueber einen zusaetzlichen Hardware-/Emulationsnachweis belegbar |

Wird A gewaehlt, wird dieser Plan als neue Revision vollstaendig konsolidiert
(Port, Adapter, Aktivierungsprotokoll, Hardware-/Emulationsnachweis der
DST-Grenzen, POSIX-String in der Tabelle) und erneut freigegeben. Die Tabelle
als einzige Zonenwahrheit, die Gueltigkeitsgrenze 1996 (B4) und der Verzicht
auf UI-Vorverdrahtung gelten fuer beide Optionen.

Weitere Ownerentscheidungen: keine. Kein neues ADR noetig (ADR-013 eingehalten,
kein Modul neu geschnitten); die Entscheidung wird in `docs/ADOPT_OR_BUILD.md`
(neuer kurzer Abschnitt "Lokale Zeit" vor "Entscheidungsnachweis") festgehalten.

## 7. Umsetzungs- und Commit-Schnitte (nach Freigabe der exakten Plan-SHA)

Vor Commit 1 wird F14 (Host-Adaptertests laufen gegen Host-libc) bestaetigt.
Nach jedem Schnitt laufen die gezielten Tests des betroffenen Bereichs; bei
gemeinsamen Vertraegen zusaetzlich die direkt betroffenen Konsumententests.

1. **Extraktion (Reuse):** `daysFromCivil` -> `device_platform/src/civil_calendar.hpp`;
   `absolute_time_internal.hpp` bindet ein; Kommentar korrigiert. Tests:
   `test_absolute_time_internal` unveraendert gruen, direkter Test der
   extrahierten Funktion; `scripts/check_architecture_boundaries.py`.
2. **Domain:** `time_zone_rule.{hpp,cpp}` (Tabelle, `findTimeZoneRule`),
   `local_time.{hpp,cpp}` (`toLocalTime`), `PreparedTimeZone::rule`;
   Testsuite `test/test_local_time` (Abschnitt 8); `sizeof`-Nachweis.
3. **Resolver/Katalog:** `EspTimeZoneResolver` und `MockTimeZoneResolver` nutzen
   die Tabelle; `containsTimeZoneId` delegiert, `kTimeZones` entfaellt.
   Konsumententests: `test_configuration_documents`,
   `test_configuration_graph_store`, `test_configuration_service`,
   `test_configuration_codecs`, `test_fermentation_ui_presentation_cache`,
   `test_device_ui_contracts`; Adapter-Host-Test des Resolvers, falls vorhanden.
4. **Doku/Status:** `ADOPT_OR_BUILD.md`, Port-Kommentar,
   `CONFIGURATION_PERSISTENCE.md` (Katalogabsatz), `ROADMAP.md`, `CHANGELOG.md`.

Nicht Teil von #178: Renderer, `formatClockText`, `ClockViewInput`,
Presentation-Quelle/-Cache, Screen, Web, Hardware.

## 8. Tests und Nachweise

Deterministisch nativ, ohne NTP/RTC. Referenzvektoren wurden unabhaengig mit
IANA-Daten (Python `zoneinfo`, `Europe/Zurich`) berechnet; Offset in Minuten.

| Fall | UTC-Sekunden | erwartete Lokalzeit | Offset | DST |
|---|---|---|---|---|
| Winter 2026-01-01 00:00Z | 1767225600 | 2026-01-01 01:00:00 | +60 | nein |
| Sommer 2026-07-01 00:00Z | 1782864000 | 2026-07-01 02:00:00 | +120 | ja |
| Fruehjahr 2026 -1 s / 0 | 1774745999 / 1774746000 | 2026-03-29 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2026 -1 s / 0 | 1792889999 / 1792890000 | 2026-10-25 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2027 -1 s / 0 | 1806195599 / 1806195600 | 2027-03-28 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2027 -1 s / 0 | 1824944399 / 1824944400 | 2027-10-31 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2024 (Schaltjahr) -1 s / 0 | 1711846799 / 1711846800 | 2024-03-31 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2024 -1 s / 0 | 1729990799 / 1729990800 | 2024-10-27 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2040 -1 s / 0 | 2216249999 / 2216250000 | 2040-03-25 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2040 -1 s / 0 | 2234998799 / 2234998800 | 2040-10-28 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Fruehjahr 2100 (kein Schaltjahr) -1 s / 0 | 4109878799 / 4109878800 | 2100-03-28 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 2100 -1 s / 0 | 4128627599 / 4128627600 | 2100-10-31 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| int32-Grenze 2038-01-19 03:14:08Z | 2147483648 | 2038-01-19 04:14:08 | +60 | nein |
| Gueltigkeitsgrenze: 1995-12-31 23:59:59Z (1 s davor) | 820454399 | `nullopt` | – | – |
| Gueltigkeitsgrenze: 1996-01-01 00:00:00Z (ab Grenze) | 820454400 | 1996-01-01 01:00:00 | +60 | nein |
| Fruehjahr 1996 -1 s / 0 | 828233999 / 828234000 | 1996-03-31 01:59:59 / 03:00:00 | +60 / +120 | nein / ja |
| Herbst 1996 -1 s / 0 | 846377999 / 846378000 | 1996-10-27 02:59:59 / 02:00:00 | +120 / +60 | ja / nein |
| Historischer Gegenfall Herbst 1995 (1995-10-01 00:00Z; IANA: +60 ohne DST; moderne Regel: DST) | 812505600 | `nullopt` (nicht still +120) | – | – |
| Historischer Gegenfall Sommer 1970 (1970-07-01 00:00Z; IANA: +60 ohne DST; moderne Regel: DST) | 15638400 | `nullopt` | – | – |
| Jahreswechsel 2026-12-31 23:30Z | 1798759800 | 2027-01-01 00:30:00 | +60 | nein |

(2100 prueft, dass die Regel nach oben ohne kuenstliche Grenze gilt; die
IANA-Referenz stammt dort aus der POSIX-Regel der tzdata und ist damit kein
unabhaengiger Beleg ueber die Regel hinaus.)

Zusaetzlich:

- fehlende trusted UTC (`nullopt`) -> `nullopt`, kein UTC-Ersatz;
- `TimeZoneRule{}` (Default `Unavailable`) -> `nullopt`; negative UTC -> `nullopt` (liegt vor der Gueltigkeitsgrenze);
- doppelter lokaler Herbstbereich: zwei verschiedene UTC-Instanzen mit
  identischer lokaler Uhrzeit sind ueber `daylightSaving` unterscheidbar;
- `findTimeZoneRule`: `Europe/Zurich` -> Regel; unbekannt/leer/falsche
  Schreibweise (`europe/zurich`, `Europe/Berlin`) -> nicht gefunden;
  `containsTimeZoneId` stimmt mit `findTimeZoneRule` fuer alle Faelle ueberein
  (bestehende Assertions Zurich/Berlin bleiben);
- Resolver: `prepare("Europe/Zurich")` traegt die Regel, alles andere
  `UnsupportedIdentifier`; Graph-Store/Service lehnen unveraendert ab;
- Extraktionstests (Commit 1) und `sizeof`-Nachweise (Commit 2);
- Konsumententests laut Commit 3 unveraendert gruen.

**Bewusst kein Nachweis:** Option B nutzt keine libc-Zeitzonenlogik; ein
newlib-/Hardwarenachweis der DST-Grenzen entfaellt. `gmtime_r` ist reine
UTC-Zerlegung und wird bereits von #126 genutzt. Gesamtbuild beider ESP-IDF-
Profile und die uebrigen Gates laufen erst im Self-Check bzw. Pre-Ready-Lauf
nach Ownerfreigabe; nicht ausgefuehrt gilt als nicht bestanden.

## 9. Risiken

| Risiko | Behandlung |
|---|---|
| Historische UTC (SNTP liefert Wert < 1996) | Gueltigkeitsgrenze 1996 in der Zonentabelle, `nullopt`; Tests an der Grenze und historische Gegenfaelle (F15) |
| Regelaenderung (EU/CH Sommerzeitabschaffung) | dokumentiertes Wartungsrisiko; Regel an einer Stelle; Firmwareupdate noetig (OTA nicht R1); gilt auch fuer Option A |
| Zweite Zonenwahrheit | eine Tabelle; Katalog und Resolver leiten ab (Abschnitt 4) |
| Extraktion beruehrt #126-Code | rein mechanisch; `test_absolute_time_internal` und ESP-Build beider Profile als Nachweis; Commit 1 getrennt |
| Verwechslung Anzeige und Run-Zeit | Lokalzeit nur Anzeige; Run/Recovery bleiben UTC/monoton |
| Option A nachtraeglich gewuenscht | O1; vollstaendig neue Planrevision |

## 10. Dokumentationswirkung

`docs/ADOPT_OR_BUILD.md` (neuer Abschnitt "Lokale Zeit": Entscheidung, Evidenz
F7-F15), `docs/CONFIGURATION_PERSISTENCE.md` (Katalogabsatz), `docs/ROADMAP.md`
(Zeile #178 zu PR-Beginn und nach Merge), Kommentar in `time_zone_resolver.hpp`,
`CHANGELOG.md`. Keine Requirements werden in der Roadmap kopiert.

## 11. Stop-Block

```text
PLAN_PATH=docs/tasks/issue-178-r1-local-time-owner-plan.md
IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```
