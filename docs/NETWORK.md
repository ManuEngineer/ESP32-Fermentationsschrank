# Netzwerk und WLAN-Bereitstellung

## Status

Dieses Dokument beschreibt den aktuellen R1-Scope fuer Netzwerkmodi,
WLAN-Ersteinrichtung, Geraetename, Adressierung und die grundlegende
Absicherung der lokalen Weboberflaeche. R1 unterstuetzt genau ein gespeichertes
Heim-WLAN oder den ausdruecklichen AP-only-Modus.

Die Ownerentscheidung fuer Issue #164 lautet `VARIANT_B`: Die lokale
HOME_WIFI-SSID-/Passworteingabe am Touchdisplay, die dafuer erforderliche
Bildschirmtastatur und der WLAN-QR mit individuellen SoftAP-Zugangsdaten sind
aus R1 deferiert. R1 verwendet dafuer den browserbasierten Setup-Pfad; die
Display-Moduswahl und die lokale Anzeige der individuellen SoftAP-Zugangsdaten
und der direkten IP bleiben R1.

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
typisierter `ConnectivityCredential`-V1-Record liegt im bestehenden
`IStateStore` unter `StateStoreKey=cc0` und `RecordTypeId=9`; SSID und Passwort
liegen gemeinsam darin und sind an die `StorageEpoch` gebunden. Es gibt weder
einen zweiten physischen Store noch eine zweite Credential-Wahrheit.

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
Ownerentscheidung `VARIANT_B` kein R1-Bestandteil. SSID, individuelles
Passwort und direkte lokale IP werden stattdessen auf dem lokalen Display
angezeigt; der Client verbindet sich manuell. Ein zusaetzlicher QR-Code nur
zum Oeffnen der Webseite ist ebenfalls nicht R1-pflichtig.

### Heim-WLAN-Modus

`AP_ONLY` und `HOME_WIFI` sind getrennte, explizit persistierbare
Benutzerentscheidungen. Der aktive Modus und die Heim-WLAN-Credentials sind
fachlich getrennte Werte. Die Credentials leben ausschliesslich im einen
`ConnectivityCredential`-V1-Record des bestehenden `IStateStore`; das
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
  -> SSID, individuelles Passwort und direkte Setup-IP lokal anzeigen
  -> Client verbindet sich manuell mit SSID und Passwort
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

### Bewusst aus R1 deferierte lokale Komfort- und Eingabepfade

Die folgenden Funktionen sind durch die Ownerentscheidung `VARIANT_B` bewusst
aus R1/#164 deferiert und werden in dieser R1-Integration weder spezifiziert
noch als Abnahmekriterium vorausgesetzt:

```text
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED_VARIANT_B
R1_TOUCH_WIFI_KEYBOARD=DEFERRED_VARIANT_B
R1_WLAN_QR=DEFERRED_VARIANT_B
PRIMARY_R1_HOME_WIFI_CREDENTIAL_INPUT=BROWSER_SETUP
```

Ein späterer Touch-Credentialpfad oder WLAN-QR benötigt eine neue
Ownerentscheidung, einen eigenen Plan und aktualisierte Acceptance Criteria.
Der browserbasierte Setup-Assistent bleibt der einzige R1-Eingabepfad für
HOME_WIFI-SSID und -Passwort.

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
7. normalen Webzugang mit Passwort aktivieren oder bewusst deaktivieren
8. Zusammenfassung anzeigen und erst nach erfolgreichem Test speichern

Ein Verbindungsfehler darf die bisherige funktionierende Konfiguration nicht
unbemerkt zerstoeren. Die neue Heim-WLAN-Konfiguration bleibt bis zum
erfolgreichen Test volatil; bei Fehlschlag bleibt die bisherige gueltige
Konfiguration unveraendert. Bei der Ersteinrichtung bleibt das Setup-WLAN
aktiv, bis eine gueltige Konfiguration gespeichert oder der Assistent bewusst
abgebrochen wurde. Ein Captive Portal ist fuer diesen Browserablauf nicht
erforderlich.

## Individuelles Passwort fuer Setup- und AP-only-SoftAP

Das temporaere Setup-WLAN und der persistente AP-only-SoftAP sind immer
geschuetzt.

Verbindliche Regeln:

- kein allgemeines, fuer alle Geraete identisches Standardpasswort
- geraetespezifisches, ausreichend zufaelliges Initialpasswort
- Anzeige lokal am Display
- spaetere Aenderung in den Netzwerkeinstellungen moeglich
- Passwort niemals im Quellcode oder Repository hinterlegen
- Passwort nicht in normalen Ereignisprotokollen oder Diagnoseanzeigen
  wiederholen

Ob Setup-WLAN und AP-only-SoftAP dasselbe geraetespezifische Passwort verwenden
oder getrennte Passwoerter erhalten, wird in `SETTINGS_AND_STORAGE.md`
festgelegt. Ein automatisch gestartetes Ersatz-WLAN nach Verlust des
Heim-WLANs ist kein R1-Verhalten; siehe [`FUTURE_SCOPE.md`](FUTURE_SCOPE.md).

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
- [x] `AP_ONLY` als unterstuetzter Modus mit persistentem geschuetztem SoftAP
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
