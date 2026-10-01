# Korrekturplan – Issue #164 / PR #171: persistenter SoftAP und Device-Name-SSID

```text
ISSUE=164
PR=171
BRANCH=agent/issue-164-network-ui-completion
WORKTREE=/tmp/issue164-network-ui.M4vluy
BASE_HEAD=daeccd995a2ba81d7af5b91587050c14dd8b84ca
SUPERSEDES_CONTRACT_ONLY=old_fixed-softap-ssid-and-per-boot-password-sections
PLAN_STATUS=PLAN_ONLY_OWNER_REVIEW_REQUIRED
PRODUCT_CODE_CHANGED=NO
TESTS=NOT_RUN_PLAN_ONLY
BUILD=NOT_RUN_PLAN_ONLY
FLASH=NOT_RUN_PLAN_ONLY
IMPLEMENTATION_ALLOWED=NO
PR_STATE=OPEN_DRAFT
READY=NO
MERGE=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```

Dieser versionierte Korrekturplan gilt ausschliesslich fuer den bestehenden
Checkout, Branch und Draft-PR #171. Er ersetzt nur die inzwischen unpassenden
SoftAP-Vertraege aus dem vorherigen Issue-164-Plan: die feste SSID
`Fermentation` und das per Boot erzeugte, nur fluetige Passwort. Die bereits
umgesetzten UI-, Renderer-, Lifetime- und Scan-Begrenzungen bleiben erhalten.
Der bisherige Plan, die Roadmap und historische Hardware-Evidence werden in
diesem Plan-Commit nicht rueckwirkend umgeschrieben.

## 1. Ziel und unveraenderte Grenzen

Der produktive Netzwerkpfad soll vor dem ersten Wi-Fi-Start einen aus der
kanonischen `UserConfiguration.deviceName` abgeleiteten SoftAP-Namen und ein
pro `StorageEpoch` persistiertes SoftAP-Passwort besitzen. Die bestehende
`ConnectivityCredential`-Domaene unter `StateStoreKey=cc0` und
`RecordTypeId=9` bleibt die einzige Credential-Persistenz. Das Passwort wird
nicht in `UserConfiguration`, einem zweiten NVS-Key, einer neuen Datenbank,
Logs, Diagnose, HTTP-Responses oder Exporten abgelegt.

Unveraendert bleiben insbesondere:

- WPA2-PSK und der bestehende sichere `ISecureRandomSource`-Pfad;
- das 42-Zeichen-Alphabet, Rejection Sampling und exakt 16 Passwortzeichen;
- LVGL `9.6.0~1`, das vorhandene `lv_qrcode`-Widget und dessen ECC-Medium-
  Pfad;
- QR-Quiet-Zone, Schwarz/Weiss-Kontrast, keine Interpolation und keine
  Antialiasing-/ECC-Sonderimplementierung;
- QR-Rechteck `{156,34,164,164}` und die bestehende manuelle Anzeige;
- Scanlimit 16, Heap-Vector-Pfad, defensive HTTP-Begrenzung auf 16/528 Byte,
  `503` bei Scanfehler und leere `200`-Antwort bei erfolgreichem Empty-Scan;
- die bestaetigten Renderer-/Lifetime-Fixes, kein neuer Worker, keine neue
  State-Machine, kein Memory-Pool, keine Aktuatorfreigabe;
- kein Full-Erase, kein NVS-Erase, kein neuer Branch und kein neuer PR.

## 2. SoftAP-SSID aus `UserConfiguration.deviceName`

### 2.1 Kanonische Ableitung

Die neue kleine, reine Ableitung wird nur aus dem bereits validierten,
kanonischen `UserConfiguration.deviceName` gespeist. Der gespeicherte
`deviceName` bleibt byte- und inhaltsgenau unveraendert. Die Ableitung darf
keine Normalisierung, Trimmung, Kleinschreibung, Lokalisierung oder sonstige
Mutation des gespeicherten Werts vornehmen.

Die SSID-Regeln sind:

1. Den UTF-8-String am Ende auf hoechstens 24 **rohe UTF-8-Bytes** begrenzen.
   Das Ende darf nur an einer vollstaendigen UTF-8-Codepoint-Grenze abgeschnitten
   werden; nie innerhalb eines Mehrbyte-Codepoints.
2. Keine feste Praefix-/Suffix-/Hash-/MAC-/Zeit-/Zufallskomponente anfuegen.
3. Den so entstandenen SSID-Kandidaten fuer den Standard-WLAN-QR escapen.
   Die Escape-Regeln bleiben die vorhandenen Regeln fuer `\\`, `;`, `,`, `:`
   und `"`.
4. Falls die Escape-Darstellung mehr als 28 Bytes benoetigt, den Kandidaten
   ausschliesslich am Ende weiter auf den laengsten noch gueltigen
   Codepoint-Prefix kuerzen, bis die escaped SSID hoechstens 28 Bytes hat.
   Damit bleiben sowohl die Rohgrenze 24 als auch das QR-Budget fuer jeden
   gueltigen Device-Name eingehalten; es wird niemals ein Ersatzname erzeugt.
5. Ein leerer oder nicht gueltig ableitbarer Kandidat ist ein fail-closed-
   Fehler vor dem Wi-Fi-Start. Es gibt keinen Fallback auf `Fermentation`,
   MAC, Zeit, Geraete-ID oder einen anderen vorhersehbaren Wert.

`deviceName` hat im bestehenden User-Configuration-Vertrag mindestens einen
gueltigen sichtbaren Unicode-Scalar. Die neue Ableitung darf diesen Vertrag
nicht lockern oder den gespeicherten Wert nachtraeglich kuerzen. Die
QR-escape-bedingte weitere Endkuerzung ist ausschliesslich eine runtime-
SSID-Darstellungsgrenze.

### 2.2 Runtime-Verhalten

Die Ableitung erfolgt im bestehenden Application-/Network-Startpfad mit dem
aktuellen Runtime-`deviceName`, nicht in `app_main` und nicht in einer zweiten
UI-Wahrheit. Ein geaenderter `deviceName` wird beim naechsten Network-Start
bzw. Boot verwendet. Ein laufender SoftAP muss dafuer nicht durch eine neue
State-Machine dynamisch umbenannt werden. Die Aenderung des Device-Namens
rotiert das persistierte SoftAP-Passwort nicht.

Der ESP-IDF-Adapter behaelt SSID und Passwort nur in seiner bestehenden
fluechtigen Wi-Fi-Konfiguration. Die Application uebergibt beide Werte vor
dem ersten `INetworkLifecycle::start`; erst danach darf SoftAP/Station
gestartet werden. Der Adapter darf die Werte nicht selbst aus MAC, Zeit oder
anderen Quellen ableiten.

## 3. Persistenter SoftAP-Password-Vertrag

### 3.1 Wert und Zufall

Das Passwort ist exakt 16 Zeichen aus dem bereits festgelegten Alphabet:

```text
ACDEFHJKMNPQRTUVWXYacdefhjkmnpqrtuvwxy3479
```

Dieses Alphabet hat 42 Zeichen und vermeidet die vereinbarungsgemaess schwer
unterscheidbaren Gruppen. Die Auswahl verwendet ausschliesslich die bestehende
sichere Zufallsquelle und Rejection Sampling mit dem bestehenden
`kRejectionLimit`; Modulo-Bias ist unzulaessig. Die Entropie des Zeichenraums
ist `16 * log2(42) = 86.27` Bit und liegt damit ueber dem Ziel von ca. 80 Bit.

Es gibt keinen deterministischen oder vorhersagbaren Fallback. Wenn die
Zufallsquelle, die Persistenz oder die Readback-Pruefung fehlschlaegt, darf
kein nur fluetiges Passwort fuer einen gestarteten SoftAP verwendet werden;
der Netzwerkstart bleibt fail-closed.

### 3.2 Ein einziger persistenter Record, Schema 2

Die bestehende `ConnectivityCredential`-Domaene wird im selben Record
erweitert. Es wird kein neuer Store, kein neuer Key und kein Feld in
`UserConfiguration` eingefuehrt.

Der fachliche V2-Inhalt ist:

```text
ConnectivityCredential {
    optional HOME_WIFI credentials;
    optional-on-wire softApPassword, in every valid V2 record exactly 16 chars;
}
```

Das optionale `HOME_WIFI` bleibt unveraendert optional. Ein V2-Record ohne
gueltiges `softApPassword` ist kein gueltiger aktiver Record; er wird
fail-closed als InvalidRecord behandelt. Die Option im RAM-/Migrationsmodell
ist nur noetig, damit ein V1-Record vor seiner Migration eindeutig als
passwordlos erkannt wird.

Die kanonische V2-Payload wird in dieser Reihenfolge geplant:

1. `HOME_WIFI`-Optionaltag, ein Byte;
2. bei gesetztem Tag: SSID als `uint16`-Laenge plus Bytes und Passwort als
   `uint16`-Laenge plus Bytes;
3. `softApPassword`-Present-Tag, ein Byte;
4. bei gesetztem Tag: `uint16`-Laenge plus genau 16 Passwortbytes.

Die V1-Payload bleibt bytekompatibel und wird weiterhin nur mit Schema 1
decodiert: ein `HOME_WIFI`-Optionaltag und, falls gesetzt, die beiden alten
Stringfelder. V1 kennt kein SoftAP-Passwort und wird nicht als fertiger
Runtime-Vertrag verwendet.

### 3.3 Geschlossene Groessenrechnung

Die bisherige V1-Maximalpayload ist:

```text
1                         HOME_WIFI-Optionaltag
+ (2 + 32)                maximale HOME_WIFI-SSID
+ (2 + 63)                maximales HOME_WIFI-Passwort
= 100 Byte                V1-Maximalpayload
+ 45 Byte                 StorageEnvelope mit CRC und allen Feldern
= 145 Byte                V1-Maximalenvelope
```

Die V2-Maximalpayload ist:

```text
1                         HOME_WIFI-Optionaltag
+ (2 + 32)                maximale HOME_WIFI-SSID
+ (2 + 63)                maximales HOME_WIFI-Passwort
+ 1                       softApPassword-Present-Tag
+ (2 + 16)                SoftAP-Passwort
= 119 Byte                V2-Maximalpayload
+ 45 Byte                 StorageEnvelope
= 164 Byte                V2-Maximalenvelope
```

Die Implementierung muss diese beiden Maxima in den zentralen
`configuration_limits`-Konstanten, im Writer, im Decoder und im Store-
Readlimit identisch verwenden. Ein Payload-/Envelope-Golden-Test muss die
berechneten Bytes und die Abweisung von Ueberlaenge absichern.

## 4. Migration und Lebenszyklus pro `StorageEpoch`

Die Netzwerkinitialisierung besitzt vor dem ersten Wi-Fi-Start diese
fail-closed Entscheidung:

| Storebefund | Aktion |
|---|---|
| aktueller Epoch, gueltiger V2-Record | `softApPassword` unveraendert laden; keine Zufallsquelle aufrufen; SSID aus aktuellem `deviceName` ableiten; starten |
| aktueller Epoch, gueltiger V1-Record | vorhandenes HOME_WIFI uebernehmen, sicheres neues SoftAP-Passwort erzeugen, V2 unter derselben Epoch mit Readback schreiben, erst danach starten |
| `NotFound` | neues sicheres Passwort erzeugen, V2 fuer aktuelle Epoch mit optional leerem HOME_WIFI schreiben und readbacken, erst danach starten |
| `OtherEpoch` | alten Record nicht uebernehmen; neues Passwort fuer die aktuelle Epoch erzeugen, neuen V2-Record ohne HOME_WIFI schreiben und readbacken, erst danach starten |
| Read-, Capacity-, CRC-, Unsupported-Schema-, Invalid- oder indeterminierter Befund | keine Neu-/Fallback-Anlage, kein Wi-Fi-Start, Recovery-/Servicepfad beibehalten |

Bei einer Migration oder Neuanlage wird die Record-Sequenz checked bestimmt;
ueberlaufende oder unklare Sequenzen sind Persistenzfehler. Ein erfolgreicher
Write gilt erst nach dem bestehenden byteidentischen Readback als committed.
`CommitOutcomeUnknown` bleibt indeterminiert und startet keinen Transport.

Ein neuer Passwortwert entsteht damit nur, wenn fuer die aktuelle
`StorageEpoch` noch kein gueltiges persistiertes V2-Passwort existiert. Ein
normaler Reboot, AP_ONLY/HOME_WIFI-Wechsel, Setup, Test-before-Commit,
Reconnect oder eine Device-Name-Aenderung erzeugt keinen neuen Wert. Ein
Factory-/Reset-Vorgang, der die StorageEpoch wechselt, erzeugt beim naechsten
Netzwerkstart einen neuen V2-Wert.

Der HOME_WIFI-Kandidatenpfad kopiert bei `testCandidate`/Commit das aktuell
gueltige `softApPassword` unveraendert in den neuen Record. Ein HOME_WIFI-
Passwortwechsel darf daher den SoftAP-Zugang nicht rotieren. Die bestehende
Commit-, Readback-, Recovery- und `CommitOutcomeUnknown`-Semantik bleibt
vollstaendig erhalten.

## 5. Startup-Ownership ohne zweite Netzwerkarchitektur

Die bisherige Erzeugung kompletter SoftAP-Credentials in `app_main` wird im
späteren Implementierungsschritt aus dem Composition-Root entfernt. Der
Composition-Root erstellt weiterhin die konkrete sichere Zufallsquelle und
die konkrete ESP-IDF-Lifecycle-Instanz, aber er erzeugt weder SSID noch
Passwort und kennt weder `StorageEpoch` noch Credential-Persistenz.

Der bestehende Application-/Network-Pfad bekommt stattdessen injiziert:

- die bestehende `ISecureRandomSource`-Instanz;
- den bereits validierten Runtime-`StorageEpoch`;
- den bestehenden `IStateStore`/`ConnectivityCredentialStore`;
- den aktuellen `UserConfiguration.deviceName`;
- den bestehenden `INetworkLifecycle`.

Vor `lifecycle.start(...)` werden in dieser Reihenfolge ausgefuehrt:

1. `cc0` unter der erwarteten Epoch laden und den oben definierten Befund
   klassifizieren;
2. bei Bedarf genau ein neues Passwort erzeugen, als V2 schreiben und
   byteidentisch ruecklesend bestaetigen;
3. SSID aus `deviceName` ableiten und die QR-Escape-Grenze pruefen;
4. die SoftAP-SSID und das bestaetigte Passwort ueber den bestehenden
   Lifecycle-Konfigurationspfad in dessen RAM-Konfiguration setzen;
5. den vorhandenen Hostnamen aus `deviceName` setzen;
6. den bestehenden Network-Start fuer `AP_ONLY` oder `HOME_WIFI` ausfuehren.

Falls der aktuelle schmale Lifecycle-Port noch keinen vor dem Start
aufgerufenen SoftAP-Konfigurationssetter besitzt, wird dafuer nur ein
vor-startbarer Port-Schritt im vorhandenen `INetworkLifecycle`-/ESP-IDF-
Adaptervertrag vorgesehen. Er darf nach dem Start ablehnen, fuehrt keine neue
State-Machine ein und fuehrt keine weitere Persistenz ein. Die Adapterfelder
bleiben RAM-only. Mock-Lifecycle und Produktionsadapter muessen denselben
Vorstart-/Fail-Closed-Vertrag abbilden.

Die manuelle Anzeige und `networkAccessPointInfo()` beziehen SSID, Passwort
und IP weiterhin aus dem bestehenden aktiven Lifecycle. Es gibt keine zweite
SSID-/Passwort-Wahrheit in Renderer, HTTP-Route oder Diagnose.

## 6. QR-Payload, Version und Geometrie

Die semantische Payload bleibt exakt:

```text
WIFI:T:WPA;S:<escaped current SSID>;P:<escaped current password>;;
```

Sie enthaelt keine URL und keine IP. Das gewaehlte Passwortalphabet benoetigt
keine WiFi-Escapes. Fuer die SSID gilt die Ableitungsgrenze aus Abschnitt 2.
Die feste Nicht-SSID-Laenge ist:

```text
"WIFI:T:WPA;S:" = 13 Byte
";P:"         =  3 Byte
password      = 16 Byte
";;"          =  2 Byte
gesamt        = 34 Byte
```

Bei maximal 28 escaped SSID-Bytes ergibt sich eine maximale Payload von
`34 + 28 = 62` Bytes. Vor der Umsetzung ist dies gegen die tatsaechlich
gepinnte LVGL-Quelle `9.6.0~1` und deren `qrcodegen`-ECC-M-Bytekapazitaet
formal zu bestaetigen. Version 4 muss 62 Bytes akzeptieren; falls die
gepinnte Quelle diese Aussage nicht exakt bestaetigt, darf nicht implementiert
werden, bevor der Owner die aufgeloeste QR-Variante freigibt.

Fuer QR-Version 4 gilt:

```text
QR-Module ohne Quiet-Zone = 17 + 4 * (4 - 1) = 33
Quiet-Zone                 = 4 Module je Seite
Module mit Quiet-Zone      = 33 + 4 + 4 = 41
Pixel je Modul             = 4
LVGL-Groesse               = 41 * 4 = 164 px
```

Der produktive LVGL-Pfad bleibt beim vorhandenen Widget und setzt explizit
Quiet-Zone, `164` Pixel, Schwarz als Dark-Color und Weiss als Light-Color.
Interpolation, Antialiasing, neue QR-/ECC-Quellen oder ein zweiter QR-Pfad
sind ausgeschlossen.

Die 320x240-Netzwerkseite bleibt geometrisch:

```text
DISPLAY_BOUNDS         = {0, 0, 320, 240}
HEADER_BOUNDS          = {0, 0, 320, 32}
PAGE_TITLE_RECT        = {8, 34, 140, 18}
CURRENT_MODE_RECT      = {8, 52, 140, 18}
SSID_RECT              = {8, 72, 140, 36}
PASSWORD_RECT          = {8, 110, 140, 54}
IP_RECT                = {8, 166, 140, 18}
QR_RECT                = {156, 34, 164, 164}
BOTTOM_CONTROLS_BOUNDS = {0, 200, 320, 40}
```

Der QR bleibt vollstaendig zwischen Header und Bottom-Controls. Die
manuellen Werte bleiben in der linken Spalte; Titel, Current Mode, SSID,
Passwort und IP ueberlappen den QR nicht. Bestehende Header-Touch- und
Bottom-Control-Rechtecke werden nur in einer Regression mitgeprueft, nicht
durch eine neue Layoutarchitektur ersetzt.

## 7. Scanvertrag erhalten

Die bereits beschlossene Scan-Korrektur ist Bestandteil dieses Korrekturplans
und darf nicht geloest oder erweitert werden:

- `driver_count` wird vor jeder `wifi_ap_record_t`-Allokation auf
  `min(driver_count, 16)` begrenzt;
- der vorhandene Heap-Vector-Pfad wird verwendet; kein grosser lokaler
  Stackpuffer im synchronen Scan-/HTTP-Pfad;
- Arbeitsrecords, `NetworkScanEntry`-Liste und HTTP-Ausgabe bleiben jeweils
  auf maximal 16 Eintraege begrenzt;
- fuer SSID-Zeilen der HTTP-Antwort gilt explizit
  `16 * (32 + 1) = 528` Byte als Maximum;
- Fehler-Scan bleibt `503`, erfolgreicher Empty-Scan bleibt eine leere
  `200`-Antwort;
- `ROOT_CAUSE=UNPROVEN` fuer den beobachteten Hardware-Scan-Ausfall bleibt
  unveraendert;
- Heap, groesster freier 8-Bit-Block und, soweit der bestehende Pfad es ohne
  neue Diagnosearchitektur erlaubt, Task-Stack-HWM werden vor/nach dem
  Browser-Scan erfasst. Diese Messung beweist keine OOM-Ursache allein.

## 8. Geplante Regressionen und Nachweise

Diese Pruefungen sind Bestandteil der spaeteren Umsetzung, wurden in diesem
Plan-Only-Schritt aber nicht ausgefuehrt.

### SSID und QR

- ASCII- und Mehrbyte-`deviceName`-Faelle pruefen: hoechstens 24 rohe UTF-8-
  Bytes, niemals abgeschnittene Codepoints, nur Endkuerzung, gespeicherter
  Name byteidentisch unveraendert;
- Namen mit WiFi-Sonderzeichen pruefen: escaped SSID hoechstens 28 Bytes,
  kein Praefix/Suffix/Hash/MAC/Zeitwert, fail-closed bei unmoeglichem Wert;
- Payload enthaelt exakt aktuelle SSID und aktuelles Passwort, keine URL/IP;
- maximale Payload ist 62 Bytes, die gepinnte ECC-M-Quelle waehlt hoechstens
  Version 4, und die QR-Draw-Command ist exakt `{156,34,164,164}`;
- produktiver LVGL-Pfad setzt Quiet-Zone `true`, Schwarz/Weiss und die
  exakte Groesse;
- Header, Page-Titel, Current Mode, SSID, Passwort, IP, QR und Bottom-
  Controls liegen innerhalb der 320x240-Grenze; linke Info-Rechtecke und QR
  ueberlappen nicht.

### Zufall und Persistenz

- SSID `deviceName`-Ableitung ist deterministisch nur aus dem aktuellen
  Konfigurationswert; Passwortableitung aus MAC/Zeit/Geraete-ID ist nicht
  vorhanden;
- Passwort ist exakt 16 Zeichen und jedes Zeichen liegt im 42-Zeichen-
  Alphabet;
- kontrollierte Zufallssequenzen pruefen Rejection Sampling und erzeugen bei
  zwei verschiedenen Sequenzen zwei verschiedene Passwoerter;
- Zufallsquellenfehler, Writefehler, Readbackfehler, Capacityfehler und
  `CommitOutcomeUnknown` starten keinen SoftAP mit volatilem Ersatz;
- V1-same-epoch-Migration, `NotFound`, gueltiger V2-Load und `OtherEpoch`
  werden mit erwarteter Sequenz, Epoch-Bindung, Home-WiFi-Erhalt bzw.
  bewusstem Verwerfen des alten Epoch-Inhalts geprueft;
- ein gueltiger V2-Load ruft die Zufallsquelle nicht auf;
- Reboot, AP_ONLY/HOME_WIFI, Setup, Reconnect und Device-Name-Aenderung
  behalten dasselbe Passwort; eine neue Factory-/Reset-Epoch erzeugt ein
  neues Passwort;
- HOME_WIFI-Kandidatencommit behaelt das SoftAP-Passwort;
- exakte V1-/V2-Payload-, Envelope- und Maximalgroessen-Goldens werden
  getestet.

### Bestehende Regressionen und Builder-Gates

Die direkt betroffenen Netzwerk-, Credential-, Renderer-, Touch-, Lifetime-
und Scan-Regressionen sowie die bereits bestaetigten #171-Tests werden nach
der Implementierung gezielt ausgefuehrt. Danach folgen Builder Self-Check,
`esp32_bringup` und `esp32_release` als vollstaendige Builds. Nicht
ausgefuehrte Tests oder Builds werden nicht als bestanden markiert.

### Hardware-Grenze

Hardware-Flash, UART, QR-Scan, Client-Join, DHCP, Browser, HOME_WIFI-
Reconnect und jede weitere reale Abnahme sind nach diesem Plan **nicht** Teil
der Planphase. Sie bleiben bis zu einem Owner-PASS des Full Implementation
Reviews gesperrt. Kein FTDI-Displaykopplungsbefund wird ohne neue Evidence als
Firmware-Crash deklariert; `ROOT_CAUSE=UNPROVEN` bleibt bestehen.

## 9. Aktive Dokumente und Review-Gate

Erst waehrend der spaeter freigegebenen Umsetzung werden die aktiven Vertraege
mit dem implementierten Stand synchronisiert:

- `docs/NETWORK.md`: Device-Name-SSID, 24-Byte-/28-Byte-Grenzen,
  persistentes SoftAP-Passwort pro Epoch, QR- und Startup-Reihenfolge;
- `docs/REQUIREMENTS.md`: Owner-Anforderungen und unveraenderte
  Sicherheits-/No-Secret-Grenzen;
- `docs/CONFIGURATION_PERSISTENCE.md`: `cc0`/Record 9, Schema V1/V2,
  exakte Payload-/Envelope-Groessen und Migrationen;
- das aktive Issue-#164-Task-Dokument: neue Vertragsfassung und Status,
  waehrend historische Hardware-Evidence unveraendert und klar als
  historisch markiert bleibt.

In dieser Planphase werden diese Dateien nicht veraendert. Nach diesem
Plan-Commit ist der naechste Gate ausschliesslich:

```text
OWNER_REVIEW_OF_NEW_PLAN_HEAD
-> OWNER_APPROVAL_OF_THIS_CORRECTION_PLAN
-> IMPLEMENTATION_ALLOWED
```

Bis zu diesem Gate bleiben `PRODUCT_CODE_CHANGED=NO`, Tests, Builds und Flash
`NOT_RUN`, PR #171 Draft, kein Ready-Wechsel, kein Merge, Issue #164 offen und
Aktuatorfreigabe ausgeschlossen.
