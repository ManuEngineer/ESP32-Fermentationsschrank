# Netzwerk und WLAN-Bereitstellung

## Status

Dieses Dokument beschreibt die in Phase 5A akzeptierten Regeln fuer
WLAN-Ersteinrichtung, Einrichtungsassistent, Ersatz-WLAN, Geraetename,
Adressierung und die grundlegende Absicherung der lokalen Weboberflaeche.

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

## WLAN-Ersteinrichtung

### Primaerer Weg: Einrichtungs-WLAN mit Webassistent

Wenn noch keine gueltige WLAN-Konfiguration vorhanden ist, startet das Geraet
ein temporaeres, geschuetztes Einrichtungs-WLAN.

Der vorgesehene Ablauf lautet:

```text
Geraet startet ohne gueltige WLAN-Konfiguration
  -> individuelles Einrichtungs-WLAN starten
  -> SSID, Passwort und QR-Code am Display anzeigen
  -> Benutzer scannt den QR-Code mit dem Mobiltelefon
  -> Mobiltelefon verbindet sich mit dem Einrichtungs-WLAN
  -> Einrichtungsassistent oeffnet sich als Captive Portal
  -> Heim-WLAN auswaehlen
  -> langes WLAN-Passwort bequem am Mobiltelefon eingeben
  -> optional Geraetename und Webzugang konfigurieren
  -> Verbindung zum Heim-WLAN pruefen
  -> Konfiguration erst nach erfolgreicher Pruefung uebernehmen
```

### Bedeutung des QR-Codes

Der primaere QR-Code enthaelt die individuellen Zugangsdaten des temporaeren
Einrichtungs-WLANs in einem gaengigen WLAN-QR-Format. Ziel ist, dass das
Mobiltelefon dem Einrichtungs-WLAN ohne manuelle Eingabe des langen Passworts
beitreten kann.

Nach dem Beitritt soll der Einrichtungsassistent ueber die Captive-Portal-
Erkennung des Mobiltelefons automatisch angeboten werden.

Da nicht jedes Endgeraet ein Captive Portal identisch behandelt, werden
zusaetzlich sichtbar angezeigt:

- SSID des Einrichtungs-WLANs
- individuelles Passwort
- lokale Einrichtungsadresse beziehungsweise IP-Adresse
- Schaltflaeche zum erneuten Anzeigen des QR-Codes

Der normale Zielablauf benoetigt damit nur einen QR-Scan. Eine manuelle
Fallback-Moeglichkeit bleibt trotzdem vorhanden.

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

## Inhalt des Einrichtungsassistenten

Der Assistent fuehrt mindestens durch:

1. Sprache auswaehlen
2. verfuegbare WLANs suchen und anzeigen
3. Heim-WLAN auswaehlen oder SSID manuell eingeben
4. WLAN-Passwort eingeben
5. Verbindung pruefen
6. Geraetename festlegen oder vorgeschlagenen Namen uebernehmen
7. normalen Webzugang mit Passwort aktivieren oder bewusst deaktivieren
8. Zusammenfassung anzeigen und speichern

Ein Verbindungsfehler darf die bisherige funktionierende Konfiguration nicht
unbemerkt zerstoeren. Bei der Ersteinrichtung bleibt das Einrichtungs-WLAN aktiv,
bis eine gueltige Konfiguration gespeichert oder der Assistent bewusst
abgebrochen wurde.

## Individuelles Passwort fuer Einrichtungs- und Ersatz-WLAN

Einrichtungs- und Ersatz-WLAN sind immer geschuetzt.

Verbindliche Regeln:

- kein allgemeines, fuer alle Geraete identisches Standardpasswort
- geraetespezifisches, ausreichend zufaelliges Initialpasswort
- Anzeige lokal am Display und als QR-Code
- spaetere Aenderung in den Netzwerkeinstellungen moeglich
- Passwort niemals im Quellcode oder Repository hinterlegen
- Passwort nicht in normalen Ereignisprotokollen oder Diagnoseanzeigen
  wiederholen

Ob Einrichtungs-WLAN und spaeteres Ersatz-WLAN dasselbe geraetespezifische
Passwort verwenden oder getrennte Passwoerter erhalten, wird in
`SETTINGS_AND_STORAGE.md` festgelegt.

## Verhalten ohne erreichbares Heim-WLAN

### Verbindungsversuch nach Start

Nach einem normalen Start versucht das Geraet zuerst, das gespeicherte Heim-WLAN
zu erreichen. Der Fermentationsprozess wartet dabei nicht auf das Netzwerk.

Der Router kann nach einem Stromausfall mehrere Minuten spaeter bereit sein als
der ESP32. Deshalb gilt ein voruebergehend fehlendes WLAN nicht als Fehler des
Fermentationsprozesses.

### Geschuetztes Ersatz-WLAN

Bleibt das Heim-WLAN laenger als eine konfigurierbare Zeit unerreichbar, startet
das Geraet ein geschuetztes Ersatz-WLAN und versucht parallel weiterhin, das
Heim-WLAN wieder zu erreichen.

Beispielanzeige:

```text
Heimnetz nicht erreichbar

Ersatz-WLAN aktiv:
Fermentationsschrank-7A31

[QR-Code anzeigen]
[Netzwerkdetails]
```

Das Ersatz-WLAN erlaubt mindestens:

- Aufruf der lokalen Weboberflaeche
- Anzeige des laufenden Prozesses
- normale zulaessige Bedienhandlungen
- Diagnose des Netzwerkzustands
- erneute WLAN-Einrichtung

Servicefunktionen bleiben auch im Ersatz-WLAN durch die Service-PIN geschuetzt.

Sobald das Heim-WLAN wieder stabil verbunden ist, darf das Ersatz-WLAN nach
einer definierten Uebergangszeit automatisch beendet werden. Ein aktuell
offener Speichervorgang darf dadurch nicht unkontrolliert abgeschnitten werden.

Die Wartezeit bis zum Start und die Uebergangszeit bis zum Beenden des
Ersatz-WLANs bleiben `TBD_COMMISSIONING` beziehungsweise fuer Phase 6 offen.

## Gespeicherte Heim-WLANs

Im ersten Release wird genau ein Heim-WLAN als aktive Konfiguration
unterstuetzt.

Die interne Konfigurationsstruktur soll jedoch so aufgebaut sein, dass spaeter
mehrere bekannte WLANs mit Prioritaetsreihenfolge ergaenzt werden koennen, ohne
das gesamte Netzwerkkonzept neu zu entwerfen.

Das erste Release benoetigt keine Benutzeroberflaeche fuer mehrere gespeicherte
Netzwerke.

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
- Bezeichnung des Einrichtungs- oder Ersatz-WLANs, soweit technisch sinnvoll

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

Standard:

- DHCP fuer IPv4-Adressierung
- mDNS-Hostname zusaetzlich zur IP-Adresse

Optional im PIN-geschuetzten Servicebereich:

- feste IPv4-Adresse
- Netzmaske
- Gateway
- DNS-Server

Ungueltige statische Netzwerkkonfigurationen duerfen nicht ohne
Plausibilitaetspruefung gespeichert werden. Ein lokaler Wiederherstellungsweg
ueber Display oder Ersatz-WLAN muss erhalten bleiben.

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

## Akzeptierte Entscheidungen aus Phase 5A

- [x] Einrichtungs-WLAN mit Webassistent als primaerer Weg
- [x] lokale WLAN-Eingabe am Touchdisplay bleibt als Fallback verfuegbar
- [x] QR-Code verbindet das Mobiltelefon mit dem geschuetzten Einrichtungs-WLAN
- [x] Captive Portal fuehrt nach dem QR-Scan in den Assistenten
- [x] geschuetztes Ersatz-WLAN startet nach Wartezeit und Heim-WLAN-Versuche laufen weiter
- [x] individuelles, aenderbares Passwort statt allgemeinem Standardpasswort
- [x] DHCP und mDNS als Standard
- [x] Geraetename ist in den normalen Einstellungen definierbar
- [x] statische IPv4-Konfiguration optional im Servicebereich
- [x] erstes Release mit einem Heim-WLAN, Architektur fuer mehrere vorbereitet
- [x] normales Webpasswort und Service-PIN sind getrennt
- [x] normaler Webpasswortschutz kann bewusst deaktiviert werden
- [x] direkter lokaler HTTP-Zugriff ohne Internetfreigabe
- [x] spaeterer Zugriff ueber VPN oder Reverse Proxy bleibt moeglich
- [x] Display und Web sind gleichberechtigt und Aktionen werden atomar verarbeitet

## Noch offen fuer Phase 5B und spaeter

- genaue Seiten und Funktionen der Weboberflaeche
- Sitzungsverwaltung und automatische Abmeldung
- konkrete Passwortregeln und Passwortaenderung
- Revisionsmodell fuer gleichzeitige Bearbeitung
- CSRF- und weitere Webschutzmassnahmen
- Standardwartezeit bis zum Ersatz-WLAN
- Lebensdauer und Wechsel des Ersatz-WLAN-Passworts
- genauer Captive-Portal-Ablauf auf unterschiedlichen Clients
- Standardgeraetename und Regel fuer eindeutige Namensendung
- Verhalten bei Wechsel zwischen Heim-WLAN und Ersatz-WLAN waehrend einer Webaktion
- spaetere Integration ueber Caddy, VPN oder anderen Reverse Proxy
