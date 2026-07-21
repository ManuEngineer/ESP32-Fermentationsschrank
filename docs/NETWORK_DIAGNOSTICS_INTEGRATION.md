# Netzwerkdiagnose und lokale Integration

## Status

Dieses Dokument ergaenzt [`NETWORK.md`](NETWORK.md) und [`WEB_UI.md`](WEB_UI.md)
um die in Phase 5C akzeptierten Regeln fuer Netzwerkdiagnose, Diagnoseexport,
VPN- und Reverse-Proxy-Zugriff, Proxy-Vertrauen und eine lokale Lese-API.

Der Fermentationsprozess bleibt von allen hier beschriebenen Netzwerkfunktionen
unabhaengig.

## Netzwerkdiagnose

Die Weboberflaeche und die lokale Diagnoseansicht zeigen mindestens:

- WLAN-Verbindungsstatus
- SSID des verbundenen Heim-WLANs
- Signalstaerke in dBm
- aktuelle IPv4-Adresse
- Geraete- beziehungsweise mDNS-Hostname
- Gateway
- DNS-Erreichbarkeit
- Status der Netzwerkzeit beziehungsweise NTP-Synchronisation
- Zeitpunkt oder Alter des letzten WLAN-Abbruchs
- Status des Einrichtungs- oder Ersatz-WLANs
- Zeitpunkt des letzten erfolgreichen Verbindungsaufbaus
- aktuelle Netzwerkbetriebsart: Heim-WLAN, Einrichtungs-WLAN oder Ersatz-WLAN

Beispiel:

```text
WLAN:            verbunden
SSID:            MeinHeimnetz
Signal:          -61 dBm
IP-Adresse:      192.168.1.84
Hostname:        fermentationsschrank.local
Gateway:         192.168.1.1
DNS:             erreichbar
NTP:             synchronisiert
Letzter Abbruch: vor 2 Tagen
Ersatz-WLAN:     aus
```

Vorgesehene Aktionen:

```text
[Verbindung neu pruefen]
[WLAN neu verbinden]
[Diagnose exportieren]
```

Frei eingebbare Ping-, Port- oder DNS-Werkzeuge sind kein Bestandteil des ersten
Releases.

## Manuelles Neuverbinden des WLANs

`WLAN neu verbinden` ist eine normale Diagnosefunktion und darf auch waehrend
eines laufenden Prozesses verwendet werden.

Verbindliche Regeln:

- Nur die Netzwerkverbindung wird neu aufgebaut.
- Der aktive Fermentations-, Kuehl- oder Halteprozess laeuft unveraendert weiter.
- Peltier-, Luefter-, Sensor- und Sicherheitslogik werden nicht neu gestartet.
- Ein laufender Programmschnappschuss wird nicht veraendert.
- Webverbindungen duerfen dabei voruebergehend abbrechen.
- Nach erfolgreicher Wiederverbindung wird der aktuelle vollstaendige
  Geraetezustand neu geladen.
- Erfolg oder Fehler der Neuverbindung wird sichtbar angezeigt und protokolliert.
- Ein fehlgeschlagener WLAN-Neustart ist allein kein Fehler des
  Fermentationsprozesses.

## Diagnoseexport

Die Diagnose kann als herunterladbare UTF-8-Datei ausgegeben werden. Mindestens
ein strukturiertes JSON-Format wird vorgesehen; eine zusaetzliche lesbare
Textdarstellung ist zulaessig.

Der Export enthaelt mindestens:

- Exportzeitpunkt und Geraetename
- Firmware- und Schema-Version
- Resetursache, soweit technisch bestimmbar
- Betriebszustand und aktuelle Prozessphase
- Netzwerkbetriebsart und Verbindungsstatus
- SSID, Signalstaerke und IP-Konfiguration
- Hostname, Gateway und DNS-Status
- NTP- und Zeitstatus
- Sensorstatus und aktuelle Messwerte
- aktuelle Aktoranforderungen, ohne direkte Steuerbefehle
- aktive Warnungen und Fehlercodes
- eine begrenzte Anzahl letzter relevanter Ereignisse

Der Export enthaelt niemals:

- WLAN-Passwort
- Passwort des Einrichtungs- oder Ersatz-WLANs
- normales Webpasswort
- Service-PIN
- Sitzungskennungen oder Anmeldetokens
- CSRF-Tokens
- private Schluessel oder andere geheime Integrationsdaten

Geheime Werte werden nicht bloss maskiert, sondern gar nicht erst in den Export
aufgenommen.

Der Export ist eine lesende Funktion. Er darf den laufenden Prozess nicht
blockieren und keine Aktoren beeinflussen.

## Zugriff ueber das bestehende WireGuard-VPN

Der ESP32 erhaelt im ersten Release keinen eigenen WireGuard-Client.

Der vorgesehene Zugriff lautet:

```text
Mobiltelefon oder Computer
  -> bestehendes WireGuard-VPN ins Heimnetz
  -> lokale IP-Adresse oder lokaler Name des ESP32
  -> direkte lokale Weboberflaeche
```

Vorteile dieses Aufbaus:

- keine zusaetzliche VPN-Schluesselverwaltung auf dem ESP32
- keine Abhaengigkeit der Regelung vom VPN
- keine direkte Freigabe des ESP32 ins Internet
- vorhandene Heimnetz- und VPN-Infrastruktur kann verwendet werden

Ob mDNS-Namen ueber das VPN auf dem jeweiligen Client aufloesbar sind, haengt
von der Heimnetz- und VPN-Konfiguration ab. Deshalb bleibt die lokale IP-Adresse
immer ein zulaessiger Zugriffsweg.

## Spaetere Einbindung ueber Caddy

Eine spaetere Reverse-Proxy-Einbindung wird als unterstuetzte Option vorgesehen:

```text
Browser
  -> HTTPS
  -> Caddy auf dem Heimserver
  -> HTTP zum ESP32 im lokalen Netz
```

Dabei gilt:

- Caddy ist kein zwingender Bestandteil des ersten Releases.
- Der ESP32 bleibt direkt im lokalen Netz erreichbar.
- Ein Ausfall des Heimservers oder Reverse Proxys beeinflusst den laufenden
  Prozess nicht.
- Eine direkte Portfreigabe vom Internet auf den ESP32 bleibt unzulaessig.
- TLS, externe Anmeldung und weitere Zugriffskontrollen koennen am Reverse Proxy
  umgesetzt werden.
- Die ESP32-Weboberflaeche muss unter einem Proxy-Pfad oder einer eigenen
  internen Domain betreibbar vorbereitet werden, ohne den direkten lokalen
  Zugriff zu verlieren.

Die konkrete Caddy-Konfiguration ist nicht Bestandteil der ESP32-Firmware und
wird nicht mit festen privaten Domainnamen oder IP-Adressen im Repository
verdrahtet.

## Vertrauen in Proxy-Header

Header wie die folgenden werden standardmaessig nicht als vertrauenswuerdige
Identitaets- oder Herkunftsinformation verwendet:

- `X-Forwarded-For`
- `X-Forwarded-Proto`
- `X-Forwarded-Host`
- `Forwarded`

Sie koennen von einem normalen Client im lokalen Netz selbst gesetzt werden.

Proxy-Header werden nur ausgewertet, wenn:

1. der Proxy-Betrieb ausdruecklich in den Serviceeinstellungen aktiviert wurde,
2. mindestens eine konkrete vertrauenswuerdige Proxy-IP oder ein eng begrenztes
   Proxy-Netz konfiguriert wurde,
3. die eingehende Verbindung tatsaechlich von diesem vertrauenswuerdigen Proxy
   stammt.

Bei direktem lokalem Zugriff oder Verbindungen von einer anderen Quelle werden
vorhandene Proxy-Header ignoriert.

Proxy-Header duerfen niemals die Service-PIN, Sicherheitspruefungen oder
Berechtigungspruefungen umgehen.

## Webpasswort hinter VPN oder Reverse Proxy

Der normale Webpasswortschutz bleibt unabhaengig vom Zugriffsweg zunaechst
aktiv.

Der Benutzer darf ihn bewusst deaktivieren, beispielsweise wenn der Zugriff
bereits durch WireGuard oder eine vorgeschaltete Caddy-Anmeldung ausreichend
begrenzt wird.

Beim Deaktivieren gelten dieselben Regeln wie beim direkten lokalen Zugriff:

- deutliche Warnung vor der Aenderung
- bewusste Bestaetigung
- Anzeige des ungeschuetzten normalen Webzugangs in den Einstellungen
- keine automatische Deaktivierung bloss weil Proxy-Header oder ein VPN erkannt
  werden
- Servicefunktionen bleiben immer durch die vierstellige Service-PIN geschuetzt
- kritische Serviceaktionen behalten ihre zusaetzlichen Bestaetigungen

Die Firmware vertraut nicht darauf, dass eine externe Anmeldung dauerhaft
vorhanden oder korrekt konfiguriert ist. Die Entscheidung zur Deaktivierung des
normalen Webpassworts bleibt eine explizite lokale Einstellung.

## Dokumentierte lokale Lese-API

Das erste Release stellt eine dokumentierte lokale, ausschliesslich lesende API
bereit. Sie ist fuer spaetere Heimserver-Integration, eigene Skripte oder eine
spaetere Home-Automation vorgesehen.

### Umfang

Mindestens lesbar sind:

- allgemeiner Geraete- und Betriebsstatus
- aktuelle Prozessphase
- Programmname und Laufkennung des aktiven Laufes
- Produkt-, Schrankluft- und Solltemperatur
- Gueltigkeit und Status der Temperatursensoren
- verbleibende und verstrichene Laufzeit, soweit verlaesslich
- aktiver Regelmodus
- aktuelle Warnungen und Fehler
- Netzwerk- und Zeitstatus in nicht geheimem Umfang

### Grundstruktur

Die oeffentliche lokale API verwendet eine versionierte Basis, beispielsweise:

```text
/api/v1/
```

Vorgesehene fachliche Ressourcen sind mindestens:

```text
GET /api/v1/status
GET /api/v1/temperatures
GET /api/v1/alerts
```

Die genaue Aufteilung darf waehrend des technischen Entwurfs angepasst werden,
solange der dokumentierte Funktionsumfang und die Versionierung erhalten
bleiben.

### Verbindliche Grenzen

Im ersten Release gibt es ueber die offiziell unterstuetzte externe API keine
Funktionen fuer:

- Programm starten oder stoppen
- Sollwerte oder Laufzeiten aendern
- Programme erstellen, bearbeiten oder loeschen
- Meldungen quittieren oder stummschalten
- WLAN- oder Systemeinstellungen aendern
- Servicebereich entsperren
- Aktoren testen oder direkt schalten
- Firmwareupdate oder Werksreset

Interne Schreibschnittstellen der eigenen Weboberflaeche gelten nicht automatisch
als dokumentierte oder stabile externe API.

### Datenregeln

- Antworten werden als UTF-8-JSON ausgegeben.
- Temperaturen enthalten Einheit, Wert und Gueltigkeitsstatus.
- Zeitwerte besitzen eine eindeutig dokumentierte Einheit.
- Zeitstempel verwenden ein eindeutiges Format und kennzeichnen, wenn die
  Netzwerkzeit noch nicht verlaesslich ist.
- Fehlende Messwerte werden als fehlend beziehungsweise ungueltig dargestellt und
  nicht durch erfundene Zahlen ersetzt.
- Warnungen und Fehler enthalten stabile maschinenlesbare Codes sowie
  uebersetzbare sichtbare Texte getrennt voneinander.
- API-Antworten enthalten keine Passwoerter, PINs, Tokens oder privaten
  Netzwerkinformationen, die nicht auch in der normalen Diagnose sichtbar sein
  duerfen.

### Zugriffsschutz

Die Lese-API folgt im ersten Release dem Schutz des normalen Webzugangs:

- Bei aktiviertem Webpasswort ist auch die Lese-API authentifiziert.
- Bei bewusst deaktiviertem Webpasswort ist die Lese-API im erreichbaren lokalen
  Netz ebenfalls ohne normale Anmeldung lesbar.
- Die Oberflaeche warnt bei dieser Konfiguration entsprechend.
- Ein Reverse Proxy oder VPN darf zusaetzlichen Schutz bereitstellen.
- Die Service-PIN ist kein API-Schluessel und wird fuer die Lese-API nicht
  verwendet.

Eine spaetere unabhaengige Token- oder Integrationsauthentifizierung bleibt
moeglich, ist aber nicht Bestandteil des ersten Releases.

## Akzeptierte Entscheidungen aus Phase 5C

- [x] ausfuehrliche Netzwerkdiagnose mit WLAN, IP, Hostname, DNS und NTP
- [x] manuelles Neuverbinden des WLANs auch waehrend eines laufenden Prozesses
- [x] Diagnoseexport als herunterladbare Text- oder JSON-Datei
- [x] keine Geheimnisse im Diagnoseexport
- [x] Fernzugriff ueber das bestehende WireGuard-VPN ins Heimnetz
- [x] kein eigener WireGuard-Client auf dem ESP32
- [x] Caddy als spaetere optionale Reverse-Proxy-Loesung
- [x] direkter lokaler Zugriff bleibt unabhaengig vom Heimserver erhalten
- [x] Proxy-Header standardmaessig ignorieren
- [x] Proxy-Header nur von ausdruecklich konfigurierten Proxy-Adressen akzeptieren
- [x] Webpasswort hinter VPN oder Caddy bewusst deaktivierbar
- [x] Service-PIN bleibt unabhaengig vom Zugriffsweg immer erforderlich
- [x] dokumentierte lokale Lese-API fuer Status, Temperaturen und Meldungen
- [x] keine offiziell unterstuetzte externe Schreib-API im ersten Release

## Noch offen fuer spaetere Phasen

- konkretes JSON-Schema und Fehlerformat der Lese-API
- genaue Authentifizierungscookies beziehungsweise Header fuer API-Aufrufe
- Abfragegrenzen und maximale Anzahl paralleler API-Clients
- konkrete Diagnoseexport-Version und maximale Ereignisanzahl
- Umfang und Maskierung optionaler Netzwerkdetails im Export
- technische Proxy-Pfadunterstuetzung und Basis-URL
- konkrete Liste vertrauenswuerdiger Proxy-Netze
- spaetere Integrations-Tokens oder Home-Automation-Anbindung
