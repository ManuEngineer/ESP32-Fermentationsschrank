# Kritische Persistenz und sichere Aktorfreigabe

## Status und Rolle

Dieses Dokument ist die kanonische Fachquelle fuer das Verhalten bei kritischen
Persistenzfehlern, unvollstaendigen Transaktionen und dem Persistenzfehler-Latch.

Es ergaenzt:

- `SYSTEM_SAFETY_AND_RECOVERY.md` fuer sichere Systemreaktionen;
- `RUN_PERSISTENCE.md` fuer Laufkontrollpunkte und Revisionen;
- `RECOVERY_AND_INTERRUPTION.md` fuer Wiederanlaufentscheidungen;
- `ACCEPTANCE_TESTS.md` fuer die Testebenen und Release-Gates.

Bei Widerspruechen gilt die Prioritaet aus `SPECIFICATION_REVIEW.md`.

## Kritischer Persistenzfehler

Kritisch ist jeder Lese-, Schreib-, Integritaets- oder Aktivierungsfehler, durch
den ein Lauf-, Sperr-, Recovery- oder aktorfreigaberelevanter Zustand nicht mehr
eindeutig und atomar bestimmt werden kann.

Beispiele:

- aktueller Laufkontrollpunkt kann nicht atomar gespeichert werden;
- Transaktionsabsicht oder neue Revision kann nicht verlaesslich persistiert
  werden;
- Fehlerjournal fuer verriegelte Ereignisse ist nicht mehr verlaesslich;
- Konfigurationsspeicher oder Rueckfallrevision ist kritisch beschaedigt;
- keine sichere Speicherrevision kann gelesen, geschrieben oder aktiviert
  werden;
- ein unvollstaendiger Transaktionsmarker wird beim Boot gefunden.

## Unmittelbare Reaktion

Ein kritischer Persistenzfehler fuehrt in dieser Reihenfolge zu:

1. neue Aktoranforderungen sofort sperren;
2. Peltier und beide H-Brueckenrichtungen AUS;
3. den fuer den aktuellen thermischen Zustand erforderlichen sicheren
   Luefternachlauf ausfuehren;
4. eine RAM-seitige Verriegelung setzen;
5. einmal versuchen, einen minimalen Persistenzfehler-Latch in einem
   reservierten redundanten Speicherbereich zu setzen;
6. in einen verriegelten schweren Systemfehler wechseln;
7. normale Prozessfortsetzung, Lauf-Recovery und Aktorfreigabe sperren;
8. Service und Diagnose verlangen.

Der sichere Ausgangszustand hat Vorrang vor wiederholten Flash-Schreibversuchen.
Schlaegt auch der minimale Latch-Schreibversuch fehl, bleibt die RAM-Verriegelung
bis zum Neustart aktiv. Der folgende Boot muss den nicht verlaesslich
bestimmbaren Speicherzustand fail-closed behandeln.

## Transaktionales Freigabeprinzip

Ein zustandsaendernder Schritt, der unmittelbar oder spaeter eine Aktorfreigabe
erlauben kann, darf erst fachlich angewendet werden, nachdem:

1. die Transaktionsabsicht erfolgreich persistiert wurde;
2. die neue Revision vollstaendig geschrieben und validiert wurde;
3. die Aktivierung der neuen Revision erfolgreich abgeschlossen wurde;
4. der Transaktionsmarker eindeutig als abgeschlossen gespeichert wurde.

Ein Fehler vor Abschluss dieser Reihenfolge darf keine teilweise fachliche
Anwendung und keine Aktorfreigabe erzeugen.

Ein unvollstaendiger, widerspruechlicher oder nicht eindeutig lesbarer
Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`.

## Persistenzfehler-Latch

Der minimale Latch:

- liegt getrennt vom normalen Laufjournal;
- verwendet einen reservierten und soweit im selben Flash sinnvoll redundanten
  Speicherbereich;
- enthaelt mindestens einen stabilen Fehlercode, eine Generation beziehungsweise
  Integritaetsinformation und den gesetzten Zustand;
- wird beim Boot vor normaler Lauf-Recovery ausgewertet;
- kann durch einen normalen Neustart, Quittierung oder fehlgeschlagenen
  Resetversuch nicht geloescht werden.

Der Latch bleibt innerhalb desselben physischen ESP32-Flashs. Er darf deshalb
keine Ausfallsicherheit gegen einen vollstaendigen physischen Flashdefekt
behaupten. Ein aktuell nicht les- oder schreibbarer kritischer Speicher
verhindert in jedem Fall Lauf-Recovery und Aktorfreigabe.

## Reset und Wiederfreigabe

Ein gesetzter Persistenzfehler-Latch darf ausschliesslich in einem bewussten
Serviceablauf zurueckgesetzt werden.

Vor dem Reset muessen alle folgenden Bedingungen erfuellt sein:

- Ursache wurde diagnostiziert und soweit moeglich behoben;
- kritischer Speicher besteht einen Lesen-Schreiben-Integritaetstest;
- mindestens eine neue Testrevision kann atomar geschrieben, gelesen,
  validiert, aktiviert und erneut gelesen werden;
- keine unvollstaendige Transaktion bleibt bestehen;
- Konfiguration, Verriegelungen und Laufzustand sind eindeutig validiert;
- die Resetentscheidung wird protokolliert.

Nach dem Latch-Reset erfolgt keine direkte Aktorfreigabe. Das System durchlaeuft
den vollstaendigen validierten Boot- beziehungsweise Recoverypfad. Die
Recoveryentscheidung muss als neue Revision erfolgreich persistiert sein, bevor
eine normale Aktorfreigabe wieder moeglich ist.

## Boot- und Recoveryregeln

Beim Boot gilt:

1. Aktoren und beide H-Brueckenrichtungen sicher AUS;
2. Persistenzfehler-Latch und Transaktionsmarker auswerten;
3. kritische Speicherbereiche lesen und validieren;
4. bei Latch, unvollstaendiger Transaktion oder Integritaetsfehler nach
   `SAFE_BOOT` wechseln;
5. keine Lauf-Recovery und keine Aktorfreigabe aus `SAFE_BOOT`;
6. nur bei eindeutig gueltigem Zustand den normalen Recoveryvertrag fortsetzen.

Vor jeder Recovery-Aktorfreigabe muss der kritische Speicher den beschriebenen
Integritaetstest bestehen und die Recoveryentscheidung als neue Revision
vollstaendig gespeichert sein.

## Verbindliche Akzeptanzanforderungen

Mindestens folgende deterministische Tests sind erforderlich:

- kritischer Laufkontrollpunkt-Schreibfehler sperrt neue Aktoranforderungen,
  schaltet das Peltier AUS und setzt die RAM-Verriegelung;
- erforderlicher Luefternachlauf bleibt trotz gesperrtem Peltier moeglich;
- erfolgreicher minimaler Latch wird beim Neustart erkannt und fuehrt zu
  `SAFE_BOOT`;
- fehlgeschlagener Latch-Schreibversuch erlaubt bis zum Neustart keine
  Aktorfreigabe;
- unvollstaendige Transaktionsabsicht fuehrt beim Boot zu `SAFE_BOOT`;
- teilweise geschriebene oder nicht aktivierte Revision wird nicht fachlich
  angewendet;
- nicht lesbarer oder nicht schreibbarer kritischer Speicher verhindert
  Lauf-Recovery und Aktorfreigabe;
- Quittierung und normaler Neustart loeschen den Latch nicht;
- Latch-Reset wird ohne bestandenen Lesen-Schreiben-Integritaetstest abgelehnt;
- Latch-Reset wird bei verbliebener unvollstaendiger Transaktion abgelehnt;
- bestandener Speichercheck und erfolgreicher Latch-Reset fuehren nicht direkt
  zur Aktorfreigabe;
- eine Aktorfreigabe ist erst nach vollstaendig validiertem Wiederanlauf und
  erfolgreich persistierter Recoveryrevision moeglich;
- Fehlercode, Latch-Setzen, Resetversuch und Wiederfreigabe werden protokolliert.

Nicht ausgefuehrte Tests sind `BLOCKED` oder `NOT_RUN`, niemals `PASS`.
