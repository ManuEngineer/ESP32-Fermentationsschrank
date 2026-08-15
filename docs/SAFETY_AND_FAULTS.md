# Sicherheit und Fehlerbehandlung

## Status

Dieses Dokument beschreibt die in Phase 8A und der finalen R3-Synchronisierung
akzeptierten Grundregeln fuer Fehlerklassen, stabile Fehlercodes, unmittelbare
Reaktionen, Quittierung, Fehlerreset, automatische Wiederfreigabe,
Zeitfortschritt, Persistenz und gleichzeitige Fehlerursachen.

Konkrete Temperatur-, Sensor-, Luefter-, Aktor-, Versorgungs- und
Softwareparameter sowie die reale Hardwareverifikation bleiben in Phase 8B,
8C und der Inbetriebnahme zu belegen; die #24-Code-/Resetpolicy ist unten
vollstaendig festgelegt. Die Matrix unterscheidet reale heutige Producer,
deterministische #24-interne Ursachen und stabile Release-1-Code-/Injection-
Contracts fuer spaetere qualifizierte Producer.

## Grundsaetze

- Ein Fehler darf niemals durch Komfortfunktionen, Mindestlaufzeiten oder einen
  alten Aktorbefehl ueberstimmt werden.
- `Quittieren` und `Fehler zuruecksetzen` sind fachlich getrennte Aktionen.
- Ein Neustart ist kein Fehlerreset.
- Automatische Wiederfreigabe ist nur fuer ausdruecklich dafuer freigegebene
  Fehlerklassen und Fehlercodes erlaubt.
- Bei unsicherem Zustand wird nicht geraten.
- Die hoechste aktive Fehlerklasse bestimmt den sicheren Ausgangszustand.
- Alle relevanten Ursachen und Folgeereignisse bleiben nachvollziehbar.
- Sicherheitsreaktionen funktionieren ohne WLAN, Weboberflaeche, Netzwerkzeit
  oder Heimserver.
- Sicherheitslogik und Fehlerdaten muessen innerhalb der Zielhardware mit 4 MB
  Flash ohne vorausgesetzte PSRAM funktionieren.

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

Durch R3 fachlich geschlossene Beispiele:

- Zieltemperatur wird langsamer als erwartet erreicht
- #22 reduziert eine ansonsten gueltige Anforderung wegen `AirLimit`

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

### Finale Release-1-Code- und Resetmatrix fuer Issue #24

Die folgenden Codes sind der verbindliche sprachunabhaengige R3-Namensraum.
Cleared-Historie ist keine aktive Latchkapazitaet. Die Producer-/Rollen- und
Korrelationsidentitaet jeder Zeile steht im Codefeld. `SAFE_BOOT-Exit` ist nie
durch einen normalen Neustart moeglich.

| Code / Producer und Ursache | Sofortreaktion | Latch / Auto-Rearm | Reset / Berechtigung | Reboot / SAFE_BOOT-Exit | Primaer/Folge |
|---|---|---|---|---|---|
| `P1-001` / bestehender Prozessautomat mit `ProcessRuntimeState::targetReachWarningIssued`, `TransitionReason::TargetReachTimeExceeded`, `ProcessMessage::TargetReachTimeExceeded` und `ProcessRunSnapshot::maximumTargetReachMinutes`, Run-Snapshot-/Prozesslauf-Identitaet | Warnung, nur bei Safetyfreigabe fortsetzen | nein / neue gueltige Prozessbewertung | kein Faultreset, nur Quittierung | kein Reboot / kein Exit | primaer; Folgebezug erlaubt |
| `O2-001` / #20/#21 Produktfuehler `STALE`/`FAILED`, Produkt-Sensorrolle, Luftfallback | Peltier AUS, nur validierte #21-Ersatzstrategie | nein / nur #21-Policy | Bediener-/#21-Reset nach Ursachefreiheit und Checks | kein Reboot / kein Exit | primaer; Eskalation separat |
| `O2-002` / #20/#21 `STALE` in Sicherheitsrolle, Sensorrolle | Peltier AUS, Nachlauf, Wiedererkennung | nein / stabile Messungen vor `FAILED` | waehrend Wiedererkennung kein Reset | kein Reboot / kein Exit | primaer; S3-Eskalation bleibt |
| `O2-003` / #20 unklare Sensorrollen-/Plausibilitaetskorrelation | Peltier AUS, keine Ersatzfreigabe | nein / nur eindeutige Evidenz | Bedienerreset nach Rollen-/Safetypruefung | kein Reboot / kein Exit | primaer; S3-003 kann folgen |
| `S3-001` / #20 Schrankluftsensor `FAILED`, Sensorrolle | Peltier/H-Bruecke AUS, Nachlauf | ja / nein | Service nach stabiler Sensor-/Safetypruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `S3-002` / #20 Aussenwaermetauscher-/Kuehlkoerpersensor `FAILED`, Sensorrolle | Peltier/Richtungen AUS, sichere Waermeabfuhr | ja / nein | Service nach Sensor-/Aktorpruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `S3-003` / #24-interne Latchprojektion eines vorhandenen #21-`CrossRolePlausibilityContext`-/`ThermalCompatibility::Incompatible`-Befunds, Rollen-/Plausibilitaetskorrelation | Peltier AUS, kein Fallback | ja / nein | Service nach Ursachefreiheit und Nachweis | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer; O2-003 bleibt |
| `S3-004` / stabiler #24-Release-1-Contract-/Injection-Code fuer Sicherheits-Eingriffsgrenze; spaeterer qualifizierter #35-Grenzproducer, obere/untere Grenzkorrelation | Leistung AUS, Richtung sperren, Impuls/Integrator verwerfen | ja / keine produktive Auto-Recovery | Service nach Checks; Reset erzeugt keine Recovery und loescht nicht automatisch | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer; Recovery folgt |
| `S3-005` / stabiler #24-Release-1-Contract-/Injection-Code fuer harte thermische Notgrenze; spaeterer qualifizierter #35-/Hardware-Grenzproducer, obere/untere Grenzkorrelation | AUS, keine Gegenrichtung, sichere Luefterstrategie | ja / nein | technische Servicepruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `S3-006` / stabiler #24-Release-1-Contract-/Injection-Code fuer Aussenluefterfehler; reale Fan-Diagnose folgt dem zustaendigen spaeteren Hardware-/Commissioning-Gate, Aussenluefterrolle | Peltier AUS, Restwaerme fail-closed | ja / nein | Service nach Fan-/Ausgangspruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `S3-007` / stabiler #24-Release-1-Contract-/Injection-Code fuer Innenluefterfehler; reale Fan-Diagnose folgt dem zustaendigen spaeteren Hardware-/Commissioning-Gate, Innenluefterrolle | Peltier AUS oder richtungsbezogen sperren | ja / nein | Service nach Fan-/Ausgangspruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `S3-008` / #23 `ActuatorWatchdogFaultEvidence`, Planner-/Diagnoseevidenz | Peltier AUS, Safety-Gate Stop, 64-bit-Evidenz sichern | ja / nein | technische Berechtigung nach #23-/Aktorchecks | standardmaessig kein Reboot / kein Exit durch Reset | primaer |
| `S3-009` / stabiler #24-Release-1-Contract-/Injection-Code fuer H-Bruecke/Strom/Ausgang/Richtung; reale Diagnose folgt dem zustaendigen spaeteren Hardware-/Commissioning-Gate, Output-Domain | beide Richtungen AUS, Plan verwerfen | ja / nein | technische Servicepruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-001` / #56 `ConfigurationRuntimeFailure`, Konfigurationsrevision | keine neue Konfiguration, Aktoren stoppen | ja / nein | Service/technisch nach Konfigurations-/Integritaetschecks | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-002` / #56/#57 `ConfigurationCommitIndeterminate`/`CommitOutcomeUnknown`, Commit-Korrelation | unklare Revision sperren, Aktoren AUS | ja / nein | Service/technisch nach eindeutigem Status | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-003` / #57 `ConfigurationUnavailable`, Recovery-/Graph-Quelle | keine Teilkonfiguration, Aktoren AUS | ja / nein | Service/technisch nach gueltiger Revision | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-004` / #57 `ConfigurationIntegrityFailure`, Graph-/Konfigurationsdomaene | Integritaetsfehler fail-closed, Aktoren AUS | ja / nein | Service/technisch nach Integritaetspruefung | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-005` / #17/#18 kritischer Laufcheckpoint, aktiver Lauf | Peltier/H-Bruecke AUS, Lauf sicher beenden | ja / nein | Service/technisch nach neuer Laufrevision | kein zusaetzlicher Reboot / kein Exit durch Reset | primaer |
| `Y4-006` / #24 Safety-State Read/Write/Capacity/Integrity, globaler Basisrecord-Marker | RAM-Latch, Aktoren AUS, Marker im selben Record versuchen | ja / nein, keine Eviction; Marker ist kein Slot | Service/technisch nach Read-/Write-/Capacitypruefung | kein zusaetzlicher Reboot; Exit nur separate Bootpolicy | primaer |
| `Y4-007` / #24 zentrale Safety-/Recoveryevidenz, ein interner Recoveryfehler | Aktoren AUS, Evidenz sichern, hoechstens einen Recovery-Restart vorbereiten | ja / nein | technische Berechtigung nach Checks | genau ein automatischer Restart je aktiver Recoveryursache/-episode; kein zweiter | primaer; S3-008 bleibt |
| `Y4-008` / unbekannter Resetgrund, fehlende/doppelte/mismatched App-Evidenz oder Safetyinput, Bootbeobachtung | `Allowed` verbieten, Aktoren AUS, fail-closed | ja / nein | bis Klaerung verboten | kein Reboot als Loesung / kein Exit ungeklart | immer primaer |
| `Y4-009` / dritter abnormaler Restart, offene SAFE_BOOT-Episode | vor Aktor-/Lauffreigabe `SAFE_BOOT` | ja / kein Auto-Rearm durch Reboot | bewusster autorisierter Exit nach allen Checks | kein automatischer zusaetzlicher Reboot; nur zugrunde liegende Codepolicy darf technischen Restart verlangen | primaer; ausloesende Latches bleiben |

Die Tabelle ist die vollstaendige R3-Policy: vier Klassen, keine reservierten
historischen Luecken und kein alter `Y4-011`-Fallback. `AirLimitReduced` bleibt
eine normale #22-Regelbegrenzung und erzeugt keinen P1-Fault. Die urspruengliche
#22-`TemperatureControlReason` bestimmt die Projektion; #23 `NoValidRequest`
ist nur die sichere Plannerklassifikation und kein eigener O2-Producer. Jede
nicht eindeutig aufloesbare Ursache wird als `Y4-008` beziehungsweise als
fail-closed Capacity-/Persistenzfall behandelt.

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
  werden. Die Entscheidung steht in der finalen Code-/Producer-Matrix oben.
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

## Durch R3 fachlich geschlossen

- Fehlerklasse, stabile Code-ID und die gekennzeichnete Zuordnung zu realem
  Producer, #24-interner Ursache oder stabilem Contract-/Injection-Producer
- unmittelbare sichere Reaktion, Latch, Auto-Rearm und Resetberechtigung je Code
- codebezogene Reboot- und SAFE_BOOT-Policy einschließlich einmaligem
  automatischem Recovery-Restart
- fail-closed Behandlung von Unknown/Unresolved, Persistenzunsicherheit und
  Capacity-Overflow außerhalb der aktiven Slotliste
- Primary-/Follow-up-Beziehung ohne Unterdrückung der eigenen Safetyreaktion

## Weiterhin offen fuer Phase 8B, 8C, #19, #29 und Inbetriebnahme

- reale Temperatur-, Warn-, Eingriffs- und harte Notgrenzwerte
- Messschwellen, Sensor-/Luefter-/Aktorerkennung sowie Pin-, Signal- und
  Rueckmeldemoeglichkeiten
- konkrete Hardware- und ESP-IDF-Resetursachen sowie Brownoutverifikation
- #35-Commissioningwerte und die tatsaechliche zulässige Recoveryversuchszahl
  innerhalb der firmwarefesten Grenze `0..2`
- echte Service-/PIN-Verifikation und technische Exitnachweise
- #19-Aufbewahrung, Bereinigung, Verdichtung und Langzeitjournal
- konkrete Speicher-/Stack-/Flash-Ressourcenmessung und Hardwaretests
- Display-, Touch-, WLAN- und Webadapter außerhalb des lokalen Safetykerns
