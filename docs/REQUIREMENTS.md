# Funktionsanforderungen fuer Release 1

## Status

Dieses Dokument fasst die verbindlichen Muss-Anforderungen zusammen. Die
Detailregeln stehen in den thematisch spezialisierten Spezifikationsdokumenten.
Bei Widerspruechen haben spaetere akzeptierte ADRs und die spezialisierten
Dokumente Vorrang.

## Produkt und Betrieb

- Das Geraet regelt Fermentationsprozesse durch Heizen, neutralen Betrieb und
  Kuehlen.
- Der Normalbetrieb funktioniert ohne Cloud, Internet, Heimserver oder WLAN.
- Die generische Zeitplattform bleibt RTC-optional und NTP-only-fähig. Das
  konkrete Fermenter-R1-Produkt verlangt jedoch eine lokale RTC der
  DS3231-Familie, damit neue produktive Läufe auch ohne WLAN/Internet eine
  trusted Zeitquelle haben. Die tatsächliche RTC-Variante bleibt bis zur
  physischen Bestätigung `TBD_HARDWARE_CONFIRMATION`.
- Ohne trusted UTC startet kein neuer produktiver Lauf.
- Touchdisplay und lokale Weboberflaeche bedienen denselben fachlichen Zustand.
- Die sichere Temperaturregelung bleibt bei Ausfall von Display, Web oder Netzwerk
  erhalten.
- Release 1 unterstuetzt Deutsch, Spanisch und Englisch.
- Die vier Standardprogramme sind Joghurt mild, Joghurt stichfest, Milchkefir und
  Wasserkefir.
- Benutzerprogramme koennen erstellt, kopiert, bearbeitet und geloescht werden.
- Das erste Release verwendet eine Fermentationstemperatur pro Programm; das
  Datenmodell darf spaetere mehrstufige Programme nicht verhindern.

## Programme und Prozess

- Ein Lauf kann produkt- oder luftgefuehrt sein.
- Bei produktgefuehrtem Betrieb ist der Produktfuehler primaer; der
  Schrankluftfuehler begrenzt und ueberwacht den Prozess.
- Bei luftgefuehrtem Betrieb ist die Schrankluft der primaere Regelwert; eine
  nicht gemessene Produkttemperatur wird nicht behauptet.
- Vorheizen ist optional und kann einen Zustand `WAITING_FOR_PRODUCT` mit
  bewusster Bestaetigung erfordern.
- Die Fermentationszeit beginnt erst nach erfolgreicher Zielqualifikation.
- Kurze Temperaturabweichungen pausieren den Timer nicht automatisch.
- Das Verhalten nach der Fermentation ist pro Programm waehlbar: beenden,
  kuehlen, zeitlich halten oder bis zur manuellen Beendigung halten.
- Es gibt einen manuellen Zeit-/Temperaturlauf und einen manuellen Haltebetrieb
  ohne Timer.
- Direkte Aktorsteuerung ist nur als geschuetzte, begrenzte Servicepruefung
  zulaessig.
- Ein aktiver Lauf verwendet einen unveraenderlichen Programmschnappschuss.
- Zieltemperatur und verbleibende Dauer duerfen nur ueber eine ausdrueckliche,
  validierte und protokollierte Laufanpassung geaendert werden.

## Temperaturregelung

- Release 1 verwendet eine zeitproportionale PI-Regelung.
- Es gibt getrennte Maschinenparameter fuer Luft/Produkt und Heizen/Kuehlen.
- Der Regler erzeugt nur abstrakte Anforderungen `HEAT`, `OFF` oder `COOL` mit
  begrenzter Zeitquote.
- Ein Impulsakkumulator darf kleine Anforderungen innerhalb fester Grenzen
  sammeln.
- Mindest-Einzeit, Mindest-Auszeit, Umschalthysterese und Polaritaetstotzeit
  werden erzwungen.
- Heizen und Kuehlen koennen nie gleichzeitig freigegeben werden.
- Der Innenluefter laeuft waehrend temperaturgeregelter Phasen dauerhaft und
  besitzt einen Nachlauf.
- Der Aussenluefter wird ohne absichtliche Vorlaufzeit gemeinsam mit dem Peltier
  eingeschaltet und besitzt einen zwingenden Nachlauf.
- Kaskadenregelung und PID-Autotuning sind Zukunftsfunktionen und werden in
  Release 1 nicht aktiv implementiert.

## Sensoren

Der erste Aufbau verwendet drei DS18B20:

1. fest eingebauter Schrankluftfuehler
2. abnehmbarer Produktfuehler
3. fest eingebauter Aussenwaermetauscher-/Kuehlkoerperfuehler

Anforderungen:

- Schrankluft- und Kuehlkoerpersensor sind fuer jede Peltierfreigabe erforderlich.
- Der Produktfuehler ist optional, ausser ein Lauf verlangt ihn ohne erlaubten
  Ersatzbetrieb.
- Der abnehmbare Produktfuehler liegt auf einem getrennten externen 1-Wire-Bus.
- Die feste Bustopologie wird nach dem GPIO-Budget bestaetigt.
- Messung erfolgt mit 12 Bit ungefaehr alle zwei Sekunden.
- CRC, Busstatus, Wertebereich, Aenderungsrate und Sensorrollen werden geprueft.
- Einzelne Fehler fuehren zunaechst zu `STALE`; dauerhafte Fehler zu `FAILED`.
- Regelung verwendet Medianfilter und sensorbezogenen Tiefpass.
- Jeder Sensor besitzt einen begrenzten individuellen Offset anhand seiner
  ROM-Adresse.

## Sicherheit

- Bei Boot, Reset, Brownout, Watchdog oder unklarer Lage bleiben Peltier und
  H-Bruecke AUS.
- BTS7960-Eingaenge benoetigen Hardware-Pulldowns oder eine nachgewiesen sichere
  externe Freigabestufe.
- Sicherheitsabschaltungen ueberstimmen Mindest-Einschaltzeiten.
- #24 verwendet die minimalen Reaktionen `Information`,
  `Blocked/ImmediateStop` und `SAFE_BOOT` mit stabiler FaultCode-Matrix.
- `Quittieren` entfernt keine Ursache und keine Aktorsperre.
- Ein Neustart ist kein Fehlerreset.
- R1 fuehrt keinen allgemeinen persistenten Safety-Latch, Restart-Zaehler,
  Resetzeitfenster oder Service-PIN-Vertrag in #24 ein.
- `SAFE_BOOT` entsteht aus aktuell untrusted System-, Config- oder
  Persistenzzustand; jede Resetcause startet all-off und revalidiert frisch.
- Eine automatische thermische `SAFETY_RECOVERY`-Gegenrichtung und neue
  Thermal-/Hardwarefaults bleiben bis #35/E5 deferiert.
- Der Peltierpfad besitzt eine 7,5-A-Ueberstromsicherung; eine unabhaengige
  Temperatursicherung bleibt E5/#35/Future und ist kein #24-R1-Producer.
- R_IS/L_IS sind in Release 1 deaktiviert, nicht angeschlossen und nicht
  implementiert; sie sind kein R1-Akzeptanz-, Test- oder DoD-Gate. Eine
  spätere Verwendung ist `FUTURE_RELEASE` und erfordert ein eigenes Issue,
  einen vollständigen Plan und ein eigenes Owner-Gate.

## Persistenz und Wiederanlauf

Ein vollstaendig validierter Current-`FERMENTING`-Run mit exakter
`priorBootPhaseElapsed`-Basis und trusted UTC wird nach #124 logisch
automatisch fortgesetzt; der Stromausfall allein verlangt keine
Benutzerbestätigung. Diese logische Recovery gibt keine Aktoren frei. Fehlt
die aktuelle trusted UTC, bleibt derselbe Current unverändert als
`RecoveryEvaluation/WaitingForTrustedTime` im RAM; das Warten schreibt keine
Persistenz. Ein technisch integerer Current ohne geltende R1-Qualifikation
wird über seinen bestehenden kanonischen Pfad behandelt, bei untrusted
Persistenz bleibt es `SAFE_BOOT`. Es gibt kein automatisches Fallback-Resume,
keine automatische Promotion und keine Charge-Rettung. Der R5.9-#90-Vertrag
darf einen vollständig validierten älteren Fallback als nicht-aktivierendes
`OLDER_VALID_CHECKPOINT_RESUME`-Angebot klassifizieren; daraus folgt beim Boot
weder Resume noch `Allowed`.

- Konfigurationen und aktive Laufkontrollpunkte sind atomar und versioniert.
- Die letzte gueltige Revision bleibt als Rueckfall erhalten.
- Wichtige Laufereignisse werden sofort gespeichert; periodische Kontrollpunkte
  standardmaessig alle 5 Minuten, einstellbar zwischen 1 und 60 Minuten.
- Direkte GPIO- oder Aktorzustaende werden nie als Wiederanlaufzustand gespeichert.
- `PREHEATING`, `COOLING` und `MANUAL_HOLDING` behalten ihre bestehenden
  expliziten `ResumeOffer`-Semantiken; non-resumable trusted Phasen behalten
  ihre bestehenden `NoActiveRun`-/Discard-Semantiken. Der #124-Current-
  `FERMENTING`-Sonderfall wird nicht pauschal als `NoActiveRun` beendet.
- `RECOVERY_EVALUATION` bleibt ohne Aktorfreigabe. Explizites Resume und Fresh
  Start verwenden den bestehenden Write-before-Apply-Pfad; die automatische
  #124-Logik ist kein Ersatz für diese Aktivierungsevidenz.
- Ein `OLDER_VALID_CHECKPOINT_RESUME`-Angebot setzt einen unbrauchbaren Current
  sowie vollstaendig validierten Head-, Slot-, CRC-, Schema-, Epoch- und
  Referenzschutz voraus. Erst explizite Resume-Entscheidung, `Applied`, FSM-
  Anwendung und frische Safety-Evidenz koennen spaeter den Gatepfad bewerten.
- Alte UTC-/NTP-/gewichtete Fortschrittskorrektur und Charge-Rettung sind
  #18/C2-Legacy, keine #24-R1-Anforderung.
- Nichtkritische Historienfehler duerfen den Prozess mit Warnung weiterlaufen
  lassen; untrusted kritische Run-Persistenz fuehrt fail-closed zu `SAFE_BOOT`.

## Bedienung und Netzwerk

- Lokales Display: 320 x 240 Pixel im Querformat, grosse Schaltflaechen, keine
  notwendige Wischgeste.
- Die Weboberflaeche ist responsiv und auf Handy, Tablet und Computer fachlich
  gleichwertig.
- Display und Web verarbeiten Aenderungen atomar und mit Revisionsschutz.
- Der normale Webzugang kann mit einem Webpasswort geschuetzt werden.
- Service-PIN- und Hardware-Servicefunktionen sind spaetere Service-Gates und
  nicht Teil des #24-R1-Safety-Resetvertrags.
- WLAN-Ersteinrichtung erfolgt bevorzugt ueber ein geschuetztes Einrichtungs-WLAN
  mit QR-Code und Captive Portal; lokale Eingabe bleibt moeglich.
- Bei laenger fehlendem Heim-WLAN kann ein geschuetztes Ersatz-WLAN starten.
- Direkte Internet-Portfreigabe auf den ESP32 ist nicht vorgesehen.
- Eine dokumentierte lokale Lese-API ist zulaessig; keine offizielle externe
  Schreib-API in Release 1.

## Diagnose, Export und Updates

- Das Display bietet kompakte, das Web vollstaendige technische Diagnose.
- Lesende Diagnose bleibt waehrend eines Laufes verfuegbar.
- Hardwaretests sind nur im Standby, PIN-geschuetzt und zeitlich begrenzt.
- Lauf-, Diagnose- und Servicebericht-Exporte enthalten keine Passwoerter, PINs,
  Tokens oder erfundenen Werte.
- UART/FT232RL ist der verbindliche Update- und Wiederherstellungsweg fuer
  Release 1.
- Release 1 benoetigt keine OTA-Slots und kein Web-OTA.

## Ressourcen

- Ziel sind 4 MB Flash ohne vorausgesetzte PSRAM.
- Firmware, Konfiguration, Laufpersistenz, Journal und Historie erhalten feste
  Budgets.
- Puffer, Programme, Meldungen und Exporte besitzen feste Maximalgroessen.
- Alte nichtkritische Protokolle und Diagrammdaten werden proaktiv bereinigt.
- Kritische Daten haben immer Vorrang vor Komfortdaten.
- Heap, niedrigster Heap, groesster freier Block, Firmwaregroesse und
  Flashbelegung werden ueberwacht.

## Bewusst offene Werte

Folgende Werte werden nicht erfunden:

- `TBD_HARDWARE`: GPIOs, Pegel, Modulvarianten und Verdrahtungsdetails
- `TBD_COMMISSIONING`: Temperaturen, Zeiten, PI-Werte, Filter, Nachlaeufe und
  Sicherheitsgrenzen
- `TBD_IMPLEMENTATION_BUDGET`: Partitions-, Firmware-, Heap- und Speicherbudgets

Die offenen Werte sind durch die Issues #29 bis #37 und
[`OPEN_POINTS.md`](OPEN_POINTS.md) nachverfolgbar.
