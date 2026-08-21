# ADR-016: Konfigurationsspeicher-Backend und Schluesselraum

- **Status:** accepted
- **Datum:** 2026-07-25
- **Entscheider:** ManuEngineer (Repository-Owner)

## Kontext

Issue #54 implementiert die anwendungsneutralen Persistenzgrundlagen: den Port
`IStateStore`, starke Speichertypen, Envelope Version 1 und eine technische
Slotmechanik.

Der Port wurde rein logisch entworfen. `StateStoreKey` erlaubt bis zu 32
beliebige Bytes einschliesslich eingebettetem NUL und ist ausdruecklich
binaersicher. Die vorhandene Spezifikation
(`docs/CONFIGURATION_PERSISTENCE.md`, `docs/SETTINGS_AND_STORAGE.md`) legt
ausfuehrlich fest, *wie* gespeichert wird, benennt aber an keiner Stelle das
produktive Speicherbackend. Diese Luecke ist erst beim Review von PR #59
aufgefallen.

Das einzige realistisch vorgesehene Backend ist die ESP32-Speicherschicht NVS.
Deren technische Grenzen sind unvereinbar mit dem aktuellen Portvertrag:

- Schluessel sind ASCII-Strings mit maximal 15 Zeichen.
- Schluessel werden nullterminiert gespeichert; eingebettetes NUL ist
  strukturell unmoeglich.
- Namespaces unterliegen derselben Laengengrenze.

Ein verlustfreier Adapter zwischen dem aktuellen Port und NVS existiert nicht:

- Eine Hashabbildung von 32 beliebigen Bytes auf 15 Zeichen erzeugt
  Kollisionsrisiko. Bei Konfigurations- und Revisionsslots ist eine stille
  Kollision ein Datenverlust ohne Fehlermeldung.
- Eine Schluesselindextabelle braucht selbst persistenten Speicher, der vor dem
  Bootstrap gelesen werden muss. Das erzeugt genau in #57 eine zirkulaere
  Abhaengigkeit.

Zusaetzlich bietet NVS bereits eine Pruefsumme je Eintrag, atomares Ersetzen je
Eintrag, Wear-Leveling und Bereinigung geloeschter Eintraege. Ohne benannte
Backendentscheidung ist nicht bewertbar, welche Envelope-Felder notwendige
Ergaenzung und welche Verdopplung sind.

Die Entscheidung muss vor Issue #55 fallen, weil dort die konkreten Dokument-,
Manifest- und Root-Schluessel definiert werden.

## Optionen

### Variante A: NVS als produktives Backend, Port auf NVS-faehigen Schluesselraum begrenzt

| Dimension | Bewertung |
|---|---|
| Umsetzungsaufwand | niedrig; Anpassung eines Werttyps und seiner Tests |
| Risiko | niedrig; erprobte, mitgelieferte Speicherschicht |
| Wartungslast | niedrig; Wear-Leveling und Integritaet sind fremdgepflegt |
| Ressourcenbedarf | eine NVS-Partition, Groesse in #29 zu bestimmen |
| Plattformtauglichkeit | hoch; 15-Zeichen-ASCII ist der kleinste gemeinsame Nenner plausibler Backends |

**Vorteile:** Atomizitaet, Integritaet und Flashlebensdauer sind geloest, ohne
sie selbst zu implementieren. Der `IStateStore`-Vertrag einschliesslich
`CommitOutcomeUnknown` bildet das reale NVS-Verhalten korrekt ab. Werte werden
als Blob gespeichert und bleiben binaersicher; nur der Schluesselraum wird
eingeschraenkt.

**Nachteile:** Schluessel muessen kurz und in einem eingeschraenkten Zeichensatz
gewaehlt werden. Die Groesse der NVS-Partition wird zu einem eigenen
Budgetposten.

### Variante B: Eigene Datenpartition mit selbst implementiertem Recordspeicher

| Dimension | Bewertung |
|---|---|
| Umsetzungsaufwand | hoch; Sektorverwaltung, Commit-Protokoll, Wear-Leveling, Bereinigung |
| Risiko | hoch; Fehler zeigen sich erst nach langer Laufzeit als Konfigurationsverlust |
| Wartungslast | hoch; vollstaendig selbst zu pflegen |
| Ressourcenbedarf | eine eigene Partition zuzueglich Reserve fuer Bereinigung |
| Plattformtauglichkeit | mittel; maximale Freiheit, aber jedes Folgeprojekt erbt denselben Wartungsaufwand |

**Vorteile:** Voller binaersicherer Schluesselraum. Der Envelope ist ohne
Ueberschneidung notwendig.

**Nachteile:** Flash vertraegt nur eine begrenzte Zahl Loeschzyklen je Sektor.
Ohne eigenes Wear-Leveling verschleisst der am haeufigsten beschriebene Sektor
zuerst. Ein stromausfallsicheres Commit-Protokoll und Garbage Collection sind
schwer zu testen und schwer zu verifizieren. Der Aufwand steht in keinem
Verhaeltnis zum gewonnenen Schluesselraum.

### Variante C: Port unveraendert lassen, Problem im Adapter loesen

Entspricht dem aktuellen Stand von PR #59. Wird verworfen: die Abbildung ist
nicht verlustfrei moeglich, und die Verschiebung in den Adapter verlagert eine
Architekturentscheidung in ein spaeteres, hardwareblockiertes Issue (#29), lange
nachdem #55 bis #57 darauf aufgebaut haben.

## Entscheidung

Variante A.

1. **Produktives Backend ist NVS.** Werte werden als Blob gespeichert, nicht als
   String, damit sie binaersicher bleiben. Namespace- und Partitionswahl sind
   Aufgabe des ESP32-Adapters und kein Bestandteil des Schluesselwerts.

2. **`StateStoreKey` wird auf den kleinsten gemeinsamen Nenner begrenzt:**
   1 bis 15 Bytes, jedes Byte aus `A`–`Z`, `a`–`z`, `0`–`9`, `_`, `.` oder `-`.
   Kein NUL, kein Leerzeichen, keine Pfadtrennzeichen. Diese Regel ist bewusst
   enger als NVS allein verlangt: sie bleibt auch fuer dateibasierte Backends
   wie LittleFS oder SD-Karte gueltig und haelt Schluessel in Logs und
   Diagnoseausgaben lesbar. Die Grenze wird im Port erzwungen, nicht im Adapter.

3. **Der Envelope bleibt backendunabhaengig, einschliesslich CRC-32.** Die
   Ueberschneidung mit der NVS-eigenen Pruefsumme wird bewusst in Kauf genommen:
   der CRC schuetzt zusaetzlich gegen Fehler in der eigenen Kodierung und macht
   das Format ohne Aenderung auf Backends ohne Integritaetspruefung verwendbar.
   Das ist fuer die Wiederverwendung nach ADR-013 der ausschlaggebende Punkt.

4. **Nicht in den Envelope gehoeren Felder, die die Plattform nicht
   interpretiert.** `ChangeOrigin` und `ChangeOperation` sind
   Anwendungssemantik und wandern in die Payload. Der Header schrumpft damit von
   41 auf 37 Bytes ohne UTC und von 49 auf 45 Bytes mit UTC. `StorageEpoch`
   bleibt im Envelope, weil eine Werksreset-Generation eine plattformseitige
   Eigenschaft des Speichers ist.

### Release-1-Produktgrenze

Die technische Atomizitaet eines einzelnen NVS-/Store-Records ist nicht die
Release-1-Garantie fuer eine mehrrecordige Konfigurations- oder Lauftransaktion.
Insbesondere wird aus einem unterbrochenen Same-Key-Write kein Produktversprechen,
auf der Recoveryebene immer exakt OLD oder NEW zu erhalten. Die owning context-
Ebene aktiviert ausschliesslich vollstaendig validierte Records, Generationen,
Referenzen und Root-/Manifestgraphen. `CommitOutcomeUnknown` bleibt unbekannt
und wird nicht geraten; ein spaeterer Readbackfehler oder ein in einer begonnenen
Readphase verlorener Record ist nicht dasselbe wie ein urspruenglich fehlender
Key. Diese Klarstellung aendert weder den schmalen `IStateStore`-Port noch die
NVS-Defaultentscheidung.

## Folgen

**Einfacher:**

- Atomizitaet, Integritaet und Flashlebensdauer sind keine Eigenentwicklung.
- Der Bootstrap in #57 braucht keine Schluesselindextabelle und damit keine
  zirkulaere Abhaengigkeit.
- Der Envelope bleibt fuer ein zweites Geraeteprojekt unveraendert verwendbar.

**Schwieriger:**

- Schluessel muessen knapp und disziplinlert vergeben werden. #55 legt dafuer
  eine dokumentierte Namenskonvention fest.
- Die Groesse der NVS-Partition wird ein eigener Budgetposten und bleibt bis zur
  realen Messung `TBD_IMPLEMENTATION_BUDGET`.

**Spaeter zu pruefen:**

- Reicht die NVS-Partition fuer Konfiguration, Slots und Secrets? Nachverfolgung
  in #29.
- Falls ein spaeteres Projekt sehr grosse Dokumente speichert, kann ein
  dateibasiertes Backend hinter denselben Port treten. Die Schluesselregel aus
  Punkt 2 ist bewusst so gewaehlt, dass das ohne Formataenderung moeglich bleibt.

## Aufgaben

1. [ ] `StateStoreKey` auf 15 Bytes und den festgelegten Zeichensatz begrenzen,
       mit eigenem Ablehnungsstatus fuer unzulaessige Zeichen.
2. [ ] `ChangeOrigin` und `ChangeOperation` aus dem Envelope entfernen; Golden-
       Vektoren neu berechnen.
3. [ ] `docs/CONFIGURATION_PERSISTENCE.md` auf Backendentscheidung, neue
       Headergroessen und Schluesselregel anpassen.
4. [ ] Registereintrag in `docs/DECISIONS.md` ergaenzen.
5. [ ] Schluesselnamenskonvention in #55 festlegen und dokumentieren.
6. [ ] NVS-Partitionsgroesse als `TBD_IMPLEMENTATION_BUDGET` in
       `docs/OPEN_POINTS.md` unter #29 aufnehmen.
