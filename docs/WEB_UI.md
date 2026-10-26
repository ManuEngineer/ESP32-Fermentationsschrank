# Lokale Weboberflaeche

## Status

Dieses Dokument beschreibt die in Phase 5B akzeptierten Regeln fuer die lokale
Weboberflaeche, Anmeldung, Sitzungen, Servicefreigabe, Live-Aktualisierung und
gleichzeitige Bedienung.

Die Netzwerkbereitstellung und grundlegenden Sicherheitsgrenzen stehen in
[`NETWORK.md`](NETWORK.md).

## R1/R2-Geltungsgrenze

Die verbindliche Releasegrenze steht in
[SPECIFICATION_REVIEW](SPECIFICATION_REVIEW.md#r1r2-zuschnitt-nach-yagni).
Gemeinsame Fachzustaende/Commands verlangen keinen identischen Funktionsumfang
von Touch und Web. In R1 bleiben alle vorhandenen sicheren Webslices erhalten:
Anmeldung/Setup, Auth/CSRF/Replay, Programme anzeigen und starten, Laufaktionen,
Meldungen, Einstellungen, Browserstatus, Sessions und DE/EN/ES. Das ist keine
pauschale Vollstaendigkeits- oder Ressourcenqualifikation.

Web-Programmeditor (D6), zusaetzliche Web-Manuelloberflaeche, vollstaendige
technische Webdiagnose/Detail-APIs, Laufcharts/Historie, Exporte und portables
Backup/Import sind `R2_VORGEMERKT` (#27/#28/#19). Neue Service-/Systemseiten
und eine gefuehrte Service-UI werden fuer R1 nicht vorausgesetzt; der gefuehrte
Zusatzablauf bleibt R2-Kandidat. Bereits implementierte Settings, lokale
Servicefunktionen, Laufcommands und ihre fachlichen Gates bleiben unveraendert.

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
- zusaetzliche Ansicht `Manueller Betrieb`: R2 vorgemerkt (#27)

Beispiel:

```text
Fermentationsschrank

Produkt       nicht angeschlossen
Schrank       22,4 °C

[Programm starten]
[Manueller Betrieb: R2 vorgemerkt]
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

Die Weboberflaeche behaelt ihre vorhandenen sicheren Laufaktionen. Die
folgende Liste fordert keine zusaetzliche R1-Manuelloberflaeche:

- Abschluss quittieren
- Laufdetails anzeigen
- optional neuen manuellen Kuehllauf starten

## Navigation

Die vorhandene R1-Shell-Navigation umfasst:

1. Uebersicht
2. Programme
3. Meldungen
4. Einstellungen

Zusaetzliche Manuell-, Diagnose-, Service- und Systemseiten sind kein neues
R1-Muss. Historische Protokolle und Diagramme sind R2 vorgemerkt; notwendige
aktuelle Meldungen und Statuswerte bleiben erhalten.

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

**Umsetzung (Issue #27, Plan a671c33).** Ein Auswahlfeld (Deutsch / English / Español,
`aria-label` „Language“) steht am Seitenanfang der Setup- und der Anmelde-/Dashboard-
Shell in jedem Authentifizierungszustand. Rein clientseitig und ohne Request:

- Start: gespeicherter Code aus `localStorage.lang`, wenn er genau `de`, `en` oder `es`
  ist; sonst die Browsersprache (erste zwei Zeichen, nur diese drei Codes), sonst
  Englisch. Ungueltige oder manipulierte Werte werden ignoriert und nie weiterverwendet.
- Wechsel: die Funktion `sl()` validiert erneut (ungueltig → Englisch), setzt `<html lang>`,
  das Auswahlfeld und alle statischen Texte; danach rendern `dr()` und `ui()` die
  dynamischen Texte und Knoepfe neu. Eine bereits aktive Bestaetigung („Sicher?“) wird in der
  neuen Sprache neu gerendert, ohne `K`, `KC`, Aktionssperren oder Bestaetigungssemantik zu
  aendern.
- Speichern: nur der validierte Code, in `try/catch`. Fehlender oder blockierter Speicher
  (Privatmodus, Quota) loest keine Exception aus; die Wahl bleibt fuer die Seite wirksam und
  faellt beim naechsten Laden auf den Vorschlag zurueck. Der Schluessel gilt fuer Setup- und
  Dashboard-Shell gemeinsam und je Browser und Ursprung; Browser/Endgeraete sind
  unabhaengig voneinander und von der Sprache des Touchdisplays.
- In der Setup-Shell behaelt eine sichtbare Ergebnis- oder Fehlermeldung beim Wechsel ihre
  Art (Schluessel) und wird in der neuen Sprache neu gerendert; eine ausstehende Antwort
  erscheint in der zum Antwortzeitpunkt gewaehlten Sprache. Der Wechsel veraendert keinen
  Formularzustand und loest keinen Request aus.
- Es gibt keinen serverseitigen Sprachzustand (kein Cookie, kein Header, keine API), keine
  Aenderung an Geraetekonfiguration, Prozessdaten, Authentifizierung, Session, CSRF,
  Polling oder Mutation. Der Ergebnistext der zuletzt ausgefuehrten Aktion behaelt bis zur
  naechsten Aktion seine Sprache; Programm-/Geraetenamen, Meldungscodes und Zeitzonen-Id
  bleiben unuebersetzt.
- Browser- und Geraeteverhalten sind nicht qualifiziert (`NOT_PROVEN`); die Wahl ist nur im
  ausfuehrenden Fake-DOM-Test belegt.

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
- Texte in Deutsch, Englisch und Spanisch; die Sprache folgt der gespeicherten
  Browserwahl, sonst der Browsersprache (Vorschlag), Englisch als Rueckfall; ein
  Auswahlfeld am Seitenanfang erlaubt den Wechsel (siehe „Sprache je Browser“);
- eine Meldung bei `403`, dass die Freigabe zuerst am Geraet erfolgen muss.

Das Formular prueft keine Passwort- oder PIN-Regeln; diese gehoeren der
Authentisierungsdomaene. Die weiterhin inline ausgelieferte Setup-Shell hat eine eigene Grenze von
4096 B (Kopf plus Anhang, mindestens 128 B Reserve). Die normale Asset-Shell
hat die getrennte Grenze 2048 B; die fruehere gemeinsame 11456-B-Grenze ist
historisch (PR #209, freigegebener Plan b7d5e2d).

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

Umsetzungsstand: statischer Shell-Anhang und produktive Gate-/Hold-Anbindung
sind softwareseitig umgesetzt ([Softwarebericht](audits/PR203_RES01_SOFTWARE_20261014.md)).
Vorhandene F1-/Software- und native F4-Nachweise werden wiederverwendet.
Ownerentscheid 18.10.2026: eine Person bedient lokal oder über einen Webclient;
keine systematische gleichzeitige Web-/Touchkampagne. Anzeige, WLAN und HTTPD
können intern dennoch gleichzeitig Heap beanspruchen. G1′ ist keine Pflicht
von PR #203; die vorbereitete Kampagne wurde vor Ausführung storniert.
Vollständige R1-Inbetriebnahme und Aktorqualifikation bleiben separat offen.

`HttpResponse::staticBodySuffix` bezeichnet optionale, unveränderliche Bytes mit
Prozesslebensdauer. Der Adapter sendet den besessenen `body` zuerst und danach
den Anhang mit dem öffentlichen ESP-IDF-6.1-`httpd_resp_send_chunk`-API; genau ein
leerer Chunk beendet eine erfolgreiche Antwort. Ein leerer Kopf wird ausgelassen.
Nach dem ersten Sendefehler folgen keine weiteren Chunks, auch kein Terminator;
der Adapter gibt diesen Fehler zurück. Ohne Anhang gilt weiterhin der bisherige
`httpd_resp_send`-Pfad. Shell-Antworten verwenden damit Chunked Transfer statt
Content-Length; die Setup-Shell bleibt bytegleich zum vorherigen Generator. Die normale
Shell referenziert jetzt das getrennte versionierte Script (siehe unten).

Die normale Shell enthaelt die sitzungsabhaengigen Konstanten `S/A/U/Q` und
referenziert `GET /assets/app.<crc8>.js`; CRC-32 bindet die URL an die Scriptbytes.
Der Asset ist oeffentlich, byte-konstant und frei von Sitzung, CSRF, Sequenz,
Credentials und Konfiguration. Nur diese exakte URL erhaelt
`Cache-Control: private, max-age=31536000, immutable`; alte/falsche Versionen,
Queries und Unterpfade fallen zur 404 durch. Netzwerk-Setup behaelt Vorrang.
Ein Asset-Ausfall zeigt einen sprachneutralen Hinweis in der Shell; ohne Script
ist keine Bedienaktion moeglich. GET-only, gleiche Metadatenpruefung, kein Cookie.

Getrennte Grenzen: normale Shell 2048 B, Setup-Shell 4096 B (beide mit mindestens
128 B Normalreserve), Asset 16384 B (mindestens 1024 B Abschlussreserve).
Der Asset liegt als unveraenderlicher Anhang im Flash, ohne Koerperkopie im Heap.
Die konkrete aktuelle Offline-Messung steht in der
[PR-209-Evidence](audits/PR209_WEB_REMAINING_SCOPE_IMPLEMENTATION.md).
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

Die Owner-freigegebene Schwelle bleibt **vorläufig (`F4_THRESHOLD=PROVISIONAL`)**:
104 Befehle × 64 B = 6656 B als struktureller ESP32-Vektorblock, Faktor 2
ergibt 13312 B Skip-Schwelle, Wiederaufnahme ab 16640 B (25% Hysterese).
`updateProductUi` prüft vor Snapshot/Touch/Renderkey; beim Skip verändert es
weder Präsentationskopie noch zuletzt gerenderten Schlüssel. Eine Warnzeile
`ui_skipped_low_memory` mit kumuliertem Zähler erscheint höchstens alle 10 s.
Ein kausal isolierter Render-/Touch-Bedarf b und vollständige transiente Peaks
sind nicht belegt; keine abschließende empirische Kalibrierung nach
max(Vektorblock, b) × 2 und keine neue Schwelle aus gemeinsamen Heap-Schwankungen.
`PROVISIONAL_PENDING_G1` bleibt nur als historischer Bezeichner im unveränderten
Produktcode/alten Belegen erhalten; keine ausstehende PR-G1′-Pflicht daraus.
Skip ist kein Bedarf 0 B. Hardware-Skip→Hold-/Pfad-Coverage bleibt NOT_PROVEN,
allein deren Fehlen ist laut Ownerentscheid kein Mergeblocker dieses PRs.

Die einmalige Vektorreserve ist durch eine nativ geprüfte strukturelle
Befehlsobergrenze begrenzt. Sie begrenzt weder Strings/LVGL noch den gesamten
Render-/Touch-Heapbedarf. Größter-Block-Prüfung und Hysterese reservieren keinen
Speicher: konkurrierende WLAN-/HTTP-Allokationen können nach bestandenem Gate
weiterhin OOM/Abort/Reset verursachen. Der Owner akzeptiert diese Restgefahr;
jeder beobachtete Reset bleibt ein Testfehler. Native Logiknachweise sind keine
Hardware-/OOM-Qualifikation. G1′ wird nicht ausgeführt; spätere freiwillige Diagnose
oder vollständige R1-Inbetriebnahme erfordern eine eigene Ownerfreigabe. Der
[RES-01-Plan §9](tasks/issue-27-res01-heap-hardening-plan.md)
trennt die aktuelle PR-Einordnung von diesen offenen Nachweisen.
C0=GO ist seit dem Ownerentscheid vom 2026-10-14 akzeptiert;
Bootminimum 5228 B und lange GET-Gesamtdauern aus G0-R3 bleiben Hardwarebefunde.
F2 ist nicht Teil dieses Umfangs. `ACTUATOR_RELEASE=NO`.

## R1-Restumfang in PR #209

Die Navigation enthaelt Uebersicht, Meldungen, Programme und Einstellungen und
ist erst mit einer gueltigen Anzeige-Kopie sichtbar. Die Fragment-Allowlist
beschraenkt sich auf diese Ansichten; aria-current und umbrechende Leiste mit
2,75-rem-Touchflaechen sind strukturell vorhanden. Das Umschalten selbst
veraendert keinen Fachzustand. Stale-/Offline-Daten bleiben gedaempft mit Abrufalter;
Mutationen verlangen weiterhin einen frischen Status und alle bestehenden Gates.

Meldungen erscheinen in API-Reihenfolge, mit unuebersetztem Code, Schweregrad und
aktiv/quittiert/stumm/erledigt/Entscheidung erforderlich. Quittieren und Stumm je
Zeile sind bei Busy, Stale/Offline, erledigten oder bereits erreichten Zustaenden
gesperrt. Die Uebersichts-Aktionsleiste bleibt bestehen.

`GET /api/v1/programs/<id>` liefert fuer einen installierten, aktivierten Eintrag
Summary und Katalogrevision aus derselben Quelle. Identifiergrenzen 1–48 B,
Session, kein Anfrage-Body, GET-only, Antwort hoechstens 3072 B. Unbekannt/
deaktiviert/nicht installiert: 404; ungueltige ID: 400; Quelle nicht verfuegbar:
503. Fehlende Werte bleiben null und nicht startbar. Die Web-Ansicht sendet nur
abweichende Startoptionen ueber den bestehenden `start-program`-Intent;
Programm, Katalogrevision und Eingaben sind an die Zwei-Klick-Bestaetigung
gebunden. Eine alte oder fehlgeschlagene Detailantwort verwirft die Auswahl.

`GET /api/v1/settings` liest Name, Zeitzone, UserConfiguration-Revision und
`nameEditable` aus dem Application-Owner. Nur `set-device-name` schreibt, als
Settings-Variante in `POST /internal/ui/run` hinter denselben Session-/CSRF-/
Sequenz-/Digest-/Replay-Gates. Settings-Body bis 1024 B, Run-Body weiter 480 B;
Antwort-/Replaybudget unveraendert. Fehlendes `r.u`, unbekannte Intents und
`set-time-zone` werden ohne Mutation verworfen. Kein `applyTimeZone`.
Namensvalidierung, Laufsperre und Commit liegen allein in der Application;
Web-Commits persistieren Herkunft WebInterface/3, Touch weiterhin LocalDisplay/2.
Hostname und WLAN-Name aendern sich erst beim naechsten normalen Netzwerk-/
Geraetestart; die UI zeigt diesen Hinweis am Namensfeld und im Ergebnis.

Die Ergebnisanzeige unterscheidet stale/busy/rejected/confirmation-required/
too-large/write-failed. Bei unavailable bzw. unbestimmtem Commit wird kein
Erfolg behauptet: Sequenz gesperrt, Settings/Status neu geladen, sichtbarer
Pruefhinweis. Ein Browser-Reload initialisiert die Sequenz erneut aus der Shell.
Native und Fake-DOM-Nachweise ersetzen weder Browserlayout/-Bedienung noch
Geraetelast, HWM oder Heap: diese bleiben NOT_PROVEN bis zum separat
freizugebenden D9-Testpunkt. Manuellbetrieb, Diagnose, Diagramme, Journale,
Exporte, Backup, Service und Programmeditor sind durch diesen PR nicht erledigt.

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
- **Groessenschranke:** 11456 Bytes (Ownerentscheid D1 = A, Plan a671c33,
  einmalig; zuvor 11264 Bytes); die gemessene authentifizierte Shell hat
  11312 Bytes (Reserve 144 Bytes, davon 128 Bytes Normalreserve, die die 19
  Bytes fuer die breiteste Mutationssequenz enthaelt, und 16 Bytes darueber).
  Keine weitere Erhoehung vorgesehen.

### Laufinformationen (Issue #27, Plan 3376a19)

`/api/v1/status` (Version 1) enthaelt additiv `run`: `null` ohne kanonischen Lauf,
sonst ein Objekt aus der unveraenderlichen Laufquelle:

- `id` (Run-ID oder `null`), `sourceKind` (`factory-catalog`, `user-program`,
  `manual-timed`, bei reinem manuellem Halten `null`), `programName` (Name des
  eingefrorenen Programms, `null` ohne Namen; kein Katalog-Join im Browser);
- `targetCelsius`: endliche Zahl aus dem bestehenden Control-Context-Resolver
  (in Cooling/CoolHolding das Kuehlziel, nicht das Fermentationsziel), sonst
  `null`; ein Sollwert ist keine Sensor- oder Aktorfreigabe;
- `completion`: `null` oder `{mode, coolingTargetCelsius, holdDurationMinutes}` mit
  `mode` in `finish`, `cool-finish`, `cool-hold-duration`, `cool-hold-manual`;
- `time`: `{scope:"phase", elapsedSeconds, remainingSeconds, remainingKind}` mit
  `remainingKind` in `timed`, `unknown`, `unbounded`, `not-applicable`.

Die Zeit ist **phasenbezogen** (D2-A), keine Gesamtlaufzeit. `elapsedSeconds` ist
die monotone Zeit seit Phaseneintritt plus nur ein exakter (untere gleich obere
Grenze) Vorzeitwert, der fuer genau diese Phase getaggt ist; fehlende Zeitquelle,
Ruecklauf, falsches Tag oder unklare Grenzen ergeben `null` und `unknown`, nie 0.
Ein Rest existiert nur fuer Fermenting und zeitbegrenztes CoolHolding (nicht
unter 0), `unbounded` fuer CoolHolding bis manuelles Ende und ManualHolding,
`not-applicable` fuer Standby/Completed. Das Ende einer Phase folgt aus dem
Prozesszustand, nie aus Rest 0. Die Sekunden sind kein fachlicher Anteil der
UI-Refresh-Revision. Der Browser zaehlt nicht weiter; bei Fehler oder Stale
bleibt die Anzeige gedimmt mit Abrufalter. Eine Zeitzahl ueber 2^53−1 oder
fehlerhafte Daten ergeben den bestehenden 503 ohne Suffix.

### Standbyinformationen (Issue #27, Plan 9656a18)

`/api/v1/status` (Version 1) enthaelt zusaetzlich, rein lesend und in **allen**
`homeMode`-Werten angezeigt:

- `deviceName`: sichtbarer Geraetename der `UserConfiguration` oder `null`, wenn
  die Konfiguration nicht lesbar ist (Anzeige „Nicht verfuegbar“);
- `networkState`: genau `connected`, `disconnected` oder `unavailable` als
  tatsaechlicher Zustand des Netzwerktransports des Geraets, abgebildet von
  `toDeviceUiNetworkStatus` (AccessPointOnly/HomeConnected → `connected`;
  SetupAccessPoint/ConnectingHome/CandidateTesting → `disconnected`; Stopped/
  Failed/unbekannt → `unavailable`). Der Zustand wird nur gemeldet, wenn der
  Lifecycle den **konfigurierten** Modus (`networkMode`) betreibt, sonst
  `unavailable`. Er sagt weder etwas ueber Internet noch ueber den Abrufstatus
  des Browsers aus;
- `clock`: `null` ohne vertrauenswuerdige UTC-Zeit, ohne darstellbare Zone oder
  ausserhalb der Jahre 0..9999, sonst `{local:"YYYY-MM-DD HH:MM", zone:"<kanonische
  Id>"}` aus dem einzigen UTC→Lokal-Owner `toLocalTime`; Minutenaufloesung, ohne
  Offset.

`networkMode` bleibt das separate Feld des gewaehlten Modus. Die Shell zeigt den
Namen als erste Zeile, danach „Netzwerk: <Modus> · <Zustand>“ (Modus: Eigener AP/
Heimnetz, Zustand: verbunden/getrennt, sonst „Nicht verfuegbar“) und
„Uhrzeit: <lokal> <Zone>“ beziehungsweise „nicht synchronisiert“, in Deutsch,
Englisch und Spanisch. Die Woerter `Online`/`Offline` bleiben dem Abrufstatus
vorbehalten. Nie wird eine Browseruhr verwendet; die Zeit gilt zum Abrufzeitpunkt
und wird bei Fehler oder Stale gedimmt mit Abrufalter angezeigt. Es entstehen
keine SSID, kein Passwort, keine SoftAP-Daten und keine Adresse in der Ausgabe.
Name/Zone und `revisions.userConfiguration` stammen aus zwei Guard-Eintritten
und sind nicht atomar zueinander (naechster Abruf konvergiert).
Die Daten kommen aus einem schmalen Application-Accessor (`deviceInfoView`),
nicht aus `FermentationUiSnapshot`; die Display-Refresh-Revision bleibt
unveraendert. Ressourcen: [Evidence](audits/PR207_STANDBY_INFO_RESOURCE_EVIDENCE_20261020.md).

Nicht Teil dieser Slices: Aktorstatus,
Gesamtlaufzeit/Programmende-ETA und die Mehrseiten-Navigation. Die Hardware-
Heapmessung H-RAM-1 (Mess-Overlay M, Standby/Ein-Client, gescriptetes Replay und
echt headless Chromium) wurde ausgefuehrt: begrenzter positiver Teilnachweis, siehe
[Evidence](audits/PR206_H_RAM_1_EVIDENCE_20261019.md); `RAM_PASS=NOT_PROVEN` fuer das
vollstaendige Laufprofil, aktiver Lauf (S4) `NOT_RUN`, Produktimage nicht
hardwarequalifiziert. Die dynamischen Reihenfolge-/Stale-Szenarien sind nur im
ausfuehrenden Fake-Fetch-Test, nicht gegen Hardware belegt.

## Servicebereich: Sicherheitsvertrag fuer spaetere Verbraucher

Eine zusaetzliche Web-Service-Seite/-Route ist keine R1-Pflicht. Die folgenden
PIN-, Sitzungs-, Frist- und Bestaetigungsregeln bleiben fuer vorhandene und
spaetere Verbraucher verbindlich; sie erteilen keine Aktorfreigabe.

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

- Netzwerkeinstellungen loeschen
- Touchkalibrierung zuruecksetzen
- Sensorzuordnung aendern
- direkte Aktortests starten

Der vollstaendige Werksreset ist in Release 1 ausschliesslich lokal zugaenglich
(PIN-geschuetzter Ablauf A und PIN-unabhaengiger Recoveryweg B+). Es gibt keine
Web-Werksreset-Route; der Serviceabschnitt beschreibt fuer andere Funktionen
zurueckgestellten Scope. PR #209 baut keine Service-Seite/-Route ohne Verbraucher.

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

## Temperaturdiagramm des aktuellen Laufes (R2 vorgemerkt)

Kein R1-Chart und keine R1-Speicherreserve sind erforderlich. Die folgenden
Darstellungsregeln bleiben fuer eine spaetere Planung erhalten; fehlende
Messungen werden nie erfunden. Ein spaeteres Diagramm zeigt mindestens:

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
- [x] vorhandene R1-Navigation fuer Uebersicht, Programme, Meldungen und Einstellungen
- R2/Future: zusaetzliche Manuell-, Diagnose-, Service- und Systemseiten nach Releasegrenze
- [x] Sprache je Browser unabhaengig von der Displaysprache
- [x] Deutsch, Spanisch und Englisch auch im Web
- [x] gemaess ADR-017 keine dauerhafte Anmeldung in Release 1
- [x] fluechtige serverseitige Sessions mit 30 Minuten Inaktivitaet,
      12 Stunden absoluter Dauer und Widerruf beim Geraeteneustart; ein reiner
      Browserneustart ist kein garantiertes Widerrufsereignis
- Sicherheitsvertrag bei Serviceverbrauchern: PIN entsperrt nur die jeweilige
  Sitzung fuer begrenzte Zeit; kein R1-Auftrag fuer neue Serviceoberflaechen
- Sicherheitsvertrag bei Serviceverbrauchern: kritische Aktionen verlangen
  eine eigene Bestaetigung
- [x] Live-Aktualisierung ohne manuelles Neuladen
- [x] Revisionsschutz gegen stilles Ueberschreiben konkurrierender Aenderungen
- R2 vorgemerkt: Temperaturdiagramm fuer den aktuellen Lauf

## Noch offen fuer Phase 5C und spaeter

- konkrete Passwortmindestanforderungen
- technische Auswahl fuer Live-Aktualisierung
- weitere HTTP-Schutzheader ueber den verbindlichen Cookie-/CSRF-Vertrag hinaus
- maximale Anzahl paralleler Webverbindungen
- Abtastrate und Reduktion der Diagrammdaten
- Speicherung und Export vergangener Laufdiagramme
- genaue Darstellung von Vergleich und Konfliktaufloesung
- Verhalten hinter Reverse Proxy und VPN
