# Technische Entscheidungen

## ADR-001: PlatformIO mit Arduino Framework

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Reproduzierbarer Build fuer lokalen Einsatz und CI.
- **Entscheidung:** PlatformIO mit Arduino Framework, C++17 und dem generischen
  Ziel `esp32dev` fuer das vorgesehene ESP32-WROOM-32E-Modul.
- **Alternativen:** ESP-IDF direkt, Arduino IDE.
- **Folgen:** Das Ziel bestaetigt keine konkrete Boardrevision oder Pinbelegung.

## ADR-002: Keine GPIO-Zuweisung vor Hardwarebestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Importierte Komponentenangaben enthalten keine verifizierte
  Anschlussbelegung oder aktive Pegel.
- **Entscheidung:** Die Firmware verwendet keine Kandidatenpins. Zahlenwerte
  duerfen nur in Beispiel- oder lokaler Hardwarekonfiguration mit explizitem
  Status `TBD_HARDWARE` beziehungsweise `confirmed_test` dokumentiert werden.
  Die lokale bestaetigte `config/pins.yaml` bleibt ignoriert.
- **Alternativen:** Plausible Standardpins als Kandidaten in Firmware verwenden.
- **Folgen:** Reale Aktoren bleiben bis zur Hardwareverifikation gesperrt.

## ADR-003: Spezifikation vor Fermentationssteuerung

- **Status:** fulfilled; superseded by ADR-012 for implementation workflow
- **Datum:** 2026-07-20
- **Kontext:** Elektrische Grenzen, Sensorzuordnung und Sicherheitsanforderungen
  des realen Aufbaus waren offen.
- **Entscheidung:** Zunaechst Projektstruktur und vollstaendige Spezifikation,
  keine Aktorsteuerung aufgrund unbestaetigter Annahmen.
- **Alternativen:** Vorlaeufige Regelungslogik implementieren.
- **Folgen:** Die Spezifikation wurde auf `docs/software-specification` erstellt.
  Nach ihrem Merge beginnt die Implementierung issueweise gemaess ADR-012.

## ADR-004: Produkt- oder luftgefuehrter Betrieb

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Ein dauerhaftes Referenzglas bildet unterschiedliche Produktmassen
  und vorgewaermte Produkte nicht verlaesslich ab. Ein direkter Produktfuehler
  soll ausserdem nicht fuer jeden Lauf zwingend sein.
- **Entscheidung:** Mit angeschlossenem und ausgewaehltem Produktfuehler ist
  dieser der primaere Prozesssensor; Schrankluft begrenzt und ueberwacht den
  Prozess. Ohne Produktfuehler ist Schrankluft der primaere Prozesssensor. Der
  Modus wird vor dem Start sichtbar bestaetigt und darf nicht unbemerkt wechseln.
- **Alternativen:** Referenzflasche; Produktfuehler zwingend; nur Lufttemperatur.
- **Folgen:** Der Produktfuehler ist abnehmbar und liegt auf einem getrennten
  externen 1-Wire-Bus. Steckverbinder bleibt `TBD_HARDWARE`.

## ADR-005: Zielqualifikation vor Timerstart

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Ein einmaliges Erreichen des Zielwerts reicht fuer einen
  reproduzierbaren Timerstart nicht aus.
- **Entscheidung:** Vor der Fermentationszeit liegt eine getrennte
  Zielqualifikation. Der massgebende Sensor muss fuer eine definierte Zeit im
  Zielband liegen. Kurze Ausreisser duerfen innerhalb festgelegter Grenzen
  toleriert werden. Erst danach startet der Timer.
- **Alternativen:** Timer beim Programmstart oder beim ersten Zielkontakt.
- **Folgen:** Vorheizen, Zielerreichung und Zielqualifikation zaehlen nicht zur
  Fermentationsdauer.

## ADR-006: Optionales Vorheizen mit zweiter Bestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Der leere Schrank soll vor dem Einsetzen bereits vorgewaermter
  Milch temperiert werden koennen.
- **Entscheidung:** Vorheizen ist pro Lauf ein- oder ausschaltbar. Nach dem
  Temperieren fordert das Geraet zum Einsetzen auf. Erst ein zweiter bewusster
  Start beziehungsweise `Weiter` beginnt Zielerreichung und Zielqualifikation.
- **Alternativen:** Produkt immer vor dem ersten Start einsetzen; automatischer
  Timerstart direkt nach Vorheizen.
- **Folgen:** Die Zustandsmaschine besitzt `WAITING_FOR_PRODUCT` und eine lokale
  akustische Meldung.

## ADR-007: Warnung statt sofortigem Abbruch bei langer Zielerreichung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Grosse Produktmasse, Oeffnen oder geringe Leistung koennen die
  Zielerreichung verzoegern, ohne einen sicheren Weiterbetrieb auszuschliessen.
- **Entscheidung:** Programme besitzen eine maximale erwartete
  Zielerreichungszeit. Bei Ueberschreitung entstehen Warnung und
  Protokolleintrag. Die Regelung versucht weiter, solange kein Sicherheitsfehler
  vorliegt.
- **Alternativen:** unbegrenzt ohne Meldung; sofortiger Abbruch.
- **Folgen:** Prozesswarnungen und Sicherheitsfehler sind getrennt modelliert.

## ADR-008: Festes 4-MB-Ressourcenbudget ohne PSRAM-Abhaengigkeit

- **Status:** accepted; amended by ADR-011
- **Datum:** 2026-07-21
- **Kontext:** Die bestellte Controllerboard-Variante ist mit 4 MB Flash
  beschrieben. Firmware, Weboberflaeche, drei Sprachen, Laufpersistenz und
  Temperaturhistorie konkurrieren um den begrenzten Speicher.
- **Entscheidung:** Release 1 muss mit 4 MB Flash funktionieren und darf keine
  PSRAM voraussetzen. Firmware, Webressourcen, Konfiguration, aktiver
  Laufzustand, Sicherheitsjournal und Historie erhalten feste Budgets. Aktiver
  Lauf und Sicherheit haben Vorrang.
- **Alternativen:** groessere Modulvariante voraussetzen; unbegrenzte Historie;
  PSRAM als Voraussetzung.
- **Folgen:** Partitions-, RAM- und Flashbudgets werden mit realen Builds und der
  Hardware gemessen. Release 1 reserviert gemaess ADR-011 keine dualen OTA-Slots.

## ADR-009: Normale Sicherungen enthalten keine Geheimnisse

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Programme und Einstellungen sollen portabel gesichert werden,
  ohne WLAN-Passwoerter, Webpasswort, Service-PIN oder Sitzungen zu verbreiten.
- **Entscheidung:** Der normale Sicherungsexport enthaelt keine Geheimnisse.
  Webpasswort und Service-PIN werden als gesalzene Pruefinformation gespeichert.
  Das wiederverwendbare WLAN-Passwort wird getrennt behandelt und nicht
  exportiert.
- **Alternativen:** portable Sicherung inklusive Geheimnissen; keine Sicherung.
- **Folgen:** Nach Import werden nicht enthaltene Zugangsdaten neu eingerichtet.
  Eine rohe Flashkopie ist kein portabler Anwendungsimport.

## ADR-010: Vergessene Service-PIN erfordert vollstaendigen Werksreset

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Ein reiner PIN-Reset wuerde bei physischem Zugriff Zugang zu einer
  bestehenden geschuetzten Konfiguration ermoeglichen.
- **Entscheidung:** Bei vergessener Service-PIN ist nur ein vollstaendiger lokaler
  Werksreset moeglich. Er loescht Benutzerprogramme, Einstellungen,
  Zugangsdaten und Historien, stellt Standards wieder her und behaelt die
  geraetespezifische Touchkalibrierung.
- **Alternativen:** PIN allein zuruecksetzen; nur neues Flashen.
- **Folgen:** Der physische Resetweg ist mehrstufig, lokal und von der
  Touchkalibrierung getrennt.

## ADR-011: UART-Update fuer Release 1, Web-OTA spaeter

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Das Projekt befindet sich in Entwicklung, jede Installation muss
  initial ueber UART geflasht werden und 4 MB Flash sind knapp.
- **Entscheidung:** FT232RL/UART ist der verbindliche Update- und Recoveryweg fuer
  Release 1. Ein Single-App-Partitionsplan ist zulaessig. Web-OTA, duale
  Firmware-Slots, signierte Webpakete und automatisches Rollback sind
  `FUTURE_RELEASE`.
- **Alternativen:** Web-OTA zwingend in Release 1; unsicheres Ueberschreiben der
  einzigen laufenden Apppartition.
- **Folgen:** Release 1 bindet keine ungenutzten OTA-Bibliotheken oder
  OTA-Speicherreserven ein. Eine spaetere Partitionsumstellung darf ein erneutes
  UART-Flashen erfordern.

## ADR-012: Software-first mit gemeinsamem Bring-up-Profil

- **Status:** accepted
- **Datum:** 2026-07-21
- **Kontext:** Die reale Hardware trifft spaeter ein, waehrend der groesste Teil
  des fachlichen Systems vorher entwickelt werden soll.
- **Entscheidung:** Fachlicher Kern, Simulation, Persistenz, UI-Modelle und Web
  werden vor der Hardware weitgehend umgesetzt. Hardwarezugriffe liegen hinter
  Ports und Adaptern. Die gleiche Codebasis besitzt `native`, `esp32_bringup`
  und `esp32_release`; es gibt kein separates Wegwerf-Testprojekt.
- **Alternativen:** auf Hardware warten; separate Testfirmware; direkter
  Hardwarezugriff im Kern.
- **Folgen:** Software-Issues koennen durch native Tests abgeschlossen werden,
  waehrend reale Verifikation separat `BLOCKED_HARDWARE` bleibt. Aktoren werden
  im Bring-up erst nach unbelasteter Pegelmessung schrittweise freigegeben.

## ADR-013: Wiederverwendbare ESP32-Geraeteplattform

- **Status:** accepted
- **Datum:** 2026-07-22
- **Kontext:** Weitere ESP32-Projekte sollen gemeinsame technische Grundlagen
  wiederverwenden koennen, ohne Fermentationslogik zu kopieren.
- **Entscheidung:** Projektgeruest, anwendungsneutrale `device_platform` und
  konkrete `fermentation_app` werden getrennt. `main.cpp` bleibt Composition
  Root. Eine Auslagerung in ein eigenes Plattform-Repository erfolgt erst bei
  einem zweiten realen Anwendungsfall oder klarem Wartungsvorteil.
- **Alternativen:** komplettes Fermentationsprojekt als Template kopieren;
  sofortige spekulative Universalbibliothek.
- **Folgen:** Neue Module muessen bewusst Plattform oder Anwendung zugeordnet und
  nativ testbar gehalten werden. Die ausfuehrliche Entscheidung steht in
  [`ADR-013_REUSABLE_DEVICE_PLATFORM.md`](ADR-013_REUSABLE_DEVICE_PLATFORM.md).

## ADR-014: Deterministischer fachlicher Zustandsautomat

- **Status:** accepted
- **Datum:** 2026-07-23
- **Kontext:** Fachliche Uebergaenge muessen nativ und mit virtueller Zeit
  reproduzierbar sein. Persistenzfehler duerfen weder einen nur teilweise
  uebernommenen Zustand noch eine neue Aktorfreigabe erzeugen.
- **Entscheidung:** Der Zustandsautomat ist hardware- und persistenzfrei. Er
  berechnet aus Zustand, unveraenderlichem Laufschnappschuss, abstrahierten
  Prozesssignalen, Ereignissen und monotoner Zeit eine Uebergangsentscheidung,
  ohne den bisherigen Zustand unumkehrbar zu veraendern. Der aufrufende
  Anwendungsteil bestaetigt und uebernimmt sie erst nach erfolgreicher atomarer
  Speicherung.
- **Alternativen:** Zustand waehrend der Berechnung direkt mutieren; Persistenz
  oder Aktorfreigaben in den Automaten integrieren.
- **Folgen:** Issue #14 definiert alle kanonischen Zustandsnamen und die erlaubte
  Topologie. Detaillierte Boot-, Recovery-, Service-, Fehlerreset- und
  Persistenzpolitik bleibt in den dafuer vorgesehenen Folge-Issues. Native Tests
  duerfen Entscheidungen mit einem einfachen In-Memory-Treiber anwenden.

## ADR-015: Programmspezifische maximale Produktwartezeit

- **Status:** accepted
- **Datum:** 2026-07-23
- **Kontext:** `WAITING_FOR_PRODUCT` benoetigt laut Spezifikation eine
  programmspezifische Maximalzeit, das Programmmodell besitzt dafuer bisher kein
  Feld.
- **Entscheidung:** Schema 5 ergaenzt `maximumProductWaitMinutes` mit einem
  gueltigen Bereich von 1 bis 1.440 Minuten. Der Wert ist fuer ausfuehrbare
  Vorheizprogramme verpflichtend, ohne Vorheizen unzulaessig und darf in
  Katalogvorlagen als `TBD_COMMISSIONING` fehlen. Die Migration von Schema 4
  erfindet keinen Wert.
- **Alternativen:** globale Wartezeit; stiller Standardwert; unbegrenzte
  Produktwartephase.
- **Folgen:** Migrierte Vorheizprogramme bleiben Katalogvorlagen, sind aber bis
  zur Konfiguration nicht ausfuehrbar. Alle entwicklerseitigen
  Programmmodell-Wertebereiche werden zentral in `program_limits.hpp` definiert;
  Hardware-, Sicherheits-, Inbetriebnahme-, Regel- und Benutzerwerte bleiben
  davon getrennt.

## ADR-016: Konfigurationsspeicher-Backend und Schluesselraum

- **Status:** accepted
- **Datum:** 2026-07-25
- **Kontext:** Der Port `IStateStore` erlaubte binaersichere Schluessel bis 32
  Byte, waehrend das einzige vorgesehene Backend NVS nur nullterminierte
  ASCII-Schluessel bis 15 Zeichen kennt. Eine verlustfreie Adapterabbildung
  existiert nicht.
- **Entscheidung:** NVS ist das produktive Backend; Werte werden als Blob
  gespeichert. `StateStoreKey` wird portseitig auf 1 bis 15 Bytes aus
  `[A-Za-z0-9_.-]` begrenzt. Der Envelope bleibt backendunabhaengig
  einschliesslich CRC-32. `ChangeOrigin` und `ChangeOperation` verlassen den
  Envelope und werden Bestandteil der Payload.
- **Alternativen:** eigene Datenpartition mit selbst implementiertem
  Recordspeicher; Abbildung erst im spaeteren ESP32-Adapter loesen.
- **Folgen:** Atomizitaet, Integritaet und Wear-Leveling bleiben fremdgepflegt.
  Der Envelope-Header schrumpft auf 37 Byte ohne und 45 Byte mit UTC. Die
  NVS-Partitionsgroesse bleibt `TBD_IMPLEMENTATION_BUDGET` und wird in #29
  bestimmt. Die ausfuehrliche Entscheidung steht in
  [`ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`](ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md).

## ADR-017: Keine dauerhafte Webanmeldung in Release 1

- **Status:** accepted
- **Datum:** 2026-07-27
- **Kontext:** Persistente Login-Tokens wuerden dauerhafte Secretpersistenz,
  Rotation, Widerruf, Browsergeraeteverwaltung, Recovery, Ablauf ohne
  zuverlaessige UTC und zusaetzliche Flash-/Securitygrenzen erfordern.
- **Entscheidung:** Keine Option `Angemeldet bleiben` in R1. Normale und
  anonyme lokale Sessions bleiben serverseitig, fluechtig und begrenzt. Ein
  ESP32-/Geraeteneustart verwirft alle Sessions; Logout, Credentialwechsel,
  Moduswechsel und Werksreset widerrufen sie ebenfalls. 30 Minuten
  Inaktivitaet, 12 Stunden absolute Dauer.
  Keine persistenten Session-, Remember-me-, Login-, Refresh- oder
  Browsergeraete-Tokens. Ein reiner Browserneustart ist kein garantiertes
  Widerrufsereignis; ein Browser kann ein Sitzungscookie bei der
  Sitzungswiederherstellung erneut senden. Spaeterer Ausbau nur mit neuer
  Entscheidung und Securitynachweis.
- **Alternativen:** bestehende dauerhafte Anmeldung; unbefristete Session;
  persistentes Geraetetoken ohne vollstaendigen Widerruf.
- **Folgen:** Nach einem ESP32-/Geraeteneustart ist eine erneute Anmeldung
  erforderlich. Eine vom Browser wiederhergestellte Session bleibt hoechstens
  bis zum Inaktivitaets-/Absolutlimit oder einem serverseitigen Widerruf
  gueltig. In R1 entsteht keine persistente Session-, Remember-me-, Login-,
  Refresh- oder Browsergeraetetoken-Domaene. Persistente Credentialverifier,
  getrennte Salts, KDF-Parameter, Credential-Epoche/-Generation,
  Vor-Sperr-Fehlversuchszaehler, Sperrstufe, aktiver Sperrzustand sowie
  Integritaets- und atomare Commitinformationen bleiben davon unberuehrt;
  wiederverwendbare WLAN-Secrets liegen getrennt in der Connectivity-Domaene.
  Die Begrenzung senkt Angriffsflaeche und Recoverykomplexitaet.

## ADR-018: Variante-B-Konfigurationspersistenz fuer Release 1

- **Status:** accepted
- **Datum:** 2026-07-27
- **Kontext:** Der bisherige Persistenzvertrag plante neben Active und Fallback
  zusaetzliche persistente Pending-Roots, Aktivierungsabsichten und vorbereitete
  Connectivity-/Authentication-Domaenen. Dieser Umfang ist fuer Release 1
  groesser als der erste reale Bedarf und vervielfacht Schutzwurzeln,
  Cut-Points, Recoveryzustaende und Abhaengigkeiten. Die bereits umgesetzten
  Grundlagen aus #54 und #55 bleiben verwertbar.
- **Entscheidung:** Release 1 verwendet Variante B mit genau einem kanonischen
  aktiven Konfigurationsgraphen und genau einer nachweislich nutzbaren
  Rueckfallgeneration. Aenderungen entstehen als fluechtige, vollstaendig
  validierte Vorschau. Alle falliblen Runtime-Ressourcen werden vor dem
  persistenten Root-Commit vorbereitet; dieser Commit ist der persistente
  Linearisierungspunkt. Das anschliessende atomare Publish des vorbereiteten
  Runtime-Snapshots allokiert, serialisiert, validiert und reserviert nichts und
  darf vertraglich nicht fehlschlagen. Liefert der Root-Write
  `CommitOutcomeUnknown` und kann ein vollstaendiger Readback beider Rootslots
  und des Zielgraphen den persistenten Ausgang nicht eindeutig bestimmen,
  bleibt der Konfigurationsdienst in einem stabil typisierten unbestimmten
  Commitzustand fail closed. Er publiziert keinen vorbereiteten Snapshot,
  erlaubt keine weitere Mutation oder Slotwiederverwendung und behauptet weder
  alten noch neuen Graphen als kanonisch. Nur ein spaeterer vollstaendig
  erfolgreicher Scan oder derselbe Scan beim Neustart darf den Zustand
  eindeutig aufloesen. Es gibt weder automatischen Rollback noch
  Factory-Fallback. Release 1 besitzt weder persistente
  Pending-Roots noch Aktivierungsintents noch vorbereitete leere Connectivity-
  oder Authentication-Manifeste, -Roots oder -Slots. Bootstrap und
  wiederaufnehmbarer Werksreset verwenden eine `StorageEpoch`; normale
  Werksresets behalten die geraetespezifische Touchkalibrierung. Reale
  Connectivity- und Authentication-Domaenen werden erst mit ihrem ersten
  produktiven Konsumenten als eigene typisierte, versionierte und
  epochengebundene Persistenz eingefuehrt.
- **Alternativen:** Variante A bereits in Release 1 mit persistentem Pending,
  Aktivierungsintent und vorbereiteten Secret-Domaenen; nur eine aktive
  Generation ohne Fallback; eine Vereinfachung ohne atomaren Root-Commit und
  vorbereiteten Runtime-Snapshot.
- **Folgen:** #16 bleibt Tracking-Issue; #56 uebernimmt Active/Fallback,
  Graphvalidierung, fluechtige Vorschau und Runtimeaktivierung, #57 Bootstrap,
  `StorageEpoch`, Korruptionssperre und Werksreset. #17 und #24 werden nur ueber
  ihre tatsaechlichen fachlichen Vertraege und das nachgelagerte
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` gekoppelt. #24 darf nicht als
  vollstaendig abgeschlossen gelten, bevor die realen Fehlerproducer aus #56
  und #57 systemweit integriert und getestet sind. Ein eigener persistenter
  globaler Mutationszaehler ist kein beschlossener Bestandteil von Release 1;
  seine Notwendigkeit bleibt bis zum Detailplan von #56
  `FINAL_SELECTION_PENDING`. ADR-010, ADR-013 und ADR-016 bleiben unveraendert
  gueltig; ADR-018 ersetzt fuer Release 1 die abweichenden Variante-A-Teile des
  bisherigen Persistenzvertrags.

## ADR-019: Rendererunabhaengige Device-UI-Shell

- **Status:** accepted
- **Datum:** 2026-08-03
- **Kontext:** Touchdisplay und Web benoetigen gemeinsame fachliche Projektionen
  und Commands, ohne Renderer, Treiber, Hardware oder Fachlogik zu koppeln.
- **Entscheidung:** Release 1 verwendet eine feste lokale Device-Shell mit
  Header und genau vier festen, kontextabhaengigen Bottom-Slots. Gemeinsame
  View-Modelle, Commands, Textschluessel, Branding-, Theme- und
  Sprachpaketvertraege bleiben rendererunabhaengig. ManuEngineer ist das
  Build-Zeit-Standardbranding; die aktive Sprache und ein enthaltenes Theme
  sind Laufzeitwahlen. LVGL bleibt bevorzugter, aber nicht ausgewaehlter
  Kandidat. Lokale Servicefreigabe dauert 10 Minuten bei Inaktivitaet ohne
  R1-Maximaldauer; die Web-Servicepolicy bleibt getrennt 5 Minuten
  Inaktivitaet/15 Minuten absolut.
- **Folgen:** #25 besitzt die gemeinsamen Contracts, #26 die simulierte lokale
  Shell und #31 erst nach Hardwarebeweis Renderer, Treiber, Assets und
  Kalibrierung. Keine physischen Taster, Encoder, Programmwahlschalter oder
  Status-LEDs werden als Bedien- oder Recoveryweg vorausgesetzt. Die
  ausfuehrliche Ownerquelle ist
  [`DEVICE_UI_ARCHITECTURE_DECISIONS.md`](DEVICE_UI_ARCHITECTURE_DECISIONS.md).
  Die konkrete visuelle R1-Ausprägung ist in
  [`DEVICE_UI_VISUAL_DESIGN.md`](DEVICE_UI_VISUAL_DESIGN.md) festgelegt.
