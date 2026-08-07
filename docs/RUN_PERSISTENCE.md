# Laufpersistenz und Wiederherstellbarkeit

## Ziel

Ein aktiver Prozess muss nach einem vollständigen Spannungsverlust soweit
rekonstruierbar sein, wie gueltige fachliche Daten vorhanden sind. Direkte GPIO-,
MOSFET- oder H-Bruecken-Zustaende werden nie wiederhergestellt.

## Grundsaetze

- Nach jedem Boot bleiben Peltier und Aktoren zunaechst AUS.
- Resetursache, Bootschleifen, persistierte Sperren und kritische
  Speicherintegritaet werden vor `STANDBY` oder Recovery bewertet.
- Laufdaten, Sensoren, Sicherheitszustand und Wiederanlaufaktion werden vor jeder
  Freigabe validiert.
- Wichtige Ereignisse werden sofort gespeichert.
- Periodische Kontrollpunkte ergaenzen die ereignisgesteuerten Speicherungen.
- Es wird nicht in jedem Sensorzyklus geschrieben.
- Kritische Laufdaten und Sicherheitsjournale haben Vorrang vor Historien.
- Release 1 funktioniert mit 4 MB Flash ohne PSRAM.
- Release 1 reserviert keine dualen OTA-Slots.
- Ein Neustart ist kein Fehlerreset und loescht keine Persistenzsperre.

## Persistierter Laufzustand

Mindestens enthalten:

- eindeutige Lauf-ID
- unveraenderlicher Programmschnappschuss
- effektive Laufwerte und Laufrevisionen
- aktuelle Prozessphase
- Regelmodus und primaerer Regelsensor
- dokumentierte Sensorwechsel
- nominelle Dauer
- kumulierter temperaturgewichteter Fortschritt
- Verlaengerungen und Korrekturen
- letzter monotoner Zeitstand
- letzter verlaesslicher UTC-Anker, sofern vorhanden
- Zeitqualitaetsstatus
- letzte gueltige Temperaturen und Qualitaetszustaende von:
  - Schrankluft
  - Produkt, sofern vorhanden
  - Kuehlkoerper/Aussenwaermetauscher
- Zielqualifikationsfortschritt
- Kuehl- oder Haltezustand
- Abschlussverhalten
- relevante Meldungsreferenzen
- letzte Zustandsaenderung
- Revisionsnummer, Schema und Integritaetsinformation
- Transaktionsstatus fuer sicherheits- und aktorwirksame Zustandsaenderungen

Nicht gespeichert werden:

- direkte H-Brueckenpegel
- letzte Heiz- oder Kuehlfreigabe als Bootbefehl
- rohe GPIO-Zustaende
- blinde Aktor-Wiederherstellungsbefehle

## Speicherereignisse

Eine neue atomare Revision entsteht mindestens bei:

- Laufstart
- jedem Prozessphasenwechsel
- Bestaetigung `Produkt eingesetzt`
- Aenderung des primaeren Regelsensors
- laufrelevantem Sensorausfall oder validierter Rueckkehr
- manueller Laufanpassung
- automatischer Fortschrittskorrektur
- neuer Warnung oder neuem Fehler mit Laufwirkung
- Quittierung oder Reset mit Zustandswirkung
- Start oder Ende von Kuehlen und Halten
- Abbruch
- Abschluss
- Wiederanlaufentscheidung

Zusammengehoerige Aenderungen duerfen in einer atomaren Revision gebuendelt
werden. Ein Schritt, der spaeter Aktoren freigeben kann, wird erst angewendet,
nachdem Transaktionsabsicht und neue Revision erfolgreich gespeichert wurden.

## Periodische Kontrollpunkte

```text
Minimum:           1 Minute
Werkseinstellung:  5 Minuten
Maximum:          60 Minuten
```

Der Wert ist eine PIN-geschuetzte Serviceeinstellung innerhalb firmwarefester
Grenzen. Zustandswechsel werden unabhaengig davon sofort gespeichert.

Das Kontrollpunktintervall ist zugleich eine Grenze fuer die Unsicherheit des
Stromausfallzeitpunkts. Es darf nicht mit der exakten Ausfalldauer verwechselt
werden.

## Fortschrittsmodell

Die Wiederherstellung verlaesst sich nicht auf einen einzelnen Countdown.
Kombiniert gespeichert werden:

- kumulierter temperaturgewichteter Fortschritt
- nominelle Dauer
- bisherige Verlaengerungen und Korrekturen
- monotone Laufzeit
- letzter UTC-Anker
- Sensor- und Zeitqualitaet

Die biologische Wirkung wird nicht durch eine erfundene Kurve behauptet. Die
Gewichtung wird bei der Inbetriebnahme kalibriert und bleibt konservativ.

## Zeitanker und Ausfallintervall

Nach dem NTP-Abgleich ist

```text
aktuelle UTC - UTC des letzten Kontrollpunkts
```

nur die obere Grenze der moeglichen Ausfalldauer. Das Geraet kann nach dem letzten
Kontrollpunkt noch bis zum naechsten planmaessigen oder ereignisbezogenen
Speicherzeitpunkt gelaufen sein.

Mindestens zu speichern beziehungsweise abzuleiten sind:

- UTC und Zeitqualitaet des letzten Kontrollpunkts
- monotone Laufzeit an diesem Kontrollpunkt
- konfiguriertes Kontrollpunktintervall
- Zeitpunkt und Art der letzten ereignisbezogenen Speicherung
- Hinweise auf ausgelassene oder verspaetete Kontrollpunkte

Recovery berechnet:

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximal moeglicher Kontrollpunktabstand)
```

Fuehren beide Grenzen nicht zur gleichen Fortschritts- oder Phasenentscheidung,
wird kein automatischer Abschluss ausgeloest. Beide Grenzen und die Konfidenz
werden angezeigt und exportiert.

## Messhistorie

### Live-Ringpuffer im RAM

- feste Maximalgroesse
- haeufige aktuelle Werte
- fluechtig
- keine notwendige Wiederherstellungsgrundlage

### Dauerhafte Aggregate

Fuer begrenzte Zeitfenster werden kompakt gespeichert:

- Mittelwert
- Minimum
- Maximum
- Sensor- beziehungsweise Gueltigkeitsstatus

Ereignisse wie Phasenwechsel, Unterbrechungen, Warnungen, Fehler und Sensorwechsel
werden getrennt mit genauer verfuegbarer Zeit markiert.

Ein Minutenfenster ist der bevorzugte Ausgangspunkt fuer den aktuellen Lauf, aber
keine Garantie fuer unbegrenzte minutengenaue Langzeitarchivierung.

## Aufbewahrung

Werkseinstellung innerhalb eines festen Budgets:

- aktiver Lauf vollstaendig
- letzte 5 abgeschlossene Laeufe detailliert
- letzte 50 Laeufe als Zusammenfassung

Die Werte sind innerhalb fester Obergrenzen konfigurierbar. Bei Platzmangel gilt:

1. temporaere Exportdaten entfernen
2. aelteste nichtkritische Diagrammdetails verdichten oder entfernen
3. aeltere detaillierte Laeufe zu Zusammenfassungen reduzieren
4. aelteste nichtkritische Zusammenfassungen entfernen
5. Sicherheits-, Reset- und Fehlerereignisse laenger erhalten
6. aktiven Lauf und Rueckfallrevisionen niemals fuer Komfortdaten opfern

Bereinigung erfolgt proaktiv vor einer Ressourcenwarnung.

## Rueckfall bei beschaedigten Daten

Ist die neueste Revision ungueltig:

1. juengste aeltere vollstaendig gueltige Revision waehlen
2. unsicheren Zeitraum als Intervall bestimmen
3. Rueckfall sichtbar melden und protokollieren
4. Phase, Sperren und Speicherintegritaet erneut bewerten
5. nur bei eindeutig sicherer Recoveryentscheidung autonom fortsetzen

Ist kein vollstaendiger Programmschnappschuss rekonstruierbar oder koennte eine
spaetere Sicherheitsverriegelung fehlen, wird nicht geraten. Das Geraet wechselt
in einen sicheren verriegelten Datenfehlerzustand.

## Kritischer Persistenzfehler

Schlaegt ein kritischer Schreibvorgang fehl:

1. neue Aktoranforderungen sperren
2. Peltier AUS und erforderlichen Luefternachlauf ausfuehren
3. RAM-seitige Verriegelung setzen
4. minimalen Persistenzfehler-Latch in einem reservierten redundanten Bereich
   schreiben
5. Fehlerzustand anzeigen und protokollieren

Der reservierte Latch ist logisch vom normalen Laufjournal getrennt. Er liegt in
Release 1 jedoch im selben physischen ESP32-Flash und darf deshalb nicht als
unabhaengige Hardware-Redundanz bezeichnet werden.

Beim Boot gilt:

- gesetzter Latch -> `SAFE_BOOT`
- unvollstaendige Transaktionsabsicht -> `SAFE_BOOT`
- nicht les- oder schreibbarer kritischer Speicher -> `SAFE_BOOT`
- Recovery erst nach erfolgreicher Lesen-Schreiben-Integritaetspruefung
- Latch-Reset nur im Serviceablauf nach nachgewiesener Speichergesundheit

## Meldungen nach Neustart

1. Meldungshistorie laden
2. Sensoren, Zeit, Sperren und System neu pruefen
3. fruehere aktive Meldungen gegen aktuelle Ursachen bewerten
4. weiterhin aktive Meldungen wieder anzeigen
5. beseitigte Ursachen als erledigt kennzeichnen
6. historische Ereignisse erhalten

Ein Neustart laedt keine nicht mehr bestehende Warnung blind als aktiv, entfernt
aber auch keine persistierte Sicherheitsverriegelung.

## Wiederanlaufreihenfolge

```text
Boot
-> alle Ausgaenge AUS
-> Resetursache, Bootschleife und Verriegelungen pruefen
-> kritischen Speicher und Transaktionsmarker pruefen
-> aktuelle und Rueckfallrevision validieren
-> COMPLETED direkt wiederherstellen, falls zutreffend
-> aktiven Laufdatensatz auswaehlen
-> Schrankluft-, Produkt- und Kuehlkoerpersensor bewerten
-> phasenbezogene Wiederanlaufaktion bestimmen
-> Entscheidung atomar speichern
-> Regelung kontrolliert freigeben, sofern erlaubt
-> Netzwerk und NTP parallel wiederherstellen
-> Ausfallintervall und Fortschritt spaeter korrigieren
-> korrigierten Zustand atomar speichern
```

Der Wiederanlauf blockiert nicht auf NTP. Ohne absolute Zeit wird kein exakter
Unterbrechungsfortschritt erfunden und kein automatischer Phasenabschluss allein
aus einer Schaetzung abgeleitet.

## Uebergabe an ein spaeteres Vorhaben: Regelsensorauswahl bei Reaktivierung

Issue #21 (Regelsensorauswahl, Ersatzbetrieb, Rueckkehrlogik) liefert den
persistierten und den laufzeitseitigen Auswahlzustand, aktiviert einen nach
einem Neustart geladenen aktiven Lauf aber bewusst **nicht** selbst. Dieser
Abschnitt haelt fest, was bereits vorhanden ist und was ein spaeteres
Vorhaben fuer die tatsaechliche Reaktivierung noch leisten muss, damit dies
nicht erneut recherchiert werden muss.

Bereits vorhanden:

- der persistierte Auswahlzustand (Herkunft, letzte Ursache, letzte
  Laufrevision der Entscheidung) wird mit jedem aktiven Lauf gespeichert und
  beim Laden unveraendert uebernommen; ein Schema-1-Bestand ohne dieses Feld
  wird auf einen expliziten, unbestimmten Herkunftswert abgebildet, nie als
  fehlend behandelt;
- der laufzeitseitige Auswahlzustand (Phase, Peltier-Freigabe, laufende
  Wartezeiten) ist ausdruecklich ausserhalb des Wireformats und wird bei
  einem geladenen aktiven Lauf fail-closed auf den Zustand "Neubewertung nach
  Neustart erforderlich" mit gesperrter Freigabe gesetzt; jede laufende
  Wartezeit oder Rueckkehrvalidierung wird dabei verworfen;
- eine reine, seiteneffektfreie Funktion berechnet aus dem persistierten
  Auswahlzustand und dem konfigurierten Programmkontext eine Empfehlung fuer
  den reaktivierten Zustand, ohne selbst etwas zu veraendern oder zu
  speichern.

Fuer die tatsaechliche Reaktivierung (geladener aktiver Lauf -> betriebsbereit)
noch zu leisten:

- diese Empfehlung anwenden und dabei den laufzeitseitigen Auswahlzustand
  endgueltig setzen, bevor die Regelung fuer diesen Lauf ueberhaupt bewertet
  wird;
- **Reihenfolge beachten:** ein aktiver Lauf verlangt beim Schreiben
  zwingend einen vorhandenen persistierten Auswahlzustand - dieser ist nach
  dem Laden bereits vorhanden und wird durch jede nachfolgende Speicherung
  (auch periodische Kontrollpunkte) unveraendert erneut mitgefuehrt. Ein
  nicht finalisierter laufzeitseitiger Zustand (`sensorSelectionRuntime`)
  laesst dadurch fuer sich allein **keinen** periodischen Kontrollpunkt
  technisch fehlschlagen - dieser Zustand ist ausserhalb des Wireformats und
  fliesst nicht in die Schreibvoraussetzung ein. Der eigentliche Vertrag ist
  ein anderer: bleibt die Reaktivierung aus, bleibt der laufzeitseitige
  Zustand fail-closed bei "Neubewertung nach Neustart erforderlich"
  (`RestartRevalidationPending`) mit gesperrter Peltier-Freigabe
  (`Blocked`) stehen - jeder weitere Kontrollpunkt schreibt diesen
  gesperrten Zustand einfach unveraendert fort, statt zu scheitern;
- die Peltier-Freigabe bleibt bis zum Abschluss dieser Neubewertung gesperrt;
  eine vorherige Freigabe aus dem Lauf vor dem Neustart wird nie blind
  uebernommen.

Test- und Zustaendigkeitsgrenze: Issue #21 prueft ausschliesslich, dass ein
Schema-1- oder Schema-2-Bestand korrekt geladen wird, dass ein geladener
aktiver Lauf jede weitere Zustandsaenderung blockiert, und dass der
laufzeitseitige Zustand beim Laden fail-closed gesetzt wird - ausschliesslich
ueber die bestehende oeffentliche Schnittstelle, ohne Zugriff auf interne
Koordinatorzustaende. Die tatsaechliche Reaktivierungsaktion, der erste
Kontrollpunkt danach und ein anschliessender erneuter Neustart mit dann
gemischter aktueller/Rueckfallrevision sind in Issue #21 **nicht** geprueft
und bleiben einem spaeteren Vorhaben vorbehalten.

## Flashstrategie

- atomare, versionierte Revisionen
- mindestens aktuelle und letzte gueltige Revision
- Transaktionsabsicht vor aktorwirksamen Zustandsaenderungen
- reservierter minimaler Persistenzfehler-Latch
- rotierende Speicherplaetze oder Wear-Leveling
- kritischer Laufdatensatz getrennt von Messhistorie
- keine Speicherung im Zwei-Sekunden-Sensorzyklus
- kompakte Binaer- oder vergleichbar effiziente kritische Datensaetze
- definierte Reaktion bei vollem oder beschaedigtem Speicher

Die konkrete Aufteilung zwischen NVS, LittleFS und eigenen Ringstrukturen wird in
#9, #10, #19 und #29 anhand realer Build- und Hardwarewerte festgelegt.

## Ressourcenbudget

Verbindlich:

- 4 MB Flash als Planungsbasis
- keine PSRAM-Abhaengigkeit
- Single-App-Layout fuer Release 1 zulaessig
- keine OTA-Reserve fuer Release 1
- getrennte Budgets fuer:
  - Firmware
  - Webressourcen und Sprachen
  - Konfiguration
  - aktiven Lauf und Rueckfall
  - Sicherheitsjournal und Persistenzfehler-Latch
  - Historie und Zusammenfassungen
  - temporaere Exporte
- Sicherheits- und Regelungslogik haben Vorrang

Konkrete Groessen bleiben `TBD_IMPLEMENTATION_BUDGET` und werden durch die
zustaendigen Issues gemessen.
