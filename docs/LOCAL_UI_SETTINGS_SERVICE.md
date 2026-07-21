# Lokales Menue, Einstellungen und Service

## Status

Dieses Dokument definiert Menuestruktur, Diagnose, Servicezugang,
Wiederherstellung, Touchkalibrierung und Sprachen. Sicherheits- und
Recoverykorrekturen aus PR #38 sind integriert.

## Hauptmenue

```text
Menue

[Programme verwalten]
[Einstellungen]
[Meldungen und Protokolle]
[Diagnose]
[Service]
[Systeminformationen]

[Zurueck]
```

Regeln:

- normale Funktionen stehen vor technischen Funktionen
- `Service` ist klar als geschuetzt gekennzeichnet
- `Diagnose` ist keine Aktorsteuerung
- ein laufender Prozess bleibt jederzeit erreichbar
- zerstoererische Funktionen stehen nicht im ersten normalen Menueblick

## Waehrend eines Laufes

Aenderbar bleiben nur ungefaehrliche Komfortwerte, beispielsweise:

- Displayhelligkeit und Abdunkeln
- Sprache
- zulaessige Toneinstellungen
- Stummschaltung gemaess Meldungsregeln

Gesperrt bleiben:

- Sensorzuordnung
- Regel-, Sicherheits-, Luefter- und Peltierparameter
- Direktstart-Freigaben
- Aktortests
- Touchkalibrierung
- Wiederherstellung und Werksreset
- Firmwareupdate mit Laufwirkung

Diagnose bleibt lesend verfuegbar und zeigt mindestens Temperaturen,
Sensorstatus, Regelmodus, Prozessphase, Aktoranforderung, Luefter,
Wiederanlaufstatus, Fehler, Netzwerk, Firmware und Resetursache.

## Normaler Servicebereich

Der normale Servicebereich ist durch eine vierstellige PIN geschuetzt.

Mindestens PIN-geschuetzt:

- Aktor- und Ausgangstests
- Sensorzuordnung
- technische Sensor- und Regelparameter
- Direktstart-Freigaben
- normale Wiederherstellungsaktionen
- normal aus dem Menue gestarteter vollstaendiger Werksreset
- Touchkalibrierung

Die PIN hebt keine Sensor-, Aktor-, Persistenz- oder Sicherheitspruefung auf.
Fehlversuche werden begrenzt oder verzoegert. Kritische Aktionen besitzen eine
eigene Bestaetigung.

### Eintritt und Aktortests

- Eintritt nur aus validiertem `STANDBY`
- nie aus aktivem Lauf, `FAULT` oder `SAFE_BOOT`
- Aktortests zeitlich und leistungsmassig begrenzt
- beim Verlassen alle Testausgaenge AUS
- erforderlicher Luefternachlauf bleibt aktiv

## Automatische Servicesperre

Nach Inaktivitaet wird der Servicebereich geschlossen:

1. laufenden Test abbrechen
2. Peltier und beide Richtungen AUS
3. Testausgaenge AUS
4. erforderlichen Nachlauf ausfuehren
5. Berechtigung verwerfen
6. sicheren Bildschirm anzeigen

Die genaue Zeit bleibt eine spaetere Serviceeinstellung innerhalb fester Grenzen.

## Normales Wiederherstellungsmenue

Im PIN-geschuetzten Servicebereich:

```text
Wiederherstellung

[Standardprogramme wiederherstellen]
[Benutzereinstellungen zuruecksetzen]
[Netzwerkeinstellungen loeschen]
[Touchkalibrierung zuruecksetzen]
[Vollstaendiger Werksreset]

[Zurueck]
```

### Standardprogramme

Stellt den unveraenderlichen Factory-Katalog wieder her. Benutzerprogramme und
Protokolle bleiben erhalten, sofern nicht ausdruecklich anders bestaetigt.

### Benutzereinstellungen

Vor dem Reset werden alle betroffenen Gruppen angezeigt.

### Netzwerk

Loescht WLAN- und Netzwerkkonfiguration, nicht Programme und Laufprotokolle, und
fuehrt in den Einrichtungszustand.

### Touchkalibrierung

Loescht nur Kalibrierwerte und startet beim naechsten geeigneten Zeitpunkt eine
neue Kalibrierung. Nicht waehrend eines Laufes.

### Normaler vollstaendiger Werksreset

- nur ohne Lauf
- PIN-geschuetzt
- mindestens zweistufig bestaetigt
- zeigt geloeschte und wiederhergestellte Daten
- stellt Factory-Programme und Ersteinrichtung her

## Vergessene Service-PIN

Es gibt keinen isolierten PIN-Bypass und keinen ungeschuetzten Servicezugang.
Es gibt jedoch einen **separaten PIN-unabhaengigen lokalen Vollreset**, weil die
vergessene PIN fuer ihre eigene Wiederherstellung nicht verlangt werden darf.

Verbindlicher Ablauf:

```text
Geraet einschalten oder SAFE_BOOT aktiv
-> physischen Recoveryweg bewusst ausloesen
-> alle Aktoren und beide BTS7960-Richtungen bleiben AUS
-> mehrstufige Warnung ueber vollstaendigen Datenverlust
-> lange bewusste lokale Bestaetigung
-> vollstaendigen Werksreset ausfuehren
-> Ersteinrichtung starten
```

Regeln:

- nicht ueber Web oder Netzwerk ausloesbar
- keine Aktor- oder Servicefunktionen werden freigeschaltet
- kein alleiniger PIN-Reset
- konkrete rohe Touchgeste oder andere physische Eingabe bleibt
  `TBD_HARDWARE`
- UART-Loeschen beziehungsweise Neu-Flashen bleibt letzter physischer Recoveryweg

## Touchkalibrierung

### Normaler Weg

PIN-geschuetzt im Servicebereich. Neue Werte werden erst nach erfolgreicher
Plausibilitaetspruefung gespeichert.

### Wiederherstellung bei unbrauchbarer Kalibrierung

```text
Geraet einschalten
-> Touch mindestens 10 Sekunden roh gedrueckt halten
-> Beruehrung ohne gespeicherte Kalibrierung erkennen
-> ausschliesslich Kalibrierungs-Recovery starten
-> Punkte erfassen und pruefen
-> erst danach speichern
```

- erlaubt keinen allgemeinen Servicezugang
- alle Aktoren bleiben AUS
- bei Abbruch bleiben alte beziehungsweise keine Werte aktiv
- Rohwerte und Controlleranbindung bleiben `TBD_HARDWARE`

Die Touchkalibrierungs-Recovery und der PIN-unabhaengige Vollreset muessen durch
unterschiedliche, eindeutig bestaetigte Ablaeufe getrennt sein.

## SAFE_BOOT-Oberflaeche

`SAFE_BOOT` bietet nur:

- passive Diagnose
- Fehler- und Resetjournal
- Exporte
- Netzwerkwiederherstellung ohne Aktorwirkung
- PIN-unabhaengigen Vollreset
- Hinweise zum UART-Recoveryweg

Nicht angeboten werden Servicebereich, Lueftertest, Summer, BTS7960 oder Peltier.

## Sprachen

Release 1:

1. Deutsch
2. Spanisch
3. Englisch

Zu uebersetzen sind Menues, Phasen, Startzusammenfassungen, Warnungen, Fehler,
Service, Diagnose, Wiederherstellung, Kalibrierung und Standardprogramme.
Benutzerdefinierte Namen werden nicht automatisch uebersetzt.

Technische Regeln:

- stabile Sprachschluessel statt sichtbarer Texte in der Prozesslogik
- gleicher Funktions- und Sicherheitsumfang in allen Sprachen
- Deutsch als definierter Fallback
- Sprachwechsel ohne Wirkung auf Programme oder Lauf
- Zeichensatz unterstuetzt Umlaute, `ñ` und Akzente

## Akzeptierte Entscheidungen

- [x] getrennte normale, Diagnose- und Servicebereiche
- [x] waehrend eines Laufes nur Komforteinstellungen
- [x] Diagnose waehrend eines Laufes nur lesend
- [x] normaler Servicebereich vierstellig PIN-geschuetzt
- [x] Aktortests nur aus validiertem `STANDBY`
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] automatische Servicesperre
- [x] normaler Werksreset PIN-geschuetzt
- [x] PIN-unabhaengiger lokaler Vollreset nur bei vergessener PIN
- [x] Touch-Recovery ueber rohe Beruehrung beim Boot
- [x] drei Sprachen
