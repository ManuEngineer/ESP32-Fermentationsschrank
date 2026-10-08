# PR #187 – Diagnose LVGL/RAM-Headroom (2026-10-07)

Reine Diagnose, **kein Produktfix**, kein Pre-Ready, kein weiterer Hardware-Smoke.
`ACTUATOR_RELEASE=NO`.

```text
PRODUCT_HEAD=fb8978f79a48c8f6741502919e059be71ae8d72d   (unveraendert)
RESOURCE_EVIDENCE_BUILD_NEW=fb8978f + probe-only instrumentation (docs/audits/PR187_RAM_DIAG_PROBE_fb8978f.patch)
RESOURCE_EVIDENCE_BUILD_BASELINE=2e81d09 + dieselbe probe-only instrumentation (PR187_RAM_DIAG_PROBE_2e81d09.patch)
BASELINE_DISPLAY=WORKING (letzte bekannte Hardwarebasis mit funktionierendem Display)
ROOT_CAUSE=PROVEN_BY_MEASUREMENT: dauerhafte Textpack-Daten im Heap (TextPackManifest/TextTranslation)
```

Der Probe-Patch fuegt ausschliesslich `logResources`/`ESP_LOGW`-Messpunkte und
eine Aufteilung von `displayRenderer->initialize()` in Messpunkt-Aufrufe hinzu
(`diag_before/after_make_renderer`, `diag_before/after_textpacks`,
`diag_before/after_display_initialize`, `sizeof`-Log). Kein Verhalten
geaendert.

## Messdaten (free_heap_bytes / minimum / groesster 8-Bit-Block)

| Messpunkt | `2e81d09` (Display OK) | `fb8978f` (Display fehlt) | Delta |
|---|---:|---:|---:|
| after_platform_begin | 170724 / 170724 / 110592 | 170724 / 170724 / 110592 | 0 |
| after_application_begin (inkl. WLAN-Start) | 73336 / 69008 / 65536 | 73236 / 69240 / 65536 | −100 |
| diag_before_make_renderer | 72016 | 72284 | +268 |
| diag_after_make_renderer | 71388 | 71404 | +16 |
| diag_before_textpacks | 71372 | 71388 | +16 |
| **diag_after_textpacks** | **46428** / 20996 / 20480 | **18564** / 16792 / 17408 | **−27864** |
| diag_before_display_initialize | 45480 / 20996 / 20480 | 17588 / 16792 / 16384 | −27892 |
| diag_after_display_initialize | 22252 / 20248 / 20480 | 7240 / 7180 / 6912 (**Allokation 12800 B fehlgeschlagen**) | — |
| after_ui_init | 21980 / 16924 / 19456 | 8212 / 7056 / 6912 | — |
| stable_home_wifi | 20112 / 16068 / 18432 | 7752 / 4696 / 6912 | −12360 |

Textpack-Groesse (gemessen): `2e81d09` 270 Translations (3 × 90), Vektor-
Kapazitaet 19440 B; `fb8978f` 549 Translations (3 × 183), 39528 B. Persistenter
Heapverbrauch von `makeFermentationUiTextPacks()` (before→after): `2e81d09`
24944 B, `fb8978f` 52824 B. Aufschluesselung `fb8978f` je Pack: 183 × 72 B
(`sizeof(TextTranslation)=72`) = 13176 B Vektor, dazu Heap-Strings fuer Schluessel
(72 von 183 > 15 Zeichen, geschaetzt ca. 2,6 kB) und Werte (ca. 1,3–1,6 kB) →
ca. 17,6 kB je Pack, ca. 52,8 kB gesamt; passt zur Messung. `sizeof` Workspace
600 → 1056 B und Gate 728 → 776 B liegen auf dem Stack und sind unerheblich.

## Zuordnung der Differenz

- Alle Schritte vor den Textpacks (Platform, Application inkl. WLAN, Renderer-
  Objekt) sind in beiden Staenden gleich (|Δ| < 0,3 kB).
- Die gesamte dauerhafte Differenz von ca. 27,9 kB entsteht in
  `makeFermentationUiTextPacks()`: die Textpacks wuchsen von 3 × 90 auf 3 × 183
  Eintraege (Issue #172 S7–S10, u. a. Seiten-, Tastatur-, Editor-, Settings-
  und Zeilentexte). Der Boot-Loop-Fix (`fb8978f`) beseitigte nur die
  zusaetzliche Kopie, nicht diese dauerhafte Groesse.
- `displayRenderer->initialize()` braucht auf der funktionierenden Basis
  ca. 23,2 kB (45480 → 22252, darunter der 12800-B-Zeichenpuffer `buf1`).
  `fb8978f` hat davor nur 17588 B frei (groesster Block 16384 B) → Fehlbetrag
  mindestens 5,6 kB nur fuer das Display; die Firmware faellt fail-closed in den
  Zustand "UI unavailable".
- Nach dem Display braucht der Normalbetrieb (WLAN, HTTP) weiteren Platz; die
  Basis hatte danach noch 20 kB frei (Minimum 16 kB).

## Kandidaten fuer den kleinsten KISS-Fix (keine Umsetzung)

Alle ohne Textentfernung, ohne LVGL-Pufferverkleinerung; die Wahl und ein
Plan-/Vertragsentscheid liegt beim Owner. Ersparnisse sind Schaetzungen aus den
Messwerten, nicht nachgemessen.

1. **Textpack-Daten flash-resident statt Heap** (`TextTranslation` haelt
   `const char*`-Sichten auf statische Tabellen statt dreier `std::string`):
   spart nahezu die gesamten ca. 52,8 kB. Groesster Effekt, aber aendert die
   Typen `TextKey`/`TextTranslation`/`TextPackManifest` in `device_platform`
   (Konsumenten: `device_ui_text.cpp`, Factory, Tests) → Vertragsaenderung.
2. **Namespace-String nicht je Translation speichern** (`TextKey.nameSpace` ist
   ein `std::string`, 24 B × 549 ≈ 13,2 kB nur fuer die immer gleiche Zeichenkette
   `"fermentation"`): spart ca. 13 kB. Deckt den Fehlbetrag (≥ 5,6 kB) allein
   ab, braucht aber ebenfalls eine Typaenderung in `TextKey`/`TextPackManifest`.
3. **Nur aktive Locale plus Englisch-Fallback resident** (2 statt 3 Packs):
   spart ca. 17,6 kB; erfordert Neuaufbau beim Sprachwechsel und aendert die
   Initialisierungsreihenfolge/Locale-Semantik.

Keiner der Kandidaten ist ohne Typ- oder Vertragsaenderung moeglich; die kleinste
Aenderung, die den Fehlbetrag rechnerisch deckt, ist Kandidat 2, die robusteste
Kandidat 1. Eine Aenderung nur in `makeFermentationUiTextPacks()` (ohne
Typaenderung) gewinnt keinen Platz mehr: der Vektor ist bereits exakt
dimensioniert (`reserve(size)`).

## Nicht verursacht durch (gemessen, ausgeschlossen)

Application/WLAN-Start, Renderer-Objekt, Workspace/Gate (Stack), Programmkatalog.

## Provenienz und Rohdaten

```text
8b9759a83ea4647d6577836473b58adaed327bc4e6bc0f89bea67318b1e68723  fb8978f-dirty probe app.bin
d6411d0a2de25ad6d1ff4f1c4f679f3f19f22427c1338e1cc38632208666b766  2e81d09-dirty probe app.bin
5d0c334662724e1be4ce4512521f0c1e5f8b8efa7146f7087fd88afee5d87606  uart_diagN.raw.txt (fb8978f+probe)
deff18e2df146b78693fb2e722419953df8a923759ee5d7681bbed774c5409b3  uart_diagB.raw.txt (2e81d09+probe)
```

Rohlogs lokal, nicht im Repository. Beide Probe-Builds wurden ohne Erase
geflasht; danach wurde das Geraet wieder mit dem exakten Produkt-Image
`fb8978f` (`evidence2/exact`) geflasht. Hardware-Smoke bleibt `BLOCKED`.
