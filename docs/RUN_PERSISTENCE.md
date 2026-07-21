# Laufpersistenz und Wiederherstellbarkeit

## Ziel

Ein aktiver Prozess muss nach einem vollständigen Spannungsverlust soweit
rekonstruierbar sein, wie gültige fachliche Daten vorhanden sind. Direkte GPIO-,
MOSFET- oder H-Brücken-Zustände werden nie wiederhergestellt.

## Grundsätze

- Nach jedem Boot bleiben Peltier und Aktoren zunächst AUS.
- Laufdaten, Sensoren, Sicherheitszustand und Wiederanlaufaktion werden vor jeder
  Freigabe validiert.
- Wichtige Ereignisse werden sofort gespeichert.
- Periodische Kontrollpunkte ergänzen die ereignisgesteuerten Speicherungen.
- Es wird nicht in jedem Sensorzyklus geschrieben.
- Kritische Laufdaten und Sicherheitsjournale haben Vorrang vor Historien.
- Release 1 funktioniert mit 4 MB Flash ohne PSRAM.
- Release 1 reserviert keine dualen OTA-Slots.

## Persistierter Laufzustand

Mindestens enthalten:

- eindeutige Lauf-ID
- unveränderlicher Programmschnappschuss
- effektive Laufwerte und Laufrevisionen
- aktuelle Prozessphase
- Regelmodus und primärer Regelsensor
- dokumentierte Sensorwechsel
- nominelle Dauer
- kumulierter temperaturgewichteter Fortschritt
- Verlängerungen und Korrekturen
- letzter monotoner Zeitstand
- letzter verlässlicher UTC-Anker, sofern vorhanden
- Zeitqualitätsstatus
- letzte gültige Temperaturen und Qualitätszustände von:
  - Schrankluft
  - Produkt, sofern vorhanden
  - Kühlkörper/Aussenwärmetauscher
- Zielqualifikationsfortschritt
- Kühl- oder Haltezustand
- Abschlussverhalten
- relevante Meldungsreferenzen
- letzte Zustandsänderung
- Revisionsnummer, Schema und Integritätsinformation

Nicht gespeichert werden:

- direkte H-Brückenpegel
- letzte Heiz- oder Kühlfreigabe als Bootbefehl
- rohe GPIO-Zustände
- blinde Aktor-Wiederherstellungsbefehle

## Speicherereignisse

Eine neue atomare Revision entsteht mindestens bei:

- Laufstart
- jedem Prozessphasenwechsel
- Bestätigung `Produkt eingesetzt`
- Änderung des primären Regelsensors
- laufrelevantem Sensorausfall oder validierter Rückkehr
- manueller Laufanpassung
- automatischer Fortschrittskorrektur
- neuer Warnung oder neuem Fehler mit Laufwirkung
- Quittierung oder Reset mit Zustandswirkung
- Start oder Ende von Kühlen und Halten
- Abbruch
- Abschluss
- Wiederanlaufentscheidung

Zusammengehörige Änderungen dürfen in einer atomaren Revision gebündelt werden.

## Periodische Kontrollpunkte

```text
Minimum:           1 Minute
Werkseinstellung:  5 Minuten
Maximum:          60 Minuten
```

Der Wert ist eine PIN-geschützte Serviceeinstellung innerhalb firmwarefester
Grenzen. Zustandswechsel werden unabhängig davon sofort gespeichert.

## Fortschrittsmodell

Die Wiederherstellung verlässt sich nicht auf einen einzelnen Countdown.
Kombiniert gespeichert werden:

- kumulierter temperaturgewichteter Fortschritt
- nominelle Dauer
- bisherige Verlängerungen und Korrekturen
- monotone Laufzeit
- letzter UTC-Anker
- Sensor- und Zeitqualität

Die biologische Wirkung wird nicht durch eine erfundene Kurve behauptet. Die
Gewichtung wird bei der Inbetriebnahme kalibriert und bleibt konservativ.

## Messhistorie

### Live-Ringpuffer im RAM

- feste Maximalgrösse
- häufige aktuelle Werte
- flüchtig
- keine notwendige Wiederherstellungsgrundlage

### Dauerhafte Aggregate

Für begrenzte Zeitfenster werden kompakt gespeichert:

- Mittelwert
- Minimum
- Maximum
- Sensor- beziehungsweise Gültigkeitsstatus

Ereignisse wie Phasenwechsel, Unterbrechungen, Warnungen, Fehler und Sensorwechsel
werden getrennt mit genauer verfügbarer Zeit markiert.

Ein Minutenfenster ist der bevorzugte Ausgangspunkt für den aktuellen Lauf, aber
keine Garantie für unbegrenzte minutengenaue Langzeitarchivierung.

## Aufbewahrung

Werkseinstellung innerhalb eines festen Budgets:

- aktiver Lauf vollständig
- letzte 5 abgeschlossene Läufe detailliert
- letzte 50 Läufe als Zusammenfassung

Die Werte sind innerhalb fester Obergrenzen konfigurierbar. Bei Platzmangel gilt:

1. temporäre Exportdaten entfernen
2. älteste nichtkritische Diagrammdetails verdichten oder entfernen
3. ältere detaillierte Läufe zu Zusammenfassungen reduzieren
4. älteste nichtkritische Zusammenfassungen entfernen
5. Sicherheits-, Reset- und Fehlerereignisse länger erhalten
6. aktiven Lauf und Rückfallrevisionen niemals für Komfortdaten opfern

Bereinigung erfolgt proaktiv vor einer Ressourcenwarnung.

## Rückfall bei beschädigten Daten

Ist die neueste Revision ungültig:

1. jüngste ältere vollständig gültige Revision wählen
2. unsicheren Zeitraum bestimmen oder kennzeichnen
3. Rückfall sichtbar melden und protokollieren
4. Prozess soweit sicher möglich autonom fortsetzen
5. Fortschritt konservativ korrigieren

Ist kein vollständiger Programmschnappschuss rekonstruierbar, wird nicht geraten.
Das Gerät wechselt in einen sicheren verriegelten Datenfehlerzustand.

## Meldungen nach Neustart

1. Meldungshistorie laden
2. Sensoren, Zeit und System neu prüfen
3. frühere aktive Meldungen gegen aktuelle Ursachen bewerten
4. weiterhin aktive Meldungen wieder anzeigen
5. beseitigte Ursachen als erledigt kennzeichnen
6. historische Ereignisse erhalten

Ein Neustart lädt keine nicht mehr bestehende Warnung blind als aktiv.

## Wiederanlaufreihenfolge

```text
Boot
-> alle Ausgänge AUS
-> Resetursache und Verriegelungen prüfen
-> aktuelle und Rückfallrevision validieren
-> Laufdatensatz auswählen
-> Schrankluft-, Produkt- und Kühlkörpersensor bewerten
-> phasenbezogene Wiederanlaufaktion bestimmen
-> Entscheidung atomar speichern
-> Regelung kontrolliert freigeben, sofern erlaubt
-> Netzwerk und NTP parallel wiederherstellen
-> Dauer und Fortschritt später korrigieren
-> korrigierten Zustand atomar speichern
```

Der Wiederanlauf blockiert nicht auf NTP. Ohne absolute Zeit wird mit niedriger
Vertrauensstufe konservativ weitergearbeitet.

## Flashstrategie

- atomare, versionierte Revisionen
- mindestens aktuelle und letzte gültige Revision
- rotierende Speicherplätze oder Wear-Leveling
- kritischer Laufdatensatz getrennt von Messhistorie
- keine Speicherung im Zwei-Sekunden-Sensorzyklus
- kompakte Binär- oder vergleichbar effiziente kritische Datensätze
- definierte Reaktion bei vollem oder beschädigtem Speicher

Die konkrete Aufteilung zwischen NVS, LittleFS und eigenen Ringstrukturen wird in
#9, #10, #19 und #29 anhand realer Build- und Hardwarewerte festgelegt.

## Ressourcenbudget

Verbindlich:

- 4 MB Flash als Planungsbasis
- keine PSRAM-Abhängigkeit
- Single-App-Layout für Release 1 zulässig
- keine OTA-Reserve für Release 1
- getrennte Budgets für:
  - Firmware
  - Webressourcen und Sprachen
  - Konfiguration
  - aktiven Lauf und Rückfall
  - Sicherheitsjournal
  - Historie und Zusammenfassungen
  - temporäre Exporte
- Sicherheits- und Regelungslogik haben Vorrang

Konkrete Grössen bleiben `TBD_IMPLEMENTATION_BUDGET` und werden durch die
zuständigen Issues gemessen.
