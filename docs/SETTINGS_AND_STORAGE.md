# Einstellungen und Persistenz

## Status

Dieses Dokument beschreibt die in Phase 6A akzeptierten Regeln fuer
Konfigurationsebenen, Aenderungsrechte, Speichervorgaenge, Validierung,
Neustartbedarf und Zeitzone.

Die Persistenz eines laufenden Prozesses, Speicherzeitpunkte, Stromausfall-
Wiederherstellung, Geheimnisse, Sicherungen und Datenaufbewahrung werden in den
Phasen 6B und 6C ergaenzt.

## Grundsaetze

- Werkseinstellungen, Benutzereinstellungen und aktive Laufdaten werden
  fachlich getrennt.
- Ein bereits gestarteter Lauf verwendet einen unveraenderlichen
  Programmschnappschuss.
- Keine teilweise geschriebene Konfiguration darf als gueltig geladen werden.
- Neue Werte werden vor der dauerhaften Uebernahme vollstaendig validiert.
- Die letzte nachweislich gueltige Konfiguration bleibt als Rueckfall erhalten.
- Eine Service-PIN hebt keine firmwarefesten Sicherheitsgrenzen auf.
- Ein laufender Prozess wird niemals wegen einer normalen Einstellung
  automatisch neu gestartet.

## Konfigurations- und Datenebenen

Das Geraet verwendet drei klar getrennte Ebenen:

```text
unveraenderliche Werkseinstellungen
        ↓
gespeicherte und validierte Benutzereinstellungen
        ↓
unveraenderlicher Schnappschuss des laufenden Prozesses
```

### 1. Unveraenderliche Werkseinstellungen

Die Firmware beziehungsweise ein geschuetzter Factory-Katalog enthaelt:

- Standardwerte der allgemeinen Geraetekonfiguration
- die vier mitgelieferten Standardprogramme
- feste Schema-Versionen und Migrationsgrundlagen
- firmwarefeste Sicherheitsgrenzen
- nicht erlaubte Aktorkombinationen
- minimale Schutzzeiten, die durch Einstellungen nicht unterschritten werden
  duerfen

Werkseinstellungen werden im normalen Betrieb nicht ueberschrieben. Ein
Zuruecksetzen stellt daraus eine neue gueltige Benutzerkonfiguration her.

### 2. Gespeicherte Benutzereinstellungen

Diese Ebene enthaelt die vom Benutzer oder Service bewusst gespeicherten Werte,
beispielsweise:

- Sprache des lokalen Displays
- Geraetename
- Display- und Toneinstellungen
- WLAN- und Webzugangskonfiguration
- Programme und Benutzervoreinstellungen
- freigegebene Serviceparameter
- Touchkalibrierung
- Zeitzone

Jede gespeicherte Konfiguration besitzt mindestens:

- Schema-Version
- Konfigurationsrevision beziehungsweise Generation
- Gueltigkeitskennzeichen oder vergleichbare Integritaetsinformation
- Zeitpunkt der Speicherung, soweit eine verlaessliche Zeitquelle vorhanden ist
- Quelle der Aenderung, beispielsweise `display`, `web` oder `service_web`

Die konkrete Speichertechnik, beispielsweise NVS, Dateisystem oder eine
Kombination, wird erst im Implementierungsentwurf festgelegt. Die fachlichen
Anforderungen gelten unabhaengig davon.

### 3. Unveraenderlicher Laufschnappschuss

Beim Start eines Programms wird ein eigener Laufschnappschuss erzeugt. Er
enthaelt alle fuer diesen Lauf wirksamen Werte, mindestens:

- Programm-ID und Programmrevision
- sichtbarer Programmname zum Startzeitpunkt
- Zieltemperatur und Fermentationsdauer
- Vorheizverhalten
- Sensorbetrieb und Sensorfehlerstrategie
- Zielqualifikationsparameter
- maximale Zielerreichungszeit
- Abschluss- und Kuehlverhalten
- zum Lauf gehoerende freigegebene Regelparameter
- Sprache unabhaengige maschinenlesbare Codes

Spaetere Aenderungen oder das Loeschen des Quellprogramms veraendern diesen
Schnappschuss nicht.

Der Schnappschuss ist ein Ausfuehrungsdatensatz und keine weitere frei
bearbeitbare Konfigurationsebene.

## Einstellungsgruppen und Aenderungsrechte

### Normale Einstellungen

Ohne Service-PIN aenderbar sind mindestens:

- Sprache des lokalen Displays
- Geraetename
- Displayhelligkeit
- Zeit bis zum Abdunkeln
- zulaessige Toneinstellungen
- Zeitzone
- WLAN-Zugangsdaten und normale Netzwerkeinrichtung
- normales Webpasswort und bewusste Deaktivierung des Webpasswortschutzes
- Benutzerprogramme und freigegebene Programmwerte

Auch normale Einstellungen koennen bei weitreichenden Auswirkungen eine
zusaetzliche Bestaetigung verlangen, beispielsweise:

- WLAN-Konfiguration ersetzen
- Webpasswortschutz deaktivieren
- ein Programm loeschen

### PIN-geschuetzte Serviceeinstellungen

Die vierstellige Service-PIN ist mindestens erforderlich fuer:

- Sensorzuordnung und technische Sensoroptionen
- statische IPv4-Konfiguration
- vertrauenswuerdige Proxy-Adressen oder Proxy-Netze
- Direktstart-Freigaben
- Touchkalibrierung aus dem normalen Serviceweg
- technische Regelparameter innerhalb freigegebener Grenzen
- Luefter- und Nachlaufparameter, soweit spaeter freigegeben
- Peltier-Totzeit innerhalb sicherer Grenzen
- Wiederherstellungsfunktionen und vollstaendigen Werksreset

### Firmwarefeste Grenzen

Nicht durch normale Einstellungen und nicht durch die Service-PIN aenderbar
sind mindestens:

- absolute Temperatur-Sicherheitsgrenzen
- unzulaessige gleichzeitige Aktoransteuerungen
- firmwarefeste Mindesttotzeit vor einer Peltier-Polaritaetsumkehr
- Bedingungen, unter denen Heizen oder Kuehlen zwingend gesperrt werden
- Ausgangszustand waehrend Boot, Reset und schwerem Fehler
- grundlegende Sensor-Plausibilitaetsanforderungen
- Integritaets- und Validierungsregeln fuer gespeicherte Daten

Aenderungen solcher Grenzen erfordern eine neue gepruefte Firmwareversion und
koennen nicht ueber die normale Bedienoberflaeche erfolgen.

## Technische Parameter innerhalb sicherer Grenzen

Technische Werte duerfen im Servicebereich veraendert werden, aber nur innerhalb
firmwarefest definierter Bereiche.

Beispielprinzip:

```text
firmwarefeste Mindesttotzeit: 3 s
freigegebener Servicebereich: 5 bis 120 s
gespeicherter Benutzerwert:   10 s
```

Dabei gilt:

- Die Firmware validiert den Wert unabhaengig von der Benutzeroberflaeche.
- Eine manipulierte Webanfrage darf keinen Wert ausserhalb des gueltigen Bereichs
  speichern.
- Ein fehlender oder ungueltiger Wert fuehrt nicht zu einer unsicheren
  Standardannahme.
- Wo kein sicherer Ersatzwert eindeutig ist, wird die betroffene Funktion
  gesperrt und ein Konfigurationsfehler gemeldet.
- Werkseinstellungen liegen ebenfalls innerhalb der firmwarefesten Grenzen.

Konkrete Zahlenwerte werden in den Phasen 7 und 8 beziehungsweise bei der
Inbetriebnahme festgelegt.

## Bearbeiten, Vorschau und Speichern

### Grundverhalten

Aenderungen werden zunaechst als Entwurf behandelt. Dauerhaft wirksam werden sie
erst nach einer bewussten Aktion `Speichern` oder einer gleichwertigen
Bestaetigung.

Ungefaehrliche und sinnvoll ruecksetzbare Werte duerfen bereits waehrend der
Bearbeitung als Vorschau wirken.

Beispiel:

```text
Displayhelligkeit aendern
  -> Vorschau sofort sichtbar
  -> Speichern uebernimmt den Wert dauerhaft
  -> Abbrechen stellt den vorherigen Wert wieder her
```

Geeignete Vorschauwerte koennen sein:

- Displayhelligkeit
- akustische Lautstaerke, sofern technisch regelbar
- Sprache der gerade verwendeten Oberflaeche
- Darstellungseinstellungen ohne Prozesswirkung

Nicht als ungespeicherte Vorschau aktiviert werden insbesondere:

- WLAN-Zugangsdaten
- statische IP-Konfiguration
- Sensorzuordnung
- Regelparameter
- Sicherheitsnahe Serviceparameter
- Proxy-Vertrauen
- Programmaenderungen
- Wiederherstellungsaktionen

### Abbrechen und Verlassen

Bei ungespeicherten Aenderungen gilt:

- `Abbrechen` stellt Vorschauwerte auf den vorherigen gespeicherten Zustand
  zurueck.
- Beim Verlassen einer Seite wird auf ungespeicherte Aenderungen hingewiesen.
- Ein Browserabbruch oder Verbindungsverlust speichert keinen Entwurf
  automatisch.
- Ein Entwurf auf einer Oberflaeche sperrt andere Oberflaechen nicht pauschal.
- Revisionsschutz verhindert das stille Ueberschreiben neuerer Daten.

## Verhalten waehrend eines laufenden Prozesses

Die Wirksamkeit von Aenderungen richtet sich nach der Einstellungsgruppe.

### Sofort zulaessig

Ohne Aenderung des Laufschnappschusses duerfen insbesondere sofort wirksam
werden:

- Sprache der Bedienoberflaeche
- Displayhelligkeit und Abdunkelverhalten
- zulaessige Toneinstellungen
- Stummschalten gemaess Meldungsregeln
- manuelles Neuverbinden des WLANs

### Nach Pruefung und bewusster Bestaetigung

Eine neue WLAN-Konfiguration darf waehrend eines Laufes vorbereitet und geprueft
werden, sofern:

- die Temperaturregelung davon unabhaengig weiterlaeuft,
- die bestehende funktionierende Konfiguration bis zur erfolgreichen Pruefung
  erhalten bleibt,
- ein Verbindungswechsel keine Aktoren oder Laufwerte veraendert,
- der Benutzer den Wechsel ausdruecklich bestaetigt.

### Nur fuer zukuenftige Laeufe

Gespeicherte Programme duerfen waehrend eines Laufes nur dann bearbeitet werden,
wenn klar angezeigt wird, dass die Aenderung ausschliesslich zukuenftige Laeufe
betrifft. Der aktive Laufschnappschuss bleibt unveraendert.

### Waehrend eines Laufes gesperrt

Gesperrt bleiben mindestens:

- Sensorzuordnung
- Regel- und Sicherheitsparameter
- technische Luefter- und Peltierparameter
- Touchkalibrierung
- Aktortests
- Wiederherstellungsfunktionen und Werksreset
- Einstellungen, die einen Neustart der Steuerung erfordern

Aktive Laufwerte koennen nur ueber spaeter ausdruecklich spezifizierte
Laufaktionen geaendert werden. Eine normale Einstellungsseite darf den laufenden
Prozess nicht still veraendern.

## Einstellungen mit Neustartbedarf

Eine Einstellung kann nach erfolgreichem Speichern als `Neustart erforderlich`
gekennzeichnet werden.

Verbindliche Regeln:

- Nach dem Speichern erfolgt kein automatischer Neustart.
- Waehrend eines laufenden Prozesses wird niemals automatisch neu gestartet.
- Die Oberflaeche zeigt dauerhaft, welche Aenderungen noch nicht aktiv sind.
- `Jetzt neu starten` wird nur in einem sicheren Zustand ohne laufenden Prozess
  angeboten.
- Ein spaeterer normaler Neustart aktiviert die gespeicherte neue Konfiguration,
  sofern sie weiterhin gueltig ist.
- Bis zum Neustart arbeitet die Firmware mit dem zuletzt aktivierten gueltigen
  Wert weiter.
- Ein erzwungener oder unerwarteter Neustart darf keine halb angewendete
  Konfiguration erzeugen.

Ob einzelne Aenderungen bereits als ausstehende Konfiguration gespeichert werden
oder erst im sicheren Zustand gespeichert werden duerfen, wird feldbezogen im
spaeteren Datenmodell festgelegt.

## Zeitbasis, Zeitzone und Darstellung

### Interne Zeitbasis

Absolute Zeitstempel werden intern in UTC gespeichert, sofern eine verlaessliche
absolute Zeit verfuegbar ist.

Laufdauern und Schutzzeiten verwenden monotone Laufzeitinformationen und duerfen
nicht durch Sommerzeitwechsel oder eine spaetere NTP-Korrektur rueckwaerts
springen.

### Konfigurierbare Zeitzone

Das Geraet besitzt eine konfigurierbare IANA-Zeitzone.

Werkseinstellung:

```text
Europe/Zurich
```

Die Zeitzone bestimmt mindestens:

- lokale Anzeige auf dem Touchdisplay
- lokale Uhrzeiten in Meldungen und Protokollen
- lokale Zeitdarstellung in Diagnoseansichten
- Standarddarstellung in der API, soweit dort lokale Werte zusaetzlich zu UTC
  angeboten werden

Browser duerfen Datums- und Zeitwerte entsprechend ihrer Sprache formatieren,
ohne die gespeicherten UTC-Zeitstempel zu veraendern.

### Unsichere Zeit

Ist NTP nach einem Start noch nicht verfuegbar:

- wird keine erfundene absolute Uhrzeit als verlaesslich ausgegeben,
- relative Laufzeit kann weitergefuehrt werden,
- Zeitstempel werden als noch nicht synchronisiert gekennzeichnet,
- nach spaeterer Synchronisation werden neue absolute Zeitinformationen
  entsprechend markiert,
- bereits protokollierte relative Reihenfolgen bleiben erhalten.

## Validierung und atomare Speicherung

### Speichervorgang

Ein dauerhafter Speichervorgang folgt logisch mindestens diesen Schritten:

```text
1. Entwurf entgegennehmen
2. Datentypen und Pflichtfelder pruefen
3. Wertebereiche und Abhaengigkeiten pruefen
4. firmwarefeste Sicherheitsgrenzen pruefen
5. neue vollstaendige Revision in einen getrennten Bereich schreiben
6. Integritaet der geschriebenen Revision pruefen
7. neue Revision atomar als aktiv markieren
8. vorherige gueltige Revision als Rueckfall behalten
```

Eine Implementierung darf diese Schritte technisch anders abbilden, muss aber
dasselbe Fehlerverhalten sicherstellen.

### Anforderungen

- Kein Feld wird einzeln als bereits gueltige Gesamtfassung betrachtet, wenn der
  restliche Datensatz noch nicht geschrieben ist.
- Stromausfall waehrend des Schreibens laesst entweder die alte oder die neue
  vollstaendige Revision gueltig, niemals eine Mischung.
- Nach dem Laden wird die Integritaet erneut geprueft.
- Unbekannte Schema-Versionen werden nicht blind interpretiert.
- Migrationen arbeiten auf einer Kopie und ersetzen die alte Revision erst nach
  erfolgreicher Validierung.
- Die letzte nachweislich gueltige Revision bleibt als Rueckfall erhalten.
- Ein Rueckfall wird sichtbar protokolliert und als Warnung angezeigt.
- Sind weder aktuelle noch letzte gueltige Benutzerdaten nutzbar, startet das
  Geraet nicht mit unsicheren Annahmen, sondern in einem sicheren
  Konfigurationsfehler- beziehungsweise Einrichtungszustand.

## Akzeptierte Entscheidungen aus Phase 6A

- [x] getrennte Werkseinstellungen, Benutzereinstellungen und Laufschnappschuesse
- [x] Werkseinstellungen bleiben unveraenderlich wiederherstellbar
- [x] ungefaehrliche Vorschau moeglich, dauerhafte Uebernahme erst mit `Speichern`
- [x] drei Berechtigungsebenen: normal, PIN-Service und firmwarefest
- [x] technische Servicewerte nur innerhalb firmwarefester Grenzen
- [x] feldbezogenes Verhalten waehrend eines laufenden Prozesses
- [x] Programmaenderungen betreffen nur zukuenftige Laeufe
- [x] kein automatischer Neustart nach Einstellungsanderungen
- [x] ausstehender Neustart wird sichtbar gekennzeichnet
- [x] interne UTC-Zeitstempel und konfigurierbare IANA-Zeitzone
- [x] `Europe/Zurich` als Werkseinstellung
- [x] vollstaendige Validierung vor Aktivierung
- [x] atomare Speicherung mit letzter gueltiger Rueckfallrevision

## Noch offen fuer Phase 6B und 6C

- genauer Inhalt des persistierten Laufzustands
- Speicherzeitpunkte waehrend eines laufenden Prozesses
- Behandlung von Flash-Verschleiss und Schreibfrequenz
- Wiederherstellung bei Stromausfall waehrend eines Speichervorgangs
- Speicherung von Messreihen und Diagrammdaten
- Aufbewahrungsdauer von Laeufen, Meldungen und Ereignissen
- Speicherung und Schutz von Passwoertern, PINs und Tokens
- Sicherungs- und Exportformat fuer Benutzerkonfiguration und Programme
- Import, Schema-Migration und Versionskompatibilitaet
- genauer Umfang einzelner Reset- und Wiederherstellungsaktionen
- Verhalten bei vollstaendig erschoepftem oder beschaedigtem Speicher
