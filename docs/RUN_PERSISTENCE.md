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
- ehrliche Fortschrittsbasis und, nur bei freigegebenem Modell, kumulierter
  temperaturgewichteter Fortschritt
- Verlaengerungen und Korrekturen
- letzter monotoner Zeitstand
- letzter verlaesslicher UTC-Anker, sofern vorhanden
- Zeitqualitaetsstatus
- Recovery-Episode mit Vor-/Nach-Ausfall-Evidenz und Segmentkennung
- ausstehender Recovery-Zeitanker, Boot-Anker und getaggte Vor-Boot-Zeit
- kumulative wirksame nominale Zeitkorrektur mit Episodenrevision
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
- fluechtige `ControlRequest`-Sequenzen, PI-Integratorwerte,
  Feedbackfenster, `lastActiveDirection` oder Qualifikator-Episoden

Nach Recovery beginnt der #22-PI-/Qualifikatorzustand leer. Eine alte
`qualificationValidSinceMillis`-Markierung bleibt ausschliesslich ein
Prozess-/Diagnosemarker und erzeugt keinen Qualifikationskredit; die bestehende
Recovery-Rebase aus `QUALIFYING_TARGET` nach `REACHING_TARGET` bleibt bestehen.

Der Qualifikatorzustand bleibt fluechtig und wird nicht persistiert. Seine
Evaluation erzeugt zunaechst einen Kandidaten; bei einer persistierbaren
Prozess- oder Markeränderung wird dieser erst nach erfolgreichem
Write-before-Apply und `applyProcessTransition()` uebernommen. Bei einem
fehlgeschlagenen Schreiben oder Anwenden bleibt der alte Evaluatorzustand
wirksam. Ohne persistierbare Aenderung darf der Kandidat RAM-only uebernommen
werden. Ein Retry bewertet deshalb den unveraenderten Live-Zustand neu und
schreibt keine Qualifikationszeit doppelt gut. Jede Decision ist dabei
single-use: ein stale oder fehlgeschlagener Kandidat wird verworfen und kann
nicht durch eine spaetere Statusumdeutung erneut angewendet werden. Der
hardwarefreie Orchestrator bildet `decision.progress` in `ProcessSignals`,
ruft ausschliesslich `decideProcessTransition()` auf und verwendet fuer den
Commit den bestehenden `RunPersistenceCoordinator`; er fuehrt weder eine
zweite Prozessmaschine noch einen zweiten Persistence-Coordinator ein.
Der Stale-Kontext bindet die bestehende `runRevision` und
`processTransitionSequence`, ohne dadurch allein die fachliche Episode zu
resetten.

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

Die biologische Wirkung wird nicht durch eine erfundene Kurve behauptet. Ein
Produktionsprovider fuer die Gewichtung bleibt ohne freigegebenes
Commissioning-Modell `unavailable`; die Firmware schreibt dann keinen
biologischen Beitrag gut. Die reine Modellgrenze akzeptiert nur eine
Fermenting-Episode mit bekannten Ausfallgrenzen und gueltiger, gefilterter
Vor-/Nach-Ausfall-Evidenz. Der Produktfuehler hat die Vertrauensstufe
`ProductPreferred`, der ausdruecklich verwendete Luftfuehler `AirReduced`.

### Recovery-/Fortschrittsvertrag (Schema 3)

Der persistierte Vertrag besteht aus den getrennten Feldern
`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`lastRecoveryEpisodeEvidence`, `priorBootPhaseElapsed`,
`nominalRecoveryAdjustment`, `recoveryEpisodeRevision` und
`runProgress`. `observedRunSeconds` bleibt die monotone beobachtete Laufzeit;
nominale Recovery-Korrekturen und gewichtete Beitraege werden nicht in diesen
Wert hineingeschrieben. Der Wert faltet die sicher beobachtete Zeit bei jedem
echten Live-Phasenwechsel aus `Fermenting`, bei einer echten
`AdjustRun`-Restdauer-Neubaseline und bei jedem echten Hop 1 aus `Fermenting`
genau einmal. Beim Hop 1 wird ausschliesslich
`thisHopAltBootLocalSeconds` dieses konkreten Boots verwendet; ein
Episode-Refresh erzeugt keinen Fold.

`ApplyRecoveryTimeCorrection` ist kumulativ und nur fuer die passende
Recovery-Episode zulaessig. Bounds, Episodenrevision, Idempotenz und
Counter-Overflow werden vor dem Schreiben geprueft. Eine wirksame Aenderung
faltet bei `AdjustRun` die seit dem Eintritt beobachtete Fermenting-Zeit genau
einmal in die Zeitbasis und setzt die Recovery-Baseline neu. Persistenz ist
Write-before-Apply; erst nach erfolgreichem atomarem Recovery-Schreiben wird
der RAM-Zustand geaendert.

`RunRecoveryCoordinator::activate` ist die schmale Orchestrierungsgrenze fuer
geladenen aktiven Lauf und Fallback. Sie delegiert die bestehende
Recovery-/Regelsensorauswahl und enthaelt keine eigene Prozessschleife.
`reevaluateRecoveryTime` leitet eine spaetere Zeitverbesserung aus demselben
Anker ab und persistiert sie als Recovery-Revision. Im Hop-1-only-Fall
`RecoveryEvaluation` mit urspruenglichem `WaitingForProduct` wird
`DefinitelyExpired` automatisch ohne Gate A als bestehender Tombstone-Pfad
beendet. `DefinitelyStillValid` darf nur mit frischem Gate-A-Kontext nach
`WaitingForProduct` resumieren; der No-context-Aufruf bleibt fail-closed.
`Uncertain` schreibt und mutiert nichts. Ein produktiver Aufrufer oder eine
allgemeine Prozessschleife ist in Release 1 nicht Bestandteil dieses Vertrags.

Die gewichtete Buchung prueft erwartete Lauf- und Episodenrevision,
Segmentkennung, Sensorberechtigung, Modellrevision und checked Bounds. Ein
Segment wird hoechstens einmal gebucht. Ein nicht gebuchtes, abgeloestes
Segment setzt die Coverage konservativ auf `PartialUnknown` und entfernt die
Obergrenze; ein gebuchtes Segment bleibt unveraendert. Eine atomare Buchung
aktualisiert nur den gewichteten Zustand und die Laufrevision. Ohne Modell,
ohne Evidenz oder bei ungueltiger Evidenz bleibt der Provider unavailable-
beziehungsweise not-eligible und erzeugt keinen Fortschritt.
`lastSourceRole` bezeichnet dabei immer die Quelle des zuletzt gebuchten
Beitrags. `confidence` bezeichnet die kumulative, konservative Vertrauensstufe:
ein einziges `AirReduced` bleibt auch nach einem spaeteren
`ProductPreferred`-Beitrag `AirReduced` und wird nie hochgestuft.

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

Die Berechnung verwendet bei einer laufenden Recovery-Episode den
unveraenderlichen Ursprungsanker und den aktuellen Recovery-Boot-Anker.
Carry-forward-Zeit aus einem frueheren Boot wird in der bekannten Zeitbasis
gefuehrt; sie erzeugt keine kuenstliche neue Ausfall-Untergrenze. Ein spaeterer
UTC-Abgleich darf nur eine echte Verbesserung der bereits persistierten unteren
Grenze oder die Aufloesung einer offenen Obergrenze nachtragen.

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
  -> RunRecoveryCoordinator::activate aufrufen
  -> bestehende Phasen-/Regelsensorauswahl delegiert bewerten
  -> Entscheidung atomar speichern
-> Regelung kontrolliert freigeben, sofern erlaubt
-> Netzwerk und NTP parallel wiederherstellen
-> Ausfallintervall und Fortschritt spaeter korrigieren
-> korrigierten Zustand atomar speichern
```

Der Wiederanlauf blockiert nicht auf NTP. Ohne absolute Zeit wird kein exakter
Unterbrechungsfortschritt erfunden und kein automatischer Phasenabschluss allein
aus einer Schaetzung abgeleitet.

## Recovery-API und Regelsensorauswahl bei Reaktivierung

Issue #21 (Regelsensorauswahl, Ersatzbetrieb, Rueckkehrlogik) liefert den
persistierten und den laufzeitseitigen Auswahlzustand. Die
`RunRecoveryCoordinator`-Grenze aktiviert einen geladenen aktiven Lauf oder
Fallback, delegiert die bestehende Empfehlung und persistiert die resultierende
Recovery-Entscheidung. Sie fuehrt keine zweite Auswahl- oder Prozesslogik ein.

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

Fuer die Reaktivierung (geladener aktiver Lauf -> betriebsbereit) gilt:

- die Empfehlung wird angewendet und der laufzeitseitige Auswahlzustand
  gesetzt, bevor die Regelung fuer diesen Lauf bewertet wird;
- **Reihenfolge beachten:** ein aktiver Lauf verlangt beim Schreiben
  zwingend einen vorhandenen persistierten Auswahlzustand (`sensorSelection`)
  - dieser ist nach dem Laden bereits vorhanden und wird durch jede
  nachfolgende Speicherung (auch periodische Kontrollpunkte) unveraendert
  erneut mitgefuehrt. `sensorSelectionRuntime` ist davon unabhaengig: er ist
  ausserhalb des Wireformats, wird von keinem Kontrollpunkt mitgeschrieben
  und fliesst nicht in die Schreibvoraussetzung ein. Solange die
  Reaktivierung aussteht, bleibt er einfach fail-closed im RAM stehen -
  "Neubewertung nach Neustart erforderlich" (`RestartRevalidationPending`)
  mit gesperrter Peltier-Freigabe (`Blocked`); periodische Kontrollpunkte
  laufen davon unberuehrt normal weiter;
- die Peltier-Freigabe bleibt bis zum Abschluss dieser Neubewertung gesperrt;
  eine vorherige Freigabe aus dem Lauf vor dem Neustart wird nie blind
  uebernommen. Bei fehlender oder ungueltiger Live-Evidenz bleibt die
  Entscheidung fail-closed.

Test- und Zustaendigkeitsgrenze: Schema-1- und Schema-2-Bestaende bleiben
kompatibel, ausserhalb des bewusst abgelehnten aktiven Fault-Falls. Die
Reaktivierungs- und Fallback-Delegation prueft die bestehenden
Recovery-/Sensorselektionsregeln; die Persistenzkoordination bleibt Eigentuemer
von Gate A, Write-before-Apply und der atomaren Recovery-Revision.

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

Der aktuelle Laufvertrag bleibt innerhalb fester Grenzen: maximal 8.192 Bytes
Payload, 8.240 Bytes Checkpoint-Record und 256 Bytes Head-Record; jeweils
maximal 32 Laufrevisionen und persistierte Command-IDs. Die neuen Recovery-
und Progressfelder nutzen ausschliesslich den bestehenden Schema-3-Datensatz,
keine unbounded Historie, keine neue Prozessschleife, keinen Netzwerk- oder
Anzeige-Puffer und keine OTA-/PSRAM-Reserve. Ein zusaetzlicher
Commissioning-Provider ist in Release 1 nicht eingebaut; ein
`TBD_IMPLEMENTATION_BUDGET` bleibt daher kein Laufzeitwert.
