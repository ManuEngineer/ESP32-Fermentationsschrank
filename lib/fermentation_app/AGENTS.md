# AGENTS.md – fermentation_app

Diese Regeln gelten fuer alle Dateien unter `lib/fermentation_app/`.

## Zweck

Dieses Verzeichnis enthaelt die konkrete Anwendung des
Fermentationsschranks: Programme, Prozesslogik, Zustandsmaschine,
fermentationsspezifische Bedienmodelle und deren Tests.

## Abhaengigkeiten

- Die Anwendung darf nur von schmalen Schnittstellen aus `device_platform`
  und von allgemeinen, hardwareunabhaengigen Bausteinen abhaengen.
- Sie darf die konkrete Klasse `DevicePlatform`, Arduino, GPIO, WLAN,
  Dateisystem oder reale Systemzeit nicht direkt verwenden.
- Sie darf nicht von `lib/device_platform_test_support/` (Mockadapter und
  Simulation fuer native Tests) abhaengen; eigene Tests verwenden eigene,
  fermentationsspezifische Testhilfen.
- Hardwarewirkungen werden ausschliesslich als abstrakte Anforderungen oder
  Kommandos an Ports uebergeben.

## Zuordnung

Alles, was Joghurt, Kefir, Kombucha, Fermentationsphasen,
Produkt-/Luftfuehrung oder die konkreten Standardprogramme kennt, bleibt in
diesem Anwendungsmodul.

Allgemeine Bausteine werden nicht vorschnell hier dupliziert. Sind sie ohne
Fermentationsbegriffe sinnvoll und nativ testbar, ist ihre Zuordnung zu
`device_platform` zu pruefen.

Massgebend ist
[`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).
