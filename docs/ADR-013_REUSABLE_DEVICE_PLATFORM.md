# ADR-013: Wiederverwendbare ESP32-Geraeteplattform

- **Status:** accepted
- **Datum:** 2026-07-22

## Kontext

Der Fermentationsschrank ist voraussichtlich nicht das letzte ESP32-Projekt. Ein
spaeteres Geraet kann beispielsweise eine Smokersteuerung mit Garraumluefter und
Fleischtemperaturfuehlern sein. Solche Anwendungen besitzen unterschiedliche
Prozesslogik, benoetigen aber viele gleiche Grundfunktionen: Buildprofile, sicheren
Boot, Zeit, Konfiguration, Persistenz, Netzwerk, Web, Diagnose, UI-Grundlagen,
Sensorqualitaet sowie native Tests und Simulation.

Ein einfaches Kopieren des fertigen Fermentationsprojekts wuerde
Fermentationsbegriffe und nicht benoetigte Funktionen in neue Projekte tragen.
Eine sofortige universelle Bibliothek wuerde dagegen Schnittstellen festlegen,
bevor ein zweiter realer Anwendungsfall bekannt ist.

## Entscheidung

Die Firmware wird ab der Projektgrundlage in drei Verantwortungsbereiche
getrennt:

```text
Projekt- und Buildgeruest
        |
wiederverwendbare Geraeteplattform
        |
konkrete Anwendung
```

Im aktuellen Repository gilt:

```text
src/main.cpp
    Zusammensetzungsstelle und Arduino-Einstieg

lib/device_platform/
    allgemeine, anwendungsneutrale Geraetedienste und deren Schnittstellen

lib/fermentation_app/
    ausschliesslich Fermentationsprogramme, Prozesszustaende und
    fermentationsspezifische Bedienlogik
```

`FermentationApplication` darf nicht von der konkreten Klasse `DevicePlatform`
abhaengen. Die Anwendung verwendet ausschliesslich die schmale Schnittstelle
`IPlatformServices`. Dadurch kann dieselbe Anwendung mit nativen Testdiensten,
einem Simulator oder der realen ESP32-Plattform betrieben werden.

Die Abhaengigkeitsrichtung ist verbindlich:

```text
main -> konkrete Plattform + konkrete Anwendung
Anwendung -> Plattform-Schnittstellen
Plattform -> allgemeine Ports und Adapter
Adapter -> ESP32/Arduino oder native Testumgebung
```

Rueckwaertsabhaengigkeiten sind unzulaessig. Insbesondere darf
`device_platform` keine Fermentationsprogramme, Fermentationszustaende oder
produktspezifischen UI-Texte kennen.

## Regeln fuer neue Module

Ein Modul gehoert zur Geraeteplattform, wenn es:

- ohne Fermentationsbegriffe sinnvoll benannt und beschrieben werden kann,
- fuer mindestens einen weiteren plausiblen Geraetetyp unveraendert nuetzlich ist,
- keine direkte Abhaengigkeit zur Fermentations-Zustandsmaschine besitzt,
- hinter Ports oder schmalen Dienstschnittstellen testbar ist,
- im Profil `native` ohne Arduino-Hardware gebaut und getestet werden kann.

Ein Modul gehoert zur Fermentations-App, wenn es beispielsweise Folgendes kennt:

- Joghurt-, Kefir- oder Kombuchaprogramme,
- `PREHEATING`, `WAITING_FOR_PRODUCT`, `FERMENTING`, `COOLING` oder andere
  Fermentationsablaeufe,
- produkt- oder luftgefuehrte Prozessentscheidungen des Fermentationsschranks,
- fermentationsspezifische Texte, Ansichten oder Standardwerte.

Allgemeine Bausteine wie Zeitquellen, Sensorqualitaet, Filter, begrenzte
Reglerbausteine, Konfigurationsrevisionen, Journale, Web-Grundrahmen und Diagnose
duerfen in der Plattform liegen. Ihre konkrete Verwendung und Parametrierung
bleibt Aufgabe der Anwendung.

## Rolle von `main.cpp`

`main.cpp` ist eine Composition Root. Die Datei darf:

- konkrete Plattform- und Anwendungsmodule instanziieren,
- deren `begin()`- und `update()`-Lebenszyklus verbinden,
- den Arduino-Einstieg und minimale Bootausgabe bereitstellen.

Sie darf keine Prozesslogik, Sensorfilter, Regelalgorithmen, Persistenzregeln oder
Aktorentscheidungen enthalten.

## Noch keine Auslagerung in ein separates Repository

Die wiederverwendbaren Module bleiben zunaechst im Fermentationsrepository. Eine
Auslagerung in `ManuEngineer/ESP32-Device-Platform` erfolgt erst, wenn mindestens
eines der folgenden Kriterien erfuellt ist:

1. Ein zweites reales ESP32-Projekt verwendet das Modul weitgehend unveraendert.
2. Das Modul besitzt eine stabile, anwendungsneutrale Schnittstelle und eigene
   native Tests.
3. Eine unabhaengige Versionierung bringt einen konkreten Wartungsvorteil.

Bei der Auslagerung wird zusaetzlich ein kleines
`ManuEngineer/ESP32-Project-Template` erstellt. Das Template enthaelt nur
Projektstruktur, CI, sichere Buildprofile, Dokumentationsvorlagen und eine kleine
Beispielanwendung. Die eigentliche gemeinsame Implementierung wird als
versionierte Bibliothek eingebunden und nicht in jedes Projekt kopiert.

## Folgen

- Das aktuelle Projekt bleibt ueberschaubar und wird nicht zu einer spekulativen
  Universalplattform.
- Neue Anwendungen koennen spaeter denselben technischen Rahmen und
  Wiedererkennungswert verwenden.
- Fehlerbehebungen in ausgelagerten Plattformmodulen koennen mehreren Projekten
  zugutekommen.
- Die Trennung erzeugt anfangs einige kleine Schnittstellen und Module, reduziert
  aber spaetere Umbauten und direkte Hardwareabhaengigkeiten.
- Folge-Issues muessen neue Klassen bewusst `device_platform` oder
  `fermentation_app` zuordnen und die Abhaengigkeitsrichtung in Tests erhalten.
