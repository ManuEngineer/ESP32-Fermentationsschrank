# Betriebszustaende und Zustandsmaschine

## Status

Dieses Dokument definiert die kanonischen Betriebszustaende und Uebergaenge fuer
Release 1. Ergaenzende Detailregeln stehen in
[`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md),
[`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md) und
[`SYSTEM_SAFETY_AND_RECOVERY.md`](SYSTEM_SAFETY_AND_RECOVERY.md).

## Grundsaetze

- Jeder Zustand hat einen klar definierten Zweck.
- Aktoren duerfen nur in Zustaenden aktiv sein, in denen dies ausdruecklich
  vorgesehen ist.
- Ein Zustandswechsel wird validiert, atomar gespeichert und protokolliert,
  bevor daraus eine neue Aktorfreigabe entsteht.
- Direkte GPIO- oder H-Brueckenzustaende sind nie Teil des fachlichen
  Wiederanlaufzustands.
- Es gibt keine allgemeine Pausenfunktion fuer laufende Fermentationen.
- Warnungen, Fehler, Quittierung und Fehlerreset bleiben getrennt.
- Ein manueller Lauf verwendet dieselben Regel- und Sicherheitsmechanismen wie
  ein gespeichertes Programm.
- Direkte Aktortests sind ausschliesslich im geschuetzten `SERVICE_MODE` aus
  validiertem `STANDBY` zulaessig.
- Die Bootklassifikation wartet nicht blockierend auf Netzwerkzeit oder NTP.
  Ein vollstaendig validierter Current-`FERMENTING`-Run wird nach dem #124-
  Zeitvertrag automatisch logisch fortgesetzt; dafuer ist keine
  Benutzerbestaetigung allein wegen des Stromausfalls erforderlich. Das ist
  keine Aktorfreigabe: Write-before-Apply, FSM-Anwendung und frische
  Configuration-/Sensor-/Hardware-/Safety-/Planner-Evidenz bleiben vor jeder
  Freigabe erforderlich. Fehlt UTC, bleibt `RecoveryEvaluation` mit der
  RAM-Disposition `WaitingForTrustedTime` bestehen; untrusted Persistenz
  bleibt `SAFE_BOOT`.
- Ein echter Sicherheitsfehler hat immer Vorrang vor automatischem Fortfahren.

## Grenze des fachlichen Zustandsautomaten

Der fachliche Zustandsautomat ist deterministisch, hardwarefrei und
persistenzfrei. Er erhaelt:

- den aktuellen fachlichen Zustand
- einen unveraenderlichen Laufschnappschuss
- abstrahierte, bereits qualitaetsgepruefte Prozesssignale
- fachliche Ereignisse beziehungsweise Benutzerentscheidungen
- monotone virtuelle Zeit

Er liefert eine noch nicht angewendete Uebergangsentscheidung mit bisherigem und
vorgeschlagenem Zustand, Grund beziehungsweise Ereigniscode, relevantem
monotonem Zeitpunkt, aktualisierten fachlichen Phasendaten und gegebenenfalls
fachlichen Meldungen.

Die Berechnung veraendert den bisherigen Zustand nicht unumkehrbar. Erst der
aufrufende Anwendungsteil darf die Entscheidung nach erfolgreicher atomarer
Speicherung bestaetigen und anwenden. Schlaegt die Speicherung fehl, bleibt der
bisherige Zustand wirksam und es entsteht keine neue Aktorfreigabe. Speicherung,
Kontrollpunkte, Rueckfallrevisionen und kritische Schreibfehler sind nicht Teil
des fachlichen Zustandsautomaten.

Die Implementierung verwendet dafuer zwei getrennte Schritte:

1. `decideProcessTransition()` liefert eine vollstaendige, noch nicht angewendete
   Entscheidung mit Ausgangs- und Folgezustand.
2. `applyProcessTransition()` uebernimmt sie nur, wenn der vollstaendige
   Ausgangszustand weiterhin uebereinstimmt und der uebergebene unveraenderliche
   Laufschnappschuss zu Ausgang, Ziel und Uebergangsgrund passt. Veraltete,
   bereits angewendete oder zum Laufkontext widerspruechliche Entscheidungen
   werden ohne Zustandsaenderung abgelehnt. Eine kritische Sicherheitsabschaltung
   bleibt davon ausgenommen und darf nie an fehlendem oder beschaedigtem
   Laufkontext scheitern.

Kritische Fehler werden vor Bedienereignissen und normalen Phasenfortschritten
ausgewertet. In `WAITING_FOR_PRODUCT` hat der belastbar erreichte Zeitablauf
Vorrang vor einer gleichzeitig eintreffenden Produktbestaetigung.

## Issue #124 Release-1-Recovery-Grenze

`RECOVERY_EVALUATION` ist der technische FSM-Zustand fuer die
Recoverybewertung. Fuer einen Current-`FERMENTING`-Run gibt es darunter die
Application-Disposition `WaitingForTrustedTime`,
`CurrentRunRecoverable` oder `RecoveryRejectedOrFailClosed`. Die Disposition
ist kein neuer FSM-Hauptzustand und wird nicht persistiert.

Bei exakter `priorBootPhaseElapsed`-Basis, gueltigem Checkpointgraph und
vertrauenswuerdiger UTC wird die Wall-Clock seit dem Checkpoint zur aktuellen
Fermentationszeit gerechnet. Unterhalb der nominellen Dauer wird der
Current-Run logisch automatisch als `FERMENTING` weitergefuehrt; bei erreichter
Dauer verwendet die FSM die normale `FermentationCompleted`-Semantik.
`runProgress.observedRunSeconds` bleibt davon getrennt und enthaelt nur
beobachtete Laufzeit. Die UTC-Differenz ist keine hergeleitete exakte
physische Ausfalldauer.

Fehlt UTC unmittelbar nach Boot, bleibt die geladene Evidenz unveraendert in
`RecoveryEvaluation/WaitingForTrustedTime`; es gibt keinen Tombstone und
keinen Persistenzschreibvorgang nur fuer das Warten. Eine spaetere Bewertung
verwendet dieselbe Revision; stale Evidenz wird fail-closed abgelehnt.
Technisch untrusted Persistenz bleibt `SAFE_BOOT` und wird nicht durch eine
neue Tombstone-Mutation verdeckt. Automatische Fallback-Promotion bleibt
ausgeschlossen.

Historische komplexe Recoverykontexte bleiben als #17/#18-Legacy dokumentiert,
sind aber kein aktiver #124-R1-Produktpfad.

## Kanonische Zustandsnamen

```text
BOOT
SAFE_BOOT
STANDBY
PREHEATING
WAITING_FOR_PRODUCT
REACHING_TARGET
QUALIFYING_TARGET
FERMENTING
COOLING
COOL_HOLDING
MANUAL_HOLDING
COMPLETED
RECOVERY_EVALUATION
FAULT
SERVICE_MODE
```

Zusaetzliche Kontexte:

```text
RECOVERY_TIME_PENDING
WARNING_REQUIRES_ACTION
```

`RECOVERY_TIME_PENDING` ist im aktuellen #124-Vertrag die semantische
Application-Disposition `WaitingForTrustedTime` unter
`RECOVERY_EVALUATION`; es ist kein neuer persistierter Prozesszustand und
ueberlagert keinen laufenden Regelzustand. `WARNING_REQUIRES_ACTION` ist ein
separater fachlicher Kontext und kein Ersatz fuer diese Recovery-Disposition.

## BOOT

### Zweck

Der ESP32 startet mit ausgeschalteten Aktoren, klassifiziert Konfiguration und
Laufstand und wählt erst danach den nächsten Zustand. Die Klassifikation ist
von der späteren Aktorpermission getrennt.

### Verbindliche Reihenfolge

```text
BOOT
  -> Peltier und beide BTS7960-Richtungen AUS
  -> alle schaltbaren Ausgaenge zunaechst AUS
  -> Resetcause nur diagnostisch erfassen; keine Restart-Akkumulation
  -> Konfigurationstrust validieren
  -> Persistenz laden und Integritaet/Transaktionsstatus auswerten
  -> gespeicherten Laufzustand klassifizieren
  -> optional Current-FERMENTING gegen den #124-Zeitvertrag bewerten
  -> Netzwerk und Zeitabgleich parallel vorbereiten
```

Die spätere Aktorpermission ist kein Boot-Klassifikationsschritt. Sie verlangt
jeweils frische Konfigurations-, Persistenz-, Sensor- und Planner-Evidenz,
einen zutreffenden expliziten Aktivierungspfad, die aktuelle stateless
`ActuationInterlock`-Bewertung sowie, soweit zutreffend, die owning
Hardware-/Adapter-Gates. Bis dahin bleibt die Permission `Unresolved`.

### Uebergaenge

```text
aktuell untrusted System-/Config-/Persistenzzustand, unvollstaendige
Transaktion oder unbekannter Producer
  -> SAFE_BOOT

historischer `Current` im terminalen `Fault`
  -> `FAULT`/TerminalFault ohne Aktivierung

gueltiger persistierter Zustand COMPLETED
  -> COMPLETED

gueltiger Current mit R1-qualifizierbarer Phase
  -> RECOVERY_EVALUATION ohne Freigabe

kein aktiver oder abgeschlossener Lauf nach gültiger Klassifikation
  -> STANDBY
```

`STANDBY`, `COMPLETED` und `RECOVERY_EVALUATION` sind vor Abschluss der
Bootklassifikation nicht erreichbar; daraus folgt keine Aktorpermission. Ein Neustart ist kein Fehlerreset; R1 fuehrt
keine allgemeine persistente Verriegelung und keine Restart-Akkumulation ein.

## SAFE_BOOT

### Zweck

Sicherer Zustand bei aktuell nicht vertrauenswuerdigem System-, Config- oder
Persistenzzustand, unvollstaendiger Transaktion oder unbekanntem Producer.
Wiederholte Neustarts und persistierte allgemeine Sperren sind keine R1-
Safety-Wahrheit.

### Aktoren

- Peltier AUS
- beide BTS7960-Richtungen AUS
- keine Heiz-, Kuehl-, Luefter- oder Summer-Aktortests
- nur ein sicherheitsbedingt erforderlicher, bereits laufender Luefternachlauf
  darf nach expliziter Fehlerregel beendet werden

### Erlaubte Funktionen

- passive Diagnose
- Fehler- und Resetjournal lesen
- Berichte exportieren
- Netzwerkwiederherstellung ohne Aktorwirkung
- passive Diagnose und Export ohne Aktorwirkung
- physischer Vollreset, UART-Recovery und erneutes Flashen bleiben spaetere
  E5-/Service-/Hardware-Gates und sind kein #24-R1-Resetvertrag

Eine Rueckkehr zu `STANDBY` erfordert beseitigte aktuelle Ursache, bestandene
Config-/Persistenz-/Safety-Validierung und den fuer den konkreten FaultCode
vorgesehenen positiven Clear-Pfad. Einen allgemeinen Service-/Fehlerklassen-
Resetvertrag gibt es in R1 nicht.

## STANDBY

### Zweck

Sicherer Ruhezustand ohne laufenden Prozess.

### Aktoren

- Peltier AUS
- Aussenluefter AUS
- Innenluefter standardmaessig AUS
- Summer AUS

### Verfuegbare Funktionen

- Temperaturen und Sensorstatus anzeigen
- Programme auswaehlen und bearbeiten
- neuen Programmlauf starten
- manuellen Zeit-/Temperaturlauf starten
- manuellen Temperatur-Haltebetrieb starten
- `SERVICE_MODE` oeffnen
- Einstellungen, Diagnose und Netzwerkfunktionen verwenden

Ein neuer Lauf startet nur nach vollstaendiger Validierung des
Programmschnappschusses, der Pflichtsensoren, der Sicherheitsfreigaben und der
kritischen Persistenz sowie trusted UTC. Im konkreten Fermenter-R1-Profil
liefert die nachgewiesene lokale DS3231-Familien-RTC diese Zeit auch ohne
WLAN/Internet; ihre tatsächliche Variante bleibt bis zur Hardwarebestätigung
offen.

## PREHEATING

### Zweck

Leeren Schrank vor dem Einsetzen des Produkts auf die Programmsolltemperatur
bringen. Je nach Ausgangslage kann dies Heizen oder Kuehlen bedeuten.

```text
Ziel des leeren Schrankes erfolgreich qualifiziert
  -> WAITING_FOR_PRODUCT
```

Nach einem Neustart wird ein technisch vertrauenswuerdiger `PREHEATING`-Lauf
als Resume-Angebot dargestellt. Die Aktorfreigabe bleibt bis zur bewussten
Bestaetigung und der vollstaendigen frischen Evidenz gesperrt; ein
automatischer Resume dieser Phase ist in R1 ausgeschlossen.

## WAITING_FOR_PRODUCT

### Zweck

Vorbereitete Temperatur halten und auf das bewusste Einsetzen des Produkts
warten.

### Verhalten

- Anzeige `Produkt einsetzen`
- akustisches, quittierbares Signal
- Temperaturregelung bleibt aktiv, soweit sicher und programmgemaess
- Fermentationstimer laeuft nicht
- pro Programm konfigurierbare maximale Wartezeit

Die sichtbare und akustische Aufforderung beim Eintritt in
`WAITING_FOR_PRODUCT` ist zugleich die Warnung vor Ablauf der Wartezeit. Release
1 besitzt keine zweite, davon getrennte Warnschwelle.

`ProductInserted` ist zunaechst ausschliesslich ein Prozessereignis. Die
Zustandsmaschine waehlt daraus keine PI-Sensorrolle. Erst nach erfolgreicher
Persistenz und Anwendung wird der bereits aufgeloeste effektive Kontext
verglichen: Nur `Air -> Product` erzeugt die committed
`ProductInserted`-Kontexttransition. Ein luftgefuehrter Lauf bleibt
`Air -> Air` und erzeugt keinen kuenstlichen Rollenwechsel oder Carry/Reset.

Der effektive Temperaturregelkontext wird vor der Aufloesung aus dem aktuellen
`RunCommandState` projiziert. Dabei liefert `ActiveRun::effectiveValues()` das
aktuelle Programmsoll, ein aktiver `ManualRunPlan` das manuelle Soll und
`COOLING`/`COOL_HOLDING` ausschliesslich das Completion-Kuehlziel. Die Phasen-
und Rollenmatrix wird danach zentral genau einmal aufgeloest; Qualifier-
Eingaben werden gegen denselben Live-Kontext sowie `runRevision` und
`processTransitionSequence` validiert. Source-Programmsoll, alte Rollenwerte
oder ein allein passender Revisionswert sind keine zulaessigen Fallbacks.

### Uebergaenge

```text
Benutzer drueckt WEITER / START
  -> REACHING_TARGET

maximale Wartezeit sicher abgelaufen
  -> Vorheizen beenden
  -> Lauf als nicht gestartet oder abgebrochen protokollieren
  -> STANDBY
```

Auch nach einem Neustart darf nicht angenommen werden, dass das Produkt bereits
eingesetzt wurde. Ein vertrauenswuerdiger `WAITING_FOR_PRODUCT`-Checkpoint
wird deshalb in R1 als `NoActiveRun` ueber den bestehenden #17-Pfad beendet;
die historische Wartezeit wird nicht fortgesetzt.

## REACHING_TARGET

### Zweck

Die fuer den Lauf massgebende Temperatur auf den Sollwert bringen.

Moegliche Aktionsrichtung:

- Heizen
- Kuehlen
- Temperatur halten, falls bereits im Zielbereich

### Uebergaenge

```text
Zielbereich erreicht
  -> QUALIFYING_TARGET

maximale Zielerreichungszeit ueberschritten
  -> Warnung; Zustand laeuft weiter, solange sicher

kritischer Sensor- oder Sicherheitsfehler
  -> FAULT
```

Nach einem Neustart wird `REACHING_TARGET` in R1 nicht fortgesetzt und nicht
neu gestartet. Ein technisch vertrauenswuerdiger `Current` wird als
`NoActiveRun` beendet; die alte Reach-Zeit wird nicht rekonstruiert.

## QUALIFYING_TARGET

### Zweck

Pruefen, ob der Sollwert fuer die festgelegte Zeit ausreichend im Zielband liegt.
Die Zielqualifikation ist nicht Teil der Fermentationszeit.

```text
Ziel erfolgreich qualifiziert
  -> FERMENTING fuer einen zeitgesteuerten Lauf
  -> MANUAL_HOLDING fuer einen manuellen Temperatur-Haltebetrieb

laengere oder deutliche Abweichung
  -> REACHING_TARGET oder Neustart der Qualifikation

kritischer Fehler
  -> FAULT
```

Nach einem Neustart wird eine zuvor nur teilweise absolvierte
`QUALIFYING_TARGET`-Phase in R1 nicht reaktiviert und nicht neu gestartet. Ein
technisch vertrauenswuerdiger `Current` wird als `NoActiveRun` beendet.

Der Evaluator liefert dafuer ausschliesslich `ProcessSignals`; er entscheidet
keinen Prozessuebergang selbst. `REACHING_TARGET` darf bei jedem positiven
Qualifikationssignal, auch bei `Grace` oder `Complete`, nur nach
`QUALIFYING_TARGET` wechseln. Erst `QUALIFYING_TARGET` mit `Complete` darf
`FERMENTING` beziehungsweise `MANUAL_HOLDING` vorschlagen. `Complete` ist
hier das alleinige Abschluss-Signal und ist unabhaengig vom Alter des
Prozessmarkers. `InBand` und `Grace` schliessen auch bei einem alten Marker
nicht ab. `Unavailable`, `Invalid` und `OutsideBand` loeschen die aktuelle
Qualifikation. Ein vorgelagertes `criticalFault`-Signal behält dabei die
kanonische Prioritaet und fuehrt auch bei `Complete` nach `FAULT`; nach dem
erfolgreichen Verlassen der Qualifikationsdomaene bleibt kein Qualifier-Kredit
im RAM.

Cooling verwendet getrennt davon `coolingTargetConditionValid`. Der
Qualifikationsfortschritt, Grace und Qualifikationsdauer sind in `COOLING`
wirkungslos. `CoolThenFinish` fuehrt nach gueltigem Kuehlziel nach
`COMPLETED`; beide Cool-and-Hold-Modi fuehren nach `COOL_HOLDING`.

## FERMENTING

### Zweck

Temperaturgefuehrte Fermentationsphase mit laufendem Fortschrittsmodell.

### Regeln

- keine allgemeine Pausenfunktion
- kurze oder moderate Temperaturabweichungen stoppen den Timer nicht automatisch
- Warnungen koennen den Lauf weiterlaufen lassen
- harte Fehler oder Sicherheitsgrenzen fuehren zu `FAULT`
- ein Phasenabschluss darf nur aus belastbaren Fortschrittsdaten abgeleitet werden

### Normale Uebergaenge

```text
Fermentationsziel erreicht, Abschluss ohne Kuehlung
  -> COMPLETED

Fermentationsziel erreicht, aktives Kuehlen vorgesehen
  -> COOLING
```

### Neustartverhalten in R1

Ein vollstaendig validierter Current-`FERMENTING`-Run ist nach #124
recoveryfaehig. Die Phasenzeit am Checkpoint ist der exakte
`priorBootPhaseElapsed`-Offset plus der sicher beobachtete Live-Abschnitt.
Danach wird `wall_clock_since_checkpoint_seconds` aus der vertrauenswuerdigen
aktuellen UTC und der UTC des exakt validierten Checkpointrecords addiert.
Alle Operationen und das Narrowing sind checked; nicht exakte Bounds,
`PartialUnknownHistory`, falsche Zeitordnung oder untrusted Evidenz fuehren
fail-closed zu `RecoveryRejectedOrFailClosed`.

Unterhalb der Dauer wird der Run mit neuem `stateEnteredAtMillis` und dem
exakten wiederhergestellten `priorBootPhaseElapsed` logisch als `FERMENTING`
fortgesetzt. Die laufende FSM addiert diesen Priorwert zum neuen Live-Abschnitt
und wartet dadurch nicht nochmals die bereits verstrichene Zeit ab. Die
beobachtete Laufzeit wird separat nur um den Checkpoint-Live-Abschnitt
erhoeht. Erreicht die Phasenzeit die Dauer, gilt die normale
`FermentationCompleted`-Semantik: `FinishWithoutCooling` nach `COMPLETED`,
alle drei Kuehlmodi nach `COOLING`.

`RECOVERY_TIME_PENDING` beziehungsweise
`RecoveryEvaluation/WaitingForTrustedTime` ist kein blockierender Bootloop
und keine Aktorfreigabe. Aktoren bleiben AUS, bis die bestehenden frischen
Safetygates erfolgreich sind. Ohne frische Sensor-Evidenz wird weder
`CoolingTargetReached` behauptet noch Zeit auf einen noch nicht begonnenen
`COOL_HOLDING`-Timer angerechnet.

## COOLING

### Zweck

Produkt beziehungsweise Schrank nach der Fermentation auf die konfigurierte
Kuehlzieltemperatur bringen.

```text
Kuehlziel erreicht, danach beenden
  -> COMPLETED

Kuehlziel erreicht, danach zeitlich begrenzt oder manuell halten
  -> COOL_HOLDING

kritischer Fehler
  -> FAULT
```

Nach einem Neustart wird ein technisch vertrauenswuerdiger `COOLING`-Lauf als
Resume-Angebot dargestellt. Die Kuehlung wird nicht automatisch fortgesetzt;
erst bewusste Bestaetigung, der bestehende Write-before-Apply-Pfad und frische
Evidenz koennen eine Aktorfreigabe ergeben.

## COOL_HOLDING

### Zweck

Kuehltemperatur fuer eine festgelegte Dauer oder bis zur manuellen Beendigung
halten.

```text
belastbar bestimmte Haltezeit abgelaufen
  -> COMPLETED

Benutzer beendet Halten
  -> COMPLETED

kritischer Fehler
  -> FAULT
```

Bei unsicherer Ausfallzeit wird kein automatisches Ende aus einem einzelnen
Schaetzwert abgeleitet. Ein technisch vertrauenswuerdiger `COOL_HOLDING`-
Checkpoint wird in R1 weiterhin nicht automatisch aktiviert; weder
Haltezeit noch `RECOVERY_TIME_PENDING` setzen die Regelung ohne den geltenden
expliziten Pfad und frische Evidenz fort.

## MANUAL_HOLDING

### Zweck

Eine manuell gewaehlte Zieltemperatur ohne Timer halten.

Der Zustand nutzt dieselbe Regel- und Sicherheitslogik wie ein Programmlauf und
endet durch bewusste Benutzeraktion oder Fehler. Nach einem Neustart ist ein
Resume-Angebot nur zulaessig, wenn keine alte Dauer benoetigt wird. Auch dann
erfolgt keine automatische Fortsetzung: Bewusste Bestaetigung,
Write-before-Apply und frische Evidenz bleiben erforderlich.

Ein manueller Haltebetrieb wechselt nie direkt von `STANDBY` nach
`MANUAL_HOLDING`. Sein validierter, unveraenderlicher manueller Laufplan
durchlaeuft:

```text
mit Vorheizen:
  STANDBY -> PREHEATING -> WAITING_FOR_PRODUCT
          -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING

ohne Vorheizen:
  STANDBY -> REACHING_TARGET -> QUALIFYING_TARGET -> MANUAL_HOLDING
```

Der Laufplan und das bestaetigte Startkommando werden von der Kommandoschicht
erstellt und validiert. Der Zustandsautomat definiert und prueft den vollstaendigen
Uebergangsweg.

## COMPLETED

### Zweck

Lauf ist fachlich abgeschlossen und wartet auf Benutzerquittierung.

### Verhalten

- Peltier AUS
- definierter Luefternachlauf
- optische Meldung `PROGRAMM BEENDET`
- kurzes akustisches Signalmuster
- abgeschlossener Lauf und Ergebnisdaten bleiben sichtbar

```text
Benutzer quittiert
  -> STANDBY
```

Ein gueltig persistiertes `COMPLETED` wird beim Boot wieder als `COMPLETED`
hergestellt. Es erfolgt weder ein automatischer Wechsel nach `STANDBY` noch der
Neustart einer Regelphase.

## Manuelles Stoppen

Es gibt keine Pause. Nach `STOP` zeigt die Bedienoberflaeche:

```text
[Abbrechen und ausschalten]
[Abbrechen und kuehlen]
[Zurueck]
```

### Abbrechen und ausschalten

- laufenden Prozess als abgebrochen markieren
- Peltier AUS
- definierten Luefternachlauf ausfuehren
- Abbruch protokollieren
- danach `STANDBY`

### Abbrechen und kuehlen

- urspruenglichen Prozess als abgebrochen markieren
- Benutzer bestaetigt Kuehlziel und Abschlussverhalten
- neuen expliziten manuellen Kuehllauf mit eigenem Schnappschuss anlegen
- diesen `AbortAndCool`-Neulauf als kanonische `NewActiveRun`-Grenze beginnen;
  PI- und Qualifier-RAM des alten Laufes werden vollstaendig geleert
- Uebergang in `REACHING_TARGET` beziehungsweise `COOLING`

## Warnungen und WARNING_REQUIRES_ACTION

Warnungen lassen den Prozess grundsaetzlich weiterlaufen, sofern die
Sicherheitslogik dies erlaubt. Sie werden sichtbar angezeigt und protokolliert.

`WARNING_REQUIRES_ACTION` wird verwendet, wenn eine fachliche Entscheidung offen
ist, etwa bei einem ausgefallenen optionalen Produktfuehler gemaess #21. Eine
fehlende aktuelle UTC im Current-`FERMENTING`-Fall ist dagegen kein
Benutzerentscheid und kein `NoActiveRun`: Sie fuehrt zu
`RecoveryEvaluation/WaitingForTrustedTime`. Nicht eindeutig aufloesbare
Persistenz- oder Zeitbelege werden `RecoveryRejectedOrFailClosed` und bleiben
fail-closed; technisch untrusted Persistenz fuehrt zu `SAFE_BOOT`.

## FAULT

Fehler bedeuten, dass der Prozess nicht normal weiterlaufen darf.

Beispiele:

- primaerer Prozesssensor ungueltig und kein validierter Ersatzbetrieb
- harte Ueber- oder Untertemperaturgrenze
- widerspruechliche H-Bruecken-Anforderung
- kritischer interner Software- oder Persistenzfehler
- sichere Ausgangszustaende nicht gewaehrleistet

### Verhalten

- neue Aktoranforderungen sperren
- Peltier unverzueglich deaktivieren
- erforderlichen Luefternachlauf gemaess Fehlerstrategie ausfuehren
- stabilen Fehlercode anzeigen und protokollieren
- kein automatisches Zurueckkehren ohne ausdruecklich erlaubte Wiederfreigabe

## RECOVERY_EVALUATION

### Zweck

Nach `BOOT` unverzueglich bestimmen, ob ein unterbrochener Lauf nach einem
geltenden R1-Vertrag logisch fortgesetzt, als nicht-aktivierendes Angebot
behandelt oder wegen untrusted Evidenz in `SAFE_BOOT` gehalten wird.
`FERMENTING`-Current-Recovery kann dabei automatisch logisch fortgesetzt
werden; eine Aktorfreigabe folgt daraus nicht.

Vor Eintritt wurden aktueller Config-/Persistenzstatus und
Transaktionsintegrität klassifiziert; frische Sensor-/Planner-Evidenz wird erst
bei einer späteren Aktorpermission geprüft. Es gibt keinen allgemeinen
persistierten Safety-Latch.
Zusaetzlich werden mindestens bewertet:

- Programmschnappschuss und Laufrevisionen
- Phase des unterbrochenen Laufes
- aktueller und letzter gueltiger Sensorstatus
- aktuelle und letzte bekannte Temperaturen
- die explizite R1-Phasenmatrix mit dem #124-Wall-Clock-Vertrag fuer Current-
  `FERMENTING`; keine gewichtete oder biologische Zeit-/Progressgutschrift
- aktive Warnungen und Fehler
- sichere phasenbezogene Aktoraktion

### Uebergaenge

```text
NoPersistedRun / NoActiveRun
  -> STANDBY, Gate Unresolved

technisch integerer Current-FERMENTING mit exakter Evidenz und UTC
  -> bestehender Write-before-Apply-Recoverykern
  -> logisch FERMENTING oder normale FermentationCompleted-Semantik

technisch integerer Current-FERMENTING ohne aktuelle UTC
  -> RECOVERY_EVALUATION / WaitingForTrustedTime, RAM-only

technisch integerer Current ohne geltende R1-Qualifikation
  -> nur ueber den jeweiligen bestehenden kanonischen Pfad behandeln
  -> keine stillschweigende Umetikettierung unklarer Evidenz

technisch untrusted Load
  -> SAFE_BOOT, keine Tombstone-Mutation und kein Resume

expliziter Fresh Start oder bestaetigtes Resume, soweit der jeweilige Pfad
dies erfordert
  -> bestehender Write-before-Apply-Pfad
  -> nach Gesamtstatus Applied, FSM-Anwendung und frischer Evidenz ggf. Allowed
```

Die Entscheidung wird detached vorbereitet. Bei #17 gilt der Gesamtstatus der
mehrstufigen Transaktion `PreparedHead -> CheckpointSlot -> CommittedHead` als
Wahrheit: erst `Applied` erlaubt die RAM-/FSM-Anwendung; ein Einzel-Write-
`Success` ist kein separater Gesamtstatus und benoetigt keinen zweiten
Readback.

## RECOVERY_TIME_PENDING

### Zweck

`RECOVERY_TIME_PENDING` beziehungsweise die Application-Disposition
`WaitingForTrustedTime` ist im #124-R1-Pfad kein Freigabekontext und kein
neuer persistierter Prozesszustand. Historische NTP-/Ausfallintervall- und
Progressrechnung bleibt #18/C2-Legacy; der aktuelle #124-Vertrag verwendet
stattdessen nur die Wall-Clock seit dem exakt validierten Checkpoint.

### Verhalten

- Aktoren bleiben `Idle/Stop` und das Gate `Unresolved`.
- Kein gewichtetes, temperaturbasiertes oder biologisches Recoveryfeld wird
  gutgeschrieben. Der neutrale exakte `priorBootPhaseElapsed`-Offset darf
  fuer die R1-Phasenkontinuitaet wiederverwendet werden.
- Ohne aktuelle UTC wird nichts gutgeschrieben und nichts persistiert; die
  Anwendung wartet RAM-only auf dieselbe vertrauenswuerdige Evidenz.
- Technische Persistenzunsicherheit fuehrt zu `SAFE_BOOT`; unklare Recovery-
  Evidenz wird nicht als `NoActiveRun` umetikettiert.

## SERVICE_MODE

### Eintritt

Nur aus validiertem `STANDBY`, nie aus `SAFE_BOOT`, `FAULT` oder einem laufenden
Prozess.

### Regeln

- Service-PIN, falls fuer spaetere Service-/Hardware-Gates benoetigt, ist
  nicht Teil des #24-R1-`ActuationInterlock`
- deutlicher Warnhinweis vor Aktortests
- Peltier- und Lueftertests zeitlich und leistungsmassig begrenzen
- Richtungswechsel, Mindest-Ausschaltzeit und Totzeit bleiben erzwungen
- Sensor- und Sicherheitspruefungen bleiben aktiv
- Fehler beendet den aktiven Test sofort
- beim Verlassen alle Ausgaenge AUS
- danach Rueckkehr zu `STANDBY`

## Manuelle Betriebsarten

### Manueller Zeit-/Temperaturlauf

Wird als temporaeres Programm mit Zieltemperatur, Dauer, Sensorbetrieb,
optionalem Vorheizen und Abschlussverhalten behandelt. Er nutzt dieselben
Zustaende und Sicherheitsregeln wie gespeicherte Programme.

### Manueller Temperatur-Haltebetrieb

Verwendet nach optionalem Vorheizen und Zielqualifikation `MANUAL_HOLDING`. Es
handelt sich nicht um direkte Aktorsteuerung.

## Tuerkontakt

Release 1 besitzt keinen Tuerkontakt. Die Zustandsmaschine darf keine
Tuerinformation als vorhandene Sicherheits- oder Prozessbedingung voraussetzen.

## Akzeptierte Entscheidungen

- [x] Bootpruefungen und aktuell untrusted System-/Config-/Persistenzzustaende
  haben Vorrang vor `STANDBY` und Recovery
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] `COMPLETED` wird nach Neustart explizit wiederhergestellt
- [x] keine allgemeine Pausenfunktion
- [x] Stop bietet Ausschalten oder einen neuen manuellen Kuehllauf
- [x] Warnungen und Fehler sind getrennt
- [x] `SERVICE_MODE` nur aus validiertem `STANDBY`; physische Service-/Reset-
      Geraete bleiben E5/Future
- [x] Produktfuehlerausfall fuehrt nicht zu stillem Sensorwechsel
- [x] R1 verwendet keine automatische Netzwerk-/NTP-Abhaengigkeit und keine
      gewichtete/biologische Recovery; ein vollstaendig vertrauenswuerdiger
      Current-`FERMENTING`-Run nutzt den einfachen #124-Wall-Clock-Vertrag
- [x] `RecoveryEvaluation/WaitingForTrustedTime` bleibt RAM-only und
      aktorfrei; automatische logische Recovery gibt allein keine
      Aktorfreigabe
- [x] kein Tuerkontakt in Release 1
