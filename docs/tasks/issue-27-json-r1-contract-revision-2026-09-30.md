# Issue #27 – proportionale JSON-R1-Vertragsrevision

## Status und Zweck

Diese kleine Revision präzisiert ausschliesslich den JSON-R1-Vertrag für die
bereits softwareseitig vorhandenen, noch nicht produktiv komponierten Slice-4B-
DTOs. Sie trennt notwendige Korrektheit von zusätzlichem Parser-Hardening und
gleicht die bestehenden Kandidaten ArduinoJson 7.4.3 und Espressif cJSON
1.7.19~2 anhand desselben proportionalen Vertrags ab. Die historische
cJSON-Messung bleibt unverändert; der isolierte Kandidatenabschluss ergänzt
ausschliesslich einen bounded NUL-Input-Gate und aktuelle R1-Evidence.

```text
ISSUE=27
PR=170
PR_STATE=OPEN_DRAFT
BRANCH=agent/issue-27-web-api-auth-main-restart
ISSUE_STATE=OPEN
BASELINE_MAIN=b871375f494701bed1834013cfeb789856983e3a
PLAN_REVISION_BASE_HEAD=04d2546dd89e1b67e30a0e0e055280d5cb6361a8
PLAN_REVISION_DATE=2026-09-30
PLAN_FIX_BASE_HEAD=025fafca27d940dce8ed9e0d496073c121c9989c
PREVIOUS_PLAN_SHA=031ba2ada63de08ff4a99547d4ec911fcbc6f864
CANDIDATE_COMPLETION_BASE_HEAD=c792c4ac1791ddbeb96864ee32fcff249e3389ae
CJSON_SPIKE_COMMIT=b5b26c204f00e63c4e3944feacde4dccdcb4ae06
PREVIOUS_APPROVED_PLAN_SHA=46e0ea470b307a34867e24337b63a9d38166d771
PLAN_STATUS=INDEPENDENT_PLAN_FIX_VERIFICATION_PENDING
OWNER_PLAN_APPROVAL=REQUIRED_AFTER_FIX_VERIFICATION
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCT_CODEC_OR_DEPENDENCY_CHANGE=NO
PRODUCT_COMPOSITION=NOT_STARTED
PRODUCT_TESTS=NOT_RUN
BUILDER_SELF_CHECK=NOT_RUN_PLAN_ONLY
CJSON_SPIKE=BOUNDED_NUL_GATE_COMPLETE
CJSON_SPIKE_NATIVE=PASS
CJSON_SPIKE_ESP_IDF_6_1_ESP32_BUILD=PASS
HARDWARE=NOT_RUN
FLASH=NOT_RUN
ACTUATOR_RELEASE=NO
```

`docs/audits/COMPONENT_EVALUATIONS.md`, der frühere ArduinoJson-/cJSON-Spike
und die vier Duplicate-Key-Testvektoren bleiben historische Evidence. Dieses
Dokument ist nach Review/Ownerfreigabe die aktuelle JSON-R1-Vertragsgrundlage;
es ersetzt nicht den freigegebenen Gesamtplan und autorisiert allein keine
Implementation oder finale Bibliotheksauswahl.

## 1. Verbindliche R1-MUST-Anforderungen

Der folgende Vertrag gilt gemeinsam für den internen Run-Mutations-DTO und die
Read-only-Antworten `status`, `temperatures` und `alerts`:

- **Harte Grenzen:** Request-Body höchstens 480 Byte, Antwort höchstens
  3072 Byte; der konkrete DTO-Umfang bleibt zusätzlich begrenzt (JSON-Tiefe 4,
  Programm-ID höchstens 48 Byte, höchstens 3 Temperaturprojektionen und
  16 Alerts). Grenze vor/nach Parsing beziehungsweise Serialisierung
  fail-closed erzwingen.
- **JSON und Schema:** syntaktisch gültiges JSON, vollständiger Bodyverbrauch
  ohne angehängte Nicht-Whitespace-Daten, festes Schema `v=1`, geschlossene
  Feld-Allowlist und alle für die gewählte Variante erforderlichen Felder.
  Unbekannte Felder, fehlende Pflichtfelder, falsche JSON-Typen und nicht
  unterstützte Versionen werden abgelehnt.
- **Werte:** Zahlen werden zusätzlich zum JSON-Typ auf Endlichkeit und den
  fachlich erlaubten Wertebereich geprüft; Integerfelder werden ohne
  Vorzeichen-/Breitenverlust geprüft. Keine `NaN`, Infinity oder ausserhalb
  des jeweiligen Feldbereichs liegende Werte als DTO ausgeben.
- **Tatsächlich vorkommende Texte:** Intent-/Enum-Texte müssen einem
  festgelegten ASCII-Wert entsprechen. Programm-IDs müssen die bestehende
  `validateLowercaseIdentifier()`-Projektvalidierung mit den kanonischen
  ID-Grenzen bestehen. Unbekannte Schlüssel bleiben durch das geschlossene
  Schema abgelehnt. Die Read-only-Ausgabe verwendet ausschliesslich die
  vorhandenen typisierten Projektionen und deren feste Text-Allowlist.
  Fehlende oder untrusted Temperaturwerte werden ausdrücklich als ungültig/
  fehlend markiert, niemals als vertrauenswürdiger Nullwert ausgegeben.
- **Revision und Konflikt:** `UserConfigurationRevision` und
  `ProgramCatalogRevision` werden verlustfrei auf den jeweiligen
  `uint64_t`-Wert abgebildet. Die Application vergleicht sie exakt mit dem
  erwarteten aktuellen Zustand; stale Revisionen behalten die bestehende
  Konfliktsemantik und dürfen keine Mutation ausführen.
- **Determinismus und Bindung:** Ein Parserlauf erzeugt höchstens ein
  vollständig validiertes `WebRunMutationDto`; bei Fehlern wird kein partieller
  DTO publiziert. Der exakte unveränderte `HttpRequest::body` wird einmal
  dekodiert und genau dieselben Bodybytes bilden den Replay-Fingerprint.
  Kein DTO aus einer zweiten Quelle, keine zweite Dekodierung und kein
  rekonstruierter/normalisierter Body für Replay.
- **Grenzen und Geheimnisse:** JSON-Bibliothekstypen bleiben in der konkreten
  Codec-Implementierung. Mutations-DTOs und Read-only-Antworten enthalten
  keine Credentials, Session-/CSRF-Geheimnisse, Service-PINs oder sonstigen
  Secrets. Auth-/Session-Geheimnisse sind kein Bestandteil dieser DTOs.

Die verbindlichen bestehenden Bounds werden nicht vergrössert. Response- und
Body-Bounds gelten auch für den jeweils maximalen konkreten DTO-Fall.

## 2. `uint64_t`-Revisionen im Webformat

```text
UINT64_WEB_REPRESENTATION=DECIMAL_STRING
FIELDS=UserConfigurationRevision,ProgramCatalogRevision
VALID_RANGE=1..18446744073709551615
ABSENT_OPTIONAL_FIELD=NO_EXPECTED_REVISION
"0"=INVALID
```

Beide Felder werden in Request und Read-only-Revisionprojektion als kanonische
dezimal kodierte JSON-Strings übertragen: 1 bis 20 ASCII-Ziffern, keine
führenden Nullen, kein Vorzeichen, Dezimalpunkt oder Exponent. Gültiger
Zahlenbereich ist exakt `1..18446744073709551615`; `0` ist reserviert und
ungültig. Ein fehlendes optionales Feld bedeutet `NO_EXPECTED_REVISION` und
wird nicht als numerischer Wert `0` interpretiert.

Browser-JavaScript-`Number` unterscheidet Integer nur bis
`Number.MAX_SAFE_INTEGER = 2^53-1`: beispielsweise werden `2^53` und `2^53+1`
auf denselben Number-Wert abgebildet. Der vollständige ESP32-`uint64_t`-Bereich
geht bis `2^64-1`. Dezimalstrings bleiben browserseitig exakt und können auf dem
ESP32 ohne Float-Konvertierung verarbeitet werden. Der Codec parst ziffernweise
mit vor jedem Schritt geprüfter Überlaufbedingung
`accumulator <= (UINT64_MAX - digit) / 10` und serialisiert denselben
Ganzzahlwert wieder dezimal. Das ist kleine lokale Codec-Logik, keine
BigInteger-Abstraktion.

Die übrigen derzeitigen Revisionsfelder sind `uint32_t` und behalten ihr
bestehendes Wireformat. Der Slice-4B-Handler und die JSON-Routen sind nicht
registriert oder extern ausgeliefert; daher ist die Korrektur Teil des noch
nicht aktivierten initialen `v=1`-Vertrags, keine Clientmigration. Ein später
aktiviertes anderes Format wäre eine neue Wire-/Kompatibilitätsentscheidung.

## 3. Hardening – nicht R1-MUST

Die folgenden Eigenschaften sind mit diesem DTO-Vertrag nicht automatisch
Pflicht:

- **Duplicate-Member-Ablehnung:** RFC 8259 sagt, Objektnamen sollten eindeutig
  sein und beschreibt bei Duplikaten unterschiedliche Empfängergebnisse.
  Für diese interne, noch nicht komponierte Route wird die strikte Ablehnung
  jedoch als Hardening eingeordnet: es gibt genau einen fest gepinnten Parser,
  einen validierten typisierten DTO-Konsumenten, keine zweite JSON-Auswertung,
  keinen Signatur-/Canonicalization-Vertrag und der Replay-Schutz bindet die
  Originalbytes. Der gepinnte Parser muss ein deterministisches Ergebnis
  liefern; jedes daraus entstehende Feld wird vollständig nach Schema, Typ,
  Allowlist, Revision und Wertebereich validiert. Es wird kein
  parserübergreifender Interpretationspfad eingeführt. Eine spätere
  Anforderung mit mehreren Parsern, signierten JSON-Bytes oder einem
  Intermediär, der Objekte erneut auswertet, würde eine erneute MUST-Prüfung
  verlangen.
- **Generische UTF-8-Validierung vor dem Parser:** Inbound-DTO-Text besteht
  derzeit nur aus ASCII-Allowlistwerten und kanonischen ASCII-Programm-IDs.
  Nicht passende Bytes in tatsächlichen Textfeldern scheitern an diesen
  Feldprüfungen; unbekannte Schlüssel scheitern am geschlossenen Schema.
  Netzwerk-JSON folgt UTF-8; ein zusätzlicher parserweiter UTF-8-Scanner ist
  trotzdem kein separates MUST, weil kein nicht-ASCII-String die geschlossene
  Inbound-Allowlist passieren kann. Gültige JSON-Syntax und die konkrete
  Feldvalidierung bleiben MUST.
- **Generische Escape-/Control-Policy:** JSON-Syntax selbst muss gültig sein,
  einschliesslich korrekter Escapes und des Verbots unescaped Controls.
  Darüber hinaus gibt es keine pauschale Anwendungsregel gegen alle Escapes
  oder sämtliche Unicode-/Control-Codepoints. Die tatsächlich akzeptierten
  Textwerte bleiben durch ASCII-Allowlist und ID-Validator beschränkt.

Die bisher vier auf strikte Duplicate-Ablehnung gerichteten Regressionen
(Root, Revision, Intent, Nested Candidate) bleiben als dokumentierte
Messvektoren erhalten. Sie sind nach dieser Revision kein R1-MUST-Gate mehr.
Die historischen Testergebnisse werden nicht umgeschrieben. Der separat
abgeschlossene cJSON-Kandidatenspike ergänzt nur den bounded NUL-Gate und
aktuelle ASCII-/ID-Akzeptanzbelege; Duplicate-/UTF-8-Verhalten bleibt
Hardening-Evidence statt Vertragsblocker.

## 4. Kandidatenvergleich gegen denselben Vertrag

| Kandidat | R1-MUST-Fit und konkrete Delta | Hardening-/Restpunkt | Neubewertung |
|---|---|---|---|
| ArduinoJson 7.4.3 | Bereits vorhandener konkreter Codec; bestehende Tests belegen exakte interne `uint64_t`-Verarbeitung. `JsonString::size()` erlaubt längenbewusste Prüfung von embedded NUL. Nötiges R1-Delta: `u`/`c` dezimale Strings, Überlauf-Parser, kanonische ID-Validierung und bestehende Bounds/Typ-/Finite-Checks als Gate. | Doppelte Member werden deterministisch last-value-wins zusammengeführt; die öffentliche API bietet keinen Event-Hook zur Duplicate-Ablehnung. Das ist unter dem revidierten Vertrag Hardening, kein Blocker. | `PASS_CANDIDATE_FOR_R1_MUST_WITH_SMALL_CODEC_DELTA` |
| Espressif cJSON 1.7.19~2 | Vorherige Host-/ESP-IDF-6.1-/ESP32-/C++17-Evidence bleibt erhalten; der neue isolierte Probe-Gate ist Host und ESP-IDF-Build PASS. Dezimalstring-Revisionen beseitigen die `double`-`uint64_t`-Kollision im Wirevertrag; `isfinite()` und Wertebereiche bleiben Codecvalidierung. Nach cJSON-Parse wird der bestehende Program-ID-Validator benutzt. | Der bounded NUL-Gate prüft nach der 480-Byte-Grenze rohe NUL-Bytes und die sechs Bytes `\\u0000`. Er verwendet weder Tokenizing noch JSON-Zustand; die Probe akzeptiert gültige ASCII-Intent-/Enum-Beispiele und kanonische Program-IDs. Duplicate-/UTF-8-Verhalten bleibt Hardening. | `PASS_CANDIDATE_FOR_R1_MUST` |

Der bisherige cJSON-Spike hat `uint64`-Kollision, NUL-Verhalten und
invalid-UTF-8-Akzeptanz unter dem früheren strengeren Vertrag gemessen; diese
historischen Ergebnisse bleiben unverändert. Die aktuelle Planrevision
ergänzt die begrenzte Suche nach rohem NUL und `\\u0000` und belegt mit dem
aktuellen ASCII-Intent-/Enum-Sample sowie dem vorhandenen
`validateLowercaseIdentifier()` die Fortgeltung erlaubter Werte. Der Gate
scannt höchstens 480 Bytes und hat weder Tokenisierung noch String-, Escape-
oder Strukturzustand. Die Probe zeigt separat, dass cJSON escaped NUL weiter
dekodiert; die Eingangsschranke lehnt diese Form vor dem Parser ab.

Der gleichwertige Kandidatenvergleich lautet:

| Kriterium | ArduinoJson 7.4.3 | Espressif cJSON 1.7.19~2 |
|---|---|---|
| Correctness | R1-MUST erfüllbar; Dezimalstring-/Overflow- und Feldvalidierung bleiben lokales Codecdelta. Länge-bewusste NUL-Prüfung ist über die öffentliche String-API möglich. | R1-MUST nach bestandenem bounded NUL-Gate erfüllbar; R1-Wire verwendet Dezimalstrings. Endlichkeit, Wertebereiche und Projektvalidator bleiben nachgelagert. |
| KISS / eigener Code | Vorhandener ungemergter Codec und Tests senken die unmittelbaren Änderungskosten; das ist nur ein kleiner Migrationsfaktor. | Zusatzcode ist ein begrenzter 480-Byte-Vorfilter ohne JSON-Parserlogik; weder Lexer noch generische JSON-Abstraktion. |
| Ressourcen / Tests | Vorhandene Native- und ESP-IDF-Profilevidence; kein integrierter Vier-Session/no-PSRAM-Vergleich. | Native-Probe und ESP-IDF-6.1-ESP32-Targetbuild PASS; Mutation 341/480 B, Responses 346/247/3048 von 3072 B. Kein integrierter Heap-/Runtimevergleich, daher kein Ressourcen-Sieger. |
| Wartung / Toolchain | Exakt gepinnt, MIT, mit dem bestehenden Projektcodec getestet; ESP-IDF-kompatibel, aber kein Espressif-Registry-Paket. | Offizielle Espressif-Komponente und MIT; exakt gepinnt und direkt mit ESP-IDF 6.1 sowie C++17-Consumer gebaut. |
| ESP-IDF-Wiederverwendung | Breiter, nicht auf ESP-IDF beschränkter Einsatz und daher portabel. | Espressif-Herkunft und Component-Manager-Paket sind ein legitimer langfristiger ESP-IDF-Plattformvorteil, aber kein automatischer Sieger. |
| Vorhandener ungemergter Code | Bestehender Codec ist ein kleiner Wechselkosten-Vorteil, nicht ausschlaggebend. | Ein begrenzter Codecwechsel wäre nötig; keine gemeinsame Plattform-/Provider-Abstraktion erforderlich. |

Damit gibt es keinen eindeutigen Sieger aus R1-Correctness oder vergleichbarer
Ressourcenevidence. Die Empfehlung bleibt deshalb Ownerauswahl nach
Independent Plan Fix Verification; Espressif-Herkunft ist ein legitimer
Tie-Breaker, der ArduinoJson-Bestand nur ein kleiner Migrationskostenfaktor.

```text
R1_JSON_MUST=HARD_BOUNDS_480_3072;VALID_COMPLETE_JSON;CLOSED_VERSIONED_SCHEMA;REQUIRED_FIELDS;EXACT_TYPES;FINITE_RANGE_CHECKED_NUMBERS;EXACT_REVISION_CONFLICTS;CANONICAL_ACTUAL_TEXT_AND_IDS;MISSING_UNTRUSTED_EXPLICIT;DETERMINISTIC_SINGLE_DECODE;NO_SECRETS;CODEC_LOCAL_LIBRARY_TYPES;EXACT_BODY_REPLAY_FINGERPRINT
R1_JSON_HARDENING=DUPLICATE_MEMBER_REJECTION;GENERIC_UTF8_REJECTION_FOR_ASCII_ONLY_FIELDS;GENERIC_ESCAPE_CONTROL_POLICY_BEYOND_JSON_SYNTAX_AND_REAL_FIELDS
UINT64_WEB_REPRESENTATION=DECIMAL_STRING
VALID_RANGE=1..18446744073709551615
ABSENT_OPTIONAL_FIELD=NO_EXPECTED_REVISION
"0"=INVALID
ARDUINOJSON_REASSESSMENT=PASS_CANDIDATE_FOR_R1_MUST_WITH_SMALL_CODEC_DELTA
CJSON_REASSESSMENT=PASS_CANDIDATE_FOR_R1_MUST
RECOMMENDED_CANDIDATE=OWNER_SELECTION_AFTER_FAIR_COMPARISON
RECOMMENDATION_REASON=NO_CLEAR_MUST_OR_RESOURCE_WINNER;ESPRESSIF_ORIGIN_VALID_TIEBREAKER;EXISTING_CODEC_SMALL_COST_ONLY
FINAL_LIBRARY_SELECTION=OWNER_PENDING
```

Beide Kandidaten stehen nun gegen denselben proportionalen R1-MUST-Vertrag.
Diese Kandidatenbewertung ist keine finale Produktauswahl und autorisiert
keinen Codecumbau. Beide Kandidaten werden nicht gleichzeitig als
Produktabhängigkeiten geführt.

Kandidatenprovenienz der vorhandenen Messungen: ArduinoJson 7.4.3 am Tag-Commit
`77771d3c07668e01d8f52acb03910c1110bb373f`; Espressif Component Registry
`espressif/cjson 1.7.19~2` am Komponentencommit
`1387cec28a9b40654be7892114bd7d26fcd3869c`, Upstream cJSON 1.7.19 am Commit
`b2890c8d76bbb64e710585ebc0a917196b9c67e7`. Paket-/Lizenzhashes und
unveränderte Rohmessungen bleiben in den verlinkten Auditdateien.

## 5. Umsetzung und Gate nach Planfreigabe

Nach unabhängiger Planprüfung und ausdrücklicher Freigabe dieses exakten
Plan-Commits:

1. Nach unabhängiger Plan-Fix-Verifikation trifft der Owner die finale
   Bibliotheksauswahl zwischen diesen beiden R1-tauglichen Kandidaten;
   Espressif-Herkunft ist ein legitimer Tie-Breaker, vorhandener ungemergter
   Code nur ein kleiner Migrationskostenfaktor. Keine dritte Bibliothek
   evaluieren.
2. Nach exakter Ownerfreigabe nur an der bestehenden privaten Codecgrenze das
   gewählte R1-MUST-Delta
   umsetzen. Keine Route in `main/` registrieren und keine produktive
   Composition.
3. Die vier Duplicate-Testvektoren behalten, aber als nichtblockierende
   Hardening-Evidence klassifizieren. Der R1-Testvertrag darf keine
   kandidatenspezifische Duplicate-Interpretation verlangen.
4. Native Tests für Dezimalformat/Overflow und die optionalen
   Revision-Grenzfälle ergänzen/ausführen:

   | JSON-Feldzustand | Ergebnis |
   |---|---|
   | `"0"` | reject |
   | `"00"` | reject |
   | `"01"` | reject |
   | `"1"` | accept |
   | `"18446744073709551615"` | accept |
   | `"18446744073709551616"` | reject |
   | Feld fehlt | `NO_EXPECTED_REVISION` |

   Dazu Revisionkonflikte ohne Mutation, Pflicht-/Zusatzfelder, Typen, finite
   und fachliche Zahlenranges, kanonische IDs/Texte, vollständigen
   JSON-Verbrauch, Raw-/Escaped-NUL soweit kandidatenspezifisch nötig,
   maximale konkrete DTOs, Replay-Bodybindung und alle Responsegrenzen testen.
5. Betroffene bestehende Route-/Session-/Application-Regressionen, beide
   ESP-IDF-Profile bei Produkt-/Dependency-Änderung, Builder Self-Check und
   `git diff --check` ausführen. Separate No-PSRAM-/Vier-Session-
   Ressourcenmessung bleibt zwingend vor produktiver Web-Mutation.

Bis dahin bleiben Auth-/Session-Composition, Application-Aufrufserialisierung,
die finale JSON-Auswahl, Produktcomposition und Vier-Session-/no-PSRAM-
Ressourcenevidence offen. Keine Touch-/#172-Arbeit, #171-Integration,
Hardware oder Flash in dieser Revision.

## Quellen

- Aktueller Codec und Schema: `lib/fermentation_app/src/web_json_codec.cpp`,
  `lib/fermentation_app/src/web_json_codec.hpp`,
  `lib/fermentation_app/src/configuration_text.cpp`.
- Vorherige reproduzierte Kandidatenmessungen:
  `docs/audits/COMPONENT_EVALUATIONS.md` und
  `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`.
- [RFC 8259 §4 – JSON object member names](https://www.rfc-editor.org/rfc/rfc8259#section-4)
  (`SHOULD`-Eindeutigkeit und unterschiedliche Empfängerreaktionen).
- [ECMAScript Number safety](https://tc39.es/ecma262/2025/#sec-number.max_safe_integer)
  (exakte Integergrenze `2^53-1`).
- [ArduinoJson 7 `JsonString`](https://arduinojson.org/v7/api/jsonstring/) und
  [`deserializeJson()`](https://arduinojson.org/v7/api/json/deserializejson/).
- [Espressif cJSON 1.7.19~2 Registry/API und Caveats](https://components.espressif.com/components/espressif/cjson/versions/1.7.19~2/readme)
  (öffentliche Struktur, Duplicate Members, NUL und UTF-8).
