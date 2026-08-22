# Sicherheit und Fehlerbehandlung

## Status

Dieses Dokument enthaelt historische Phase-8A-Regeln und den vorrangigen
Release-1-FaultCode-Vertrag fuer unmittelbare Reaktionen, Quittierung,
Persistenz und gleichzeitige Fehlerursachen.

Konkrete Temperatur-, Sensor-, Luefter-, Aktor-, Versorgungs- und
Softwarefehler werden in Phase 8B und 8C ergaenzt.

## Issue #24: verbindlicher Release-1-KISS-Vertrag

Fuer den Release-1-Safety-Core gilt dieser Abschnitt vorrangig vor den
historischen allgemeinen Fehlerklassen- und Recoverynotizen dieses Dokuments:

- Der Gate-Default ist `Unresolved`; Boot, Reset und Restore halten den
  abstrakten Aktorpfad bei Idle/Stop.
- R1 verwendet nur `Information`, `Blocked/ImmediateStop` und `SAFE_BOOT`.
  Die historische Vierklassenbeschreibung ist fuer #24 keine neue API und
  keine zusätzliche Safety-Wahrheit.
- SafetyCore verwendet die endliche FaultCode-/Disposition-Matrix aus der
  Umsetzung. Ack ist nur Anzeige/Journaling und loescht weder Ursache noch Gate.
- Es gibt in #24 keinen generischen persistenten Safety-Latch, keinen
  Restart-Zaehler, kein Resetzeitfenster, keine Service-PIN-Pflicht und keine
  automatische `SAFETY_RECOVERY`-Gegenrichtung.
- Resetcause ist Diagnose. Jeder Boot validiert Configuration, Persistenz und
  aktuelle Evidenz neu; ein Neustart erzeugt weder automatisch aktivierendes
  Resume noch `Allowed`. Ein vollstaendig validierter #90-Fallback darf nur als
  nicht-aktivierendes Resume-Angebot beobachtbar werden.

Historische Detailabschnitte zu spaeteren Produzenten, thermischen Grenzen und
Serviceablaeufen bleiben als Zukunfts-/Nachfolgekontext erhalten, werden aber
nicht als Release-1-#24-Laufzeitvertrag umgesetzt.

## Grundsaetze

- Ein Fehler darf niemals durch Komfortfunktionen, Mindestlaufzeiten oder einen
  alten Aktorbefehl ueberstimmt werden.
- `Quittieren` und `Fehler zuruecksetzen` sind fachlich getrennte Aktionen.
- Ein Neustart ist kein Fehlerreset.
- Automatische Wiederfreigabe ist kein allgemeiner Fehlerklassenvertrag; ein
  Clear ist nur ueber den positiven Pfad des konkreten FaultCodes erlaubt.
- Bei unsicherem Zustand wird nicht geraten.
- Die hoechste aktive FaultCode-Prioritaet bestimmt die Disposition. R1
  verwendet keine universelle Fehlerklassenhierarchie und keine zweite
  Fehlerzustandsmaschine.
- Aktive Ursachen bleiben in einer endlichen, festen FaultCode-Maske
  nachvollziehbar; Historie und Journaling bleiben beim bestehenden
  `IEventJournal`, nicht im SafetyCore.
- Sicherheitsreaktionen funktionieren ohne WLAN, Weboberflaeche, Netzwerkzeit
  oder Heimserver.
- Sicherheitslogik und Fehlerdaten muessen innerhalb der Zielhardware mit 4 MB
  Flash ohne vorausgesetzte PSRAM funktionieren.

## Verbindliche R1-FaultCode- und Lifecycle-Matrix

R1 verwendet keine universelle Fehlerhierarchie. Die folgenden acht Codes sind
die endliche SafetyCore-Menge; technische Detailursachen bleiben beim
jeweiligen Producer. Alle Codes sind stabile typisierte Wire-Werte und werden
nicht aus UI-Texten gebildet.

| Code | Producer/Detail | Disposition und Gate | Clear-Regel | Ack / Neustart |
|---|---|---|---|---|
| `ConfigurationRuntimeFailure` | `ConfigurationService` + Detail-Cause | `BlockedImmediateStop` | kein Auto-Clear; frische Config und expliziter neuer Start | nur Anzeige; Reboot gibt nicht frei |
| `ConfigurationUnavailable` | `ConfigurationRecoveryService`/Config-Projection | `SAFE_BOOT` | frische gueltige Config-/Recovery-Evidenz | nur Anzeige; keine neue Persistenz |
| `ConfigurationIntegrityFailure` | `ConfigurationRecoveryService` | `SAFE_BOOT` | frische passende Integritaets-/Recovery-Evidenz | nur Anzeige; keine neue Persistenz |
| `ConfigurationCommitIndeterminate` | `ConfigurationService` | `SAFE_BOOT` | eindeutig aufgeloestes Commit-/Recovery-Ergebnis | nur Anzeige; keine neue Persistenz |
| `RunPersistenceUntrusted` | #17 Load-/Coordinator-Status | `SAFE_BOOT` | vertrauenswuerdiger Load und Coordinator-Zustand; untrusted Load wird nicht tombstoned | nur Anzeige; keine neue Persistenz |
| `SafetySensorUnavailable` | #20/#21 Qualitaets-/Auswahlprojektion | `BlockedImmediateStop` | frische gueltige kanonische Sensor-/Auswahlevidenz | nur Anzeige; kein Sensor-Latch |
| `ActuatorRequestWatchdog` | #23 echter Planner-RAM-Latch | `BlockedImmediateStop` | nur expliziter #23-Reset mit frischer Evidenz und aktivem Latch | Ack/Request loeschen nicht; Reboot verliert nur RAM-Latch |
| `SystemProducerUnknown` | unbekannter/unmapped Producer | `SAFE_BOOT` | alle erforderlichen Producer aktuell bekannt und frisch validiert | nur Anzeige; keine neue Persistenz |

Bei mehreren aktiven Codes dominiert `SAFE_BOOT`; die Primaerauswahl ist
deterministisch, alle anderen aktiven Bits bleiben bis zu ihrem eigenen
Clear-Vertrag erhalten. `Ack` quittiert hoechstens den einzelnen Code und
veraendert weder Gate noch Clear-Zustand. Safety-Reaktion und Gate werden vor
Journal/Notification bestimmt; deren Fehler koennen keine Abschaltung
rollbacken.

## C2-Legacy/Future – historische Vierklassenbeschreibung, nicht #24-R1

Die folgenden historischen Abschnitte bleiben fuer spaetere Issues und
Commissioning referenzierbar, sind aber kein #24-R1-Vertrag. Insbesondere
gelten dort genannte persistente Latches, Service-PIN-Resets, automatische
`SAFETY_RECOVERY`, thermische Codes, gewichteter Fortschritt und automatische
Wiederfreigaben nicht fuer Release 1.

## C2-Legacy: historische Fehlerklassen- und Recoveryregeln

Die folgenden Phase-8A-Abschnitte sind fuer spaetere/Legacy-Kontexte erhalten.
Sie sind kein aktiver #24-R1-Vertrag; im R1-Safety-Core gelten ausschliesslich
FaultCode, Disposition, bounded Multi-Fault und die kanonischen Producerpfade.

## Fehlerklassen

Die Firmware verwendet vier fachliche Fehlerklassen. Sichtbare Meldungstexte
werden uebersetzt; die internen Klassen und Fehlercodes bleiben sprachunabhaengig
und stabil.

### Klasse 1: Hinweis oder Prozesswarnung

Bedeutung:

- Abweichung, Unsicherheit oder erwartungswidriges Verhalten
- keine unmittelbare Gefahr
- sichere Temperaturregelung ist weiterhin moeglich

Standardreaktion:

- Prozess laeuft weiter
- sichtbare Meldung und gegebenenfalls akustisches Signal
- Ereignis wird protokolliert
- Quittierung entfernt nicht automatisch die Ursache

Beispiele, vorbehaltlich Phase 8B und 8C:

- Zieltemperatur wird langsamer als erwartet erreicht
- Netzwerkzeit ist voruebergehend nicht verfuegbar
- Historienbereinigung wurde notwendig
- Lauf wird nach einer Unterbrechung mit geringerer Zeitkonfidenz fortgesetzt

### Klasse 2: Behebbarer Betriebsfehler

Bedeutung:

- normale sichere Regelung ist voruebergehend nicht oder nur in einem explizit
  vorgesehenen Ersatzbetrieb moeglich
- Ursache kann ohne technischen Eingriff verschwinden oder durch eine normale
  Bedienhandlung behoben werden

Standardreaktion:

- Peltier wird sicher ausgeschaltet, sofern kein validierter Ersatzbetrieb
  ausdruecklich erlaubt ist
- notwendige Luefterlogik bleibt aktiv
- Laufzustand und Fortschritt werden gesichert
- Wiederaufnahme ist nach erneuter Pruefung moeglich
- Fehler kann je nach Fehlercode automatisch oder bewusst zurueckgesetzt werden

Beispiele:

- kurzzeitiger, validierbar wiederkehrender Produktfuehlerausfall
- voruebergehend veraltete Regelanforderung
- behebbarer Bedien- oder Konfigurationskonflikt ohne Sicherheitsverletzung

### Klasse 3: Verriegelter Sicherheitsfehler

Bedeutung:

- gefaehrlicher Zustand oder ein Zustand, dessen Sicherheit nicht eindeutig
  nachgewiesen ist
- unbeaufsichtigte automatische Wiederfreigabe ist unzulaessig

Standardreaktion:

- Peltier sofort sicher AUS
- H-Bruecke deaktivieren
- Fehler persistent verriegeln
- notwendige Luefterreaktion ausfuehren
- bewusster Fehlerreset erst nach beseitigter Ursache und bestandenen Pruefungen
- je nach Fehlercode Service-PIN erforderlich

Beispiele:

- absolute Ueber- oder Untertemperatur
- unzulaessige H-Bruecken-Ansteuerung
- fuer die Sicherheitsfreigabe notwendiger Schrankluftfuehler ausgefallen
- nachgewiesener oder stark vermuteter Aussenluefterfehler bei Peltierbetrieb

### Klasse 4: Schwerer Systemfehler

Bedeutung:

- sichere Software-, Speicher-, Zeitbasis- oder Hardwarefunktion kann nicht mehr
  ausreichend garantiert werden
- ein normaler Prozessbetrieb ist nicht zulaessig

Standardreaktion:

- sicherer Stillstand
- Peltier und H-Bruecke deaktiviert
- Luefter nur gemaess sicherer Restwaerme- und Fehlerstrategie
- keine automatische Prozessfortsetzung
- persistente Systemfehlermeldung, soweit noch sicher speicherbar
- Service beziehungsweise technische Diagnose erforderlich

Beispiele:

- kritischer Lauf- und Rueckfalldatensatz nicht rekonstruierbar
- wiederholter interner Software-Watchdog oder Integritaetsfehler
- kritischer Konfigurations- oder Flashfehler
- Sicherheitsaufgabe beziehungsweise Aktorlogik liefert keinen verlaesslichen
  Zustand

## Stabile Fehlercodes und sichtbare Texte

Jeder Fehler besitzt mindestens:

- stabilen maschinenlesbaren Fehlercode
- Fehlerklasse
- primaere Komponente oder Teilsystem
- Entstehungszeit beziehungsweise monotonen Zeitbezug
- Aktiv-, quittiert-, beseitigt- und verriegelt-Status
- unmittelbare Sicherheitsreaktion
- zulaessige Wiederfreigabestrategie
- erforderliche Berechtigung fuer einen Reset
- optionale Zuordnung zu einem Primaerfehler

Uebersetzte Texte werden aus dem stabilen Fehlercode und strukturierten
Zusatzdaten erzeugt. Der Fehlercode darf nicht vom angezeigten deutschen,
spanischen oder englischen Text abhaengen.

## Unmittelbare Reaktion bei einem sicherheitsrelevanten Fehler

Bei einem verriegelten Sicherheitsfehler oder schweren Systemfehler gilt logisch
mindestens:

```text
Fehler erkannt
  -> neue Peltierfreigaben sofort sperren
  -> aktive Peltierleistung sofort beenden
  -> beide H-Brueckenrichtungen deaktivieren
  -> Impulsakkumulator verwerfen
  -> Integralanteil sperren beziehungsweise in sicheren Zustand ueberfuehren
  -> Aussenluefter-Nachlauf beziehungsweise erforderliche Dauerbelueftung starten
  -> Innenluefter gemaess Fehlerursache behandeln
  -> Fehler- und Laufzustand atomar sichern, soweit Speicher sicher verfuegbar
  -> optisch und akustisch melden
```

Verbindliche Regeln:

- Eine Mindest-Einschaltzeit darf die sofortige Sicherheitsabschaltung nicht
  verzoegern.
- Eine Mindest-Auszeit oder Totzeit darf eine Abschaltung nicht verhindern.
- Heizen und Kuehlen werden beide gesperrt, bis der konkrete Fehlercode eine
  Richtung oder den gesamten Peltierbetrieb wieder freigibt.
- Ein alter Reglerausgang, Impulsakkumulator oder Integratorwert wird nach einem
  Fehler nicht blind nachgeholt.
- Der Aussenluefter wird nicht pauschal zusammen mit dem Peltier abgeschaltet,
  solange Restwaerme abgefuehrt werden muss.
- Der Innenluefter kann je nach Ursache weiterlaufen, nachlaufen oder abgeschaltet
  werden. Die Fehlercode-Matrix folgt in Phase 8B und 8C.
- Ist persistentes Speichern selbst Teil des Fehlers, hat der sichere
  Ausgangszustand Vorrang vor einem weiteren Schreibversuch.

## Quittieren und Fehler zuruecksetzen

### Quittieren

`Quittieren` bedeutet ausschliesslich:

- der Benutzer hat die Meldung wahrgenommen
- die akustische Wiederholung darf gemaess Meldungsregeln reduziert oder beendet
  werden
- die Ursache und Fehlerklasse bleiben unveraendert
- eine bestehende Aktorsperre bleibt bestehen
- die Quittierung wird mit Quelle und Zeitpunkt protokolliert

Quittieren ist kein Fehlerreset und keine Bestaetigung, dass der Zustand wieder
sicher ist.

### Fehler zuruecksetzen

`Fehler zuruecksetzen` darf nur angeboten oder akzeptiert werden, wenn:

1. die aktuelle Fehlerursache nicht mehr besteht,
2. die fuer den Fehlercode erforderlichen Sensor-, Hardware- und
   Plausibilitaetspruefungen bestanden sind,
3. keine gleich- oder hoeherklassige aktive Fehlerursache die Freigabe verhindert,
4. der aktuelle Laufzustand noch sicher rekonstruierbar ist,
5. die fuer den Fehlercode erforderliche Berechtigung vorliegt.

Berechtigungen:

- einfache behebbare Betriebsfehler duerfen durch einen normalen Bediener
  zurueckgesetzt werden, sofern der Fehlercode dies erlaubt
- sicherheitskritische oder technische Fehler verlangen die Service-PIN
- schwere Systemfehler koennen zusaetzlich einen Neustart, Wartungstest oder
  technischen Service verlangen

Ein Resetversuch wird abgelehnt und begruendet, wenn die Ursache weiterhin
besteht oder die erneute Pruefung fehlschlaegt.

## Automatische Wiederfreigabe

Automatische Wiederfreigabe ist standardmaessig verboten und wird nur pro
Fehlercode ausdruecklich erlaubt.

Ein automatisch behebbarer Fehler muss mindestens besitzen:

- definierte und nachweisbare Fehlerursache
- definierte stabile Fehlerfreiheitszeit
- erneute Sensor- und Plausibilitaetspruefung
- eindeutige sichere Wiederaufnahmeaktion
- begrenzte Anzahl automatischer Wiederholungen beziehungsweise Schutz gegen
  schnelles Ein-/Ausschalten
- sichtbaren und protokollierten Wiederaufnahmevorgang

Zulaessiges Beispiel:

- Produktfuehler war voruebergehend gestoert und kehrt nach der bereits
  spezifizierten stabilen Validierung in einen produktgefuehrten Lauf zurueck

Nicht automatisch wieder freigegeben werden mindestens:

- verriegelte Sicherheitsfehler
- schwere Systemfehler
- absolute Ueber- oder Untertemperatur
- unzulaessige H-Bruecken-Kombination
- Fehler, deren Ursache nicht eindeutig nachweisbar beseitigt ist
- Fehler mit vorgeschriebenem Service-PIN-Reset

Das Verschwinden eines Messwertalarms allein ist bei einer verriegelten Ursache
nicht ausreichend.

## Zeitfortschritt waehrend eines Fehlers

Der Fermentationsfortschritt wird nicht pauschal nach der Wanduhr weitergezaehlt.

Verbindliche Regeln:

- Ist keine sichere Temperaturregelung moeglich, entsteht kein normaler voller
  Prozessfortschritt.
- Liegt die relevante Temperatur trotz Fehler weiterhin in einem wirksamen
  Bereich und ist die Messqualitaet ausreichend, darf eine begrenzte
  temperaturgewichtete Anrechnung erfolgen.
- Unsichere oder veraltete Messwerte duerfen keinen erfundenen Fortschritt
  erzeugen.
- Unterbrechungsdauer, Temperaturverlauf, Sensorqualitaet und Fehlerklasse werden
  bei der Wiederaufnahme beruecksichtigt.
- Nach Wiederaufnahme wird eine Laufzeit- oder Fortschrittskorrektur berechnet,
  sichtbar gemacht und gespeichert.
- Ist die Unsicherheit zu gross, wird nicht automatisch geraten. Abhaengig von
  Phase und Fehlercode wird der Lauf sicher beendet oder eine ausdrueckliche
  Benutzerentscheidung verlangt.
- Eine unterbrochene Zielqualifikation gilt nicht als erfolgreich abgeschlossen.

Die genaue temperaturgewichtete Berechnung folgt den Regeln aus
`RECOVERY_AND_INTERRUPTION.md` und `TEMPERATURE_CONTROL.md`.

## Persistenz ueber Neustarts

### Persistent verriegelte Fehler

Verriegelte Sicherheitsfehler und schwere Systemfehler werden persistent
gespeichert, soweit der Speicher selbst verlaesslich funktioniert.

Nach einem Neustart:

- bleiben Peltier und H-Bruecke zunaechst AUS
- wird der persistierte Fehlerdatensatz validiert
- wird die aktuelle Ursache erneut geprueft
- bleibt die Verriegelung bestehen, auch wenn die Ursache scheinbar verschwunden
  ist
- ist ein bewusster, berechtigter Fehlerreset erforderlich

Ein Neustart oder Spannungsunterbruch loescht keine Verriegelung.

### Neu bewertete Warnungen und behebbare Fehler

Hinweise, Warnungen und automatisch behebbare Betriebsfehler werden beim Start
anhand des aktuellen Zustands neu bewertet:

- Historie bleibt erhalten.
- Weiterhin bestehende Ursachen werden wieder aktiv.
- Beseitigte Ursachen werden als erledigt gekennzeichnet.
- Ein alter Fehler wird nicht allein deshalb aktiv gehalten, weil er vor dem
  Neustart aktiv war.
- Ein historisch wichtiges Ereignis bleibt trotzdem nachvollziehbar.

## Fortsetzung eines Laufes nach Fehlerbehebung

Ein Lauf darf nur in seine vorherige oder eine fachlich passende Folgephase
zurueckkehren, wenn mindestens:

- Programmschnappschuss und Laufrevision gueltig sind
- aktuelle Sensoren und erforderliche Sicherheitsbedingungen bestanden sind
- Zeitbasis und Unterbrechungsdauer ausreichend bestimmt oder konservativ
  behandelbar sind
- der wirksame Regelsensor eindeutig feststeht
- Fehlerreset beziehungsweise automatische Wiederfreigabe gueltig abgeschlossen
  ist
- die phasenbezogene Wiederanlaufregel eine Fortsetzung erlaubt

Phasenregeln:

- Unterbrochene Zielqualifikation beginnt erneut.
- Fermentation wird nur mit validiertem und gegebenenfalls korrigiertem
  Fortschritt fortgesetzt.
- Kuehlung und Halten werden aus aktuellem Sensorzustand neu abgeleitet.
- Direkte Ausgangszustaende werden niemals wiederhergestellt.
- Bei zu grosser Unsicherheit wird der Lauf nicht an einer erfundenen
  Countdownstelle fortgesetzt.
- Ein sicher beendeter oder abgebrochener Lauf wird nicht durch einen Reset
  wieder aktiviert.

Die Wiederaufnahmeentscheidung wird vor der Aktorfreigabe atomar gespeichert.

## Mehrere gleichzeitige Fehler

Alle gleichzeitig relevanten Fehlerursachen werden erfasst. Es wird nicht nur der
erste oder letzte Fehler gespeichert.

### Prioritaet

- Die hoechste aktive Fehlerklasse bestimmt den sicheren Ausgangszustand.
- Innerhalb derselben Klasse bestimmt eine definierte Fehlerprioritaet die
  wichtigste sichtbare Meldung.
- Niedriger priorisierte Fehler bleiben in der Meldungsliste und im Protokoll.
- Eine Quittierung der Hauptmeldung quittiert nicht automatisch alle anderen
  Fehler.

### Primaer- und Folgefehler

Fehler duerfen als wahrscheinlich primaer oder sekundaer verknuepft werden:

```text
Primaerfehler:
Aussenluefter nicht nachweisbar

Folgeereignisse:
Kuehlkoerpertemperatur steigt
Schranklufttemperatur ueberschreitet Grenze
```

Verbindliche Regeln:

- Zeitliche Reihenfolge, Fehlercodes und Messwerte bleiben erhalten.
- Die Kennzeichnung als Folgefehler darf die eigene Sicherheitsreaktion nicht
  unterdruecken.
- Eine vermutete Kausalitaet wird als solche gekennzeichnet und nicht als sicherer
  Fakt ausgegeben, wenn sie nicht eindeutig nachweisbar ist.
- Das Ruecksetzen eines Primaerfehlers ist nur moeglich, wenn auch alle fuer die
  Freigabe relevanten Folgeursachen beseitigt sind.
- Ein Folgefehler kann nach Wegfall des Primaerfehlers weiterhin aktiv oder
  verriegelt bleiben.

## Fehlerzustandsmodell

Ein Fehlerdatensatz besitzt mindestens folgende fachliche Zustaende:

```text
ACTIVE_UNACKNOWLEDGED
ACTIVE_ACKNOWLEDGED
CAUSE_CLEARED_LOCKED
CLEARED
```

Optional koennen je Fehlercode weitere interne Zustaende verwendet werden,
beispielsweise:

```text
RECOVERY_VALIDATING
AUTO_RECOVERY_DELAY
RESET_REJECTED
```

Bedeutung:

- `ACTIVE_UNACKNOWLEDGED`: Ursache aktiv, noch nicht quittiert
- `ACTIVE_ACKNOWLEDGED`: Ursache aktiv, Benutzer hat Meldung wahrgenommen
- `CAUSE_CLEARED_LOCKED`: Ursache aktuell nicht mehr nachweisbar, Verriegelung
  verlangt aber bewussten Reset
- `CLEARED`: Ursache beseitigt und Fehler gemaess Regeln freigegeben

Der sichtbare Begriff `Quittiert` darf niemals mit `Behoben` oder `Freigegeben`
verwechselt werden.

## Akzeptierte Entscheidungen aus Phase 8A

- [x] vier Fehlerklassen: Hinweis/Prozesswarnung, behebbarer Betriebsfehler,
      verriegelter Sicherheitsfehler und schwerer Systemfehler
- [x] sicherheitsrelevanter Fehler schaltet das Peltier sofort aus
- [x] beide H-Brueckenrichtungen werden deaktiviert
- [x] Impulsakkumulator wird verworfen und Integralanteil gesperrt
- [x] notwendiger Aussenluefter-Nachlauf bleibt erhalten
- [x] Innenluefterverhalten wird pro Fehlerart festgelegt
- [x] `Quittieren` und `Fehler zuruecksetzen` sind getrennte Aktionen
- [x] Resetberechtigung wird pro Fehlercode festgelegt
- [x] automatische Wiederfreigabe nur fuer ausdruecklich geeignete
      Betriebsfehler
- [x] verriegelte Sicherheits- und Systemfehler verlangen bewussten Reset
- [x] Fermentationsfortschritt wird phasen-, temperatur- und
      qualitaetsbezogen behandelt
- [x] verriegelte Fehler bleiben ueber Neustarts erhalten
- [x] Warnungen und automatisch behebbare Fehler werden beim Boot neu bewertet
- [x] Fortsetzung nur nach vollstaendiger Zustands- und Sicherheitsvalidierung
- [x] unterbrochene Zielqualifikation beginnt erneut
- [x] alle gleichzeitigen Fehler werden erfasst
- [x] hoechste aktive Fehlerklasse bestimmt den sicheren Ausgangszustand
- [x] Primaer- und Folgefehler bleiben getrennt nachvollziehbar

## Noch offen fuer Phase 8B und 8C

- konkrete Fehlercodes und Klassen fuer Schrankluft- und Produktfuehler
- Warn-, Begrenzungs- und absolute Temperaturgrenzen
- Verhalten bei Ueber- und Untertemperatur je Heiz-/Kuehlrichtung
- Erkennung und Reaktion bei Innen- oder Aussenluefterfehler
- verfuegbare Rueckmeldung zur Luefterfunktion ohne zusaetzliche Hardware
- BTS7960-/H-Bruecken-Fehler und unzulaessige Ausgangskombinationen
- Verhalten bei Versorgungseinbruch und Brownout
- Software-Watchdogs und Task-Ueberwachung
- Speicher-, Konfigurations- und Integritaetsfehler
- Fehler bei Display, Touch, WLAN und Weboberflaeche
- konkrete Resetberechtigung je Fehlercode
- konkrete automatische Fehlerfreiheitszeiten
- Fehlerzaehler, Wiederholungsgrenzen und Eskalationsregeln
- Fehlerprotokollformat und Aufbewahrung
