# Lokale Programmauswahl und Programmverwaltung

## Status

Dieses Dokument ergaenzt [`LOCAL_UI.md`](LOCAL_UI.md) um die in Phase 4B
akzeptierten Bedienablaeufe fuer Programmauswahl, Start, Bearbeitung,
Neuanlage und Loeschen.

## Programmliste

Programme werden als grosse Listenzeilen dargestellt. Kacheln sind nicht die
primaere Darstellung, weil eigene Programmnamen laenger sein koennen und der
resistive Touch ausreichend grosse Ziele benoetigt.

Beispiel:

```text
Programme

[Joghurt mild                         >]
[Joghurt stichfest                    >]
[Milchkefir                           >]
[Wasserkefir                          >]

[Zurueck]                    [Weiter]
```

Verbindliche Regeln:

- Standardprogramme stehen zuerst.
- Danach folgen Benutzerprogramme.
- Innerhalb beider Gruppen bleibt die Reihenfolge stabil und nachvollziehbar.
- Fuer weitere Seiten werden grosse Schaltflaechen wie `Zurueck` und `Weiter`
  beziehungsweise `Auf` und `Ab` am rechten Inhaltsrand verwendet. Die vier
  festen Bottom-Slots bleiben dabei erhalten.
- Wischgesten sind nicht erforderlich.
- Die Liste darf nicht auf vier oder fuenf Programme begrenzt sein.
- Ein deaktiviertes oder ungueltiges Programm wird klar gekennzeichnet und kann
  nicht gestartet werden.

Favoriten und automatische Sortierung nach letzter Verwendung sind kein
Bestandteil des ersten Releases.

## Antippen eines Programms

Das Antippen einer Programmzeile oeffnet direkt die Startzusammenfassung. Eine
zusaetzliche vorgeschaltete Aktionsseite ist nicht vorgesehen.

Beispiel:

```text
Joghurt mild

Ziel:       42,0 °C
Dauer:      8:00 h
Vorheizen:  Ja
Regelung:   Produkt, sonst Luft
Danach:     Kuehlen und halten

[START]  [Bearbeiten]  [Kopieren]
[Zurueck]
```

Die genaue Anordnung kann wegen der Displaygroesse auf zwei Bildschirmseiten
oder in einen Haupt- und einen Unterdialog aufgeteilt werden. Alle Aktionen
muessen jedoch mit grossen Touchflaechen erreichbar sein.

## Aenderungen nur fuer den naechsten Lauf

Auf der Startzusammenfassung koennen die fuer den Lauf freigegebenen Werte
angetippt und geaendert werden, beispielsweise:

- Zieltemperatur
- Fermentationsdauer
- Vorheizen EIN/AUS
- Sensorbetrieb
- Abschlussverhalten
- Kuehlziel

Diese Aenderungen gelten standardmaessig **nur fuer den gestarteten Lauf**. Sie
ueberschreiben das gespeicherte Programm nicht.

Verbindliche Regeln:

- Geaenderte Laufwerte werden vor `START` sichtbar gekennzeichnet.
- `Zuruecksetzen` stellt in der Startzusammenfassung die gespeicherten Werte des
  Programms wieder her.
- Dauerhafte Aenderungen erfolgen ausschliesslich ueber `Bearbeiten` und einen
  bewussten Speichervorgang.
- Beim Start wird ein unveraenderlicher Programmschnappschuss fuer diesen Lauf
  erzeugt.

## Lokale Bearbeitung

Programme koennen vollstaendig am Touchdisplay verwaltet werden. Das Geraet
bleibt damit auch ohne Netzwerk administrierbar.

Lokal bearbeitbar sind mindestens:

- Name
- optionale Notiz
- Zieltemperatur
- Fermentationsdauer
- Vorheizen
- Sensorvorschlag
- Verhalten bei Ausfall des Produktfuehlers
- maximale Zielerreichungszeit
- Abschluss- und Kuehlverhalten

Die konkreten Felder werden mit dem Programmdatenmodell abgestimmt. Technische
Regel- oder Sicherheitsparameter koennen weiterhin dem geschuetzten
Servicebereich vorbehalten bleiben.

## Bildschirmtastatur

Fuer Namen und kurze Notizen wird eine lokale Bildschirmtastatur vorgesehen.

Mindestanforderungen:

- Buchstaben
- Ziffern
- Leerzeichen
- Bindestrich und wenige uebliche Sonderzeichen
- Rueckschritt
- Eingabe loeschen
- Abbrechen
- Uebernehmen

Die Tastatur muss fuer den resistiven Touch ausreichend grosse Tasten besitzen.
Falls nicht alle Zeichen auf eine Seite passen, sind klare Umschalttasten fuer
Buchstaben, Ziffern und Sonderzeichen zulaessig. Wischgesten sind nicht
notwendig.

## Neues Programm anlegen

Beim Erstellen eines neuen Programms werden zwei Wege angeboten:

```text
Neues Programm

[Vorlage kopieren]
[Leer erstellen]
[Zurueck]
```

### Vorlage kopieren

- Standard- oder Benutzerprogramm auswaehlen
- neue unabhaengige Programm-ID erzeugen
- Namen vor dem Speichern anpassen
- alle Werte als editierbare Kopie uebernehmen

### Leer erstellen

- mit einem validierten leeren Programmentwurf beginnen
- alle Pflichtwerte muessen vor dem Speichern beziehungsweise spaetestens vor
  dem Aktivieren gesetzt sein
- ein unvollstaendiger Entwurf darf gespeichert werden, muss aber deutlich als
  nicht startbereit gekennzeichnet sein

## Standardprogramme bearbeiten und loeschen

Die vier mitgelieferten Standardprogramme duerfen:

- gestartet
- fuer einen einzelnen Lauf angepasst
- dauerhaft bearbeitet
- kopiert
- auf ihre jeweilige Werkseinstellung zurueckgesetzt
- aus der aktiven Programmliste geloescht werden

### Technische Bedeutung von `Loeschen`

Ein Standardprogramm wird aus Benutzersicht geloescht und erscheint danach
nicht mehr in der aktiven Programmliste. Die unveraenderliche Werksvorlage wird
intern jedoch nicht vernichtet. Sie bleibt Bestandteil eines geschuetzten
Factory-Katalogs, damit ein spaeterer Werksreset die urspruenglichen
Standardprogramme wiederherstellen kann.

Dadurch gilt:

- ein geloeschtes Standardprogramm wird nicht bei jedem normalen Neustart
  automatisch neu angelegt
- seine Programm-ID darf nicht versehentlich fuer ein Benutzerprogramm
  wiederverwendet werden
- ein normaler Benutzer kann es nach dem Loeschen nicht unbemerkt durch einen
  Neustart zurueckholen
- `Geraet auf Werkseinstellungen zuruecksetzen` stellt alle Werksprogramme
  wieder her

Der genaue Umfang des Werksresets wird in `SETTINGS_AND_STORAGE.md` festgelegt.
Mindestens werden dabei die Standardprogramme in ihrem urspruenglichen Zustand
wiederhergestellt.

## Loeschen eines Programms

Das Loeschen jedes Programms erfordert eine zweistufige Bestaetigung.

Erster Dialog:

```text
Programm "Joghurt Kultur X"
wirklich loeschen?

[LOESCHEN]
[Abbrechen]
```

Zweiter Dialog:

```text
Endgueltig aus der Programmliste entfernen?

[JA, LOESCHEN]
[Zurueck]
```

Zusaetzliche Regeln:

- Die bestaetigenden Schaltflaechen duerfen nicht an derselben Position wie die
  vorherige normale Aktion liegen.
- Der Name des betroffenen Programms wird in beiden Dialogen angezeigt.
- Bei einem Standardprogramm wird darauf hingewiesen, dass es erst durch einen
  Werksreset wiederhergestellt wird.
- Ein aktuell laufendes Programm kann nicht geloescht werden.
- Der Schnappschuss eines bereits gestarteten Laufes bleibt von spaeterem
  Loeschen des Quellprogramms unberuehrt.
- Abgeschlossene Protokolle duerfen durch das Loeschen eines Programms nicht
  verschwinden.

## Werksreset im Servicebereich

Im geschuetzten Servicebereich wird eine Funktion vorgesehen:

```text
Geraet auf Werkseinstellungen zuruecksetzen
```

Sie ist nicht mit dem normalen Zuruecksetzen eines einzelnen Programms zu
verwechseln.

Mindestanforderungen:

- nur aus einem sicheren Zustand ohne laufenden Prozess
- deutliche Anzeige, welche Daten betroffen sind
- mindestens zweistufige Bestaetigung
- keine Ausloesung durch blosses langes Druecken einer einzelnen Taste
- Wiederherstellung aller vier Standardprogramme aus dem unveraenderlichen
  Factory-Katalog
- danach sicherer Neustart beziehungsweise Rueckkehr in den Einrichtungsablauf

Ob WLAN-Daten, Touchkalibrierung, Benutzerprogramme, Protokolle und weitere
Einstellungen ebenfalls geloescht werden, wird erst in
`SETTINGS_AND_STORAGE.md` verbindlich festgelegt.

## Akzeptierte Entscheidungen aus Phase 4B

- [x] Programmliste als grosse Listenzeilen
- [x] Standardprogramme zuerst, danach Benutzerprogramme
- [x] Antippen fuehrt direkt zur Startzusammenfassung
- [x] Aenderungen auf der Startseite gelten nur fuer den einzelnen Lauf
- [x] vollstaendige lokale Programmerstellung und Bearbeitung
- [x] neues Programm aus Vorlage oder leerem Entwurf
- [x] Standardprogramme duerfen bearbeitet, kopiert, zurueckgesetzt und geloescht
      werden
- [x] Werksvorlagen bleiben intern unveraenderlich erhalten
- [x] Werksreset stellt geloeschte Standardprogramme wieder her
- [x] zweistufige Loeschbestaetigung fuer Standard- und Benutzerprogramme

## Noch offen fuer Phase 4C und spaeter

- genaue Detailansicht waehrend eines Laufes
- STOP- und Abbruchdialoge
- Darstellung und Quittierung mehrerer Meldungen
- Bedienablauf fuer `WAITING_FOR_PRODUCT`
- Darstellung eines automatischen Wiederanlaufs
- Verhalten bei automatisch angewendetem Sensorersatz
- konkrete Bildschirmtastatur und maximale Namenslaenge
- genauer Umfang des Werksresets
