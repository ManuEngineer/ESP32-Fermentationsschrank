# PR #203 – G2-Web B0–B4: zusammenhängender Browser-Funktionstest

**Ownerauftrag und einmalige Ausführungsfreigabe.** Codex soll den nachfolgenden **einen** B0–B4-Lauf koordinieren, ausführen, auswerten und die bereinigte Evidence auf PR #203 veröffentlichen. **Keine Freigabefragen zwischen erfolgreichen Phasen.** Benötigte manuelle Chrome-/Touch-Handlungen beim Owner kurz und konkret anfordern.

## Fester Umfang

- Produkt-PR-Stand bei Auftragserteilung: `0049af07db2d7287955842ef70936fb70eac6862`; installierte Softwarequelle: `a9a1156a1d44d4679957526bbc34a30be6422e4e` (`esp32_release`). Unveränderter geprüfter Diagnosemonitor: `893f3bb73a50b484ce79b3b80722873452d998fc`, `g2-trend`, Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01`.
- **Genau eine kontrollierte UART-Portöffnung** für Live-Bootnachweis und kontinuierliche Überwachung ist freigegeben. Bei vollständigem Nachweis von Quelle, `ready`, deaktivierten Aktoren, Release-Profil, `HomeConnected` und neuen Heartbeats anschliessend ohne Unterbruch B0–B4.
- Browser ist **Windows-Chrome des Owners**, nicht Selkies. Ein ESP32-Tab, normales WLAN-/LAN-Setup, keine künstliche Last, keine Mutationen/Programmstarts/Werksresets. Aktoren bleiben physisch getrennt und softwareseitig deaktiviert.
- **Kein** Neubuild, Flash, Erase, Rollback, zusätzlicher Reset, WLAN-/TCP-/HTTPD-Konfigurationswechsel oder Produkt-/Monitorcode-Fix. Keine Wiederholung des bereits abgeschlossenen isolierten Erstabruf-Diagnoseauftrags.

## Aufzeichnung zuerst, dann Navigation

Codex und Owner prüfen **vor B1**, dass die Dateien tatsächlich auf dem Windows-Rechner angelegt werden können:

1. In Chrome `chrome://net-export` in einem **separaten Tab** öffnen, `Start Logging to Disk` wählen, Dateipfad bestätigen. NetLog läuft über **alle Phasen einschliesslich der Ruhephase**; nur den **ESP32-Tab** für B3 schliessen. Keine sensitiven Rohdaten in GitHub.
2. DevTools → Network: Aufzeichnung aktiv, **Preserve log** aktiv; Cache-/Netzwerkbedingungen unverändert. Einmal den **HAR-Export mit Inhalt** an einem unkritischen Testeintrag üben, ohne vorher den ESP32 abzurufen. Sicherstellen, dass HAR gespeichert werden kann. Bei Problemen mit einer der Aufzeichnungen **vor dem eigentlichen Teststart** beheben; keine Fehlversuche ohne gesicherten Trace.
3. Host-UTC/monotone Referenz, Windows-Chrome-Version und Browserzeit sowie vorhandene UniFi-Signal-/Retrywerte notieren. Bekannte andere ESP32-Clients und Tabs schliessen; unbekannte Clients als Einschränkung kennzeichnen. Passwörter, Cookies, Tokens und komplette HAR/NetLogs ausschliesslich privat speichern.

## Ein zusammenhängender Lauf

| Phase | Durchführung / Evidence |
|---|---|
| **B0** | Live-Bootnachweis und durchgehende UART-Überwachung; Browser-Trace und Zeitmarken bereit. |
| **B1** | Einmalige Navigation zu `/`, vollständigen HTML-Abschluss und tatsächliche Ladezeit erfassen; normal anmelden (keine Secrets in Berichte); Zustand *Online/Bereit* bzw. erwartete fehlende Sensoren dokumentieren. Anonyme/abgelaufene Session-401 getrennt von echten Fehlern bewerten. |
| **B2** | **15 Minuten** angemeldete Weboberfläche mit normalem 5-s-Polling, ohne Mutationen. Messpunkte **0/5/10/15 min**: HTTP-Status, vollständiger JSON-/Bodyabschluss, Requeststarts/-enden, Fehler, ausstehende/überlappende Gruppen, Sichtbarkeit/Frischeanzeige, Latenzen und relevante UART-/UniFi-Daten. Die Single-Flight-Eigenschaft am **authentifizierten** Pfad prüfen. |
| **B3** | **Zuerst HAR B0–B2 dauerhaft speichern und Datei kontrollieren.** ESP32-Tab schliessen; Chrome/NetLog darf offen bleiben. **20 Minuten** kein ESP32-HTTP, kein Ping und keine aktive Erreichbarkeitsprobe. UTC-Marken für Schliessen/Start/Ende; vorhandene UniFi-Daten rein passiv bei 0/10/20 min. |
| **B4** | Nach 20 Minuten genau **einen** neuen ESP32-Tab öffnen. Ersten Dokumentrequest samt Transferabschluss/Fehlerklasse sichern; falls erforderlich normal neu anmelden. Bei Erfolg **10 Minuten** normales authentifiziertes Polling mit Messpunkten **0/5/10 min**. Zum Schluss **B4-HAR exportieren**, Datei prüfen, **NetLog stoppen und Datei prüfen**, dann UART geordnet beenden. |

## Fehlerbehandlung – Evidence nicht zerstören

- **Echte Geräte-/Safety-Anomalie** (unerwarteter Reset, Panic/Abort, Watchdog, Brownout, Heap-Allokationsfehler, Stackoverflow oder anderer Monitor-Hard-Stop): sofort Last/weitere Browseraktionen stoppen, Rohdaten sichern, **FAIL/HOLD**. Kein automatisch zweiter Hardwarelauf, Flash oder Rollback.
- **Reiner Browser-/HTTP-/Transportfehler ohne Geräte-Safety-Anomalie:** Abnahme der betroffenen Phase als **FAIL_OBSERVED** markieren. **Tab und DevTools nicht sofort schliessen.** Zuerst vollständige Fehlerklasse, fehlgeschlagenen Request, Console/Screenshot, HAR **auf Platte** und UTC-Zeit sichern. Danach ist **genau ein bewusst ausgelöster zweiter Seitenaufruf als begrenzte Erholungsdiagnose** genehmigt; seinen Verlauf im selben Trace erfassen und erneut exportieren. Keine dritte Navigation oder automatische Wiederholungsserie. Der zweite Erfolg macht den ersten Fehler **nicht** zu PASS. Anschliessend Lauf geordnet beenden; ausgefallene B-Phasen `NOT_RUN` belassen.
- Erwartete Ausnahmen: nur der bekannte `/favicon.ico`-404 und eindeutig erwartete anonyme/abgelaufene Session-401; kein stillschweigendes Ausnehmen anderer 4xx/5xx, Bodyabbrüche oder Timeouts.
- Keine verändernden Reparaturmassnahmen während des Versuchs. Bei einer organisatorischen Verzögerung vor B0 darf Codex auf den Owner warten, ohne ein zusätzliches Freigabegate zu eröffnen.

## Abschluss / Reviewübergabe

- Private Originale (B0–B2-HAR, B4-HAR, NetLog, UART/Chunks, Screenshots und gegebenenfalls Fehler-HAR) mit Hash/Grösse und eindeutigen Zeitmarken erhalten; öffentlich nur allowlist-bereinigte Request-/Zeit-/Status-/Fehler-/Socket- und Ressourcen-Metadaten.
- Im PR-Bericht **für B0–B4 je PASS / FAIL / NOT_RUN / NOT_PROVEN** ausweisen und Fehlerzeiten mit Browser-/UART-/UniFi-Daten korrelieren, ohne Ursachen zu erfinden. Erfolgreiche API-Header **allein** genügen nicht: Body-/JSON-Abschluss und Polling-Serialisierung beurteilen. Bereits historische FAIL/HOLD-Ergebnisse nicht rückwirkend überschreiben.
- **Keine** vorsorgliche Firmwarekorrektur, kein zweiter Testauftrag und kein neuer Full Review: Ergebnisse zur **unabhängigen Ergebnisprüfung** übergeben. Auch bei erfolgreichem B0–B4 bleiben G1/F4-/Sensor-/Aktor-Gesamtqualifikationen separat offen.

**Ownerfreigabe dieses Auftrags:** `G2_B0_B4_ONE_RUN=AUTHORIZED` · `FLASH=NO` · `ACTUATOR_RELEASE=NO` · `PR203_FINAL_GO=HOLD` bis zur unabhängigen Abnahme.
