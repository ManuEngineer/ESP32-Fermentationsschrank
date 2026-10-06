# Issue #181 – SNTP-Trust auf NTP-only Cold Boot (Bugfix am #126-Zeitowner)

## Planstatus und Baseline

```text
ISSUE=181
SCOPE=BUGFIX_ABSOLUTE_TIME_OWNER_ISSUE_126
BASE_BRANCH=main
BASE_SHA=9cfffdfae73f8903b6feabf74acc20d754014b49
PLAN_REVISION=2
SUPERSEDES_REVISION=1_AT_4884870ba02f7896b6f7a68db74dfe38058dee5c
PLAN_STATUS=AWAITING_OWNER_APPROVAL
OWNER_APPROVED_PLAN_SHA=NONE
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCTION_CODE_CHANGED=NO
PR179=SEPARATE_NO_126_FIX_THERE
HARDWARE_RTC_DS3231SN=NOT_CONNECTED_NTP_ONLY
ESP_IDF_PIN=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
ACTUATOR_RELEASE=NO
```

Plan-only. Diese Revision ist eigenstaendig und vollstaendig. Es gibt keine
Produktionslogik, keine produktiven Tests und keine Toolchain-/Pin-Aenderung vor
Freigabe der exakten Plan-SHA. Der Auftrag liegt als Markdown vor
(`Issue126_SNTP_Trust_Bug_Planauftrag.md`, nicht Teil des Repository).

## 1. Ziel und Nicht-Ziele

Ziel:

```text
NTP-only Cold Boot (keine RTC, Systemzeit nie gesetzt)
    -> vor erfolgreichem Sync:   ITimeSource::unixTimeSeconds() == nullopt
    -> nach erfolgreichem Sync:  echte aktuelle trusted UTC
    -> niemals Epoche+Uptime als trusted UTC
```

Nicht-Ziele: keine zweite Uhr, keine synthetische UTC, keine historische
Jahresgrenze als Plausibilitaet, kein neues Time-State-Framework, keine neue
Callback-/Event-/Zeitqualitaets-API, keine Recovery-/Run-Neuimplementierung,
RTC bleibt optional und der DS3231SN-Pfad darf nicht regressieren, kein
Zeitzonen-/UI-Code, kein #172-Codefix, kein ESP-IDF-Pin-Bump.

## 2. Verifizierte Ausgangslage

| # | Befund | Status | Beleg |
|---|---|---|---|
| F1 | Auf Hardware ohne RTC zeigt der Header Uptime seit 1970 (01:02 bei 17:24) und `HeaderClock` meldet `Zeit vertrauenswuerdig`. | `OBSERVED` | PR #179 (Branch `agent/issue-172-pr-a-touch-navigation`), Evidence-Datei `docs/audits/PR179_HW_SMOKE_20261006_EVIDENCE.md` Abschnitt 5 @ `a36641e4ea849e1e7e3c33abadb116084215575f`; die Datei liegt nicht auf `main` und nicht auf der Basis dieses PR |
| F2 | `EspIdfSntpTimeCoordinator::initialize()` setzt `smooth_sync = true` fest; `poll()` promotet Trust bei jeder `COMPLETED`-Beobachtung, unabhaengig vom Modus. | `VERIFIED_SOURCE` | `esp_idf_sntp_time_coordinator.cpp`, `absolute_time_internal.hpp` (`SntpArbitration`) |
| F3 | `esp_netif_sntp_init` setzt nur bei `smooth_sync` den Modus auf `SMOOTH` und nie zurueck auf `IMMED`; der Default `IMMED` ist eine implizite Annahme. | `VERIFIED_SOURCE` | `components/esp_netif/lwip/esp_netif_sntp.c:75-77` |
| F4 | Im Modus `IMMED` gilt `settimeofday(server tv)` und danach `COMPLETED` in derselben Funktion. | `VERIFIED_SOURCE` | `components/lwip/apps/sntp/sntp.c:42-44` |
| F5 | Im Modus `SMOOTH` ruft `sntp_sync_time` `adjtime()`; nur bei Rueckgabe `-1` faellt es auf `settimeofday` zurueck, sonst `IN_PROGRESS`; `COMPLETED` erst, wenn `adjtime(NULL,&out)` Restkorrektur 0 meldet. | `VERIFIED_SOURCE` | `sntp.c:45-59, 86-98` |
| F6 | Der #126-Plan nimmt an, ESP-IDF mache bei sehr grosser Abweichung selbst einen Sofortschritt (`ESP_IDF_DOCUMENTED_LARGE_DELTA_IMMEDIATE_STEP=ACCEPTED_PLATFORM_BEHAVIOR`, §6.2/§6.3). | `ASSUMPTION_REFUTED_BY_SOURCE` (siehe H1/H2) | `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md` §6.2/§6.3 |

Beobachtet und als gesichert behandelt (Auftrag):

```text
NTP_ONLY_COLD_BOOT_SYSTEM_TIME_NEAR_EPOCH=OBSERVED
SNTP_PATH_PROMOTES_TIME_TO_TRUSTED=OBSERVED
TRUSTED_UTC_IS_WRONG=OBSERVED
```

### 2.1 Ursachenhypothesen (gepinnter ESP-IDF v6.1, Quelltextanalyse)

```text
ROOT_CAUSE=UNPROVEN
```

Die Root-Cause ist **nicht bewiesen**. Die Quelltextanalyse liefert zwei
unabhaengige Glieder; das erste allein erklaert die Beobachtung **nicht**.

**H1 – 32-Bit-Truncation in `adjtime()` (`VERIFIED_SOURCE`, Zielwirkung
`UNPROVEN`).** `components/esp_libc/src/time.c:193`:
`tx.offset = delta->tv_sec * 1000000L + delta->tv_usec;` mit
`struct timex::offset` als `long` (`platform_include/sys/timex.h`). Auf
Xtensa/ESP32 ist `long` voraussichtlich 32 Bit (Nachweis N1, offen). Die
Konvertierung wrappt modulo 2^32; der Bereichstest in
`esp_libc_timekeeping_adjtime_apply` (`timekeeping.c:114`, Grenze
2 146 000 000 us) sieht nur den gewrappten Wert und laesst rund 99,9 % aller
Deltas durch. Fuer das Beobachtungsdatum (Delta rund 1,7913e9 s) ergibt die
Rechnung einen gewrappten Offset von rund −1830 s (60 s Uptime) bis −2070 s
(300 s Uptime) – der Sofortschritt-Rueckfall (`adjtime == -1`) greift
praktisch nie. Upstream hat genau diese Truncation auf `master` inzwischen
korrigiert (Commit `bdf98cf0d`, „reject out-of-range adjtime() deltas
instead of overflowing“); der gepinnte Stand `v6.1` enthaelt den Fix nicht.
Allein erklaert H1 den frueh beobachteten Trust nicht: Ein Slew von rund
−1830 s bei Korrekturrate 1/64 braeuchte rund 32 h bis `COMPLETED`.

**H2 – Slew auf nie gesetzter Uhr wird still verworfen (`VERIFIED_SOURCE`,
Zielwirkung `UNPROVEN`).** `timekeeping.c:33`:
`if ((boot_time == 0) || ...) s_adjtime_start_us = 0;`. Unter
`CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER` liegt `boot_time` in
`RTC_BOOT_TIME_LOW/HIGH_REG` (`esp_time_impl.c:68-90`) und ist nach Power-on
0, solange nie `settimeofday` lief. Dann setzt der erste Lesezugriff
`adjtime(NULL,&out)` in `sntp_get_sync_status()` die Slew-Zustandsvariable
zurueck, liefert Restkorrektur 0 und damit sofort `COMPLETED`, **ohne dass
die Uhr bewegt wurde**. Das passt zur Beobachtung (Uhr = Epoche + Uptime,
Status `COMPLETED`, Trust promotet) und ist deltaunabhaengig. H2 ist auf
`master` unveraendert vorhanden.

**Arbeitsthese:** H2 verursacht den beobachteten NTP-only-Cold-Boot-Fehler,
H1 ist ein davon unabhaengiger zweiter Defekt fuer Deltas ueber rund
35,8 min (`2^31 us`). Beide verlassen sich auf dieselbe falsche
#126-Annahme (F6). Die Arbeitsthese bleibt bis zu den Nachweisen N1-N3
Hypothese.

### 2.2 Nachweise (billigster zuerst)

| ID | Nachweis | Ort | Gate |
|---|---|---|---|
| N1 | `sizeof(long)==4` und `sizeof(time_t)==8` fuer das Target `esp32` per `static_assert`/`-dM -E` mit dem Projekt-Compiler; Narrowing modulo 2^N belegt. Zusaetzlich Build-Konfigurationsnachweis fuer den getesteten ESP32-Profilbuild (`esp32_bringup`/`esp32_release`): wirksames `CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=y` bzw. die wirksame `CONFIG_LIBC_TIME_SYSCALL_*`-Auswahl, aus dem erzeugten `sdkconfig` des Profils zitiert (indikativ im Arbeitsbaum-`sdkconfig`: `ESP_TIME_FUNCS_USE_RTC_TIMER=y`, `ESP_TIME_FUNCS_USE_ESP_TIMER=y`, `LIBC_TIME_SYSCALL_USE_RTC_HRT=y`). Keine neue Kconfig-Einstellung. | off-device | kein Gate |
| N2 | Upstream-Vergleich `release/v6.1` gegen `master` fuer `time.c`/`timekeeping.c`: H1-Fix `bdf98cf0d` auf `master` vorhanden (bereits gesichtet), H2 unveraendert. Ein Pin-Bump ist eine materielle Toolchain-Aenderung und nur betrachtete Alternative (A3). | off-device | kein Gate |
| N3 | Einmalige Diagnose-Probe auf dem NTP-only-ESP32 (Bring-up-Profil, nicht Produktcode, nicht in `main` zu mergen): **nur nach echter Spannungsunterbrechung (Power-Cycle)**: Reset-Grund `ESP_RST_POWERON` (oder gleichwertig eindeutig belegter Power-on-Reset), rohes `time(nullptr)` nahe Epoche vor dem ersten Sync, `adjtime`-Rueckgabe mit echtem Delta, direkt danach `adjtime(NULL,&out)`, Status-Sequenz, `sntp_get_sync_mode()`, `time(nullptr)` nach `COMPLETED`. `EN/CHIP_PU`, `esp_restart()`, Panic/WDT und andere Resetpfade sind **kein** Cold-Boot-Beweis. Keine neue Reset-Abstraktion. | on-device | **Owner-Gate O3** |

Statusregel: `ROOT_CAUSE=PROVEN` wird nur gesetzt, wenn die Evidence die jeweils
notwendige Toolchain-/Buildkonfiguration (N1) und die Hardwarebeobachtung (N3,
Power-on-Reset, Epoche-nahe Zeit) tatsaechlich belegt; sonst bleibt
`ROOT_CAUSE=UNPROVEN`. Der Fix (C1/C2) ist
gegen beide Hypothesen robust (siehe 3.1), N3 entscheidet nur die
Formulierung in Doku und Evidence, nicht den Fix.

## 3. Fix-Design (kleinste robuste Aenderung)

Bevorzugte KISS-Richtung des Auftrags, bestaetigt: **untrusted → `IMMED`,
trusted → `SMOOTH`**, entschieden aus dem bestehenden Trust-Latch.

### 3.1 Bausteine

1. **Reine Entscheidung** in `lib/device_platform_esp_idf/src/absolute_time_internal.hpp`
   (nativ testbar, kein ESP-IDF-Include):

   ```text
   SntpSyncMode selectSntpSyncMode(bool systemTimeTrusted)
       untrusted -> Immediate, trusted -> Smooth
   ```

   Ein Projekt-Enum `SntpSyncMode` (zwei Werte) bleibt im internen Header;
   der Adapter bildet es auf `SNTP_SYNC_MODE_IMMED/SMOOTH` ab.

2. **Trust-Quelle ohne zweite Wahrheit:** neuer read-only Accessor
   `EspTimerTimeSource::absoluteTimeTrusted() const noexcept`, der den
   vorhandenen `UtcHighWaterPublicationGate::trusted()` unter dem bestehenden
   Mutex liest. **Nicht** `unixTimeSeconds()` verwenden (verschiebt als
   Nebeneffekt die High-Water-Marke). Kein Config-Bool am Koordinator.

3. **Modus explizit setzen** (nicht nur `smooth_sync=false`):
   `esp_sntp_set_sync_mode(...)` (offizieller Alias `sntp_set_sync_mode`) in
   `initialize()` **und** in `start()` aus dem dann aktuellen Trust
   (`start()` wird von #89 erneut aufgerufen). `config.smooth_sync` bleibt
   `false`; der Modus kommt ausschliesslich aus der reinen Entscheidung.

4. **Expliziter Trust-Gate (Auftrag §3).** `SntpArbitration` promotet aus
   *untrusted* nur, wenn die `COMPLETED`-Beobachtung unter `IMMED` stattfand
   (`sntp_get_sync_mode()` als Beobachtungsparameter, nur Lesen). Damit ist
   „`COMPLETED` ⇒ `settimeofday(server tv)` lief" (`sntp.c:42-44`) eine
   geprueft Invariante statt Annahme: `COMPLETED` + `SMOOTH` + untrusted ⇒
   **kein Promote** (fail-closed, auch gegen H2). Aus *trusted* bleibt das
   bestehende Verhalten (Promote ist dann ein No-op, RTC-Sync nach
   `COMPLETED` unveraendert).

5. **Nach dem ersten Promote:** Wechsel auf `SMOOTH` fuer den stuendlichen
   Resync (`CONFIG_LWIP_SNTP_UPDATE_DELAY=3600000`), weil dann die Systemzeit
   gesetzt und `boot_time != 0` ist und weil `IMMED` bei jedem
   Rueckwaertsschritt ueber das High-Water-Gate kurz `nullopt` erzeugen
   wuerde. Umsetzung als Teil derselben Entscheidung: nach `promote` wird
   `selectSntpSyncMode(true)` angewandt. Entscheidung O2.

### 3.2 Verhalten nach dem Fix

```text
Boot ohne RTC                     -> untrusted -> IMMED
Erster erfolgreicher Sync         -> settimeofday(server) -> COMPLETED
                                     -> Promote -> unixTimeSeconds() = echte UTC
                                     -> Modus SMOOTH fuer Folgesyncs
Sync fehlgeschlagen / ausstehend  -> kein COMPLETED -> kein Promote
COMPLETED unter SMOOTH, untrusted -> kein Promote (Gate)
RTC-seeded (trusted)              -> SMOOTH, unveraendert (kleine Drift)
```

### 3.3 Restrisiko (Pflichtpunkt, nicht still behoben)

Auch im RTC-seeded Pfad mit `SMOOTH` greift H1, sobald die RTC mehr als rund
35,8 min von NTP abweicht: Der Slew wird falsch dimensioniert oder in die
falsche Richtung angewandt, `COMPLETED` kommt nie bzw. spaet, die RTC wird
nicht korrigiert. Das ist **keine Regression**, sondern dieselbe falsche
Annahme (F6). Hardwarebezug: Die RTC ist aktuell nicht verbunden; das Risiko
wird erst mit angeschlossener RTC relevant. Entscheidung O1.

## 4. Betroffene Module, Vertraege, Dokumente

- `device_platform_esp_idf` (Adapter, ADR-013): `absolute_time_internal.hpp`,
  `esp_idf_sntp_time_coordinator.{hpp,cpp}`, `esp_timer_time_source.{hpp,cpp}`.
  Keine neue Abhaengigkeit, keine Aenderung an `ITimeSource` und
  `device_platform`/`fermentation_app`.
- Composition (`main/app_main.cpp`): voraussichtlich keine Aenderung; die
  Reihenfolge RTC-Seed → `sntp.initialize()` → `start()` bleibt.
- **Materielle Abweichung vom #126-Vertrag:** F6 wird widerlegt. Der
  historische #126-Plan wird nicht umgeschrieben. Anzupassen ist das
  kanonische Dokument `docs/ARCHITECTURE.md` (Zeile „NTP-Synchronisierung
  folgt der offiziellen ESP-IDF-SNTP-Semantik", ca. Zeile 406): neuer
  Satz zu Modus je Trust-Zustand und Promote-Gate. `docs/ROADMAP.md`:
  Zeile fuer Issue #181 (beim PR-Start, ohne Anforderungskopie).
- `docs/audits/PR179_HW_SMOKE_20261006_EVIDENCE.md` wird nicht geaendert
  (Evidenz); sie benennt Truncation als Ursache, was hier zur Arbeitsthese
  H2 praezisiert wird (Verweis im neuen Evidence-Dokument).

## 5. Umsetzungs- und Commit-Schnitte (nach Freigabe)

Es gilt der kanonische Workflow (`AGENTS.md`, `docs/AGENT_WORKFLOW.md`):
Ownerfreigabe der exakten Plan-SHA vor der Implementation, erneute Freigabe nur
bei materieller Abweichung. Die Schnitte C0-C3 sind Struktur und benoetigen kein
zusaetzliches Owner-Gate je Commit.

| Schnitt | Inhalt | Gezielter Nachweis |
|---|---|---|
| C0 | Nachweise N1/N2 off-device; N3 nur nach Owner-Gate O3 (Probe-Branch/-Profil, nicht Teil des Fix-PR-Codes). Ergebnis in `docs/audits/ISSUE181_SNTP_TRUST_ROOT_CAUSE_EVIDENCE.md`. | Evidence-Datei |
| C1 | `SntpSyncMode`, `selectSntpSyncMode`, Arbitration-Gate samt nativen Tests. | `test_absolute_time_internal` |
| C2 | Adapter: Accessor `absoluteTimeTrusted()`, `esp_sntp_set_sync_mode` in `initialize()`/`start()`/nach Promote, `smooth_sync=false`. | native Adaptertests wo moeglich, beide ESP-IDF-Profile bauen |
| C3 | `ARCHITECTURE.md`-Satz, ROADMAP-Aktualisierung bei Statuswirkung. | Doku-Konsistenz |

Native Tests (C1), alle gegen die reine Logik ohne ESP-IDF:

- `selectSntpSyncMode`: untrusted → Immediate, trusted → Smooth;
- NTP-only: vor Sync liefert die Publikationsgrenze `nullopt` (bestehender
  `UtcHighWaterPublicationGate`-Test bleibt gruen);
- `Completed` + `Immediate` + untrusted → Promote und RTC-Sync einmalig;
- `Completed` + `Smooth` + untrusted → **kein** Promote (neuer
  Fail-closed-Test);
- `Reset`/`InProgress`/`Other` → kein Promote, kein RTC-Write (bestehend);
- trusted + `Completed` + `Smooth` → bestehendes Verhalten (Promote No-op,
  RTC-Sync, Retry nach naechstem Sync);
- bestehende Retrograde-/High-Water-Tests unveraendert gruen.

Self-Check nach der Implementation: `bash scripts/run_pre_ready_gates.sh
self-check`; vollstaendiger Pre-Ready-Lauf nur nach Independent Review mit
`OPEN_BLOCKERS=0` und ausdruecklicher Owner-Anweisung.

## 6. Hardware-Retest (verbindlich, nach Software-Review)

Auf demselben NTP-only ESP32 (keine RTC, kein Erase):

1. **Kaltstart ausschliesslich per echter Spannungsunterbrechung (Power-Cycle).**
   Grund: Unter `ESP_TIME_FUNCS_USE_RTC_TIMER` haelt die RTC-Zeitbasis
   (`boot_time`) Resets; nur ein Power-on-Reset setzt sie zurueck. `EN/CHIP_PU`,
   `esp_restart()`, Panic/WDT und andere Resetpfade starten mit fast richtiger,
   aber untrusted Zeit und beweisen den Cold-Boot-Fall **nicht**.
2. Evidence enthaelt `ESP_RST_POWERON` (oder gleichwertig eindeutig belegten
   Power-on-Reset) und das **rohe `time(nullptr)` nahe Epoche vor dem ersten
   Sync**; „untrusted" allein genuegt nicht.
3. Vor dem Sync keine trusted UTC (`HeaderClock`: nicht vertrauenswuerdig).
4. Nach dem Sync Header-Uhr gegen unabhaengige Europe/Zurich-Referenz
   vergleichen; Abweichung maximal 1 Minute.
5. `HeaderClock` zeigt trusted + `Europe/Zurich` + dieselbe Lokalzeit.
6. Wiederholung jeweils als echter Power-Cycle (zweiter Cold-Boot-Beweis); beide
   Laeufe mit `ESP_RST_POWERON`-Beleg.
7. Keine Panic/WDT/Brownout/`heap_alloc_failed`; `ACTUATOR_RELEASE=NO`.

Danach PR #179 mit dem gemergten Fix-`main` synchronisieren und **nur** die
S4-Hardware-Fix-Verification wiederholen (kein erneuter Full Review S1-S4,
solange der Integrationsdiff lokal bleibt).

## 7. Betrachtete Alternativen

- **A1 – nur `smooth_sync=false` global:** verworfen; verliert die
  monotone Glaettung im trusted Pfad (Retrograde-`nullopt` bei Resync).
- **A2 – eigene Delta-Schwelle/Jahresgrenze:** verworfen (Auftrag: keine
  erfundene Schwelle, keine kosmetische Jahresgrenze).
- **A3 – ESP-IDF-Pin-Bump auf einen Stand mit `bdf98cf0d`:** loest nur H1,
  nicht H2; materielle Toolchain-Aenderung; nicht Teil dieses Plans.
- **A4 – Server-Zeit per Sync-Callback gegen Systemzeit pruefen:**
  zusaetzliche Callback-Abstraktion ohne belegten Bedarf; das
  Modus-Gate (3.1/4) schliesst die Luecke strukturell. Nur bei widerlegter
  Arbeitsthese neu bewerten.

## 8. Offene Owner-Entscheidungen

| ID | Frage | Empfehlung |
|---|---|---|
| O1 | Restrisiko 3.3 (RTC-seeded, Abweichung ueber 35,8 min unter `SMOOTH`): (a) akzeptieren und dokumentieren, (b) immer `IMMED` fuer den ersten Sync pro Boot (kollidiert mit der Smooth-Entscheidung aus #126 und erzeugt Retrograde-`nullopt` bei zu schneller RTC), (c) Upstream-Fix/Pin-Bump als eigenes Issue. | (a) jetzt; (c) als Follow-up-Issue bewerten, sobald die RTC angeschlossen ist |
| O2 | Nach dem ersten Promote auf `SMOOTH` wechseln oder bei `IMMED` bleiben. | Wechsel auf `SMOOTH` |
| O3 | Freigabe der Diagnose-Probe N3 auf der Hardware (Bring-up-Profil, nicht im Fix-PR). | Freigeben, ca. ein Boot-Zyklus |

## 9. Risiken

- Arbeitsthese H2 kann auf dem Geraet widerlegt werden; der Fix bleibt
  trotzdem korrekt (beide Hypothesen werden strukturell abgefangen), nur
  Doku und Evidence-Text aendern sich.
- `sntp_set_sync_mode` ist ein ungeschuetzter `volatile`-Setter; die
  Aufrufe erfolgen aus der Plattformschleife, der Sync-Pfad liest ihn im
  lwIP-Thread. Sie gelten nur zwischen Syncs (Intervall mindestens 15 s);
  die Atomaritaet eines `volatile` Enums genuegt, ein Lock wird nicht
  eingefuehrt.
- Der erste Sync pro Boot kann einen grossen Sprung vorwaerts erzeugen; das
  ist beabsichtigt (Systemzeit war nie gesetzt) und vom High-Water-Gate nicht
  betroffen, da vorher keine UTC publiziert wurde.

## 10. Akzeptanzkriterien

```text
AC1 NTP-only vor Sync: unixTimeSeconds() == nullopt
AC2 NTP-only nach erstem Sync: echte aktuelle UTC, Abweichung <= 1 min zur Referenz
AC3 Kein Trust-Promote bei COMPLETED unter SMOOTH aus untrusted
AC4 RTC-seeded Pfad: trusted, SMOOTH, RTC-Sync nach COMPLETED unveraendert
AC5 Bestehende Retrograde-/High-Water-Tests gruen
AC6 Beide ESP-IDF-Profile bauen; gezielte Static-Analysis ohne neue Befunde
AC7 Hardware-Retest 6 (echte Power-Cycles mit ESP_RST_POWERON-Beleg, zweifach) PASS, ACTUATOR_RELEASE=NO
AC8 Root-Cause-Status in Evidence korrekt (PROVEN nur mit belegtem N1-Build-Config und N3-Hardwarebeobachtung)
```
