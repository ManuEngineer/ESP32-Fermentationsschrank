# Netzwerk und WLAN-Bereitstellung

## Status

Dieses Dokument beschreibt den aktuellen R1-Scope fuer Netzwerkmodi,
WLAN-Ersteinrichtung, Geraetename, Adressierung und die grundlegende
Absicherung der lokalen Weboberflaeche. R1 unterstuetzt genau ein gespeichertes
Heim-WLAN oder den ausdruecklichen AP-only-Modus.

Die Ownerentscheidung fuer Issue #164 lautet `VARIANT_B_QR_RETAINED`: Die lokale
HOME_WIFI-SSID-/Passworteingabe am Touchdisplay, die dafuer erforderliche
Bildschirmtastatur sind aus R1 deferiert. Der WLAN-QR zum Beitritt in den
geschuetzten Setup-/AP-only-SoftAP mit einer aus dem aktuellen
`UserConfiguration.deviceName` abgeleiteten SSID und einem pro `StorageEpoch`
persistierten 16-stelligen SoftAP-Passwort ist R1-pflichtig;
nur ein separater Webseiten-QR bleibt Future Scope. R1 verwendet fuer
Heim-WLAN-Credentials den browserbasierten Setup-Pfad; Display-Moduswahl und
lokale Anzeige der aktuellen SoftAP-Zugangsdaten sowie der direkten IP bleiben
R1.

Die genaue Weboberflaeche, Sitzungsverwaltung und Konfliktbehandlung werden in
`WEB_UI.md` ergaenzt.

## Grundsaetze

- Der Fermentationsprozess und alle Sicherheitsfunktionen funktionieren ohne
  WLAN, Internet, Heimserver oder Cloud.
- Netzwerkfunktionen sind Bedien- und Diagnosekomfort, aber keine Voraussetzung
  fuer die Temperaturregelung.
- Der ESP32 wird niemals direkt ueber eine Portfreigabe aus dem Internet
  erreichbar gemacht.
- Zugangsdaten, Passwoerter und sonstige Geheimnisse werden nicht in das
  Repository, in normale Protokolle oder in Diagnoseexporte geschrieben.
- Display und Weboberflaeche greifen auf denselben fachlichen Geraetezustand zu.

## R1-Netzwerkmodi und WLAN-Ersteinrichtung

### Moduswahl am lokalen Display

Beim Setup waehlt der Benutzer am lokalen Display zwischen:

- **Mit Heim-WLAN verbinden** (`HOME_WIFI`)
- **Ohne Heim-WLAN betreiben** (`AP_ONLY`)

Die Auswahl darf spaeter ueber die normalen Netzwerkeinstellungen geaendert
werden. Sie ist keine Web- oder Cloud-Voraussetzung.

```text
INITIAL_NETWORK_MODE_SELECTION=DISPLAY
R1_NETWORK_MODE_AP_ONLY=SUPPORTED
R1_NETWORK_MODE_HOME_WIFI=SUPPORTED
HOME_WIFI_COUNT=1
HOME_WIFI_MODE=RECOMMENDED
AP_ONLY_MODE=SUPPORTED_EXPLICITLY
R1_NETWORK_WEB_TRANSPORT=NATIVE_ESP_IDF_HTTP
NETWORK_MODE_SELECTION_CONTRACT=R1_ISSUE164
INITIAL_NETWORK_SELECTION=UNSELECTED_UNTIL_USER_CHOICE
USER_SELECTABLE_NETWORK_MODES=AP_ONLY_HOME_WIFI_ONLY
PHYSICAL_DISPLAY_TOUCH_PROOF=ISSUE31
ISSUE164_BLOCKED_BY_PHYSICAL_DISPLAY_PROOF=NO
```

Der typisierte, rendererunabhaengige Auswahlvertrag gehoert zu Issue #164.
Der reale Nachweis von Displayanzeige, Touchinteraktion und Renderer bleibt
Issue #31 zugeordnet. Issue #164 darf den Auswahlvertrag nativ implementieren
und testen, ohne auf den physischen Display-/Touchnachweis zu warten.

Der Auswahlvertrag ist als Netzwerkmodus-Auswahl im bestehenden
`UserConfiguration`-Dokument persistent. Vor der ersten Benutzerentscheidung
und bei der Migration ist der interne Zustand `UNSELECTED`; er ist kein dritter
Benutzermodus. `UserConfiguration` enthaelt weder eine `HOME_WIFI`-SSID noch
ein wiederverwendbares WLAN-Passwort.

Factory-, V1- und V2-Records migrieren beim Lesen nach `UNSELECTED`. Sie leiten
weder `HOME_WIFI` noch `AP_ONLY` implizit aus Credentials ab. Genau ein
typisierter `ConnectivityCredential`-Record (Schema 1 oder 2) liegt im bestehenden
`IStateStore` unter `StateStoreKey=cc0` und `RecordTypeId=9`; HOME_WIFI-SSID
und -Passwort sowie das SoftAP-Passwort liegen gemeinsam darin und sind an die
`StorageEpoch` gebunden. Die SoftAP-SSID wird aus dem aktuellen Device-Name
abgeleitet und nicht persistiert. Es gibt weder einen zweiten physischen Store
noch eine zweite Credential-Wahrheit.

Fehlende oder ungueltige Credentials starten im expliziten `HOME_WIFI`-Modus
den Setup-Flow und leiten niemals automatisch nach `AP_ONLY` um.

### AP-only-Modus

Der AP-only-Modus ist ein voll unterstuetzter R1-Betriebsmodus:

- persistenter, geschuetzter SoftAP;
- lokaler HTTP-Zugang des gemeinsamen nativen HTTP-Unterbaus ohne separaten
  WLAN-Einrichtungsassistenten; die vollstaendige normale R1-Weboberflaeche
  bleibt Eigentum von Issue #27;
- mDNS beziehungsweise `*.local` als bevorzugter Komfortzugang, soweit der
  Client dies unterstuetzt;
- direkte lokale IP als verbindlicher Fallback;
- kein Captive Portal erforderlich;
- keine App und kein CLI erforderlich.

```text
AP_ONLY_SOFTAP=PERSISTENT
AP_ONLY_NORMAL_WEB_UI=YES
ISSUE164_SHARED_HTTP_FOUNDATION=YES
ISSUE164_NORMAL_R1_WEB_UI=NOT_IMPLEMENTED_BY_ISSUE164
ISSUE27_NORMAL_WEB_UI_OWNERSHIP=PRESERVED
AP_ONLY_CAPTIVE_PORTAL_REQUIRED=NO
MDNS_ACCESS=PREFERRED_NOT_REQUIRED
DIRECT_IP_FALLBACK=REQUIRED
SPECIAL_APP_OR_CLI_REQUIRED=NO
```

Ein WLAN-QR fuer den Beitritt zum geschuetzten SoftAP ist nach der
Ownerentscheidung `VARIANT_B_QR_RETAINED` ein R1-Bestandteil. Die SSID wird
aus dem aktuellen `UserConfiguration.deviceName` abgeleitet und auf hoechstens
24 rohe UTF-8-Bytes beziehungsweise 28 QR-escaped Bytes am Ende begrenzt; das
pro `StorageEpoch` persistierte individuelle Passwort und die direkte lokale
IP werden zusaetzlich auf dem lokalen Display angezeigt
und bleiben der manuelle Fallback. Ein zusaetzlicher QR-Code nur zum Oeffnen
der Webseite ist nicht R1-pflichtig.

### Heim-WLAN-Modus

`AP_ONLY` und `HOME_WIFI` sind getrennte, explizit persistierbare
Benutzerentscheidungen. Der aktive Modus und die Heim-WLAN-Credentials sind
fachlich getrennte Werte. Die Credentials leben ausschliesslich im einen
`ConnectivityCredential`-Record (Schema 1 oder 2) des bestehenden `IStateStore`; das
Vorhandensein oder Fehlen von Credentials darf den Modus nicht implizit
erraten oder aendern.

Die Zustandslogik lautet:

```text
if mode == AP_ONLY:
    persistent protected AP_ONLY SoftAP starten
    nicht wegen fehlender HOME_WIFI-Credentials in HOME_WIFI-Setup wechseln

if mode == HOME_WIFI and valid_home_wifi_credentials_exist:
    mit den gespeicherten HOME_WIFI-Credentials verbinden

if mode == HOME_WIFI and no_valid_home_wifi_credentials_exist:
    temporaeren geschuetzten HOME_WIFI-Setup-SoftAP starten

if explicit_home_wifi_reconfiguration_requested:
    HOME_WIFI-Setup-Flow starten
```

Bei einem Wechsel `AP_ONLY -> HOME_WIFI` ohne gueltige Credentials wird der
Setup-Flow gestartet. Bei `HOME_WIFI -> AP_ONLY` werden vorhandene Heim-WLAN-
Credentials nicht automatisch geloescht; eine solche Loeschung benoetigt eine
spaetere ausdrueckliche Entscheidung im Persistenzvertrag. Es gibt keine
zusaetzliche Multi-WLAN- oder implizite Modus-State-Machine.

Nur bei `HOME_WIFI` ohne gueltige Credentials oder bei einer ausdruecklichen
Heim-WLAN-Neukonfiguration stellt das Geraet einen temporaeren geschuetzten
Setup-SoftAP bereit. Die Einrichtung erfolgt browserbasiert ohne App- oder
CLI-Zwang:

```text
Display waehlt HOME_WIFI
  -> temporaeren geschuetzten Setup-SoftAP starten
  -> abgeleitete SSID, individuelles Passwort, WLAN-QR und direkte Setup-IP lokal anzeigen
  -> Client verbindet sich per WLAN-QR oder manuell mit SSID und Passwort
  -> Benutzer oeffnet die normale Setup-Seite per Browser, mDNS oder direkter IP
  -> Heim-WLAN scannen und auswaehlen oder SSID manuell eingeben
  -> Passwort eingeben
  -> neue Credentials nur volatil als Kandidat verwenden
  -> Heim-WLAN testen, waehrend der Setup-Pfad erhalten bleibt
  -> nach erfolgreichem Test ueber den Projektvertrag committen
  -> normaler Betrieb im Heim-LAN ueber DHCP/mDNS und direkte IP als Fallback
```

Captive Portal ist fuer diesen Ablauf nicht erforderlich. Bei einem
fehlgeschlagenen Test bleibt die bisherige gueltige Konfiguration unveraendert.
Ein automatischer produktiver Commit in eine zweite ESP-WiFi-/Component-NVS-
Wahrheit ist unzulaessig.

### Bewusst aus R1 deferierte lokale Eingabepfade

Die folgenden Funktionen sind durch die Ownerentscheidung `VARIANT_B` bewusst
aus R1/#164 deferiert und werden in dieser R1-Integration weder spezifiziert
noch als Abnahmekriterium vorausgesetzt:

```text
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED
R1_TOUCH_WIFI_KEYBOARD=DEFERRED
PRIMARY_R1_HOME_WIFI_CREDENTIAL_INPUT=BROWSER_SETUP
```

Ein späterer Touch-Credentialpfad benötigt eine neue Ownerentscheidung, einen
eigenen Plan und aktualisierte Acceptance Criteria. Der browserbasierte
Setup-Assistent bleibt der einzige R1-Eingabepfad für HOME_WIFI-SSID und
-Passwort.

### WLAN-QR zum SoftAP-Beitritt

Der WLAN-QR ist R1-Pflicht und dient ausschließlich dem Beitritt in den
geschützten Setup-/AP-only-SoftAP. Er wird aus derselben lokalen
`networkAccessPointInfo()`-Quelle wie die sichtbare SSID-/Passwort-/IP-
Projektion erzeugt:

```text
QR_PURPOSE=JOIN_SOFTAP
QR_SOURCE=networkAccessPointInfo()
QR_SSID=CURRENT_DEVICE_NAME_DERIVED_SSID
QR_PASSWORD_LENGTH=16
QR_PAYLOAD=CURRENT_DERIVED_SSID_AND_CURRENT_PERSISTED_PASSWORD
QR_FORMAT=WIFI_T_WPA_S_CURRENT_DERIVED_SSID_P_16_CHARS
QR_CONTAINS_WEB_URL=NO
QR_CONTAINS_AP_IP=NO
QR_RECT={156,34,164,164}
QR_QUIET_ZONE=YES
QR_CONTRAST=BLACK_ON_WHITE
QR_INTERPOLATION=NO
QR_ANTIALIASING=NO
LVGL_VERSION=9.6.0~1
MANUAL_FALLBACK=SSID_PASSWORD_DIRECT_IP_VISIBLE
SECOND_CREDENTIAL_SOURCE=NO
SECRET_LOGGING=NO
```

Der Payload verwendet das übliche WLAN-QR-Format mit korrektem Escaping
relevanter Sonderzeichen. Der QR wird nur in der lokalen Displayprojektion
verwendet; es gibt keine Kopie in allgemeine UI-Snapshots, Web/API, Logs,
Diagnose, Export oder Persistenz. Ein separater Webseiten-QR bleibt Future
Scope.

## Inhalt des Heim-WLAN-Setup-Assistenten

Dieser browserbasierte Assistent gilt fuer den explizit am Display gewaehlten
Modus `HOME_WIFI`. Er fuehrt mindestens durch:

Er ist der verbindliche und einzige R1-Eingabepfad fuer HOME_WIFI-SSID und
-Passwort; die deferierte Touch-Tastatur ist kein alternativer R1-Weg.

1. Sprache auswaehlen
2. verfuegbare WLANs suchen und anzeigen
3. Heim-WLAN auswaehlen oder SSID manuell eingeben
4. WLAN-Passwort eingeben
5. Heim-WLAN mit den neuen Zugangsdaten testen, waehrend der Setup-Pfad aktiv
   bleibt
6. Geraetename festlegen oder vorgeschlagenen Namen uebernehmen
7. Zusammenfassung anzeigen und erst nach erfolgreichem Test speichern
   (Ende des Assistenten)

Der normale Webzugang (Passwort aktivieren oder bewusst deaktivieren) ist
**nicht** mehr Teil dieses Assistenten, sondern ein eigener Schritt **nach
erfolgreichem Netzwerk-Setup** (siehe unten). Grund: Mit dem erfolgreichen Test
speichert der Assistent die Konfiguration und der Setup-Ablauf endet; die
Einrichtung des Webzugangs ist an eine lokale Freigabe am Geraet gebunden.

Ein Verbindungsfehler darf die bisherige funktionierende Konfiguration nicht
unbemerkt zerstoeren. Die neue Heim-WLAN-Konfiguration bleibt bis zum
erfolgreichen Test volatil; bei Fehlschlag bleibt die bisherige gueltige
Konfiguration unveraendert. Bei der Ersteinrichtung bleibt das Setup-WLAN
aktiv, bis eine gueltige Konfiguration gespeichert oder der Assistent bewusst
abgebrochen wurde. Ein Captive Portal ist fuer diesen Browserablauf nicht
erforderlich.

## SoftAP-Zugangsdaten fuer Setup- und AP-only-SoftAP

Das temporaere Setup-WLAN und der persistente AP-only-SoftAP sind immer
geschuetzt.

Verbindliche Regeln:

- die SoftAP-SSID wird ausschliesslich aus dem aktuellen
  `UserConfiguration.deviceName` abgeleitet; sie wird an vollstaendigen UTF-8-
  Codepoint-Grenzen auf hoechstens 24 rohe und hoechstens 28 QR-escaped Bytes
  am Ende begrenzt, ohne Suffix, Hash, MAC, Zeit oder Zufallsanteil
- das Passwort hat exakt 16 ASCII-Zeichen aus
  `ACDEFHJKMNPQRTUVWXYacdefhjkmnpqrtuvwxy3479`
- die 42 Zeichen werden mit der bestehenden sicheren Zufallsquelle und
  Rejection Sampling gleichverteilt ausgewaehlt (ca. 86,27 Bit fuer 16 Zeichen)
- das Passwort wird fuer die aktuelle `StorageEpoch` einmal sicher erzeugt,
  in `cc0`/RecordTypeId 9 als Schema-2-Credential bestaetigt persistiert und
  bei normalen Boots, Moduswechseln und Device-Name-Aenderungen unveraendert
  wiederverwendet
- bei einem Fehler der Zufallsquelle gibt es keinen deterministischen Fallback
- Anzeige lokal am Display und als QR-Code; WPA2-PSK bleibt aktiv
- Passwort niemals im Quellcode, in Logs, Diagnose oder Web/API ausgeben; die
  einzige Persistenz ist der bestehende `cc0`-Record mit Schema 1/2

Schema 1 bleibt vollstaendig lesbar und wird beim naechsten Network-Start mit
erhaltenen HOME_WIFI-Credentials in Schema 2 migriert. Schema 2 besitzt immer
ein gueltiges 16-stelliges SoftAP-Passwort ohne Optionaltag im Wireformat; der
aktuelle Writer schreibt ausschliesslich Schema 2. V1-Payload/Envelope sind
auf 100/145 Bytes, V2 auf 118/163 Bytes begrenzt.

Setup-WLAN und AP-only-SoftAP verwenden die aktuelle fluechtige SoftAP-
Konfiguration. Ein automatisch gestartetes Ersatz-WLAN nach Verlust des
Heim-WLANs ist kein R1-Verhalten; siehe [`FUTURE_SCOPE.md`](FUTURE_SCOPE.md).

### Begrenzter WLAN-Scan

Der synchrone WLAN-Scanpfad ist auf hoechstens 16 Access Points begrenzt.
`driver_count` wird vor der `wifi_ap_record_t`-Allokation auf
`min(driver_count, 16)` begrenzt; die bestehende Heap-Vector-Arbeitsmenge und
die `NetworkScanEntry`-Liste werden ebenfalls auf 16 begrenzt. Es wird kein
grosser lokaler Stackpuffer eingefuehrt.

Die HTTP-Scanantwort enthaelt hoechstens 16 Eintraege und hoechstens
`16 x (32 + 1) = 528` Byte SSID-Zeilen. Ein fehlgeschlagener Scan liefert
weiterhin `503`; ein erfolgreicher leerer Scan liefert weiterhin `200` mit
leerem Antwortkoerper. Der beobachtete Browser-Scan-Ausfall ist als
`ROOT_CAUSE_SCAN_FAILURE=UNPROVEN` dokumentiert und wird nicht nachtraeglich
als bewiesener OOM-Fehler bezeichnet.

## Verhalten ohne erreichbares Heim-WLAN

### Verbindungsversuch nach Start

Im Modus `HOME_WIFI` versucht das Geraet nach einem normalen Start zuerst, das
gespeicherte Heim-WLAN zu erreichen, sofern gueltige Credentials vorhanden
sind. Im Modus `AP_ONLY` startet es den persistenten geschuetzten SoftAP. Der
Fermentationsprozess wartet in keinem Modus auf das Netzwerk.

Der Router kann nach einem Stromausfall mehrere Minuten spaeter bereit sein als
der ESP32. Deshalb gilt ein voruebergehend fehlendes WLAN nicht als Fehler des
Fermentationsprozesses.

### Kein automatisches Fallback-AP in R1

Nach einem normalen Start versucht das Geraet, das gespeicherte Heim-WLAN
wieder zu erreichen. Ein voruebergehender Verlust des Heim-WLANs gilt nicht als
Fehler des Fermentationsprozesses. R1 startet dabei keinen zusaetzlichen
Fallback-AP automatisch. Der AP-only-Modus muss stattdessen ausdruecklich am
lokalen Display ausgewaehlt werden und ist dann ein eigener persistenter
Betriebsmodus.

Automatisches Fallback-AP und die zugehoerige Recovery-Qualifikation sind
spaeterer Future Scope und kein aktuelles R1-Gate; siehe
[`FUTURE_SCOPE.md`](FUTURE_SCOPE.md).

## Gespeicherte Heim-WLANs

Im ersten Release wird genau ein Heim-WLAN als aktive Konfiguration
unterstuetzt.

Eine zweite gespeicherte WLAN-Konfiguration und eine Prioritaetsauswahl sind
kein Bestandteil von R1. Mehrere gespeicherte WLANs bleiben spaeterer Future
Scope und werden nicht vorsorglich als zweiter Credential- oder
Persistenzpfad vorbereitet; siehe [`FUTURE_SCOPE.md`](FUTURE_SCOPE.md).

## Geraetename, Hostname und lokale Adresse

### Benutzerdefinierter Geraetename

Der Benutzer kann in den normalen Einstellungen einen sichtbaren Geraetenamen
festlegen, beispielsweise:

```text
Fermentationsschrank
```

Der Name wird verwendet fuer:

- lokale Oberflaechen und Systeminformationen
- Identifikation im Netzwerk
- Ableitung eines gueltigen Hostnamens
- Bezeichnung des Setup- oder AP-only-WLANs, soweit technisch sinnvoll

### Technischer Hostname

Aus dem sichtbaren Geraetenamen wird ein technisch gueltiger, stabiler Hostname
erzeugt. Ungueltige Zeichen werden eindeutig ersetzt oder entfernt. Bei
Namenskonflikten kann eine kurze geraetespezifische Endung verwendet werden.

Der lokale Zugriff soll standardmaessig ueber mDNS moeglich sein, zum Beispiel:

```text
http://fermentationsschrank.local
```

Die Oberflaeche zeigt immer auch die aktuell zugewiesene IP-Adresse an, falls
mDNS auf einem Client nicht funktioniert.

### Adressierung

Im `HOME_WIFI`-Modus gilt fuer das Heim-LAN:

- DHCP fuer IPv4-Adressierung
- mDNS-Hostname bevorzugt zusaetzlich zur IP-Adresse
- direkte lokale IP als verbindlicher Fallback

Im `AP_ONLY`-Modus gilt die lokale AP-IP als verbindlicher direkter Zugang;
mDNS beziehungsweise `*.local` bleibt ein bevorzugter Komfortzugang, soweit
der Client ihn unterstuetzt.

Optional im PIN-geschuetzten Servicebereich:

- feste IPv4-Adresse
- Netzmaske
- Gateway
- DNS-Server

Ungueltige statische Netzwerkkonfigurationen duerfen nicht ohne
Plausibilitaetspruefung gespeichert werden. Ein lokaler Wiederherstellungsweg
ueber Display oder den ausdruecklich gewaehlten AP-only-/Setup-Pfad muss
erhalten bleiben.

## Anmeldung an der lokalen Weboberflaeche

### Normaler Webzugang

Die Weboberflaeche kann mit einem gemeinsamen normalen Webpasswort geschuetzt
werden.

Ablauf der Ersteinrichtung nach erfolgreichem Netzwerk-Setup:

1. Der Netzwerkassistent ist abgeschlossen (`HOME_WIFI` bzw. `AP_ONLY` aktiv).
2. Am Geraet wird `Sprache` -> `Webzugang` -> `Web-Setup` gewaehlt; das
   fluechtige Freigabefenster von 10 Minuten oeffnet sich.
3. Der Browser ruft die Weboberflaeche auf (Direkt-IP oder mDNS). Im Zustand
   `unprovisioned` zeigt sie das Einrichtungsformular.
4. Der Benutzer richtet Passwortschutz (empfohlen) ein oder deaktiviert ihn
   bewusst nach Warnung und Bestaetigung und legt die getrennte Service-PIN fest.
5. Danach erfolgt die normale Anmeldung; die Einrichtung erzeugt keine Session.

Das Detail (Route, Statuscodes, Fenster) steht in
[WEB_UI.md](WEB_UI.md#ersteinrichtung-des-webzugangs).

Verbindliche Regeln:

- Webpasswort und vierstellige Service-PIN sind getrennte Zugangsdaten.
- Die Service-PIN wird nicht als normales Webpasswort verwendet.
- Passwortschutz ist die empfohlene und standardmaessig vorausgewaehlte
  Konfiguration im Einrichtungsassistenten.
- Der Benutzer kann den normalen Webpasswortschutz in den Einstellungen bewusst
  deaktivieren.
- Beim Deaktivieren wird deutlich gewarnt, dass jedes Geraet im erreichbaren
  lokalen Netz die normale Weboberflaeche bedienen kann.
- Das Deaktivieren des Webpassworts deaktiviert niemals den PIN-Schutz fuer
  Servicefunktionen.
- Passwoerter werden nicht im Klartext angezeigt, protokolliert oder exportiert.

Die genaue Passwortspeicherung, Sitzungsdauer, Abmeldung und Behandlung
fehlgeschlagener Anmeldungen werden in `SETTINGS_AND_STORAGE.md` und
`WEB_UI.md` festgelegt.

### HTTP im lokalen Netz

Der ESP32 stellt die Weboberflaeche im ersten Release direkt ueber HTTP im
lokalen Netz bereit.

Sicherheitsgrenzen:

- nur fuer ein vertrauenswuerdiges lokales Netz vorgesehen
- keine direkte Portfreigabe aus dem Internet
- keine Cloud-Abhaengigkeit
- ein Webpasswort ist ueber direktes HTTP nicht gegen Mitlesen im lokalen Netz
  kryptografisch geschuetzt
- fuer spaeteren Fernzugriff oder erhoehte Vertraulichkeit wird VPN oder ein
  Reverse Proxy mit TLS verwendet

Eine spaetere Einbindung ueber Caddy, einen anderen Reverse Proxy oder WireGuard
ist zulaessig. Die direkte lokale Erreichbarkeit des ESP32 bleibt dennoch
bestehen, damit die Bedienung nicht vom Heimserver abhaengt.

## Gleichzeitige Bedienung ueber Display und Web

Display und Weboberflaeche sind fachlich gleichberechtigte Bedienquellen.

Verbindliche Regeln:

- Beide Oberflaechen sehen denselben aktuellen Geraetezustand.
- Befehle werden atomar verarbeitet.
- Jede veraendernde Aktion wird mit Quelle `display` oder `web` protokolliert.
- Ein noch nicht gespeicherter Bearbeitungsdialog sperrt die andere Oberflaeche
  nicht pauschal.
- Erst `Speichern`, `Starten`, `Stoppen`, `Quittieren` oder eine vergleichbare
  bewusste Aktion veraendert den fachlichen Zustand.
- Veraltete Bearbeitungsdaten duerfen neuere Aenderungen nicht still
  ueberschreiben.
- Bei einem Konflikt wird die Aktion abgelehnt oder eine erneute Bestaetigung mit
  dem aktuellen Stand verlangt.
- Sicherheits- und Fehlerlogik hat Vorrang vor beiden Bedienquellen.

Die konkrete Versions- oder Revisionspruefung fuer konkurrierende Aenderungen
wird in `WEB_UI.md` spezifiziert.

## Aktuelle R1-Entscheidungen fuer die Integration

- [x] lokale Displayauswahl zwischen `HOME_WIFI` und `AP_ONLY`
- [x] `AP_ONLY` als unterstuetzter Modus mit persistentem geschuetztem SoftAP,
      aus `UserConfiguration.deviceName` abgeleiteter SSID und persistiertem
      16-Zeichen-Passwort pro `StorageEpoch`
- [x] lokaler HTTP-Zugang im `AP_ONLY`-Modus ueber den gemeinsamen Unterbau;
      die vollstaendige normale R1-Weboberflaeche bleibt Eigentum von Issue #27
- [x] `HOME_WIFI` als unterstuetzter und empfohlener Modus
- [x] genau ein gespeichertes Heim-WLAN
- [x] temporaerer geschuetzter Setup-SoftAP fuer die browserbasierte
      Heim-WLAN-Einrichtung
- [x] nativer ESP-IDF-HTTP-Pfad als gewaehlter R1-Webtransport
- [x] Scan/Auswahl oder manuelle SSID-Eingabe sowie Passwort als fluechtiger
      Kandidat
- [x] Test vor Commit; fehlgeschlagener Test bewahrt die aktive Konfiguration
- [x] Projekt-Konfigurationsdomain als Eigentumer der WLAN-Zugangsdaten
- [x] kein zweiter Credential-/Persistenzspeicher
- [x] DHCP und mDNS im Heim-LAN sowie direkte IP als Fallback
- [x] direkte AP-IP und bevorzugtes mDNS im `AP_ONLY`-Modus
- [x] Captive Portal, App und CLI sind keine R1-Voraussetzung
- [x] automatisches Fallback-AP und mehrere gespeicherte WLANs sind nicht R1
- [x] physischer Display-/Kamera-QR-Test ist deferred und nicht
      auswahlblockierend
- [x] QR-Layout `{156,34,164,164}` mit Quiet-Zone und Schwarz/Weiss-
      Hochkontrast im bestehenden LVGL-Pfad
- [x] synchroner WLAN-Scan und HTTP-Scanantwort auf 16 Eintraege begrenzt;
      maximale SSID-Zeilenantwort 528 Byte
- [x] grundlegender Reconnect zum selben gespeicherten Heim-WLAN ist R1
- [x] normales Webpasswort und Service-PIN sind getrennt
- [x] direkter lokaler HTTP-Zugriff ohne Internetfreigabe
- [x] Display und Web bleiben fachlich gleichberechtigte Bedienquellen

## Noch offen fuer die R1-Integration und spaeter

- genaue Seiten und Funktionen der Weboberflaeche
- Sitzungsverwaltung und automatische Abmeldung
- konkrete Passwortregeln und Passwortaenderung
- Revisionsmodell fuer gleichzeitige Bearbeitung
- CSRF- und weitere Webschutzmassnahmen
- Standardgeraetename und Regel fuer eindeutige Namensendung
- konkrete Persistenz- und Recovery-Details innerhalb des bestehenden
  Projektvertrags
- spaetere Integration ueber Caddy, VPN oder anderen Reverse Proxy

Automatisches Fallback-AP, Captive Portal, mehrere gespeicherte WLANs,
Webseiten-QR und erweiterte Langzeit-/Lastqualifikation sind bewusst aus R1
herausgenommen und in [`FUTURE_SCOPE.md`](FUTURE_SCOPE.md) referenziert.
