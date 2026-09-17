# Netzwerk und WLAN-Bereitstellung

## Status

Dieses Dokument beschreibt den aktuellen R1-Scope fuer Netzwerkmodi,
WLAN-Ersteinrichtung, Geraetename, Adressierung und die grundlegende
Absicherung der lokalen Weboberflaeche. R1 unterstuetzt genau ein gespeichertes
Heim-WLAN oder den ausdruecklichen AP-only-Modus.

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
```

### AP-only-Modus

Der AP-only-Modus ist ein voll unterstuetzter R1-Betriebsmodus:

- persistenter, geschuetzter SoftAP;
- direkt die normale lokale R1-Weboberflaeche, ohne separaten
  WLAN-Einrichtungsassistenten;
- mDNS beziehungsweise `*.local` als bevorzugter Komfortzugang, soweit der
  Client dies unterstuetzt;
- direkte lokale IP als verbindlicher Fallback;
- kein Captive Portal erforderlich;
- keine App und kein CLI erforderlich.

```text
AP_ONLY_SOFTAP=PERSISTENT
AP_ONLY_NORMAL_WEB_UI=YES
AP_ONLY_CAPTIVE_PORTAL_REQUIRED=NO
MDNS_ACCESS=PREFERRED_NOT_REQUIRED
DIRECT_IP_FALLBACK=REQUIRED
SPECIAL_APP_OR_CLI_REQUIRED=NO
```

Der WLAN-QR darf den Beitritt zum geschuetzten SoftAP vereinfachen. Ein
zusaetzlicher QR-Code nur zum Oeffnen der Webseite ist nicht R1-pflichtig.

### Heim-WLAN-Modus

Wenn der Benutzer `HOME_WIFI` waehlt oder noch keine Heim-WLAN-Konfiguration
vorhanden ist, stellt das Geraet einen temporaeren geschuetzten Setup-SoftAP
bereit. Die Einrichtung erfolgt browserbasiert ohne App- oder CLI-Zwang:

```text
Display waehlt HOME_WIFI
  -> temporaeren geschuetzten Setup-SoftAP starten
  -> SSID, Passwort, QR und direkte Setup-IP lokal anzeigen
  -> Client verbindet sich per WLAN-QR oder manuell
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

### Bedeutung des WLAN-QR-Codes

Der WLAN-QR-Code enthaelt ausschliesslich die individuellen Zugangsdaten des
geschuetzten Setup- oder AP-only-SoftAPs in einem gaengigen WLAN-QR-Format.
Zusaetzlich werden fuer den direkten Fallback lokal angezeigt:

- SSID des geschuetzten SoftAPs;
- individuelles Passwort;
- lokale Setup-Adresse beziehungsweise AP-IP;
- Moeglichkeit, den QR-Code erneut anzuzeigen.

Ein Webseiten-QR ist davon getrennt und bleibt optionaler Future Scope.

### Lokale Eingabe am Touchdisplay

SSID und WLAN-Passwort koennen zusaetzlich am Touchdisplay eingegeben werden.
Dieser Weg ist insbesondere als Not- und Offlineweg vorgesehen, nicht als
bevorzugte Eingabemethode fuer lange Passwoerter mit vielen Sonderzeichen.

Die Bildschirmtastatur muss deshalb auch bei WLAN-Zugangsdaten mindestens
unterstuetzen:

- Gross- und Kleinbuchstaben
- Ziffern
- Leerzeichen, soweit fuer SSIDs erforderlich
- gaengige Sonderzeichen
- verdeckte Passwortanzeige mit optionaler kurzzeitiger Sichtbarkeit
- Loeschen, Rueckschritt, Abbrechen und Uebernehmen

## Inhalt des Heim-WLAN-Setup-Assistenten

Dieser browserbasierte Assistent gilt fuer den explizit am Display gewaehlten
Modus `HOME_WIFI`. Er fuehrt mindestens durch:

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
- Anzeige lokal am Display und als QR-Code
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

Nach einem normalen Start versucht das Geraet zuerst, das gespeicherte Heim-WLAN
zu erreichen. Der Fermentationsprozess wartet dabei nicht auf das Netzwerk.

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
- [x] normale lokale R1-Weboberflaeche im `AP_ONLY`-Modus
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
