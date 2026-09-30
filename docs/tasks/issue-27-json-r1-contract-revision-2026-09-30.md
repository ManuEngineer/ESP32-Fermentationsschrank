# Issue #27 – proportionale JSON-R1-Vertragsrevision

## Status und Zweck

Diese kleine Revision präzisiert ausschliesslich den JSON-R1-Vertrag für die
bereits softwareseitig vorhandenen, noch nicht produktiv komponierten Slice-4B-
DTOs. Sie trennt notwendige Korrektheit von zusätzlichem Parser-Hardening und
bewertet nur die bereits gemessenen Kandidaten ArduinoJson 7.4.3 und Espressif
cJSON 1.7.19~2 erneut. Die früheren Messungen bleiben unverändert; ihre
damaligen `FAIL_CANDIDATE`-Resultate werden nicht rückwirkend umgeschrieben,
sondern gegen den hier proportionalisierten Vertrag eingeordnet.

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
SUPERSEDES_PLAN_SHA=c7c5a5c06d3cc07b4f8bf3de7d3c39a07c8d97f4
PREVIOUS_APPROVED_PLAN_SHA=46e0ea470b307a34867e24337b63a9d38166d771
PLAN_STATUS=INDEPENDENT_PLAN_FIX_VERIFICATION_PENDING
OWNER_PLAN_APPROVAL=REQUIRED_AFTER_FIX_VERIFICATION
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCT_CODEC_OR_DEPENDENCY_CHANGE=NO
PRODUCT_COMPOSITION=NOT_STARTED
PRODUCT_TESTS=NOT_RUN_PLAN_ONLY
BUILDER_SELF_CHECK=NOT_RUN_PLAN_ONLY
CANDIDATE_REASSESSMENT=EXISTING_EVIDENCE_AND_CURRENT_SOURCE_ONLY
NEW_SPIKE_OR_BUILD=NOT_RUN
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
Dieser Plan-only-Schritt ändert weder die Tests noch deren historische
Spike-Ergebnisse; eine spätere Umsetzung muss sie nach Ownerfreigabe als
Hardening-Evidence statt als Vertragsblocker einordnen.

## 4. Kandidatenvergleich gegen denselben Vertrag

| Kandidat | R1-MUST-Fit und konkrete Delta | Hardening-/Restpunkt | Neubewertung |
|---|---|---|---|
| ArduinoJson 7.4.3 | Bereits vorhandener konkreter Codec; bestehende Tests belegen exakte interne `uint64_t`-Verarbeitung. `JsonString::size()` erlaubt längenbewusste Prüfung von embedded NUL. Nötiges R1-Delta: `u`/`c` dezimale Strings, Überlauf-Parser, kanonische ID-Validierung und bestehende Bounds/Typ-/Finite-Checks als Gate. | Doppelte Member werden deterministisch last-value-wins zusammengeführt; die öffentliche API bietet keinen Event-Hook zur Duplicate-Ablehnung. Das ist unter dem revidierten Vertrag Hardening, kein Blocker. | `PASS_CANDIDATE_FOR_R1_MUST_WITH_SMALL_CODEC_DELTA` |
| Espressif cJSON 1.7.19~2 | Bereits gemessene ESP-IDF-6.1-/ESP32-/C++17- und Native-Builds sowie begrenzte konkrete DTO-/Response-Tests bleiben gültige Candidate-Evidence. Dezimalstring-Revisionen beseitigen die cJSON-`double`-`uint64_t`-Kollision; `isfinite()` plus fachliche Rangechecks sind normale Codecvalidierung. Programm-IDs werden durch denselben vorhandenen ASCII-Projektvalidator geprüft. | Die öffentliche Baumstruktur erhält Duplikate; `GetObjectItemCaseSensitive()` allein reicht nicht, ein öffentlicher begrenzter Baumdurchlauf kann sie erkennen, muss sie aber nicht ablehnen. Wegen C-String-NUL ist vor cJSON ein kleiner bounded Gate nötig: nach Body-Längenprüfung rohe `NUL`-Bytes sowie die JSON-Escapeform (Backslash, `u`, vier Nullziffern) ablehnen. Kein Lexer: kein erlaubtes R1-Feld kann NUL enthalten; die festen ASCII-Text-/ID-Gates lehnen sonstige nichtkanonische Feldwerte ab. Generische UTF-8-Ablehnung ist nicht nötig. | `CONDITIONAL_PASS_CANDIDATE_FOR_R1_MUST_WITH_BOUNDED_NUL_AND_FIELD_GATES` |

Der cJSON-Spike hat `uint64`-Kollision, NUL-Verhalten und invalid-UTF-8-
Akzeptanz unter dem früheren strikten Vertrag tatsächlich gemessen. Diese
Messungen werden nicht umbenannt: die neue Revision beseitigt die
`uint64`-Ursache durch das vorgegebene Wireformat und bindet NUL-/Textprüfung
an die realen ASCII-only Schemafelder. Für cJSON sind diese gezielten
Prüfungen bislang eine klar begrenzte Planauflage, keine bereits
implementierte oder erneut ausgeführte PASS-Evidence. Falls sie sich nicht
ohne zweiten Lexer fail-closed umsetzen lassen, ist cJSON für diesen Vertrag
nicht geeignet; kein Lexer wird ergänzt. Die begrenzte Suche nach der
NUL-Escapeform kann keine derzeit gültige Text-/ID-Eingabe ausschliessen, da
keines der geschlossenen ASCII-Felder einen Backslash zulässt. Bei späterem
Einführen freier Inbound-Texte wäre diese Annahme neu zu prüfen.

```text
R1_JSON_MUST=HARD_BOUNDS_480_3072;VALID_COMPLETE_JSON;CLOSED_VERSIONED_SCHEMA;REQUIRED_FIELDS;EXACT_TYPES;FINITE_RANGE_CHECKED_NUMBERS;EXACT_REVISION_CONFLICTS;CANONICAL_ACTUAL_TEXT_AND_IDS;MISSING_UNTRUSTED_EXPLICIT;DETERMINISTIC_SINGLE_DECODE;NO_SECRETS;CODEC_LOCAL_LIBRARY_TYPES;EXACT_BODY_REPLAY_FINGERPRINT
R1_JSON_HARDENING=DUPLICATE_MEMBER_REJECTION;GENERIC_UTF8_REJECTION_FOR_ASCII_ONLY_FIELDS;GENERIC_ESCAPE_CONTROL_POLICY_BEYOND_JSON_SYNTAX_AND_REAL_FIELDS
UINT64_WEB_REPRESENTATION=DECIMAL_STRING
VALID_RANGE=1..18446744073709551615
ABSENT_OPTIONAL_FIELD=NO_EXPECTED_REVISION
"0"=INVALID
ARDUINOJSON_REASSESSMENT=PASS_CANDIDATE_FOR_R1_MUST_WITH_SMALL_CODEC_DELTA
CJSON_REASSESSMENT=CONDITIONAL_PASS_CANDIDATE_FOR_R1_MUST_WITH_BOUNDED_NUL_AND_FIELD_GATES
RECOMMENDED_CANDIDATE=ArduinoJson_7.4.3
RECOMMENDATION_REASON=KISS_YAGNI_CORRECTNESS
FINAL_LIBRARY_SELECTION=OWNER_PENDING
```

Die Empfehlung ist eine Engineering-Empfehlung, keine finale Ownerauswahl.
ArduinoJson wird wegen der bereits vorhandenen Codecimplementierung und der
kleinsten korrekt begrenzten Änderung empfohlen. cJSON bleibt ein möglicher
Alternativkandidat nur unter den oben genannten kleinen Input-/Feld-Gates.
Beide Kandidaten werden nicht gleichzeitig als Produktabhängigkeiten geführt.

Kandidatenprovenienz der vorhandenen Messungen: ArduinoJson 7.4.3 am Tag-Commit
`77771d3c07668e01d8f52acb03910c1110bb373f`; Espressif Component Registry
`espressif/cjson 1.7.19~2` am Komponentencommit
`1387cec28a9b40654be7892114bd7d26fcd3869c`, Upstream cJSON 1.7.19 am Commit
`b2890c8d76bbb64e710585ebc0a917196b9c67e7`. Paket-/Lizenzhashes und
unveränderte Rohmessungen bleiben in den verlinkten Auditdateien.

## 5. Umsetzung und Gate nach Planfreigabe

Nach unabhängiger Planprüfung und ausdrücklicher Freigabe dieses exakten
Plan-Commits:

1. Ownerentscheidung für einen der beiden Kandidaten dokumentieren; weder
   automatisch die Empfehlung übernehmen noch eine dritte Bibliothek
   evaluieren.
2. Nur an der bestehenden privaten Codecgrenze das gewählte R1-MUST-Delta
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
