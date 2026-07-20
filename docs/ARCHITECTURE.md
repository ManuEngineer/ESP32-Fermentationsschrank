# Architektur

## Zielbild

```text
Bedienung / Web
       |
Anwendungslogik
       |
Zustandsmaschine
       |
Regler und Sicherheitslogik
       |
Hardwareabstraktion
       |
Sensoren und Aktoren
```

## Empfohlene Module

| Modul | Verantwortung |
|---|---|
| `Application` | Initialisierung und Zusammenspiel |
| `StateMachine` | Explizite Betriebszustaende und Uebergaenge |
| `SafetyManager` | Grenzwerte, Verriegelungen und Fehlerreaktionen |
| `SensorManager` | Einlesen, Plausibilisierung und Filterung |
| `OutputManager` | Sichere Ansteuerung aller Aktoren |
| `SettingsStore` | Persistente Konfiguration |
| `WebServer` | Lokale Bedienoberflaeche und API |
| `Diagnostics` | Logging und Fehlercodes |

## Abhaengigkeitsregel

Die Fachlogik darf nicht direkt von Arduino-GPIO-Funktionen abhaengen.
Hardwarezugriffe werden in kleinen Adaptern gekapselt. Dadurch kann die Logik
in der nativen PlatformIO-Umgebung getestet werden.

## Zustandsmaschine

Projektspezifische Zustaende hier dokumentieren:

```text
BOOT
  -> STANDBY
  -> RUNNING
  -> FINISHED
  -> FAULT
```
