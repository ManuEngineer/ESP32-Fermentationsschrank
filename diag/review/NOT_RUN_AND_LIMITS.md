# Evidenzgrenzen / unveränderte Gates

Hardware-Preflight/Flash/UART/Touch/HTTP/Last/Recovery: NOT_RUN. Keine aktuelle
Geräteidentität, Kalibrierung oder Partition über UART/ROM behauptet. OS-USB-
Enumeration allein ist kein Hardwarequalifikationsnachweis. Kein Pre-Ready.
Software-Review PASS/GO auf d35ee85 ist Ownerbericht; G1-Overlay bisher nur Builder-
Self-Check, kein unabhängiger G1-Ergebnisreview und kein Hardware-PASS.

Messung arbeitet mit allokationsfreien Checkpoints/festen Aggregaten, nicht mit
allokierendem IDF-local-min-Bookkeeping. Zwischenpunkte/temporäre Strings/LVGL-
Spitzen können fehlen; paralleles WLAN/HTTP und Reporting beeinflussen Heap/Zeit.
Vorläufige Schwelle bleibt PROVISIONAL_PENDING_G1. Maximaler Stichprobenbedarf
liefert nur eine Untergrenze, keine allgemeine OOM-Sicherheit. Fehlende echte
Pfad-/Zustands-/Sprach-/Text-/Touch-/Hold-/Spitzen-Evidence hält Qualifikation HOLD.
Kein künstlicher Speicherdruck, Gate-Bypass oder simuliertes Touch-Hardware-PASS.

Overlay ggü. reinem Produkt: DRAM BSS +1368 B, DRAM initialisierte Daten +84 B,
IRAM Text +244 B, Flash Text +2972 B, Flash RoData +736 B (ELF-Sektionen).
Diese belegte statische Reserve sowie zusätzliche feste Probe-Stackframes und
Log-/Messlaufzeit verfälschen den Vergleich zum reinen Produkt. Zusätzlicher
worst-case Stackbedarf ist statisch nicht exakt qualifiziert; realen Main/HTTPD-
HWM messen, Grenzwerte gelten. Keine Umrechnung des Diagrammheaps in TX-Puffer.
Bootmin5228 B und fünf lange GETs>3s/max5431.081ms aus G0 bleiben Befunde;
8s Login/3s GET sind Socketoperationsbudgets, keine Gesamtdauer-Grenzen.

Erste Gate-Anfrage deckt EINEN P-A/Recovery-Lauf plus bestätigte reale Referenz-
Bedienfälle ab. Restliche diagnostische Matrix/Pfade OPEN_G1, reine Produkt-
bestätigung OPEN_G2; insgesamt P-A3x20min/P-A2/P-B/P-R unverändert offen.
Keine automatische Folgephase. RES-01=BLOCKER, PR203_FINAL_GO=HOLD,
G2=NOT_AUTHORIZED, ACTUATOR_RELEASE=NO.
