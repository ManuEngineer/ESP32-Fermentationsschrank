# Funktionsanforderungen

## Allgemeine Muss-Anforderungen

- Das Geraet startet ohne automatische Prozessfortsetzung in einem sicheren,
  nicht heizenden und nicht kuehlenden Zustand.
- Firmware muss ohne angeschlossene Hardware kompilieren.
- Einstellungen werden validiert, bevor sie angewendet oder gespeichert werden.
- Alle lokalen Bedienhandlungen erhalten eine eindeutige Rueckmeldung.
- Fach- und Sicherheitslogik soll soweit sinnvoll nativ testbar sein.
- Fehlercodes und Diagnoseausgaben muessen nachvollziehbar sein.

## 1. Bedienung

Primäre lokale Bedienung über das 2,8-Zoll-Touchdisplay. Zusätzlich lokale Weboberfläche über WLAN.

Die Kernfunktionen müssen ohne Internet verfügbar sein:

- Programm auswählen
- Start
- Stopp
- aktuelle Luft- und Referenztemperatur anzeigen
- Solltemperatur anzeigen
- Phase und Status anzeigen
- Restzeit anzeigen
- Fehler anzeigen und sicher abschalten

## 2. Programme

Vier bis fünf Programmslots, über die Weboberfläche veränderbar. Die Programme sollen als Datentabelle gespeichert werden.

Vorgesehene Namen:

1. Joghurt mild
2. Joghurt stichfest
3. Milchkefir
4. Wasserkefir
5. Kombucha / Benutzerprogramm

Programmdaten siehe `config/programs.example.yaml`.

Anforderungen:

- Werte persistent im Flash speichern
- Standardprogramme im Firmwarecode als Rückfallebene
- Import/Export als JSON oder YAML über die Weboberfläche
- Änderungen während eines laufenden Programms erst beim nächsten Start verwenden

## 3. Zustandsmaschine

Mindestzustände:

- `STANDBY`
- `TEMPERING` – Zieltemperatur anfahren, je nach Istwert heizen oder kühlen
- `STABILIZING` – Zielbereich über definierte Zeit halten
- `FERMENTING` – Fermentationstimer läuft
- `COOLING` – nach Programmende aktiv abkühlen, sofern aktiviert
- `HOLDING_COLD` – Kühltemperatur halten, sofern aktiviert
- `FINISHED`
- `FAILURE`

Der Fermentationstimer startet erst, wenn die Referenztemperatur die Zieltemperatur erreicht hat und die Stabilisierungskriterien erfüllt sind.

## 4. Temperaturregelung

- Luftsensor für schnelle Regelung und absolute Sicherheitsgrenzen
- Referenzsensor für Prozessfortschritt und Timerstart
- Hysterese- oder PI-basierte zeitproportionale Regelung
- keine hochfrequente Peltier-PWM in der ersten Implementierung
- lange Schaltfenster, zum Beispiel 30–60 s
- aktive Gegenrichtung während der normalen Temperaturhaltung vermeiden; zunächst Ausschalten und passives Ausklingen bevorzugen

## 5. Heizen/Kühlen

BTS7960-Zustände:

| Zustand | RPWM | LPWM | ENABLE |
|---|---:|---:|---:|
| Aus | LOW | LOW | LOW |
| Richtung A | HIGH | LOW | HIGH |
| Richtung B | LOW | HIGH | HIGH |
| Verboten | HIGH | HIGH | beliebig |

Welche Richtung physisch Heizen bzw. Kühlen entspricht, wird beim Inbetriebnahmetest festgestellt und konfiguriert.

Richtungswechsel:

1. ENABLE LOW
2. RPWM und LPWM LOW
3. mindestens 2 s warten
4. neue Richtung setzen
5. ENABLE HIGH

## 6. Lüfter

- Innenlüfter: für gleichmässige Schranktemperatur während eines laufenden Programms
- Aussenlüfter: mindestens bei aktivem Peltier
- konfigurierbarer Nachlauf nach Peltier-Abschaltung
- bei Fehler abhängig vom Fehlerbild sicher weiterlaufen oder abschalten; Entscheidung explizit dokumentieren

## 7. Touch-Oberfläche

Vorgesehene Seiten:

1. **Standby/Programmauswahl**
2. **Programmübersicht vor Start**
3. **Laufender Prozess** mit Temperaturen, Sollwert, Phase und Restzeit
4. **Fertig**
5. **Fehlerseite**
6. **Netzwerk-/Serviceinformationen**

Touchflächen gross genug für Fingerbedienung. Resistiven Touch nach Einbau kalibrieren und Kalibrierwerte persistent speichern.

## 8. Weboberfläche

- responsive für Smartphone
- lokales Dashboard
- Programmtabelle bearbeiten
- Temperaturverlauf anzeigen
- Start/Stopp
- Netzwerkstatus und IP-Adresse
- OTA-Firmwareupdate
- optional eigener Access Point, wenn kein bekanntes WLAN erreichbar ist

## 9. Persistenz

Zu speichern:

- Programmtabelle
- Sensorrollen und DS18B20-ROM-Adressen
- Touchkalibrierung
- WLAN-Konfiguration
- Regelparameter
- letzte Fehlerursache

Keine automatische Fortsetzung eines laufenden Heiz-/Kühlprogramms nach unkontrolliertem Stromausfall, solange keine explizite und sichere Wiederanlauflogik implementiert ist.

## 10. Fehlerbehandlung

Mindestens folgende Fehler erkennen:

- Luftsensor fehlt oder liefert CRC-/Plausibilitätsfehler
- Referenzsensor fehlt oder liefert CRC-/Plausibilitätsfehler
- Lufttemperatur über Sicherheitsgrenze
- Zieltemperatur wird innerhalb maximaler Anfahrzeit nicht erreicht
- widersprüchliche H-Brückenanforderung
- Konfigurationsdatei defekt
- Watchdog-/Software-Neustart

Bei `FAILURE`:

- Peltier sofort aus
- Richtungssignale LOW
- Fehler gut sichtbar auf Display und Webseite
- keine automatische Wiederaufnahme ohne Quittierung bzw. Neustart nach behobenem Fehler

## 11. Nicht-Ziele der aktuellen Integrationsstufe

- keine eigentliche Fermentations-, Temperatur- oder Zeitsteuerung
- keine produktive Ansteuerung von Peltier, BTS7960 oder Lueftern
- keine Uebernahme von GPIO-Kandidaten in Firmware
- keine fertige Displayoberflaeche, Web-API, OTA- oder Persistenzimplementierung
- keine Cloud-Anbindung

`src/main.cpp` bleibt bis zur realen Hardwareverifikation ein minimaler,
nicht blockierender Build- und serieller Hardwaretest-Einstieg ohne
Aktor-GPIO-Konfiguration.

## 12. Akzeptanzkriterien

- [x] `pio run` erfolgreich
- [x] `pio test -e native` erfolgreich
- [x] keine endgueltige GPIO-Zuordnung in der Firmware
- [x] sicherer Einstieg konfiguriert keine unbekannten Ausgaenge
- [ ] sichere Ausgangszustaende am realen Board gemessen
- [ ] Sensorfehler fuehrt nach Implementierung zur Abschaltung
- [ ] Neustart fuehrt nicht zu unbeabsichtigtem Start
- [ ] Dokumentation entspricht dem realen Aufbau
