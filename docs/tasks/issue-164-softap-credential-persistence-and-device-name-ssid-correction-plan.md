# Korrekturplan – Issue #164 / PR #171: persistenter SoftAP und Device-Name-SSID

```text
ISSUE=164
PR=171
BRANCH=agent/issue-164-network-ui-completion
WORKTREE=/tmp/issue164-network-ui.M4vluy
BASE_HEAD=daeccd995a2ba81d7af5b91587050c14dd8b84ca
BASE_PLAN_HEAD=32cb8f1cc46c2cea532458a1c5db5581da552052
SUPERSEDES_CONTRACT_ONLY=old_fixed-softap-ssid-and-per-boot-password-sections
PLAN_STATUS=PLAN_FIX_READY_INDEPENDENT_VERIFICATION
PLAN_FIX_SCOPE=P1_P2_B1_B3_B4
PRODUCT_CODE_CHANGED=NO
TESTS=NOT_RUN_PLAN_ONLY
BUILD=NOT_RUN_PLAN_ONLY
FLASH=NOT_RUN_PLAN_ONLY
IMPLEMENTATION_ALLOWED=NO
NEXT_GATE=INDEPENDENT_PLAN_FIX_VERIFICATION
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
    softApPassword, in every valid V2 record required and exactly 16 chars;
}
```

Das optionale `HOME_WIFI` bleibt unveraendert optional. Ein V2-Record ohne
exakt 16 Zeichen aus dem festgelegten SoftAP-Alphabet ist kein gueltiger
Record und kann strukturell nicht als gueltiger V2-Record entstehen. Decoder
und fachliche Validierung lehnen fehlende, zu kurze, zu lange oder sonst
ungueltige Werte fail-closed als InvalidRecord ab. Eine optionale Darstellung
von `softApPassword` im RAM-/Migrationsmodell ist nur zulaessig, damit ein
V1-Record vor seiner Migration eindeutig als "noch kein SoftAP-Passwort"
erkannt wird; diese RAM-Optionalitaet darf nicht in V2 serialisiert werden.

Die kanonische V2-Payload wird in dieser Reihenfolge geplant:

1. `HOME_WIFI`-Optionaltag, ein Byte;
2. bei gesetztem Tag: SSID als `uint16`-Laenge plus Bytes und Passwort als
   `uint16`-Laenge plus Bytes;
3. `softApPassword` als `uint16`-Laenge plus genau 16 Passwortbytes,
   ohne Present-Tag.

Die V1-Payload bleibt bytekompatibel und wird weiterhin ausschliesslich mit
Schema 1 decodiert: ein `HOME_WIFI`-Optionaltag und, falls gesetzt, die beiden
alten Stringfelder. V1 kennt kein SoftAP-Passwort und wird nicht als fertiger
Runtime-Vertrag verwendet.

### 3.3 Schema-Kompatibilitaet und Writer-Vertrag

`ConnectivityCredentialStore::load()` akzeptiert fuer
`RecordTypeId=9` ausschliesslich Schema 1 und Schema 2. Die Schemaauswahl
erfolgt vor der Payloaddekodierung und bindet jeweils genau ein Layout:

- Schema 1 wird nur mit dem bestehenden V1-Layout dekodiert; die vorhandenen
  V1-Bytes bleiben vollstaendig kompatibel und ergeben im Migrationsmodell
  "noch kein SoftAP-Passwort".
- Schema 2 wird nur mit dem neuen V2-Layout ohne
  `softApPassword`-Present-Tag dekodiert. Der feste Laengenprefix und die
  anschliessende fachliche Pruefung erzwingen genau 16 gueltige Zeichen.
- Jede andere Schema-Version bleibt `UnsupportedSchema`/Invalid und darf
  weder als V1 noch als V2 umgedeutet werden.
- Der aktuelle Writer schreibt ausschliesslich Schema 2 und immer ein
  gueltiges, genau 16-stelliges SoftAP-Passwort.

Ein V2-Payload ohne das Passwortfeld, mit einem falschen Laengenwert, mit
ungueltigen Alphabetzeichen oder mit Restbytes ist damit kein gueltiger V2-
Record. Der Read-/Write-/Readback-Vertrag, die Epoch-Bindung, die bestehende
`cc0`-/Record-Type-9-Zuordnung und die Record-Sequenzsemantik bleiben
unveraendert.

### 3.4 Geschlossene Groessenrechnung

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
+ (2 + 16)                SoftAP-Passwort
= 118 Byte                V2-Maximalpayload
+ 45 Byte                 StorageEnvelope
= 163 Byte                V2-Maximalenvelope
```

Die Implementierung muss diese beiden Maxima in den zentralen
`configuration_limits`-Konstanten, im Writer, im Decoder und im Store-
Readlimit identisch verwenden. Ein Payload-/Envelope-Golden-Test muss die
berechneten Bytes, das fehlende Present-Tag und die Abweisung von fehlendem,
ungueltigem oder ueberlangem Passwort absichern.

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

## 6. B1 – Netzwerkfehler bleiben vom Fermentationskern getrennt

Der Netzwerkpfad ist ein optionaler lokaler Dienst und keine Voraussetzung fuer
die Initialisierung des Fermentations-/Safety-Kerns. Fuer die spaetere
Umsetzung gilt deshalb der explizite Vertrag:

```text
network credential/random/persistence failure
OR network lifecycle start failure
OR HTTP start failure

-> network remains stopped/fail-closed
-> FermentationApplication core initialization continues
-> RunPersistenceCoordinator/load/classification continue
-> local fermentation/Safety remain governed only by existing contracts
-> no global service-required state solely because networking failed
```

Der heute sichtbare Kontrollfluss

```cpp
if (!initializeNetwork(...)) {
    return true;
}
```

darf in der spaeteren Umsetzung nicht dazu fuehren, dass ein reiner
Netzwerkfehler die anschliessende Erstellung oder Nutzung des
`RunPersistenceCoordinator`, die Run-Load-/Classification-Pfade oder die
bestehende Safety-/Readiness-Bewertung ueberspringt. Nach einem fehlenden oder
fehlgeschlagenen Netzwerkstart muss die Application mit Netzwerkstatus
`unavailable`/`stopped` in den bereits vorhandenen Core-Initialisierungspfad
weiterlaufen.

`initializeNetwork()` darf Fehler aus Credential-Erzeugung, Credential-
Persistenz/Readback, SSID-Ableitung, Lifecycle-Start oder HTTP-Start nicht
ueber einen globalen `requireService()`-Pfad in eine Abhaengigkeit des
Fermentationskerns von WLAN verwandeln. Ein solcher Fehler bleibt lokal am
Netzwerk-/Web-Dienst beobachtbar; die bestehenden Core-Faults und
Safety-Vertraege werden nicht erweitert. Eine zweite Application-State-
Machine, ein Netzwerk-Worker oder ein paralleler Readiness-Owner wird nicht
eingefuehrt.

Die spaetere Umsetzung muss mindestens diese getrennten Regressionen
vorsehen:

1. SoftAP-Credential-Write-/Readbackfehler: kein Network-Start, Core-Boot und
   Run-/Safety-Initialisierung laufen weiter.
2. Network-Lifecycle-Startfehler: Netzwerk bleibt unavailable/stopped, Core-
   Boot laeuft weiter.
3. HTTP-Startfehler: der Network-Lifecycle wird wieder gestoppt/fail-closed,
   Web bleibt unavailable, Core-Boot laeuft weiter.
4. Run-/Safety-Readiness bleibt identisch unabhaengig vom Network-Erfolg.

## 7. B3 – Recordexistenz ist nicht HOME_WIFI-Verfuegbarkeit

Ein gueltiger V2-Record kann ausschliesslich ein persistiertes SoftAP-
Passwort enthalten. Die vollstaendige Credential darf fuer SoftAP-
Passwort-Preservation im RAM gehalten werden; sie ist aber kein Beleg fuer
vorhandene HOME_WIFI-Credentials. Die fachliche Entscheidung lautet an jeder
Stelle exakt:

```cpp
validHomeWifiCredentials = currentCredential.homeWifi.has_value();
```

Unzulaessig ist die Abkuerzung "Credential-Record vorhanden bedeutet
HOME_WIFI verfuegbar". `currentCredential` und die darin verschachtelte
`homeWifi`-Optionalitaet bleiben getrennt. Nur bei `has_value()` darf das
HOME_WIFI-Paar an den Station-/Candidate-Pfad uebergeben werden.

Die Startup-Entscheidungen bleiben damit:

| Credential-/Modusbefund | Erwarteter Pfad |
|---|---|
| V2: SoftAP-Passwort vorhanden, `homeWifi` absent, HOME_WIFI ausgewaehlt | `HomeWifiSetup`, `setupFlowActive=true` |
| V2: SoftAP-Passwort vorhanden, `homeWifi` absent, AP_ONLY ausgewaehlt | `AccessPointOnly` |
| V2: SoftAP-Passwort und `homeWifi` vorhanden, HOME_WIFI ausgewaehlt | normaler `HomeWifi`-Pfad |
| setup-only V2 plus erfolgreicher HOME_WIFI-Kandidatencommit | HOME_WIFI wird ergaenzt, dasselbe SoftAP-Passwort bleibt erhalten |

Der AP_ONLY-Start darf niemals deshalb in Setup oder HOME_WIFI wechseln, weil
der V2-Record existiert. Umgekehrt darf HOME_WIFI ohne verschachtelte
Credentials nicht als normal verbunden gestartet werden. Die bestehende
Setup-/Candidate-/Commit-Semantik, Epochbindung und fail-closed Behandlung
bleiben unveraendert.

## 8. B4 – aktueller `deviceName` bei jedem Network-Restart

Der Ownervertrag gilt fuer jeden Application-owned Start-/Restart-Pfad:

```text
deviceName changes
-> next real network restart uses newly derived SSID
-> persisted SoftAP password stays unchanged
```

Das muss explizit fuer alle folgenden Pfade gelten:

- Boot und `initializeNetwork()`;
- `applyNetworkMode()`;
- `beginHomeWifiReconfiguration()`.

Vor jedem tatsaechlichen `INetworkLifecycle::start()` oder Restart bezieht
der Application-Owner den aktuellen Runtime-
`UserConfiguration.deviceName` ueber den bestehenden `ConfigurationService`
oder uebergibt die frisch daraus abgeleitete SSID an den bestehenden
Network-Pfad. Der `NetworkConfigurationService` liest dafuer niemals direkt
die Configuration-Persistenz. Er erhaelt den aktuellen SSID-Wert vom
Application-Owner und verwendet das SoftAP-Passwort unveraendert aus dem
aktuellen gueltigen V2-Credential.

Die konkreten Pfade sind damit geplant:

1. **Boot / `initializeNetwork()`**: Nach dem Runtime-Configuration-Read wird
   der damals aktuelle `deviceName` abgeleitet und vor dem ersten Lifecycle-
   Start gesetzt.
2. **`applyNetworkMode()`**: Nach dem bestehenden Configuration-Commit wird
   die aktuelle Runtime-Konfiguration erneut ueber den `ConfigurationService`
   bezogen. Der nachfolgende Network-Start/Restart bekommt daraus die neue
   SSID; ein gueltiges SoftAP-Passwort wird nicht neu erzeugt.
3. **`beginHomeWifiReconfiguration()`**: Vor dem expliziten HOME_WIFI-
   Restart stellt der Application-Owner den aktuellen `deviceName` bereit.
   Der bestehende Network-Pfad darf nicht mit einem beim Boot gespeicherten
   Namen arbeiten und darf fuer diese Aktualisierung keine direkte
   Store-Lesung einfuehren.

Wenn ein konkreter bestehender Pfad nach der spaeteren Implementierung den
Lifecycle tatsaechlich nicht neu startet, wird das als Befund dokumentiert und
der naechste reale Restartpunkt mit aktuellem `deviceName` geprueft. Eine
kuenstliche Restart-Logik oder neue State-Machine wird nicht eingefuehrt.

## 9. QR-Payload, Version und Geometrie

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
`34 + 28 = 62` Bytes. Fuer die gepinnte LVGL-Quelle `9.6.0~1` gilt im
Encoding-Modell:

```text
QR_PAYLOAD_MAX_BYTES                    = 62
qrcodegen_getMinFitVersion(Ecc_MEDIUM,62) = 4
QRCODEGEN_MIN_FIT_VERSION_MAX           = 4
```

Das ist die minimale qrcodegen-Fit-Version, nicht die Anzahl der Module, die
LVGL bei der gewaehlten Canvas-Groesse zeichnet. Der produktive Pfad setzt
weiterhin `lv_qrcode_set_quiet_zone(qrCode, true)`. LVGL 9.6.0 verwendet dabei
zusaetzlich `get_satisfied_size()`. Fuer den Minimum-Fit-Version-4-Payload und
eine Canvas-Groesse von 164 px ist der tatsaechliche LVGL-Rendervertrag:

```text
LVGL_EFFECTIVE_QR_VERSION_AT_164PX = 5
QR_MODULE_SCALE                    = 4PX
eigentliche QR-Module              = 37
QR-Flaeche                         = 37 * 4 = 148 px
LVGL_QUIET_ZONE_MARGIN             = 8PX_PER_SIDE
Canvas                             = 8 + 148 + 8 = 164 px
```

Die fruehere Aussage "4 Quiet-Zone-Module pro Seite / 41 gerenderte Module"
ist damit aus dem Vertrag entfernt. Es werden keine 41 gerenderten Module
und keine separate viermodulige Quiet-Zone implementiert oder behauptet.

Der aktuell kuerzere WLAN-Payload besitzt ebenfalls
`qrcodegen_getMinFitVersion(Ecc_MEDIUM, payload) = 4` und laeuft bereits durch
denselben LVGL-9.6.0-Quiet-Zone-Pfad. Die Regression muss den aktuellen
Payload und den maximal erlaubten 62-Byte-Payload gemeinsam pruefen und
beweisen, dass beide dieselbe effektive LVGL-Version 5, denselben
4-px-Modulmassstab, dieselbe 148-px-QR-Flaeche und denselben 8-px-Rand je
Seite erhalten. Damit gilt im Planvertrag:

```text
QR_READABILITY_MUST_NOT_REGRESS = REQUIRED
```

Diese Regression fuehrt keine Layoutaenderung, keine eigene Quiet-Zone-
Berechnung, keinen Custom-QR und keine neue QR-/ECC-Abhaengigkeit ein. Die
reale Smartphone-Lesbarkeit bleibt ein separates Hardware-Abnahmekriterium
nach dem spaeteren Implementation Review.

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

## 10. Scanvertrag erhalten

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

## 11. Geplante Regressionen und Nachweise

Diese Pruefungen sind Bestandteil der spaeteren Umsetzung, wurden in diesem
Plan-Only-Schritt aber nicht ausgefuehrt.

### SSID und QR

- ASCII- und Mehrbyte-`deviceName`-Faelle pruefen: hoechstens 24 rohe UTF-8-
  Bytes, niemals abgeschnittene Codepoints, nur Endkuerzung, gespeicherter
  Name byteidentisch unveraendert;
- Namen mit WiFi-Sonderzeichen pruefen: escaped SSID hoechstens 28 Bytes,
  kein Praefix/Suffix/Hash/MAC/Zeitwert, fail-closed bei unmoeglichem Wert;
- Payload enthaelt exakt aktuelle SSID und aktuelles Passwort, keine URL/IP;
- maximale WLAN-Payload ist 62 Bytes und
  `qrcodegen_getMinFitVersion(Ecc_MEDIUM,62)` ist maximal Version 4;
- aktueller kuerzerer und maximaler 62-Byte-Payload durchlaufen denselben
  LVGL-9.6.0-Quiet-Zone-Pfad mit effektiver Version 5, 37 QR-Modulen,
  4-px-Modulmassstab, 148-px-Flaeche und 8-px-Rand je Seite;
- die QR-Draw-Command bleibt exakt `{156,34,164,164}` und
  `QR_READABILITY_MUST_NOT_REGRESS` wird als Regression abgesichert;
- produktiver LVGL-Pfad setzt Quiet-Zone `true`, Schwarz/Weiss und die
  exakte Groesse;
- Header, Page-Titel, Current Mode, SSID, Passwort, IP, QR und Bottom-
  Controls liegen innerhalb der 320x240-Grenze; linke Info-Rechtecke und QR
  ueberlappen nicht.

### B1, B3 und B4

- SoftAP-Credential-Write-/Readbackfehler fuehren zu keinem Network-Start;
  der Core-Boot mit `RunPersistenceCoordinator`, Run-Classification und
  Safety-Readiness laeuft weiter.
- Ein Network-Lifecycle-Startfehler laesst das Netzwerk unavailable/stopped,
  ohne den Fermentationskern in einen globalen `service-required`-Zustand zu
  setzen.
- Ein HTTP-Startfehler stoppt den Network-Lifecycle wieder fail-closed und
  laesst nur Web unavailable; der Core-Boot laeuft weiter. Die vier B1-Faelle
  werden getrennt von den normalen Core-Faults verifiziert.
- V2 mit SoftAP-Passwort und ohne HOME_WIFI plus Auswahl `HOME_WIFI` ergibt
  `HomeWifiSetup` mit `setupFlowActive=true`; derselbe V2-Record unter
  `AP_ONLY` ergibt `AccessPointOnly`.
- V2 mit HOME_WIFI startet unter `HOME_WIFI` normal; ein erfolgreicher
  setup-only HOME_WIFI-Kandidatencommit ergaenzt HOME_WIFI und bewahrt exakt
  dasselbe SoftAP-Passwort.
- Nach `deviceName`-Aenderung von A nach B prueft ein echter
  `applyNetworkMode()`-Restart die SSID aus B bei unveraendertem Passwort P.
- Nach `deviceName`-Aenderung von A nach B prueft ein echter
  `beginHomeWifiReconfiguration()`-Restart die SSID aus B bei unveraendertem
  Passwort P. Startet ein konkreter Pfad nicht wirklich neu, wird das als
  Befund dokumentiert und am naechsten echten Restartpunkt geprueft; keine
  kuenstliche Restart-Logik wird hinzugefuegt.

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
  getestet; V1 wird ausschliesslich mit Schema-1-Layout, V2 ausschliesslich
  ohne SoftAp-Present-Tag dekodiert, und der Writer erzeugt nur Schema 2.

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

## 12. Aktive Dokumente und Review-Gate

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
Plan-Fix-Commit ist der naechste Gate ausschliesslich:

```text
INDEPENDENT_PLAN_FIX_VERIFICATION
-> IMPLEMENTATION_ALLOWED=NO_UNTIL_OWNER_PASS
```

Bis zu diesem Gate bleiben `PRODUCT_CODE_CHANGED=NO`, Tests, Builds und Flash
`NOT_RUN`, PR #171 Draft, kein Ready-Wechsel, kein Merge, Issue #164 offen und
Aktuatorfreigabe ausgeschlossen.
