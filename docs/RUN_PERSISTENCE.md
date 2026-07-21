# Laufpersistenz und Wiederherstellbarkeit

## Status

Dieses Dokument beschreibt die in Phase 6B akzeptierten Regeln fuer den
persistierten Laufzustand, Speicherzeitpunkte, Kontrollpunkte,
Temperaturhistorie und den sicheren Wiederanlauf nach Stromausfall.

Es ergaenzt [`SETTINGS_AND_STORAGE.md`](SETTINGS_AND_STORAGE.md) und
[`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md).

## Grundsaetze

- Ein laufender Prozess muss nach vollstaendigem Spannungsverlust rekonstruierbar
  sein, soweit die gespeicherten Daten gueltig sind.
- Direkte Ausgangsbefehle werden nicht als wiederherzustellender Zustand
  gespeichert.
- Nach jedem Boot bleiben Peltier und weitere Ausgaenge zunaechst sicher AUS.
- Aktoranforderungen werden erst nach Validierung von Laufdaten, Sensoren,
  Sicherheitsbedingungen und Wiederanlaufaktion neu abgeleitet.
- Es wird nicht bei jedem Sensorzyklus oder jede Sekunde in den Flash geschrieben.
- Speicherverbrauch und Flash-Verschleiss sind verbindliche Entwurfsgrenzen.
- Keine Funktion des ersten Releases darf PSRAM voraussetzen.

## Vollstaendiger Wiederherstellungsdatensatz

Der persistierte Laufzustand enthaelt mindestens:

- eindeutige Lauf-ID
- unveraenderlichen Programmschnappschuss
- aktuelle interne Prozessphase
- benutzerverstaendlichen Laufstatus als ableitbare Information
- aktuell wirksamen Regelmodus
- primaeren Regelsensor und dokumentierte Sensorwechsel
- kumulierten temperaturgewichteten Fermentationsfortschritt
- nominelle Fermentationsdauer
- bisherige Laufzeitverlaengerungen und Korrekturen
- letzten monotonen Laufzeitstand
- letzten verlaesslichen UTC-Zeitanker, falls vorhanden
- letzten bekannten Zeitqualitaetsstatus
- letzte gueltige Produkt- und Schranklufttemperatur samt Zeitbezug
- Zielqualifikationsfortschritt, soweit fuer die aktuelle Phase relevant
- Kuehl- oder Haltezustand
- Abschlussverhalten
- aktive beziehungsweise noch zu bewertende Meldungsreferenzen
- letzte relevante Zustandsaenderung
- Revisionsnummer und Integritaetsinformation des Kontrollpunkts

Nicht als Wiederherstellungszustand gespeichert werden:

- direkte H-Bruecken-Pegel
- gespeicherte Heiz- oder Kuehlfreigaben ohne erneute Sicherheitspruefung
- rohe GPIO-Zustaende
- ein Befehl, einen Aktor nach dem Boot blind wieder einzuschalten

## Speicherung bei wichtigen Ereignissen

Ein neuer Laufzustand wird unmittelbar beziehungsweise ohne absichtliche lange
Verzoegerung gespeichert bei mindestens:

- Start eines neuen Laufes
- jedem internen Zustandswechsel
- Bestaetigung `Produkt eingesetzt`
- Wechsel des primaeren Regelsensors
- Ausfall oder Rueckkehr eines Sensors, soweit laufrelevant
- automatischer oder manueller Laufzeitkorrektur
- neuer Warnung oder neuem Fehler mit Einfluss auf den Lauf
- Quittierung oder Benutzeraktion mit Zustandswirkung
- Start oder Ende einer Kuehlphase
- Beginn oder Ende eines Kuehlhaltens
- manuellem Abbruch
- Programmabschluss
- automatischer Wiederanlaufentscheidung

Mehrere unmittelbar zusammengehoerende Aenderungen duerfen in einer einzigen
atomaren Revision zusammengefasst werden. Dabei darf kein sicherheitsrelevanter
Zwischenzustand verloren gehen.

## Periodische Kontrollpunkte

Zusaetzlich zu ereignisgesteuerten Speicherungen wird waehrend eines aktiven
Laufes periodisch ein Kontrollpunkt angelegt.

Festgelegter Einstellbereich:

```text
Minimum:            1 Minute
Werkseinstellung:   5 Minuten
Maximum:           60 Minuten
```

Der Wert ist eine PIN-geschuetzte Serviceeinstellung innerhalb dieser
firmwarefesten Grenzen.

Hinweise in der Bedienoberflaeche:

- Ein kuerzeres Intervall reduziert den moeglichen Fortschrittsverlust nach einem
  ploetzlichen Stromausfall, erhoeht aber Schreibaktivitaet und Speicherbelastung.
- Ein laengeres Intervall reduziert Schreibaktivitaet, vergroessert aber die
  Unsicherheit zwischen dem letzten Kontrollpunkt und dem Stromausfall.
- Zustandswechsel und andere wichtige Ereignisse werden unabhaengig von diesem
  Intervall sofort gespeichert.
- Ein Wert von 60 Minuten bedeutet nicht, dass eine Zustandsaenderung bis zu einer
  Stunde ungespeichert bleiben darf.

## Mehrfach gespeicherter Fermentationsfortschritt

Die Wiederherstellung stuetzt sich nicht auf einen einzigen Restzeitwert.
Gespeichert werden kombiniert:

```text
- kumulierter temperaturgewichteter Fortschritt
- nominelle Fermentationsdauer
- bisherige Verlaengerungen und Korrekturen
- letzter monotoner Laufzeitstand
- letzter verlaesslicher UTC-Zeitanker
```

Dadurch kann die Firmware Inkonsistenzen erkennen und muss weder einer absoluten
Uhrzeit noch einem einzelnen Countdown blind vertrauen.

Die genaue mathematische Berechnung des temperaturgewichteten Fortschritts wird
in `TEMPERATURE_CONTROL.md` und anhand praktischer Versuche festgelegt. Die
Persistenz speichert nur die dafuer notwendigen stabilen Zwischenergebnisse und
Eingangsinformationen.

## Temperatur- und Diagrammdaten

### Live-Daten im RAM

Aktuelle Sensorwerte duerfen fuer die Live-Anzeige in einem begrenzten
Ringpuffer im RAM gehalten werden. Dieser Puffer ist fluechtig und wird nach
Stromausfall nicht als Wiederherstellungsgrundlage vorausgesetzt.

Der RAM-Puffer besitzt eine feste Maximalgroesse. Er darf nicht mit der Laufdauer
unbegrenzt wachsen.

### Dauerhafte verdichtete Messdaten

Dauerhaft werden keine unbeschraenkten Rohmessreihen gespeichert. Stattdessen
werden fuer definierte Zeitfenster kompakte Aggregate abgelegt:

- Mittelwert
- Minimum
- Maximum
- Gueltigkeits- beziehungsweise Sensorstatus

Zustandswechsel, Stromunterbrechungen, Warnungen, Fehler und Sensorwechsel werden
mit ihrem genauen verfuegbaren Zeitpunkt separat markiert.

Das Zielbild lautet:

```text
Live-Anzeige:         haeufige aktuelle Messwerte im begrenzten RAM-Ringpuffer
Dauerhafte Historie:  verdichtete Zeitfenster mit Mittelwert, Minimum, Maximum
Ereignisse:           getrennte genaue Markierungen
```

Ein Minutenfenster ist der bevorzugte Ausgangspunkt fuer den aktuellen Lauf,
aber kein Recht auf unbegrenzte minutengenaue Langzeitarchivierung. Vor der
Implementierung wird ein verbindliches Speicherbudget erstellt. Abhaengig von
Flashgroesse, Partitionsplan, maximaler Laufdauer und Aufbewahrung koennen
laengere historische Daten nach einem dokumentierten Verfahren weiter
verdichtet werden.

Verbindliche Ressourcengrenzen:

- Messdaten werden kompakt und nicht als wiederholte ausfuehrliche JSON-Dokumente
  im Flash gespeichert.
- Die Anzahl dauerhaft gespeicherter Messpunkte ist begrenzt.
- Historische Laeufe duerfen exportiert, zusammengefasst oder nach einer spaeter
  definierten Aufbewahrungsregel entfernt werden.
- Der aktive Lauf und seine sichere Wiederherstellung haben Vorrang vor alten
  Diagrammdaten.
- Voller oder beschaedigter Historienspeicher darf die Temperaturregelung nicht
  zum Absturz bringen.
- Ein Speicherengpass wird fruehzeitig gemeldet und fuehrt zu kontrollierter
  Reduktion nichtkritischer Historie, nicht zum blinden Verlust des aktiven
  Wiederherstellungsdatensatzes.

Die konkrete Fensterlaenge, Datensatzgroesse, Ringpuffergroesse und Anzahl
aufbewahrter Laeufe werden nach Verifikation der tatsaechlichen Modulvariante und
des Partitionsplans festgelegt.

## Meldungen nach einem Neustart

Nach dem Laden eines Laufes werden Meldungen nicht einfach unveraendert als aktiv
uebernommen.

Vorgesehener Ablauf:

1. bisherige Meldungshistorie laden
2. Sensoren, Zeitstatus und Systemzustand neu pruefen
3. jede zuvor aktive Meldung anhand der aktuellen Ursache neu bewerten
4. weiterhin aktive Meldungen wieder aktiv anzeigen
5. beseitigte Ursachen als erledigt kennzeichnen
6. historisch wichtige Ereignisse, beispielsweise einen Sensorwechsel, im
   Protokoll erhalten

Damit bleibt die Historie nachvollziehbar, ohne eine nicht mehr bestehende
Warnungsursache faelschlich als aktuell darzustellen.

## Rueckfall bei beschaedigtem Kontrollpunkt

Ist der neueste Kontrollpunkt ungueltig oder beschaedigt:

1. auf den juengsten aelteren vollstaendig gueltigen Kontrollpunkt zurueckfallen
2. die verlorene beziehungsweise unsichere Zeitspanne bestimmen oder als unsicher
   kennzeichnen
3. die Unsicherheit sichtbar melden und protokollieren
4. den Prozess soweit sicher moeglich autonom fortsetzen
5. Laufzeit und Fortschritt konservativ korrigieren

Ein Rueckfall geschieht niemals still.

Ist auch der aeltere Datensatz unvollstaendig oder der Programmschnappschuss nicht
sicher rekonstruierbar, wird nicht geraten. Das Geraet wechselt in einen sicheren
Konfigurations- beziehungsweise Laufdatenfehlerzustand. Die genaue Fehlerklasse
wird in `SAFETY_AND_FAULTS.md` festgelegt.

## Reihenfolge des automatischen Wiederanlaufs

Verbindlicher Ablauf:

```text
Boot
  -> alle Ausgaenge sicher AUS
  -> aktuelle Laufrevision und Rueckfallrevision validieren
  -> geeigneten Laufdatensatz auswaehlen
  -> Sensoren und grundlegende Sicherheit pruefen
  -> phasenbezogene Wiederanlaufaktion bestimmen
  -> Wiederanlaufentscheidung als neue atomare Revision speichern
  -> erforderliche Regelung kontrolliert freigeben
  -> Netzwerk und NTP parallel wiederherstellen
  -> Unterbrechungsdauer und Laufzeitkorrektur nachtragen
  -> korrigierten Zustand erneut atomar speichern
```

Die Aktoren werden somit nicht zuerst gestartet und erst nachtraeglich
dokumentiert.

## Flash-Verschleiss und Speicherstrategie

Die Implementierung muss eine schreibverteilende beziehungsweise journalartige
Strategie verwenden. Ein einzelner fester Flashsektor darf nicht fuer jeden
Kontrollpunkt immer wieder vollstaendig geloescht werden.

Mindestanforderungen:

- keine Speicherung bei jedem Sensorzyklus
- atomare, versionierte Kontrollpunkte
- mindestens aktuelle und letzte gueltige Revision
- rotierende Speicherplaetze oder eine geeignete Wear-Leveling-Schicht
- begrenzte und nachvollziehbare Schreibfrequenz
- Messhistorie getrennt vom kritischen Wiederherstellungsdatensatz
- definierte Reaktion bei knappem oder beschaedigtem Speicher

Die konkrete Auswahl zwischen NVS, LittleFS, einer eigenen Ringstruktur oder
einer Kombination wird erst nach dem Partitions- und Ressourcenentwurf getroffen.

## Ressourcenbudget fuer den vorhandenen ESP32

Vor Beginn der Implementierung wird die tatsaechliche Modulvariante anhand der
Beschriftung beziehungsweise Flash-Erkennung verifiziert.

Bis dahin wird konservativ geplant:

- Zielplattform ist die kleinste plausible Variante mit 4 MB Flash.
- Es wird keine vorhandene PSRAM vorausgesetzt.
- Firmware, Weboberflaeche, OTA-Reserve, Konfiguration, aktiver Laufzustand und
  Messhistorie erhalten getrennte Speicherbudgets.
- Features werden nicht allein deshalb implementiert, weil sie auf einer groesseren
  Modulvariante gerade noch Platz finden.
- Historie und Webressourcen werden begrenzt beziehungsweise komprimiert; die
  Sicherheits- und Regelungslogik hat Vorrang.
- Vor der eigentlichen Programmierung wird der geplante Partitionsplan samt
  maximalem Flash- und RAM-Budget dokumentiert und mit einer Test-Firmware
  gemessen.

## Akzeptierte Entscheidungen aus Phase 6B

- [x] vollstaendiger Wiederherstellungsdatensatz statt minimaler Laufkennung
- [x] keine direkte Ausgangsansteuerung als wiederherzustellenden Zustand speichern
- [x] sofortige Speicherung wichtiger Ereignisse plus periodische Kontrollpunkte
- [x] Kontrollpunktintervall im Servicebereich von 1 bis 60 Minuten
- [x] Werkseinstellung fuer Kontrollpunkte 5 Minuten
- [x] Fortschritt aus mehreren unabhaengig pruefbaren Informationen rekonstruieren
- [x] Live-Messwerte im begrenzten RAM-Ringpuffer
- [x] dauerhafte Messhistorie als kompakte Mittelwert-/Minimum-/Maximum-Aggregate
- [x] exakte Ereignismarkierungen getrennt von Messaggregaten
- [x] Meldungshistorie laden und aktive Meldungen danach neu bewerten
- [x] sichtbarer Rueckfall auf den letzten gueltigen Kontrollpunkt
- [x] Wiederanlaufentscheidung vor Aktorfreigabe atomar speichern
- [x] konservative Planung fuer 4 MB Flash ohne vorausgesetzte PSRAM
- [x] verbindlicher Partitions- und Ressourcenentwurf vor Implementierungsbeginn

## Noch offen fuer Phase 6C und spaeter

- genaue Flashgroesse und PSRAM-Ausstattung der gelieferten Modulvariante
- konkreter Partitionsplan einschliesslich OTA-Strategie
- genaue Datensatzgroesse der Kontrollpunkte
- konkrete Wear-Leveling- beziehungsweise Journaltechnik
- konkrete Messfensterlaenge und Langzeitverdichtung
- Anzahl lokal aufbewahrter abgeschlossener Laeufe
- Export, Sicherung und Import von Laufhistorien
- Verhalten bei vollstaendig erschoepftem oder physisch beschaedigtem Speicher
- Speicherung und Schutz von Passwoertern, PINs und Tokens
