# Architektur

## Aktueller Stand

`src/main.cpp` ist absichtlich nur ein sicherer Build- und Diagnoseeinstieg. Er
konfiguriert keine unbestaetigten GPIOs und implementiert keine
Fermentationssteuerung.

## Zielbild

```text
ILI9341/XPT2046 und Web
            |
       Application
            |
  Fermentations-Zustandsmaschine
            |
 SafetyManager / Temperaturregler
            |
       OutputManager
       /           \
BTS7960/Peltier   4 MOSFET-Kanaele
            ^
      DS18B20 SensorManager
```

## Vorgesehene Module

| Modul | Verantwortung |
|---|---|
| `Application` | Initialisierung und Zusammenspiel |
| `StateMachine` | Explizite Betriebszustaende und Uebergaenge |
| `SafetyManager` | Messalter, Grenzwerte, Verriegelungen, Fehlerreaktionen |
| `Ds18b20Adapter` | Asynchrones Einlesen und CRC-/Busdiagnose |
| `PeltierDriver` | BTS7960-Abstraktion, AUS, Totzeit und Richtungsverriegelung |
| `OutputManager` | Sichere Ansteuerung der vier MOSFET-Kanaele |
| `DisplayAdapter` | ILI9341-Ausgabe ohne Sicherheitsverantwortung |
| `TouchAdapter` | XPT2046-Eingabe und Kalibrierung |
| `SettingsStore` | Validierte persistente Konfiguration |
| `Diagnostics` | Logging und Fehlercodes |

Diese Module sind Zielarchitektur und im aktuellen Grundgeruest noch nicht
implementiert.

## Abhaengigkeitsregel

Fach- und Sicherheitslogik darf nicht direkt von Arduino-GPIO-, SPI- oder
OneWire-Funktionen abhaengen. Hardwarezugriffe werden in kleinen Adaptern
gekapselt, damit Verriegelungen und Fehlerreaktionen nativ testbar bleiben.

## Vorgesehene Zustaende

Die Anforderungen sehen `STANDBY`, `TEMPERING`, `STABILIZING`, `FERMENTING`,
optional `COOLING` und `HOLDING_COLD`, `FINISHED` sowie `FAILURE` vor. Jeder
Betriebszustand muss sicher nach `FAILURE` wechseln koennen.

Diese Zustandsmaschine ist noch nicht implementiert. Bis zur bestaetigten
Pinbelegung existiert nur der sichere Diagnoseeinstieg ohne Aktor-GPIOs.
