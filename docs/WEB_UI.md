# Lokale Weboberflaeche

## Status

Dieses Dokument beschreibt die in Phase 5B akzeptierten Regeln fuer die lokale
Weboberflaeche, Anmeldung, Sitzungen, Servicefreigabe, Live-Aktualisierung und
gleichzeitige Bedienung.

Die Netzwerkbereitstellung und grundlegenden Sicherheitsgrenzen stehen in
[`NETWORK.md`](NETWORK.md).

## Grundsaetze

- Die Weboberflaeche bietet auf Mobiltelefon, Tablet und Computer denselben
  fachlichen Funktionsumfang.
- Das Layout passt sich der Bildschirmgroesse an.
- Die mobile Bedienung ist kein nachtraeglicher Sonderfall, sondern ein
  gleichwertiges Zielgeraet.
- Die Weboberflaeche verwendet denselben fachlichen Zustand und dieselben
  Sicherheitsregeln wie das Touchdisplay.
- Keine Webaktion darf Sensor-, Aktor- oder Sicherheitspruefungen umgehen.
- Ein Ausfall der Weboberflaeche beeinflusst den laufenden Prozess nicht.
- Touch und Web verwenden dieselben rendererunabhaengigen View-Modelle,
  typisierten Commands, erwarteten Revisionen und strukturierten
  Command-Ergebnisse. Layout, Navigation, Browser-Sprache, Transport- und
  Sitzungsgrenzen bleiben oberflaechenspezifisch.
- Plattform- und App-Texte besitzen getrennte Namensraeume. Renderer und
  Transportadapter veraendern Fachzustaende nie direkt.

## Responsive Darstellung

Die Oberflaeche wird responsiv aufgebaut.

### Mobiltelefon

- einspaltige Darstellung
- grosse Touchflaechen
- kompakte, eindeutig erreichbare Navigation
- wichtige Laufdaten ohne horizontales Scrollen
- Diagramme passen sich der verfuegbaren Breite an

### Tablet und Computer

- breitere Tabellen und Diagramme
- Seitenmenue oder vergleichbare dauerhafte Navigation moeglich
- Prozessdaten und Detailinformationen duerfen nebeneinander dargestellt werden

Die fachlichen Funktionen unterscheiden sich nicht nach Geraeteklasse.

## Zustandsabhaengige Startseite

Die Startseite richtet sich wie das lokale Display nach dem aktuellen
Geraetezustand.

### Standby

Mindestens sichtbar:

- Geraetename
- Produkt- und Schranktemperatur; die technische Rolle bleibt
  Schrankluftfuehler
- Produktfuehlerstatus
- aktive Meldungen
- Netzwerk- und Zeitstatus in kompakter Form
- `Programm starten`
- `Manueller Betrieb`

Beispiel:

```text
Fermentationsschrank

Produkt       nicht angeschlossen
Schrank       22,4 °C

[Programm starten]
[Manueller Betrieb]
```

### Laufender Prozess

Mindestens sichtbar:

- Programmname
- benutzerverstaendliche Prozessphase
- Produkt-, Schrank- und Solltemperatur
- verstrichene und verbleibende Zeit
- Abschlussverhalten
- hoechste aktive Meldung
- `Details`, `Meldungen` und `STOP`

Beispiel:

```text
Joghurt mild
Fermentation laeuft

Produkt       41,8 °C
Schrank       42,3 °C
Soll          42,0 °C
Restzeit      06:14

[Details] [Meldungen] [STOP]
```

### Abgeschlossener Prozess

Die Weboberflaeche bietet dieselben fachlichen Aktionen wie das Display:

- Abschluss quittieren
- Laufdetails anzeigen
- optional neuen manuellen Kuehllauf starten

## Navigation

Die fachlichen Hauptbereiche sind:

1. Uebersicht
2. Programme
3. Manueller Betrieb
4. Meldungen und Protokolle
5. Einstellungen
6. Diagnose
7. Service
8. System

Auf breiten Bildschirmen kann dies als Seitenmenue dargestellt werden. Auf
Mobiltelefonen wird eine kompakte, klar beschriftete Navigation verwendet.
Wichtige Funktionen duerfen nicht ausschliesslich hinter schwer erkennbaren
Symbolen versteckt werden.

Eine spaetere visuelle Zusammenfassung einzelner Technikpunkte bleibt moeglich,
solange Zugriffsrechte und fachliche Trennung erhalten bleiben.

## Sprache je Browser

Jeder Browser beziehungsweise jedes Endgeraet kann seine eigene Sprache fuer die
Weboberflaeche speichern.

Unterstuetzt werden:

- Deutsch
- Spanisch
- Englisch

Die Sprache des Touchdisplays bleibt davon unabhaengig.

Beispiel:

- lokales Display: Deutsch
- Browser des Benutzers: Deutsch
- Browser der Partnerin: Spanisch

Verbindliche Regeln:

- Beim ersten Aufruf darf die Browsersprache als Vorschlag verwendet werden.
- Der Benutzer kann die Sprache jederzeit manuell aendern.
- Die Auswahl wird lokal im Browser oder in der Websitzung gespeichert.
- Eine Sprachwahl veraendert keine Prozessdaten oder Geraeteeinstellungen.
- Benutzerdefinierte Programmnamen und Notizen werden nicht automatisch
  uebersetzt.
- Fehlende Uebersetzungen verwenden zuerst Englisch und danach den sichtbaren
  technischen Schluessel. Die Regel gilt gleichermassen fuer Touch und Web,
  ohne die browserindividuelle Sprachwahl zu veraendern.

## Normale Webanmeldung

Diese Regeln gelten, wenn der normale Webpasswortschutz aktiviert ist.

### Anmeldung

- gemeinsames normales Webpasswort
- getrennt von der vierstelligen Service-PIN
- kein Benutzerkonten- oder Rollenmodell im ersten Release
- Passwort wird bei der Eingabe verdeckt dargestellt
- Fehlversuche werden global pro Credential gezaehlt: nach fuenf falschen
  Passwortpruefungen 30 Sekunden Sperre, weitere Fehlversuchsbloecke
  verdoppeln bis hoechstens 15 Minuten
- Vor-Sperr-Zaehler, Sperrstufe und aktiver Sperrzustand werden atomar und
  neustartfest gefuehrt; ein beantworteter Fehlversuch verschwindet nicht
  durch Neustart und Persistenzfehler ergeben niemals Freigabe

### Keine dauerhafte Anmeldung in Release 1

Gemaess [ADR-017](DECISIONS.md#adr-017-keine-dauerhafte-webanmeldung-in-release-1)
gibt es in Release 1 keine Option `Angemeldet bleiben`.
Normale Sessions bleiben serverseitig, fluechtig und begrenzt. Sie enden nach
30 Minuten Inaktivitaet, spaetestens nach 12 Stunden absolut, und werden bei
einem Geraeteneustart verworfen. Persistente Login-, Refresh- oder
Browsergeraete-Tokens sind nicht vorgesehen. Ein spaeterer Ausbau benoetigt
eine neue Entscheidung und einen eigenen Securitynachweis.

Ein reiner Browserneustart ist kein garantiertes Logout- oder
Widerrufsereignis. Stellt der Browser das Sitzungscookie wieder her, bleibt die
serverseitige Session hoechstens bis zum Inaktivitaets-/Absolutlimit, Logout,
Credential-/Moduswechsel, Werksreset oder einem anderen serverseitigen
Widerruf gueltig. Browser-Fingerprints, `beforeunload`-Garantien oder andere
Lifecycle-Hacks sind dafuer nicht vorgesehen.

Die 30-Minuten-Inaktivitaetsfrist wird nur durch explizit erfasste relevante
Benutzeraktivitaet verlaengert. Cookie-/Session-Lookup, Browser-Reload,
read-only Polling und passive Snapshot-Aktualisierung verlaengern sie nicht;
die absolute 12-Stunden-Grenze bleibt unabhaengig davon bestehen. Das separate
Timing der Web-Servicelease bleibt davon unberuehrt.

### Betrieb ohne normales Webpasswort

Ist der normale Webpasswortschutz bewusst deaktiviert:

- entfaellt die Passwortpruefung, nicht aber die Sitzung;
- der Server erzeugt eine begrenzte fluechtige anonyme lokale Session mit
  mindestens 128 Bit kryptografisch zufaelliger Kennung, derselben Cookiepolicy,
  einem sitzungsgebundenen CSRF-Token sowie denselben Session- und
  Ressourcenlimits; sie enthaelt keine Benutzeridentitaet oder Rolle und wird
  beim Neustart verworfen;
- jede Person im erreichbaren lokalen Netz kann normale Funktionen bedienen;
  eine dauerhafte sichtbare Warnung weist darauf hin;
- normale Mutationen verlangen weiterhin Session, CSRF, vorgesehene Methode
  und Content-Type, Origin-/Referer-/Fetch-Metadata-Pruefung, erwartete
  Revision, Konflikt-/Wiederholungsschutz sowie Fach- und Safetypruefung;
- der Servicebereich bleibt zusaetzlich durch Service-PIN, PIN-KDF,
  neustartfeste Sperrlogik, sitzungsgebundene Servicefreigabe und
  Hardware-/Safetygates geschuetzt.

Der Moduswechsel ist selbst eine geschuetzte Mutation. Aktiviert zu deaktiviert
verlangt eine gueltige Session, CSRF, erwartete Revision sowie ausdrueckliche
Warnung und Bestaetigung; danach werden alle bisherigen Sessions widerrufen.
Deaktiviert zu aktiviert setzt das neue Passwort atomar und widerruft alle
anonymen Sessions. Bei der Ersteinrichtung ist Passwortschutz empfohlen und
vorausgewaehlt; Deaktivierung ist nur bewusst nach Warnung moeglich.

### Ersteinrichtung des Webzugangs

Dieser Abschnitt beschreibt den implementierten R1-Ablauf fuer den Zustand
`unprovisioned` (noch kein Webpasswort entschieden). Er ersetzt nicht die
geschuetzte Moduswechsel-Mutation oben; eine spaetere Passwortaenderung oder
ein Moduswechsel ist nicht Teil der Ersteinrichtung.

**Lokale Freigabe.** Die Ersteinrichtung ist nur moeglich, nachdem sie am Geraet
bewusst freigegeben wurde: Display `Sprache` -> `Webzugang` -> `Web-Setup`.
Die Freigabe ist ein fluechtiges Fenster von 10 Minuten (monotone Zeit, keine
Verlaengerung, keine Persistenz). Es endet bei erfolgreicher Einrichtung, Ablauf,
Werksreset, Neustart, Netzwerkmoduswechsel und `HOME_WIFI`-Neueinrichtung, sowie
sobald der Authzustand nicht mehr `unprovisioned` mit moeglichem Bootstrap ist.
Der HTTP-Server ist nicht an ein Interface gebunden; die Freigabe, nicht die
Erreichbarkeit, ist die Sicherheitsgrenze. Innerhalb des Fensters kann der erste
Client, der die Route erreicht, die Einrichtung abschliessen.

**Formular.** Im Zustand `unprovisioned` liefert `/` ein kompaktes
Einrichtungsformular statt der Anmeldeseite (Login, Statusabfrage und
Sitzungsbedienung haben in diesem Zustand keine Funktion):

- Option `Passwortschutz` (empfohlen, vorausgewaehlt) mit Passwortfeld;
- Option `Passwortschutz deaktivieren` mit sichtbarer Warnung und ausdruecklicher
  Bestaetigung; ohne Bestaetigung wird nichts gesendet;
- ein getrenntes Feld fuer die vierstellige Service-PIN in beiden Optionen (die
  Service-PIN bleibt bei deaktiviertem Webpasswort geschuetzt);
- Texte in Deutsch, Englisch und Spanisch nach Browsersprache, Englisch als
  Rueckfall;
- eine Meldung bei `403`, dass die Freigabe zuerst am Geraet erfolgen muss.

Das Formular prueft keine Passwort- oder PIN-Regeln; diese gehoeren der
Authentisierungsdomaene. Die Shell bleibt innerhalb der Groessenschranke von
8192 Bytes (Ownerentscheid fuer #27, Gesamtlaenge aus besessenem Kopf und
statischem Anhang; sie
betrug bis zur Startseiten-Umsetzung 6656 Bytes, siehe unten).

**Route.** `POST /api/v1/provision`, nur mit `Content-Type: application/json`
(optional `charset=utf-8`), gleiche Origin-/Fetch-Metadata-Pruefung wie die
Anmeldung, Body hoechstens 1024 Bytes. Weder Session noch CSRF-Token noch
Mutationssequenz sind vorausgesetzt; vor der Einrichtung existiert keine
Session. Zulaessige Schluessel genau:

- Passwortschutz: `mode:"protect"`, `password`, `servicePin`;
- Deaktivierung: `mode:"disable"`, `servicePin`, `confirmDisable:true`.

| Status | Bedeutung |
|---|---|
| `200` | eingerichtet (`passwordProtection` `enabled` oder `disabled`) |
| `400` `invalid-json` / `413` / `415` / `403 origin-rejected` / `405` | Request-Vertrag verletzt |
| `403 provisioning-not-allowed` | kein offenes lokales Freigabefenster |
| `409 already-provisioned` | Webzugang ist bereits eingerichtet |
| `422 invalid-credentials` | Passwort oder PIN verletzen die Domaenenregeln |
| `503 recovery-required` | Recovery oder unklarer Zustand |
| `503 provisioning-failed` | KDF- oder Persistenzfehler |

Bei Erfolg wird **keine** Session erzeugt und kein Cookie gesetzt. Der Benutzer
meldet sich danach normal an (Passwortschutz) bzw. erhaelt beim Laden von `/`
die anonyme lokale Session (deaktivierter Passwortschutz). Fehler vor dem ersten
Schreibzugriff lassen den Zustand `unprovisioned` wiederholbar; ein Fehler nach
dem ersten Schreibzugriff fuehrt zum bestehenden Recovery-Zustand, den nur der
lokale Werksreset aufloest.

Passwort und PIN erscheinen nie in URL, Antwort, Log, Snapshot oder Diagnose.
Die Verbindung ist unverschluesseltes HTTP im lokalen Netz (siehe NETWORK.md).

## Sitzungstechnische Mindestregeln

Unabhaengig von der spaeteren konkreten Bibliothek gelten:

- Sitzungskennungen duerfen nicht als dauerhaft sichtbare URL-Parameter verwendet
  werden.
- Sitzungskennungen enthalten mindestens 128 Bit kryptografisch zufaelligen
  Inhalt und sind nicht erratbar.
- Das Sessioncookie verwendet `HttpOnly`, `SameSite=Strict`, `Path=/` und kein
  `Domain`-Attribut.
- Unter direktem lokalem HTTP kann `Secure` technisch nicht erzwungen werden;
  bei spaeterem TLS-Betrieb muss es aktiviert werden koennen.
- Zustandsveraendernde Webanfragen benoetigen einen sitzungsgebundenen
  CSRF-Token mit mindestens 128 Bit Zufall im Header `X-CSRF-Token`, die
  vorgesehene Methode und den vorgesehenen Content-Type sowie Origin-,
  ersatzweise Referer-, und soweit vorhanden Fetch-Metadata-Pruefung. Eine
  erwartete fachliche Revision bleibt ein separates Gate.
- Abmeldung und Passwortaenderung machen die betroffene Sitzung ungueltig.
- Sitzungs- und Servicefreigaben werden nicht in normale Diagnoseexporte
  aufgenommen.

Mutation-Sequenzen sind pro Session monoton und atomar reserviert. Eine Session
haelt hoechstens acht abgeschlossene Replay-Outcomes und eine In-Flight-Anfrage;
nach Abschluss erhalten identische Retrys exakt dasselbe Outcome. Eine
konkurrierende In-Flight-Anfrage, Payloadwechsel, Sequenzluecken, abgelaufene
Sequenzen und `uint64`-Overflow bleiben fail-closed. Der exakte
Request-Fingerprint ist ein
SHA-256-Replay-Digest (32 Bytes); ein abgeschlossener Replay-Outcome besteht aus
Digest und kompaktem endlichem Outcome-Code, ohne dynamische
Replay-Payload-Puffer. Fuer vier Sessions samt je acht Outcomes betraegt der
Ziel-Replay-RAM 1760 Bytes (`WebSessionManager` Ziel 2344 Bytes); die feste
`WebSessionManager`-Speichergroesse wird zusaetzlich zur Compile-Zeit auf
4096 Bytes begrenzt. Die frueheren Werte (512-Byte-Fingerprint,
29.952-Byte-Replay-Payload, ca. 31 KiB) sind nur noch historische
Vor-Candidate-C-Evidence. Der fuer die erste Web-Mutationsroute geforderte integrierte
Vier-Session-Ressourcennachweis ist erfuellt
(`docs/audits/PR170_HW_GATE_20261005/`); die Ressourcenqualifikation der
Shell-Erweiterung aus #27 ist auf Hardware `NOT_RUN`.

## Laufbedienung in der Weboberflaeche (Issue #27)

Die Shell bedient den Lauf ueber die produktive Mutationsroute
`POST /internal/ui/run` (Session, CSRF und Replay-Vertrag wie oben; die
Application entscheidet fachlich, die Shell spiegelt keine zweite Domainregel).
Die Mutationssequenz wird pro Session in die Shell eingebettet (`0` =
Mutation nicht verfuegbar) und gilt nach Anmeldung erst nach erneutem Laden.

| Aktion | Angeboten in | Bestaetigung |
|---|---|---|
| Stop | `active-run`, `waiting` | zwei Klicks |
| Abschliessen | `completed` | zwei Klicks |
| Start | `standby` mit gewaehltem Programm und passender Katalogrevision | zwei Klicks |
| Quittieren, Stumm | bei aktiver Meldung (Ziel-ID am Button) | ein Klick |

Der erste Klick bindet die Bestaetigung an Modus, Laufrevision, Katalogrevision
und gewaehltes Programm; jeder Wechsel dieser Werte, ein fehlgeschlagener
Statusabruf oder eine andere Aktion verwirft sie. Unveraenderte 5-s-Polls
verwerfen sie nicht. Bei fehlgeschlagenem Statusabruf sind alle Aktionen
deaktiviert, bis wieder ein gueltiger Status vorliegt.

**Programmliste.** `GET /api/v1/programs` (Session, nur lesend) liefert
`{"version":1,"catalogRevision":"<n>","programs":[{"id","name"}]}` fuer
installierte und aktivierte Programme, hoechstens
`configuration_limits::kMaximumProgramCount` Eintraege und 3072 Bytes Body. Eine
Ueberschreitung oder eine fehlende Katalogquelle ergibt `503`, nie eine
Teilantwort oder eine falsche leere Liste; ein wirklich leerer Katalog ergibt
`programs:[]`. `Start` ist nur freigegeben, wenn die geladene `catalogRevision`
der `revisions.programCatalog` aus `/api/v1/status` entspricht; der Start
uebergibt genau diese Revision. Bei Abweichung oder Ladefehler wird die Auswahl
verworfen und die Liste neu geladen, ohne automatische Mutation.

Die Shell-JavaScript-Logik ist nativ nur als Text-/Zustandsvertrag getestet;
ein ausfuehrender Browsertest ist `NOT_RUN`.

## Shell-Transport und UI-Speicherreserve (RES-01)

Umsetzungsstand: statischer Shell-Anhang und native Gate-/Hold-Logik vorbereitet.
Die produktive F4-Anbindung wartet auf die Schwellenentscheidung gemäß
[Softwarebericht](audits/PR203_RES01_SOFTWARE_20261014.md); F4 ist noch nicht
als vollständiger produktiver Fix verifiziert. Der folgende F4-Abschnitt
beschreibt den freigegebenen Zielvertrag.

`HttpResponse::staticBodySuffix` bezeichnet optionale, unveränderliche Bytes mit
Prozesslebensdauer. Der Adapter sendet den besessenen `body` zuerst und danach
den Anhang mit dem öffentlichen ESP-IDF-6.1-`httpd_resp_send_chunk`-API; genau ein
leerer Chunk beendet eine erfolgreiche Antwort. Ein leerer Kopf wird ausgelassen.
Nach dem ersten Sendefehler folgen keine weiteren Chunks, auch kein Terminator;
der Adapter gibt diesen Fehler zurück. Ohne Anhang gilt weiterhin der bisherige
`httpd_resp_send`-Pfad. Shell-Antworten verwenden damit Chunked Transfer statt
Content-Length; ihre zusammengesetzten HTML-/JS-Bytes bleiben gleich.

Die normale Shell reserviert 1536 B für ihren variablen Kopf; ihr konstanter
6651-B-Anhang liegt im Flash. Die Provisionierungs-Shell teilt ebenfalls ihren
konstanten Script-Anhang ab. Die 8192-B-Grenze gilt für Kopf **plus** Anhang.
Alle Antwort-Ersetzungen löschen den Anhang, insbesondere JSON und Fehler:
auch eine zunächst aufgebaute, anschließend zu große Shell liefert ausschließlich
Status 503, `application/json; charset=utf-8` und
`{"error":"web-shell-unavailable"}`. Es folgen keine HTML-/JS-Bytes.

F4 ist eine begrenzte Risikominderung. Ein ausgelassener UI-Schritt friert die
Anzeige ein und verarbeitet keinen Touch; Regelung und Safety laufen unabhängig
weiter. Der Skip gibt den bestehenden Werksreset-Hold frei und sperrt dessen
erneutes Starten bis zu tatsächlich beobachtetem physischem Loslassen nach dem
letzten Skip und einem danach beobachteten neuen kalibrierten Kontakt. Alte
NoContact-Zustände, Lock-Timeouts, Controllerfehler und fehlende Kalibrierung
lösen diese Sperre nicht. Der vorhandene Factory-Reset-Flow bleibt alleiniger
Owner der Stufen, der fünf Sekunden und des Resets, für Variante A und B.

Die einmalige Vektorreserve ist durch eine nativ geprüfte strukturelle
Befehlsobergrenze begrenzt. Sie begrenzt weder Strings/LVGL noch den gesamten
Render-/Touch-Heapbedarf. Größter-Block-Prüfung und Hysterese reservieren keinen
Speicher: konkurrierende WLAN-/HTTP-Allokationen können nach bestandenem Gate
weiterhin OOM/Abort/Reset verursachen. Der Owner akzeptiert diese Restgefahr;
jeder beobachtete Reset bleibt ein Testfehler. Hardwarequalifikation und konkrete
Schwellenkalibrierung gehören zu separat freigegebenem G1/G2, nicht zum nativen
Logiknachweis. C0=GO ist seit dem Ownerentscheid vom 2026-10-14 akzeptiert;
Bootminimum 5228 B und lange GET-Gesamtdauern aus G0-R3 bleiben Hardwarebefunde.
F2 ist nicht Teil dieses Umfangs. `ACTUATOR_RELEASE=NO`.

## Startseite der angemeldeten Shell (R1-Slice, Issue #27)

Die authentifizierte Shell zeigt statt der rohen JSON-Ausgabe eine lesbare,
responsive Uebersicht, ausschliesslich aus den vorhandenen Read-only-APIs
(`/api/v1/status`, `/temperatures`, `/alerts`; drei Abrufe, 5-s-Takt,
unveraendert). Es gibt keinen neuen Endpunkt und keine Codec-Aenderung.

- **Ueberschrift** nach `homeMode` (`standby`, `active-run`, `waiting`,
  `completed`, `restricted`, `recovery`, `unavailable`) in Deutsch, Englisch
  und Spanisch; ein unbekanntes Token wird unveraendert angezeigt.
  `processState` wird nicht zusaetzlich lokalisiert.
- **Temperaturen** in fester Reihenfolge Schrank, Produkt, Kuehlung: Wert mit
  einer Nachkommastelle nur bei `valid`; sonst `--` mit Grund (`stale`,
  `failed`); eine nicht gelieferte Rolle erscheint als nicht verfuegbar, nie
  als `0`.
- **Meldungen:** Zaehler und je aktiver Meldung der Code (Bindestriche zu
  Leerzeichen) mit Schweregrad als Klasse sowie Marker quittiert/stumm; die
  Meldungscodes selbst sind nicht lokalisiert.
- **Verbindung und Alter:** `Offline` (nie ein gueltiger Snapshot), `Online`,
  `Stale` mit `Stand vor N s`; die letzten Werte bleiben gedimmt sichtbar.
  `N` ist das Alter des **Statusabrufs** seit dessen Beginn, keine Aussage ueber
  das Alter des Sensor-Messwerts (dafuer gilt die Sensor-`quality`).
- **Aktionssperre:** `P` (Gating) wird bei einem angewendeten Fehler und bei
  Ueberschreiten von 15 s seit dem letzten Abrufbeginn `null`; die Anzeigekopie
  (`LP`/`LT`) gibt nie Aktionen frei.
- **Reihenfolge:** jeder Abruf erhaelt eine Generation (`G`); ein Ergebnis,
  Erfolg wie Fehler, einer aelteren als der bereits angewendeten Generation
  (`H`) wird verworfen, ohne Wirkung auf Stand, Alter oder Verbindungsstatus.
- **Darstellung:** einspaltig mit grossen Bedienflaechen, ab 40 rem zwei
  Spalten; dynamischer Text nur ueber `textContent`.
- **Groessenschranke:** 8192 Bytes (Ownerentscheid D1 = A); die gemessene
  authentifizierte Shell hat 8048 Bytes (Reserve 144 Bytes, inklusive der
  19 Bytes fuer die breiteste Mutationssequenz).

Nicht Teil dieses Slices (benoetigen neue API-Felder, eigener Slice): Geraete-
und Programmname, Solltemperatur, verstrichene/verbleibende Zeit,
Abschlussverhalten, Netzwerk-/Zeitstatus, Aktorstatus und die Mehrseiten-
Navigation. Browsertests, Hardware-Heapmessung der groesseren Shell und die
dynamischen Reihenfolge-/Stale-Szenarien sind `NOT_RUN`.

## Servicebereich in der Weboberflaeche

Der Web-Servicebereich wird durch dieselbe vierstellige Service-PIN geschuetzt
wie der lokale Servicebereich.

### Freigabe

Nach korrekter PIN-Eingabe wird der Servicebereich fuer genau diese fluechtige
normale oder anonyme lokale Websession freigegeben: 5 Minuten Inaktivitaet,
hoechstens 15 Minuten absolut. Diese bewusst strengere netzwerkseitige
Securitygrenze bleibt von der lokalen Touch-Servicefreigabe getrennt und wird
nicht auf deren 10-Minuten-Inaktivitaet vereinheitlicht.

Verbindliche Regeln:

- Servicefreigabe ist getrennt von der normalen Webanmeldung beziehungsweise
  anonymen lokalen Session.
- Die Freigabe gilt nur fuer die jeweilige Websitzung.
- Sie wird nach Inaktivitaet automatisch verworfen.
- Abmeldung verwirft auch die Servicefreigabe.
- Schliessen oder Ablauf einer normalen Sitzung verwirft die Servicefreigabe.
- Ein anderes angemeldetes Endgeraet wird dadurch nicht ebenfalls entsperrt.
- PIN-Fehlversuche werden global gezaehlt: nach drei Fehlern 30 Sekunden
  Sperre, weitere Bloecke verdoppeln bis hoechstens 30 Minuten. Der
  sicherheitsrelevante Fehlversuchs-/Sperrzustand ist atomar neustartfest.

### Kritische Aktionen

Auch bei entsperrtem Servicebereich benoetigen kritische Aktionen eine eigene
Bestaetigung, beispielsweise:

- vollstaendiger Werksreset
- Netzwerkeinstellungen loeschen
- Touchkalibrierung zuruecksetzen
- Sensorzuordnung aendern
- direkte Aktortests starten

Ein Werksreset oder eine vergleichbar weitreichende Aktion darf bei laengerer
Inaktivitaet eine erneute PIN-Eingabe verlangen.

## Live-Aktualisierung

Temperaturen, Restzeit, Prozessphase, Aktorstatus und Meldungen aktualisieren sich
ohne manuelles Neuladen.

Die technische Umsetzung kann je nach Ressourcen und Bibliotheken erfolgen ueber:

- Server-Sent Events
- WebSocket
- vergleichbaren Push-Mechanismus
- kontrolliertes periodisches Nachladen als Fallback

Die Spezifikation legt nicht vorzeitig ein bestimmtes Protokoll fest. Das
sichtbare Verhalten ist verbindlich:

- neue Messwerte erscheinen laufend
- Zustandswechsel werden zeitnah dargestellt
- Warnungen und Fehler werden ohne komplettes Neuladen sichtbar
- Verbindungsabbruch wird klar angezeigt
- veraltete Werte werden nicht als aktuell dargestellt
- nach Wiederverbindung wird ein vollstaendiger aktueller Zustandsabzug geladen

Die Weboberflaeche zeigt den Zeitpunkt beziehungsweise das Alter des letzten
gueltigen Status, falls die Live-Verbindung unterbrochen ist.

## Gleichzeitige Bedienung und Revisionsschutz

Display, mehrere Browser und mehrere Browser-Tabs duerfen gleichzeitig lesend
verbunden sein.

Jeder bearbeitbare Datensatz besitzt eine Revisionskennung beziehungsweise eine
vergleichbare Version.

Vorgesehener Ablauf:

```text
Browser A laedt Programmrevision 12
Browser B speichert Programmrevision 13
Browser A versucht seine alte Revision 12 zu speichern
  -> Speichern wird nicht still ausgefuehrt
  -> Konflikt anzeigen
  -> aktuellen Stand laden oder Aenderungen vergleichen
```

Verbindliche Regeln:

- Veraltete Daten duerfen neuere Aenderungen niemals still ueberschreiben.
- Jede veraendernde Aktion enthaelt die erwartete Revision.
- Bei Abweichung wird die Aktion abgelehnt oder bewusst neu bestaetigt.
- Start, Stop, Quittieren und vergleichbare Laufaktionen werden atomar
  verarbeitet.
- Eine bereits durch eine andere Quelle ausgefuehrte Aktion wird nicht doppelt
  ausgefuehrt.
- Die Quelle wird mindestens als `display`, `web` oder `service_web`
  protokolliert.
- Sicherheitslogik darf eine grundsaetzlich gueltige Webaktion jederzeit
  ablehnen.

Beispielmeldung:

```text
Programm wurde inzwischen an einer anderen Oberflaeche geaendert.

[Aktuellen Stand laden]
[Aenderungen vergleichen]
```

## Temperaturdiagramm des aktuellen Laufes

Die Weboberflaeche zeigt fuer den aktuellen Lauf ein Diagramm mit mindestens:

- Produkttemperatur, sofern verfuegbar
- Schranktemperatur
- Solltemperatur
- Phasenwechseln
- Warnungs- und Unterbrechungsmarkierungen, soweit sinnvoll

Anforderungen:

- mobil und am Computer lesbar
- Achsen, Einheiten und Zeitbasis eindeutig
- fehlende Messwerte als Luecke und nicht als erfundene Verbindung darstellen
- Sensorwechsel sichtbar kennzeichnen
- Laufzeitkorrektur nach Stromausfall nachvollziehbar machen
- Diagramm darf den Regelprozess nicht durch uebermaessige Ressourcenlast
  beeintraechtigen

Vollstaendige Diagramme vergangener Laeufe sind nicht verbindlicher Bestandteil
von Phase 5B. Umfang, Aufbewahrung und Export historischer Messdaten werden in
Phase 6 und Phase 9 festgelegt.

## Akzeptierte Entscheidungen aus Phase 5B

- [x] gleicher Funktionsumfang auf Handy, Tablet und Computer
- [x] responsive, mobil optimierte Darstellung
- [x] zustandsabhaengige Web-Startseite
- [x] getrennte Navigation fuer Uebersicht, Programme, manuellen Betrieb,
      Meldungen, Einstellungen, Diagnose, Service und System
- [x] Sprache je Browser unabhaengig von der Displaysprache
- [x] Deutsch, Spanisch und Englisch auch im Web
- [x] gemaess ADR-017 keine dauerhafte Anmeldung in Release 1
- [x] fluechtige serverseitige Sessions mit 30 Minuten Inaktivitaet,
      12 Stunden absoluter Dauer und Widerruf beim Geraeteneustart; ein reiner
      Browserneustart ist kein garantiertes Widerrufsereignis
- [x] Service-PIN entsperrt nur die jeweilige Sitzung fuer begrenzte Zeit
- [x] kritische Serviceaktionen verlangen eine eigene Bestaetigung
- [x] Live-Aktualisierung ohne manuelles Neuladen
- [x] Revisionsschutz gegen stilles Ueberschreiben konkurrierender Aenderungen
- [x] Temperaturdiagramm fuer den aktuellen Lauf

## Noch offen fuer Phase 5C und spaeter

- konkrete Passwortmindestanforderungen
- technische Auswahl fuer Live-Aktualisierung
- weitere HTTP-Schutzheader ueber den verbindlichen Cookie-/CSRF-Vertrag hinaus
- maximale Anzahl paralleler Webverbindungen
- Abtastrate und Reduktion der Diagrammdaten
- Speicherung und Export vergangener Laufdiagramme
- genaue Darstellung von Vergleich und Konfliktaufloesung
- Verhalten hinter Reverse Proxy und VPN
