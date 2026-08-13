# Laufkommandos, Meldungen und Bedienaktionen

## Status

Dieses Dokument legt die fuer Issue #15 implementierte fachliche Semantik fuer
Laufkommandos, Meldungen und konkurrierende Bedienaktionen fest.

Es ergaenzt insbesondere:

- [`STATE_MACHINE.md`](STATE_MACHINE.md)
- [`PROGRAMS.md`](PROGRAMS.md)
- [`RUNTIME_BEHAVIOR.md`](RUNTIME_BEHAVIOR.md)
- [`LOCAL_RUNTIME_UI.md`](LOCAL_RUNTIME_UI.md)
- [`TEMPERATURE_CONTROL.md`](TEMPERATURE_CONTROL.md)
- [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md)

## Architekturgrenze

Die Kommandoschicht ist Teil von `lib/fermentation_app/`, weil sie konkrete
Fermentationslaeufe, Prozesszustaende, Laufanpassungen und Bedienaktionen kennt.

Sie bleibt:

- hardwareunabhaengig
- persistenzfrei
- unabhaengig von Display, Web, WLAN und Arduino
- im Profil `native` deterministisch testbar

Display und Web sind spaeter nur Adapter derselben fachlichen Kommandoschicht.
Es entsteht kein allgemeines Command-Framework in `device_platform`.

Die Kommandoschicht erzeugt weder GPIO-Pegel noch direkte Aktorfreigaben. Sie
liefert ausschliesslich fachliche Entscheidungen, Meldungen und abstrakte
Wirkungsabsichten.

## Zweistufiges Entscheidungsmodell

Alle lauf-, meldungs- oder verriegelungswirksamen Kommandos werden zweistufig
verarbeitet:

```text
aktuellen fachlichen Stand und Kommando validieren
  -> noch nicht angewendete CommandDecision erzeugen
  -> spaeter in #17 Transaktionsabsicht und neue Revision atomar speichern
  -> Entscheidung bestaetigen und anwenden
  -> erst danach neue Aktoranforderungen ableiten
```

Die Entscheidungsfunktion veraendert weder Laufzustand, Laufrevisionen,
Meldungszustand noch Verriegelungen.

Eine Entscheidung darf nur angewendet werden, wenn:

- ihr erwarteter Ausgangszustand noch aktuell ist
- die erwarteten Sequenzen und Revisionen noch uebereinstimmen
- sie nicht bereits verworfen oder angewendet wurde
- der zugehoerige Lauf- oder Fehlerkontext weiterhin gueltig ist

Schlaegt die spaetere Persistenz fehl, bleibt der bisherige fachliche Zustand
wirksam. Es entsteht keine neue Aktorfreigabe.

Issue #15 implementiert noch keine produktive Persistenz. Native Tests duerfen
einen einfachen In-Memory-Treiber verwenden, der Entscheidungen bewusst
verwirft oder bestaetigt.

## Gemeinsamer Kommando-Umschlag

Ein fachliches Kommando besitzt mindestens:

- eine eindeutige Kommando-ID
- die Quelle, mindestens lokales Display oder Web
- einen monotonen Zeitbezug
- die erwartete Zustandssequenz
- soweit relevant die erwartete Laufrevision
- soweit relevant die erwartete Meldungs- oder Fehlerrevision
- die fachlichen Eingabedaten
- bei kritischen Benutzeraktionen eine ausdrueckliche Bestaetigung

Die konkrete C++-Benennung bleibt Implementierungsdetail. Die Semantik dieser
Felder ist verbindlich.

## Konflikte und Idempotenz

Lokales Display und Web sind fachlich gleichberechtigt. Es gibt keine pauschale
Quellenprioritaet.

Verbindliche Regeln:

1. Das erste gueltig angewendete Kommando gewinnt.
2. Ein weiteres Kommando, das auf einer veralteten Zustands-, Lauf-, Meldungs-
   oder Fehlerrevision basiert, wird eindeutig als Konflikt beziehungsweise
   veralteter Stand abgelehnt.
3. Ein abgelehntes Kommando veraendert keine fachlichen Daten.
4. Eine bereits verarbeitete Kommando-ID wird nicht nochmals ausgefuehrt,
   solange ihre ID noch im begrenzten, gleitenden In-Memory-Fenster der
   zuletzt verarbeiteten Kommandos steht (siehe `run_command_limits`). Dieses
   Fenster ist bewusst kein unbegrenztes Verlaufsprotokoll: eine verdraengte,
   aeltere ID kann nicht mehr als Wiederholung erkannt werden. Eine
   laengerfristige, sitzungsuebergreifende Wiederholungserkennung ist kein
   Bestandteil von Issue #15 und folgt mit #27; persistierte
   Kommandoatomaritaet folgt mit #17.
5. Eine Wiederholung derselben Kommando-ID liefert dasselbe fachliche Ergebnis
   beziehungsweise einen eindeutig als Wiederholung erkennbaren Erfolg, solange
   sie noch innerhalb des Fensters aus Punkt 4 liegt.
   Die gemeinsame Umschlag- und Idempotenzpruefung erfolgt vor der fachlichen
   Auswertung einer Stopoption. Deshalb bleibt auch eine Wiederholung mit einem
   unbekannten oder fehlerhaft deserialisierten Stopwert als `AlreadyProcessed`
   erkennbar; nur eine frische ID mit unbekanntem Stopwert ist `InvalidInput`.
6. Sicherheitsereignisse sind keine konkurrierenden Komfortkommandos. Eine
   kritische Sicherheitsentscheidung hat immer Vorrang.
7. Transportwiederholungen, Anmeldung und konkrete Web-Protokolle folgen in
   Issue #27. Issue #15 definiert nur die fachliche Semantik.
8. Jeder fuer Konflikterkennung genutzte Revisionszaehler
   (Lauf-, Meldungs- und Fehlerrevision sowie die Revision einzelner
   Laufzeitmeldungen) wird vor jeder Erhoehung auf seine Kapazitaetsgrenze
   geprueft. Ein bereits an der Grenze stehender Zaehler liefert `Kapazitaet
   oder erforderlicher Kontext fehlt` ohne jede Teilwirkung, statt still
   ueberzulaufen.

Mindestens unterscheidbare Ergebnisarten sind:

- vorgeschlagen
- angewendet
- keine Aenderung
- nicht bestaetigt
- im aktuellen Zustand unzulaessig
- ungueltige Eingabe
- Konflikt oder veralteter Stand
- bereits verarbeitet
- Sicherheitsfreigabe fehlt
- Kapazitaet oder erforderlicher Kontext fehlt

## Bestaetigter Laufstart

Vor jedem normalen Start wird aus den validierten Eingaben eine
Startzusammenfassung erzeugt. Sie enthaelt mindestens die in
`LOCAL_UI.md` und `LOCAL_RUNTIME_UI.md` festgelegten Angaben.

Erst das bestaetigte Startkommando darf:

- einen unveraenderlichen Laufschnappschuss beziehungsweise manuellen Laufplan
  vorschlagen
- den fachlichen Startuebergang der Zustandsmaschine anfordern
- die fuer die spaetere Persistenz benoetigten Ereignisse liefern

Fehlende oder ungueltige Pflichtwerte, ein veralteter Katalogstand, ein
unzulaessiger Prozesszustand oder eine fehlende Sicherheitsfreigabe fuehren zu
einer Ablehnung ohne Teilwirkung.

Die Reihenfolge ist verbindlich: Umschlag-, Revisions-, Zustands-,
Sicherheits- und Eingabepruefung sowie die vollstaendige Planvalidierung laufen
immer zuerst und unabhaengig von der Bestaetigung. Erst danach entscheidet die
Bestaetigung, ob eine bereits vollstaendig gueltige Anfrage als `NotConfirmed`
mit Startzusammenfassung zurueckgegeben oder tatsaechlich angewendet wird. Eine
ungueltige, veraltete oder sicherheitsseitig abgelehnte Anfrage wird niemals
durch eine fehlende Bestaetigung maskiert; sie liefert immer ihren eigenen
Ablehnungsgrund (`InvalidInput`, `StaleState`, `SafetyRejected`, ...), auch
wenn `confirmed == false` gesetzt ist.

### Programmaufloesung und Katalogstand

Das interne `ProgramStartRequest` traegt ein bereits fachlich aufgeloestes,
vertrauenswuerdiges Programmdokument. Die Kommandoschicht prueft nur noch
Lauf-, Sicherheits- und Werteregeln, nicht aber, ob dieses Dokument weiterhin
dem aktuellen Katalog- oder Programmstand entspricht.

Vor einer echten UI- oder Web-API-Anbindung (#16, #27) darf ein Aufrufer kein
frei mitgeliefertes Programmdokument ungeprueft als Startvertrag einliefern.
Die aufrufende Schicht muss die Programmdefinition stattdessen ueber
Programm-ID und erwartete Katalog- beziehungsweise Programmrevision aus der
aktuellen `RuntimeConfigurationSnapshot` (#16) aufloesen. Ist das Programm
seither geaendert oder geloescht worden, ist das Ergebnis `StaleState` oder ein
typisierter Aufloesungsfehler der aufrufenden Schicht - kein stiller Fallback
auf einen veralteten oder erfundenen Stand. Der aktive Lauf erhaelt danach
weiterhin seinen eigenen unveraenderlichen Schnappschuss, unabhaengig von
spaeteren Katalogaenderungen.

Issue #15 fuehrt weder `ConfigurationService` noch eine provisorische
Katalogpersistenz ein.

## Manueller Laufplan

Manuelles Temperaturhalten und `Abbrechen und kuehlen` verwenden denselben
unveraenderlichen manuellen Laufplan.

Der Laufplan enthaelt mindestens:

- eindeutige Lauf-ID
- Zieltemperatur
- ausgewaehlten Sensorbetrieb
- Vorheizen EIN oder AUS
- maximale Produktwartezeit nur bei Vorheizen
- Zielband und Qualifikationsdauer
- Laufart `ManualHolding`
- Quelle und Erstellungszeit

Er enthaelt keine Fermentationsdauer. Der Lauf endet durch eine bewusste
Benutzeraktion oder einen Fehler.

Der kanonische Zustandsweg bleibt:

```text
mit Vorheizen:
  STANDBY -> PREHEATING -> WAITING_FOR_PRODUCT
          -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING

ohne Vorheizen:
  STANDBY -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING
```

Ob zur Zielerreichung geheizt oder gekuehlt wird, entscheidet spaeter die
Regel- und Sicherheitslogik aus aktuellem Messwert und Zielwert. Es wird kein
eigener Zustand `MANUAL_COOLING` eingefuehrt.

## Stoppen und Abbrechen

`STOP` selbst oeffnet nur den bereits spezifizierten Auswahl- und
Bestaetigungsdialog.

Ein unbekannter oder fehlerhafter Auswahlwert wird nicht stillschweigend als
`Abbrechen und ausschalten` behandelt. Er wird als ungueltige Eingabe
abgelehnt, ohne Zustandsuebergang, Laufabbruch, Wirkungsabsicht oder sonstige
Mutation.

### Zurueck

`Zurueck` verwirft den Dialog. Lauf, Meldungen, Zustandsmaschine und
Aktoranforderungen bleiben unveraendert.

### Abbrechen und ausschalten

Die Entscheidung schlaegt atomar vor:

- den aktuellen Lauf als abgebrochen zu markieren
- den Benutzerabbruch als fachliches Ereignis zu erzeugen
- den vorgesehenen Uebergang nach `STANDBY` anzufordern
- eine abstrakte Absicht fuer sicheren Peltier-Stopp und erforderlichen
  Luefternachlauf bereitzustellen

Die konkrete Aktor- und Nachlaufsteuerung bleibt ausserhalb von Issue #15.

### Abbrechen und kuehlen

Der Benutzer bestaetigt vorab mindestens Kuehlziel und Halteverhalten.

Die CommandDecision bildet eine unteilbare fachliche Gesamtaktion:

1. bisherigen Programmlauf als abgebrochen markieren
2. Benutzerabbruch protokollierbar machen
3. neuen validierten manuellen Laufplan mit eigenem Schnappschuss erzeugen
4. den neuen `AbortAndCool`-Lauf als kanonische `NewActiveRun`-Grenze beginnen;
   PI- und Qualifier-RAM des alten Laufes werden nicht in den neuen Lauf
   uebernommen
5. den neuen Lauf ueber `REACHING_TARGET` beziehungsweise den kanonischen
   manuellen Zustandsweg starten

Der alte Lauf darf nicht bereits beendet werden, wenn der neue manuelle Laufplan
ungueltig ist oder nicht erzeugt werden kann. Eine spaetere Persistenz muss beide
Teile als zusammengehoerige atomare Revision behandeln.

Die Entscheidungsfunktion fuehrt beide Prozessuebergaenge ausschliesslich auf
einem lokalen Kandidatenzustand aus. Erst wenn Abbruch und manueller Neustart
vollstaendig erfolgreich sind, wird dieser Kandidat als `after` uebernommen.
Scheitert der zweite Uebergang beispielsweise an Prozesszeit, Sequenzkapazitaet
oder Snapshot-Invarianten, bleibt `after` strukturell identisch zu `before` und
es werden keine Wirkungsabsichten ausgegeben.

## Laufanpassungen

Zieltemperatur und verbleibende Dauer werden nur ueber eine ausdrueckliche
Laufaktion mit Vorschau und Bestaetigung geaendert.

Die Vorschau zeigt mindestens:

- alten Wert
- neuen Wert
- betroffene Laufphase
- erkennbare fachliche Auswirkung
- Hinweis auf erneute Zielqualifikation, soweit erforderlich
- Hinweis, wenn keine automatische biologische Zeitkorrektur erfolgt

Der Programmschnappschuss und das gespeicherte Quellprogramm bleiben
unveraendert. Die bestaetigte Aenderung erzeugt eine protokollierbare
append-only Laufrevision mit Quelle und Zeitbezug.

Auch die Kombination aus neuer `ActiveRun`-Revision, neuem
`ProcessRunSnapshot` und gegebenenfalls erforderlichem `TargetChanged`-
Prozessuebergang wird zuerst vollstaendig auf einem lokalen Kandidatenzustand
validiert. Eine spaete Ablehnung verwirft den gesamten Kandidaten; insbesondere
bleiben Zielwert, Revisionshistorie, Prozesszustand und Prozessschnappschuss in
`after` unveraendert.

Die Kommandoschicht bildet den aktuellen Prozesszustand auf den kleinen, von
`ProcessState` unabhaengigen `RunAdjustmentPhaseContext`
(`BeforeFermentation` oder `Fermenting`) ab; `ActiveRun` selbst kennt die
Zustandsmaschine nicht und lehnt unbekannte Kontextwerte ab. Der Phasenkontext
ist nur Eingabe der Entscheidung und wird nicht in `RunRevision` persistiert.
Die persistierbare Revision speichert stattdessen die daraus abgeleitete,
typisierte Wirkung (`RunAdjustmentEffect`: `None`,
`RestartTargetQualification` oder
`ContinueFermentationWithoutRequalification`). Eine spaetere Wiederherstellung
kann daher unbekannte oder widerspruechliche Wirkungen ablehnen, aber keinen
nicht gespeicherten Phasenkontext nachtraeglich pruefen: eine unveraenderte
Zieltemperatur mit gesetzter Requalifizierungswirkung, eine geaenderte
Zieltemperatur ohne oder mit unbekannter Wirkung sowie jeden unbekannten
Effekt-Enumwert werden abgelehnt.

### Zieltemperatur vor der Fermentationsphase

Bei einer Zieltemperaturaenderung in `PREHEATING` oder `REACHING_TARGET` wird
die laufende Zielerreichung mit dem neuen Zielwert neu bewertet. Bei einer
Aenderung in `QUALIFYING_TARGET` wird die bereits teilweise absolvierte
Qualifikationszeit verworfen und die Zielerreichung mit dem neuen Zielwert neu
gestartet (Ruecksprung nach `REACHING_TARGET`). In keinem dieser drei Faelle
wird Qualifikationszeit auf das neue Ziel uebertragen.

### Zieltemperatur waehrend `FERMENTING`

Eine bestaetigte Zieltemperaturaenderung waehrend `FERMENTING`:

- laesst den Prozesszustand in `FERMENTING`
- laesst die verbleibende Dauer ohne Unterbrechung weiterlaufen
- setzt den neuen Sollwert erst nach Anwendung der CommandDecision in Kraft
- erzeugt keine blockierende erneute Zielqualifikation
- berechnet keine automatische Laufzeitkorrektur

Die Vorschau weist ausdruecklich darauf hin, dass der biologische Prozess
weiterlaeuft und Release 1 aus der Sollwertaenderung keine erfundene
biologische Zeitkorrektur ableitet.

Nach erfolgreichem Persistenz-/Apply-Pfad wird fuer den fluechtigen #22-Kern
eine `TargetContextChange`-Commit-Information bereitgestellt. Bei einem
fehlgeschlagenen Persistenz- oder Apply-Pfad entsteht keine solche Information;
der alte Ziel- und Integratorkontext bleibt wirksam. Die Information ist kein
neues Lauf- oder Wirefeld.

Eine spaetere temperaturgewichtete Fortschrittskorrektur bleibt Issue #18 und
der Inbetriebnahme vorbehalten.

### Reine Restdaueranpassung

Eine reine Aenderung der verbleibenden Dauer loest keine neue
Zielqualifikation aus. Die Restdauer darf auf null gesetzt werden, sofern der
aktuelle Zustand und die bestehende Laufsemantik einen unmittelbaren Abschluss
zulassen.

Bereits abgeschlossene Phasen werden nicht rueckwirkend umgedeutet.

### Nicht passende Phasen

Aenderungen von Fermentationsziel oder Fermentationsrestzeit werden in
`COOLING`, `COOL_HOLDING`, `MANUAL_HOLDING`, `COMPLETED`, `FAULT`,
`SAFE_BOOT`, `RECOVERY_EVALUATION` und `SERVICE_MODE` abgelehnt.

Eine spaetere eigene Zielwertanpassung fuer einen bereits laufenden manuellen
Haltebetrieb ist nicht Bestandteil von Issue #15. Der manuelle Lauf kann sicher
beendet und mit einem neuen bestaetigten Laufplan gestartet werden.

## Abschluss

`COMPLETED` bleibt ein fachlich abgeschlossener Lauf, der auf eine bewusste
Quittierung wartet.

`AcknowledgeCompletion` darf nur aus `COMPLETED` entschieden werden. Nach
spaeterer erfolgreicher Persistenz fuehrt seine Anwendung nach `STANDBY`.

`Jetzt kuehlen` erzeugt wie `Abbrechen und kuehlen` einen neuen manuellen Lauf
mit eigenem Laufplan. Der abgeschlossene Ursprungslauf bleibt unveraendert als
abgeschlossen protokolliert.

Quittierung und anschliessender manueller Kuehlstart werden ebenfalls nur als
vollstaendig erfolgreicher Kandidatenzustand uebernommen. Scheitert der zweite
Uebergang, bleibt der abgeschlossene Ursprungslauf in `after` unangetastet.

## Meldungsmodell

Issue #15 definiert fachliche Meldungen und akustische Absichten, keine
konkreten Displaytexte, Summerfrequenzen oder GPIO-Signale.

Eine Meldung besitzt mindestens:

- stabilen maschinenlesbaren Meldungscode
- Meldungsklasse und Prioritaet
- Quelle beziehungsweise fachlichen Ausloeser
- monotonen Zeitbezug
- Aktiv-, quittiert- und erledigt-Status
- Angabe, ob eine Benutzerentscheidung erforderlich ist
- abstrakte akustische Absicht
- soweit relevant eine Lauf-, Zustands- oder Fehlerreferenz

Die Prioritaetsreihenfolge aus `RUNTIME_BEHAVIOR.md` bleibt verbindlich.
Niedrigere Meldungen werden nicht geloescht, nur weil eine hoehere prominent
angezeigt wird.

## Quittieren und Stummschalten

`Quittieren` und `Stummschalten` sind getrennte Kommandos.

`Quittieren`:

- bestaetigt nur die Wahrnehmung
- veraendert die zugrunde liegende Ursache nicht
- loest keine Verriegelung
- entfernt keine weiterhin aktive Warnung oder Fehlerursache
- erzeugt eine protokollierbare Meldungsrevision

`Stummschalten`:

- beendet beziehungsweise unterdrueckt nur die aktuell zulaessige akustische
  Wiederholung
- veraendert sichtbare Meldung, Ursache und Verriegelung nicht
- kann durch eine neue Meldung hoeherer Prioritaet ueberstimmt werden

Eine Quittierung der prominentesten Meldung quittiert nicht automatisch andere
aktive Meldungen.

## Fehler zuruecksetzen

Issue #15 definiert das fachliche Kommando `Fehler zuruecksetzen` und dessen
Ergebnisstruktur, aber keine konkrete Fehlercode-, Sensor-, Hardware- oder
Berechtigungspolitik.

Das Kommando verwendet eine aktuelle, bereits qualifizierte
Resetfreigabebewertung der spaeteren Sicherheitslogik. Diese Bewertung drueckt
mindestens aus:

- ob der Reset erlaubt ist
- ob die Ursache weiterhin aktiv ist
- ob erforderliche Sicherheitspruefungen bestanden sind
- ob die erforderliche Berechtigung erfuellt ist
- auf welche Fehlerrevision sich die Bewertung bezieht
- einen stabilen Ablehnungsgrund

Ohne positive und noch aktuelle Resetfreigabe wird das Kommando abgelehnt.

Issue #15:

- liest keine realen Sensoren oder GPIOs
- kennt keine konkrete Service-PIN
- erfindet keine Fehlercode-Matrix
- loest niemals durch `Quittieren` eine Verriegelung

Die Erzeugung der Resetfreigabebewertung und die vollstaendige Resetpolitik
bleiben Issue #24. Die atomare Speicherung einer erfolgreichen Resetentscheidung
bleibt Issue #17.

## Tests fuer Issue #15

Native Tests decken mindestens ab:

- bestaetigten und nicht bestaetigten Laufstart
- ungueltige und veraltete Startdaten
- alle Stopoptionen einschliesslich vollstaendig atomarem `Abbrechen und kuehlen`
- manuellen Laufplan mit und ohne Vorheizen
- Berechnen, Verwerfen und Anwenden jeder laufwirksamen Entscheidung
- Konflikte zwischen Display und Web ohne Quellenprioritaet
- Wiederholung derselben Kommando-ID
- veraltete Zustands-, Lauf-, Meldungs- und Fehlerrevisionen
- Zieltemperaturaenderung vor und waehrend `FERMENTING`
- weiterlaufende Restdauer bei Zielaenderung waehrend `FERMENTING`
- Restdaueranpassung in zulaessigen und unzulaessigen Phasen
- Quittieren, Stummschalten und Fehlerreset als getrennte Aktionen
- erlaubte und abgelehnte deterministische Resetfreigabebewertungen
- keine direkte Persistenz, Hardwarewirkung oder Abhaengigkeit von Display/Web
- phasengerechte Laufrevision bei Zielaenderung (vor der Fermentationsphase je
  Einzelphase, waehrend `FERMENTING`, reine Restdaueranpassung) sowie
  Ablehnung widerspruechlicher oder unbekannter Phasen-/Wirkungswerte bei der
  Wiederherstellung
- Kapazitaetsgrenze der Lauf-, Meldungs- und Fehlerrevision sowie der
  einzelnen Meldungsrevision: Ablehnung ohne Teilwirkung an der Grenze, genau
  eine weitere Erhoehung unterhalb der Grenze
- Ablehnung eines unbekannten `StopOption`-Werts ohne Mutation
- Idempotenzpruefung vor Stopwertauswertung: Wiederholung einer bereits
  angewendeten Stop-ID bleibt auch mit unbekanntem Stopwert `AlreadyProcessed`
- vollstaendiger struktureller `before`-/`after`-Vergleich fuer Ablehnungen,
  einschliesslich spaeter Fehler nach einer ersten erfolgreichen Teiloperation
  bei Laufanpassung, `Abbrechen und kuehlen` und `Jetzt kuehlen`
- Verfuegbarkeit der Startzusammenfassung fuer eine gueltige, aber noch nicht
  bestaetigte Anfrage, ohne dass eine fehlende Bestaetigung eine ungueltige,
  veraltete oder sicherheitsseitig abgelehnte Anfrage maskiert

## Abgrenzung zu Folge-Issues

- #16 implementiert `ConfigurationService` und die Programmaufloesung ueber
  Katalog- beziehungsweise Programmrevision aus der
  `RuntimeConfigurationSnapshot`; Issue #15 nimmt dies nicht vorweg.
- #17 implementiert atomare Laufpersistenz, Kontrollpunkte und
  Persistenz-vor-Anwendung.
- #18 implementiert Wiederanlauf und temperaturgewichteten Fortschritt.
- #24 implementiert Fehlerklassen, Verriegelungen und die konkrete
  Fehlerresetfreigabe.
- #25 und #26 implementieren gemeinsame UI-Modelle und die lokale Oberflaeche.
- #27 implementiert Webtransport, Anmeldung, konkrete Webkonfliktbehandlung
  sowie sitzungsgebundene Transportwiederholungserkennung ueber das in Issue
  #15 nur begrenzt gleitende In-Memory-Idempotenzfenster hinaus.

Issue #15 darf diese Folge-Issues nicht mit provisorischen Hardware-,
Persistenz-, Sicherheits- oder Transportimplementierungen vorwegnehmen.

## Implementierungszuordnung

Die produktive, hardwareunabhaengige Umsetzung liegt in:

- `lib/fermentation_app/src/run_commands.hpp/.cpp` fuer Kommando-Umschlaege,
  manuelle Laufplaene, Startzusammenfassungen, Meldungen, Resetbewertungen und
  zweistufige `CommandDecision`s
- `lib/fermentation_app/src/run_command_limits.hpp` fuer die festen Grenzen der
  verarbeiteten Kommando-IDs, Laufzeitmeldungen und Wirkungsabsichten; die
  Idempotenz-IDs bilden ein gleitendes Fenster und blockieren den Betrieb bei
  erreichter Puffergrenze nicht
- `lib/fermentation_app/src/run_snapshot.hpp/.cpp` fuer die ebenfalls
  zweistufige Entscheidung und Anwendung einer append-only Laufrevision
- `lib/fermentation_app/src/process_state_machine.hpp/.cpp` fuer die explizite
  Neubeurteilung nach einer Zielaenderung vor `FERMENTING`

`test/test_run_commands/` prueft die Kommando- und Konfliktsemantik;
`test/test_run_snapshots/` prueft zusaetzlich, dass eine berechnete
Laufanpassung bis zur Anwendung keine Mutation erzeugt und eine inzwischen
veraltete Entscheidung nicht uebernommen wird.

Die Kommandoschicht liefert nur abstrakte Wirkungsabsichten wie sicheren
Peltier-Stopp oder Luefternachlauf. Persistenz, konkrete Aktorsteuerung,
Resetpolitik, UI und Transport bleiben gemaess obiger Abgrenzung bei den
Folge-Issues.
