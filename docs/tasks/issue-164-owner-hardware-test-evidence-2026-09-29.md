# Issue #164 – Owner-Hardwaretest-Evidence, 2026-09-29

## Umfang und Firmwareprovenienz

Dieser Bericht hält ausschließlich tatsächlich ausgeführte Owner-Interaktionen
und die dazugehörige UART-Diagnose fest. Er ist kein Fix und keine Freigabe für
weitere physische Tests.

```text
ISSUE=164
PR=171
PR_STATUS=OPEN_DRAFT
PR_HEAD_AT_CAPTURE=f4a62f6b8cd12c167accb7966547ea25e6d7e408
FLASH_PROFILE=esp32_release
FIRMWARE_SOURCE_SHA=e9f1f8bd81393182208421ea11fc72bfd45f7bcc
ELF_SHA256=6dd371ededc469479fb288223adac03a88c251e7457b8b7fe6bebfe186c3438d
SOURCE_TO_PR_HEAD_CHANGE=DOCS_ROADMAP_ONLY
UART_BACKTRACES=8
UART_SW_CPU_RESET=8
PRODUCT_CODE_CHANGED=NO
REBUILD=NO
REFLASH=NO
ACTUATOR_RELEASE=NO
```

Der geschützte UART-Mitschnitt wurde lokal mit Modus `0600` aufbewahrt; sein
SHA-256 ist
`f505121bb9e656e6514a49aa5b61c6681c658db4f1e333b481be84e96a60ab73`.
Der Rohmitschnitt und Zugangsdaten sind nicht Teil dieses Commits.

## Tatsächliche Owner-Beobachtungen

- Der SoftAP war aktiv; die Netzwerkseite zeigte SoftAP-Daten und WLAN-QR.
- Der Owner konnte den QR scannen und ein WLAN-Profil auf dem Telefon anlegen.
  Der Client-Join schlug fehl, nachdem das auf dem Gerät angezeigte Passwort
  nicht mehr mit dem zuvor gescannten Wert übereinstimmte.
- Der Owner bediente die Heimnetz-Auswahl. Das WLAN-Symbol änderte seine Farbe;
  ein erfolgreicher Heimnetzbeitritt wurde nicht erreicht. Versuche, auf AP
  zurückzuschalten, gingen mit wiederholten Abstürzen einher.
- Der Owner meldete zwei kurze, jeweils etwa eine Sekunde dauernde weiße
  Displayphasen mit Rückkehr zur Netzwerkseite. Ein anhaltend weißer Zustand
  wurde nicht beobachtet.
- Ein Foto zeigt die Netzwerkseite. Eine formale Owner-Bestätigung der
  vollständigen 320x240-Lesbarkeit liegt nicht vor. Zugangsdaten aus dem Foto
  werden absichtlich nicht wiedergegeben.

## UART- und Stackbefund

Offline-Symbolisierung der acht Backtraces mit dem zum Releaseprofil passenden
`esp32_release`-ELF ergibt:

| Lokale Zeit | Stackbereich |
|---|---|
| 17:41:19 | `scanGroupMetadata<4u>()` / `MetadataScanResult::records.reserve()` während der Konfigurationsvalidierung im Netzwerkmodus-Commit |
| 17:41:33, 17:41:41, 17:41:53, 17:54:55, 17:55:01 | `make_unique<ResolutionContext>()` in `ConfigurationService::confirmPreview()` während `applyNetworkMode()` |
| 17:56:11, 17:56:18 | `std::vector<ScreenDrawCommand>::push_back()` beim Erzeugen/Rendern der Netzwerkseite |

Alle acht Stacks führen über `operator new` und
`__wrap___cxa_allocate_exception()` in `cxx_exception_stubs.cpp`, wo `abort()`
aufgerufen wird. UART meldet danach jeweils `SW_CPU_RESET`. Damit ist die
Fehlerklasse „fehlgeschlagene dynamische C++-Allokation, danach Abort/Reset“
belegt. Die genaue Ursache der Allokationsverweigerung ist mit der Aufnahme
nicht weiter bestimmbar; insbesondere gibt es keine Heap-Messung exakt am
fehlgeschlagenen Allokationspunkt und keinen Nachweis, der Heap-Budget,
Fragmentierung oder Speicherbeschädigung eindeutig unterscheidet.

Vor Netzwerkseiten-Taps protokollierte Ressourcenstichproben lagen bei
8.020–9.596 Bytes freiem Heap, 5.888–7.424 Bytes größtem 8-bit-Block und
2.360–3.776 Bytes minimalem Heap. Vor dem ersten Render-Absturz betrug der
letzte Messpunkt um 17:55:33 9.500 Bytes freien Heap und 7.680 Bytes größten
8-bit-Block. Vor dem zweiten lag der Messpunkt um 17:56:14 bei 9.420 Bytes
frei und 7.680 Bytes größtem 8-bit-Block. Diese Vorher-Werte lokalisieren den
Fehler nicht weiter.

## Erklärung der QR-/Passwortabweichung

Im ausgeführten Firmwarestand erzeugt `makeNetworkConfig()` in
`main/app_main.cpp` das SoftAP-Passwort bei jedem App-Start aus 16 Zufallsbytes
und hält es nur in der flüchtigen Adapterkonfiguration. Nach jedem Reset ist
deshalb ein zuvor gescannter QR-/Passwortwert veraltet. Das erklärt den
fehlgeschlagenen Client-Join nach den Resets; es erklärt nicht die Ursache der
Allokationsfehler.

## Testmatrix und Abschlussstatus

| Prüffeld | Ergebnis |
|---|---|
| SoftAP/AP_ONLY | SoftAP im UART aktiv beobachtet; Umschalten nicht erfolgreich abgeschlossen |
| Heimnetz-Auswahl | Berührt; Symbolfarbe änderte sich, aber kein erfolgreicher WLAN-Join |
| Netzwerkseite/Touch | Teilweise ausgeführt; wiederholte Abstürze, Test danach gestoppt |
| 320x240-Lesbarkeit | Foto vorhanden; vollständige Lesbarkeit nicht als PASS bestätigt |
| SoftAP-Daten/QR-Anzeige | Auf dem Display beobachtet; Werte nicht in diesem Bericht gespeichert |
| WLAN-QR-Scan | Owner meldet erfolgreich; Profil auf Telefon angelegt |
| Client-Join | FAIL nach Reset: zuvor gescannte Zugangsdaten waren veraltet |
| Direkte AP-IP/Browser-Setup | `NOT_RUN` |
| Heimnetz-Credential-Eingabe | `NOT_RUN` |
| Test-before-Commit | `NOT_RUN` |
| Geplanter Owner-Neustart/Persistenz/Reconnect | `NOT_RUN`; unerwartete Absturz-Resets zählen nicht als dieser Test |
| Vollständige Ressourcen-Abnahmematrix | `NOT_RUN`; nur die oben genannten UART-Stichproben liegen vor |
| Aktorfreigabe | `NO` |

```text
DIAGNOSIS=ALLOCATION_FAILURE_THEN_ABORT_AND_SW_CPU_RESET
ALLOCATION_FAILURES=6_NETWORK_MODE_COMMIT_2_NETWORK_PAGE_RENDER
UNDERLYING_HEAP_CAUSE=UNRESOLVED
SOFTAP_PASSWORD_ROTATES_PER_APP_START=YES
FIXED=NO
HARDWARE_TESTS_AFTER_REPEATED_CRASHES=STOPPED
```

Keine Produktänderung, kein Build, kein Reflash und keine weitere Touch-/Reset-
Aktion erfolgten. Eine Korrektur oder weitere physische Prüfung benötigt einen
neuen konkreten Auftrag.
