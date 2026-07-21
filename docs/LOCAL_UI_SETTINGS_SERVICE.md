# Lokales Menue, Einstellungen und Service

## Status

Dieses Dokument ergaenzt [`LOCAL_UI.md`](LOCAL_UI.md) um die in Phase 4D
akzeptierten Regeln fuer Menuestruktur, Einstellungen, Diagnose, Servicezugang,
Wiederherstellung, Touchkalibrierung und Sprachen.

Die genaue visuelle Gruppierung des Hauptmenues darf spaeter anhand eines
Bedienprototyps angepasst werden. Die hier festgelegte fachliche Trennung und die
Zugriffsregeln bleiben davon unberuehrt.

## Hauptmenue

Vorgesehene erste Struktur:

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

- Die Funktionen bleiben fachlich getrennt, auch wenn einzelne Punkte spaeter in
  der Darstellung zusammengefasst werden.
- Haeufige normale Funktionen stehen vor technischen Funktionen.
- `Service` ist deutlich als geschuetzter Bereich gekennzeichnet.
- `Diagnose` darf nicht mit direkten Aktortests verwechselt werden.
- Ein laufender Prozess bleibt jederzeit ueber einen klaren Rueckweg erreichbar.
- Gefaehrliche oder zerstoererische Funktionen erscheinen nicht im ersten
  normalen Menueblick.

Die endgueltige Gruppierung wird nach einem ersten Displayprototyp erneut
geprueft.

## Einstellungen waehrend eines laufenden Prozesses

Waerend eines laufenden Prozesses bleiben nur ungefaehrliche
Komforteinstellungen aenderbar.

Beispiele fuer zulaessige Aenderungen:

- Displayhelligkeit
- Zeit bis zum Abdunkeln
- Sprache der Bedienoberflaeche
- akustische Lautstaerke, sofern die Hardware eine Regelung erlaubt
- Stummschaltung einer aktuellen Meldung gemaess Meldungsregeln

Gesperrt bleiben insbesondere:

- gespeicherte Programmdaten und Werkseinstellungen
- Sensorzuordnung
- Regel- und Sicherheitsparameter
- Direktstart-Freigaben
- Aktortests
- Touchkalibrierung
- Wiederherstellungsfunktionen und Werksreset
- Firmwareupdate, sofern es den laufenden Prozess beeinflussen kann

Aenderungen, welche den aktiven Programmschnappschuss betreffen, erfolgen nur
ueber ausdruecklich vorgesehene Laufaktionen. Ein normales Einstellungsmenue darf
den laufenden Prozess nicht unbemerkt veraendern.

## Diagnose waehrend eines laufenden Prozesses

Die Diagnose bleibt waehrend eines Laufes als reine Leseansicht verfuegbar.

Mindestens sichtbar sein duerfen:

- Produkt- und Schranklufttemperatur samt Sensorstatus
- aktiver Regelmodus und primaerer Regelsensor
- Sollwert und aktuelle Prozessphase
- Peltieranforderung und Totzeitstatus
- Zustand von Innen- und Aussenluefter
- Laufzeit-, Netzwerkzeit- und Wiederanlaufstatus
- aktive Warnungen und Fehlercodes
- WLAN- und Netzwerkstatus
- Softwareversion und Resetursache

Nicht zulaessig waehrend eines Laufes sind:

- direkte Aktortests
- Aenderung der Sensorzuordnung
- Kalibrierungs- oder Ausgangstests
- veraendernde Serviceaktionen

## Servicebereich mit verpflichtender PIN

Der Servicebereich ist immer durch eine vierstellige PIN geschuetzt.

Die PIN-Abfrage ist erforderlich fuer mindestens:

- direkte Aktor- und Ausgangstests
- Sensorzuordnung und technische Sensoreinstellungen
- Freigabe eines Direktstarts ohne normale Startzusammenfassung
- technische Regelparameter, soweit spaeter freigegeben
- Wiederherstellungsfunktionen
- vollstaendigen Werksreset
- Touchkalibrierung aus dem Servicebereich

Regeln:

- Die PIN darf nicht offen im normalen Einstellungsmenue angezeigt werden.
- Fehlversuche werden begrenzt beziehungsweise zeitlich verzoegert, damit nicht
  unbegrenzt schnell geraten werden kann.
- Die PIN-Eingabe verwendet einen ausreichend grossen Ziffernblock.
- Ein korrekter PIN-Eintrag hebt keine Sensor-, Aktor- oder Sicherheitspruefung
  auf.
- Kritische Aktionen benoetigen trotz geoeffnetem Servicebereich eine eigene
  Bestaetigung.
- Die Behandlung einer vergessenen Service-PIN wird in
  `SETTINGS_AND_STORAGE.md` festgelegt. Sie darf keinen ungeschuetzten Zugang zu
  Aktortests ermoeglichen.

## Automatische Servicesperre

Nach einer einstellbaren Inaktivitaetszeit wird der Servicebereich automatisch
verlassen und wieder gesperrt.

Beim automatischen oder manuellen Verlassen gilt:

1. laufende Servicetests werden beendet
2. Peltierausgaenge werden sicher deaktiviert
3. schaltbare Testausgaenge werden ausgeschaltet
4. erforderlicher Luefternachlauf wird ausgefuehrt
5. die Serviceberechtigung wird verworfen
6. das Geraet kehrt in einen sicheren Bildschirm zurueck

Die konkrete Zeitspanne bleibt bis zur spaeteren Einstellungsphase offen.

## Wiederherstellungsmenue

Im geschuetzten Servicebereich werden getrennte Wiederherstellungsaktionen und
ein vollstaendiger Werksreset angeboten.

```text
Wiederherstellung

[Standardprogramme wiederherstellen]
[Benutzereinstellungen zuruecksetzen]
[Netzwerkeinstellungen loeschen]
[Touchkalibrierung zuruecksetzen]
[Vollstaendiger Werksreset]

[Zurueck]
```

### Standardprogramme wiederherstellen

- stellt fehlende oder geloeschte Standardprogramme aus dem unveraenderlichen
  Factory-Katalog wieder her
- setzt nur nach ausdruecklicher Auswahl auch veraenderte Standardprogramme auf
  Werkseinstellungen zurueck
- veraendert Benutzerprogramme und Protokolle nicht

### Benutzereinstellungen zuruecksetzen

Der genaue Umfang wird in `SETTINGS_AND_STORAGE.md` festgelegt. Vor der
Bestaetigung wird eine Liste der betroffenen Einstellungsgruppen angezeigt.

### Netzwerkeinstellungen loeschen

- entfernt gespeicherte WLAN-Zugangsdaten und zugehoerige Netzwerkkonfiguration
- veraendert Programme und Laufprotokolle nicht
- fuehrt anschliessend in einen definierten Netzwerk-Einrichtungszustand

### Touchkalibrierung zuruecksetzen

- loescht nur die gespeicherten Kalibrierwerte
- startet beim naechsten geeigneten Zeitpunkt den Kalibrierungsablauf
- darf nicht waehrend eines laufenden Prozesses ausgefuehrt werden

### Vollstaendiger Werksreset

Mindestregeln:

- nur ohne laufenden Prozess
- PIN-geschuetzter Servicezugang
- mindestens zweistufige Bestaetigung
- klare Auflistung aller geloeschten beziehungsweise wiederhergestellten Daten
- Wiederherstellung der vier Standardprogramme
- Rueckkehr in einen sicheren Erst-Einrichtungsablauf

Ob Benutzerprogramme, Protokolle, Sprache, PIN und weitere Daten beim
vollstaendigen Reset geloescht werden, wird in `SETTINGS_AND_STORAGE.md`
verbindlich festgelegt.

## Touchkalibrierung

### Normaler Weg

Die Touchkalibrierung ist im PIN-geschuetzten Servicebereich erreichbar.
Kalibrierwerte werden erst gespeichert, nachdem alle Kalibrierpunkte erfolgreich
geprueft wurden.

### Wiederherstellungsweg bei unbrauchbarer Kalibrierung

Eine fehlerhafte Kalibrierung darf den Benutzer nicht dauerhaft vom Geraet
aussperren.

Vorgesehener Ablauf:

```text
Geraet einschalten
  -> Touch waehrend des Starts mindestens 10 Sekunden gedrueckt halten
  -> Rohberuehrung ohne vorhandene Kalibrierwerte erkennen
  -> geschuetzten Kalibrierungs-Wiederherstellungsmodus starten
  -> Kalibrierpunkte erfassen
  -> Plausibilitaetspruefung
  -> neue Werte erst nach erfolgreicher Bestaetigung speichern
```

Regeln:

- Die Erkennung des langen Tastendrucks darf nicht von den gespeicherten
  Kalibrierwerten abhaengen.
- Der Wiederherstellungsmodus erlaubt nur die Touchkalibrierung und keinen
  allgemeinen ungeschuetzten Servicezugang.
- Wird die Kalibrierung abgebrochen oder ist sie unplausibel, werden keine neuen
  fehlerhaften Werte gespeichert.
- Alle Aktoren bleiben waehrend dieses Ablaufs sicher AUS.
- Der genaue Rohwertbereich und die Touchcontroller-Anbindung werden erst nach
  der Hardwareverifikation festgelegt.

## Sprachen im ersten Release

Das erste Release unterstuetzt:

1. Deutsch
2. Spanisch
3. Englisch

### Umfang der Uebersetzung

Zu uebersetzen sind mindestens:

- Haupt- und Untermenues
- Phasen- und Statusbezeichnungen
- Startzusammenfassungen
- Warnungen, Fehler und Handlungsanweisungen
- Service- und Diagnosebeschriftungen
- Wiederherstellungs- und Bestaetigungsdialoge
- Einrichtungs- und Kalibrierungsablaeufe
- die Namen und Beschreibungen der mitgelieferten Standardprogramme

Benutzerdefinierte Programmnamen und Notizen werden nicht automatisch uebersetzt.

### Technische Regeln

- Bedienungstexte werden ausserhalb der eigentlichen Prozesslogik verwaltet.
- Die Programmlogik verwendet stabile Sprachschluessel und keine Vergleiche mit
  sichtbaren Texten.
- Alle drei Sprachen muessen dieselben Funktionen und Sicherheitsinformationen
  anbieten.
- Fehlende Uebersetzungen duerfen keine leere oder unverstaendliche
  Sicherheitsmeldung erzeugen.
- Als definierter Fallback wird Deutsch verwendet, bis spaeter ein anderer
  Fallback beschlossen wird.
- Die Sprache kann im normalen Einstellungsmenue gewechselt werden, auch waehrend
  eines laufenden Prozesses.
- Ein Sprachwechsel veraendert keine Programme, Sensorwerte oder Laufzustaende.
- Zeichenkodierung und verwendete Schriftarten muessen deutsche, spanische und
  englische Zeichen einschliesslich Umlaute, `ñ` und Akzente korrekt darstellen.

Die konkrete Speicherung, die Werkssprache und die Sprachauswahl bei der ersten
Einrichtung werden in `SETTINGS_AND_STORAGE.md` vervollstaendigt.

## Akzeptierte Entscheidungen aus Phase 4D

- [x] getrennte erste Hauptmenuepunkte; spaetere visuelle Zusammenfassung bleibt
      moeglich
- [x] waehrend eines Laufes nur ungefaehrliche Komforteinstellungen aenderbar
- [x] Diagnose waehrend eines Laufes nur lesend
- [x] Servicebereich immer durch vierstellige PIN geschuetzt
- [x] Servicebereich sperrt sich nach Inaktivitaet automatisch
- [x] beim Verlassen des Servicebereichs werden Testausgaenge sicher deaktiviert
- [x] einzelne Wiederherstellungsfunktionen plus vollstaendiger Werksreset
- [x] Touchkalibrierung normal im Servicebereich
- [x] Touch-Wiederherstellung durch mindestens 10 Sekunden Rohberuehrung beim Boot
- [x] Deutsch, Spanisch und Englisch im ersten Release
- [x] sichtbare Texte werden von der Prozesslogik getrennt und uebersetzbar
      aufgebaut

## Noch offen fuer spaetere Phasen

- konkrete PIN-Erstvergabe und Wiederherstellung bei vergessener PIN
- genaue automatische Sperrzeit des Servicebereichs
- genauer Datenumfang jeder Wiederherstellungsaktion
- Speichermodell der Sprachdateien und Texte
- Werkssprache und Ablauf der ersten Sprachauswahl
- konkrete Schriftarten und Schriftgroessen
- endgueltige Menuegruppierung nach Displayprototyp
- technische Erkennung des zehnsekundigen Roh-Touchs am konkreten Controller
