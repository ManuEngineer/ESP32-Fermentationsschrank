# Future Scope

## Status und Geltungsgrenze

```text
FUTURE_SCOPE_REFERENCE_NON_NORMATIVE
FUTURE_SCOPE_TRACKING_ISSUE=163
R1_IMPLEMENTATION_ISSUE=164
R1_GATE=NOT_APPLICABLE
ISSUE164_OWNER_DECISION=VARIANT_B
R1_BROWSER_HOME_WIFI_SETUP=RETAINED
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED_VARIANT_B
R1_TOUCH_WIFI_KEYBOARD=DEFERRED_VARIANT_B
R1_WLAN_QR=DEFERRED_VARIANT_B
IMPLEMENTATION_AUTHORIZATION=NO
```

Dieses Dokument sammelt bewusst aus R1 entfernte oder fuer spaeter
verschobene Produktideen. Es erweitert keine aktuellen R1-SSOTs, ist kein
R1-Gate und erteilt keine Implementierungsfreigabe. Die R1-Entscheidungen fuer
Issue #89 stehen in [`NETWORK.md`](NETWORK.md) und im abgeschlossenen
Evaluationsnachweis.

Bei einer spaeteren Aktivierung muessen `main`, konkreter Produktbedarf,
aktuelle Architektur sowie die betroffenen Security-, Safety-, Recovery-,
Persistenz- und Webvertraege revalidiert werden. Danach ist ein eigener Plan
mit Ownerfreigabe erforderlich. Future-Punkte werden nicht automatisch zu
Pflichttests.

Das zugehoerige allgemeine Tracking-Issue ist [#163 – Product enhancements /
deferred scope](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/163).
Die separate R1-Integration ist als [#164 – R1 WLAN AP-only und Heim-WLAN ueber
nativen ESP-IDF-HTTP-Pfad integrieren](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/164)
angelegt. PR #165 ist auf `main` integriert; der offene lokale
Touch-Abschluss wird ueber PR #171 plan- und owner-gesteuert weitergefuehrt.

## WLAN: bewusst nicht R1

### Automatisches Ersatz-WLAN

Moegliche spaetere Erweiterung:

- bei laengerem Verlust des Heim-WLANs einen zusaetzlichen geschuetzten AP
  starten;
- parallel weiter versuchen, das Heim-WLAN zu erreichen;
- die normale lokale Weboberflaeche ueber den Ersatz-AP anbieten;
- den Ersatz-AP nach stabiler Rueckkehr des Heim-WLANs kontrolliert beenden.

Das ist nicht R1. R1 bietet die lokale Moduswahl am Display mit einem
ausdruecklichen AP-only-Modus sowie einen einzelnen gespeicherten
Heim-WLAN-Pfad. Ein moeglicher Aktivierungstrigger ist ein nachgewiesener
Bedarf bei headless/entferntem Einsatz oder ein konkretes reales
Wiederherstellungsproblem.

### Captive Portal

Eine spaetere Variante koennte die automatische OS-/Browser-Erkennung und
Portaleroeffnung anbieten. Das ist nicht R1, weil normaler Browserzugriff
ueber den bevorzugten mDNS-Namen beziehungsweise die direkte lokale IP als
Fallback ausreicht. Ein konkreter UX-Befund, dass manuelle Navigation auf
realen Zielgeraeten problematisch ist, ist Voraussetzung fuer eine neue
Bewertung.

### Mehrere gespeicherte Heim-WLANs

Eine spaetere Variante koennte mehrere bekannte Netze priorisieren und als
Fallback verwenden. Das ist nicht R1: Ein stationaerer Fermenter verwendet
typischerweise genau ein Heim-WLAN. Es gibt deshalb keine vorsorgliche
Mehrfach-WLAN-Abstraktion im R1-Scope. Aktivierung setzt einen realen
Produktbedarf und einen neuen, gegen den Persistenzvertrag geprueften Plan
voraus.

### Lokale HOME_WIFI-Credentialeingabe und Bildschirmtastatur

Die direkte Eingabe von HOME_WIFI-SSID und -Passwort am Touchdisplay sowie die
dafuer erforderliche Bildschirmtastatur sind nach der Ownerentscheidung fuer
Issue #164 (`VARIANT_B`) bewusst nicht Teil von R1. Der browserbasierte
Setup-Assistent bleibt der vollstaendige lokale R1-Eingabepfad. Eine spaetere
Aktivierung benoetigt einen konkreten UX-/Betriebsbedarf, einen neuen Plan und
ein eigenes Owner-Gate; sie darf keinen zweiten Credential- oder
Persistenzpfad erzeugen.

### WLAN-QR fuer individuelle SoftAP-Zugangsdaten

Der WLAN-QR mit individuellen Zugangsdaten des Setup- oder AP-only-SoftAPs ist
nach derselben Ownerentscheidung ein spaeterer Komfortpfad und kein R1-Gate.
SSID, individuelles Passwort und direkte lokale IP bleiben fuer den
manuellen SoftAP-Beitritt auf dem Display sichtbar. Ein spaeterer QR darf
keinen zweiten Credentialpfad oder Webserver einfuehren und benoetigt einen
eigenen Plan sowie neue Acceptance Criteria.

### Zusaetzlicher Webseiten-QR

Ein optionaler QR-Code koennte direkt die lokale Webadresse oeffnen. Er ist
zusammen mit dem WLAN-QR ein spaeterer Komfortpfad und kein R1-
Pflichtbestandteil. Er darf nicht als Ersatz fuer den direkten-IP-Fallback
oder als zweiter Credentialpfad eingefuehrt werden.

## Erweiterte Qualifikation nur bei realem Bedarf

Die folgenden Nachweise werden nicht automatisch als Future-Pflichten
ausgeloest:

- laenger dauernder Reconnect-Stress;
- Clientlast-Untersuchungen zu Leaks, Handles, Watchdog und Jitter;
- eine weitergehende Plattformmatrix;
- der physische Display-/Kamera-QR-Nachweis, soweit er nach angeschlossener
  Displayhardware noch relevant ist.

Ein konkreter spaeterer Produktbedarf, ein reproduzierbares Fehlerbild oder
belastbare UX-Evidence muss den jeweiligen Nachweis begruenden. Erst danach
werden Umfang, Testorakel und Acceptance Criteria neu festgelegt.

## Verwandte Future-Issues

- [#114 – Advanced Safety & Recovery / Safety Core v2](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/114)
  bleibt die spezifische Referenz fuer spaetere Safety-/Recovery-Erweiterungen.
- [#33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/33)
  bleibt die spezifische Referenz fuer die reservierte R_IS/L_IS- und
  Peltier-Hardwarequalifikation.

Die Inhalte dieser Issues werden hier nicht dupliziert. Dieses Dokument
verweist nur auf ihre Rolle im deferred Scope.
