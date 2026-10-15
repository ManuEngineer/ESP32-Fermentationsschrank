# G1b OBS1 – Versuch endete im Testboot: `uart-unexpected-reset` (Werkzeug-Artefakt, kein Lastlauf)

Freigabe: Owner, genau ein OBS1-Lauf im Beobachtungsmodus (`G1B_OBSERVE_GATE.md` @ `b034577568d0d6cfac7a6b6c52aad0262a8bda20`, Werkzeugcommit `373bafb948d15f25cb8668981c53bf905003ae99`); Beisein, Aktortrennung und unveränderter Gerätezustand seit E-FB vom Owner vor dem Start ausdrücklich bestätigt. H0 bestanden. Ein Port-Öffnungsreset (Testboot, 09:53:17 UTC), kein Flash/esptool. **Es wurde weder der Mitschnitt gestartet noch `START_LOAD` angelegt; es gab keine Last und keine Messung.** Nach dem automatischen Abbruch keine weitere Geräteaktion. Rohdaten bleiben privat; Evidence unter `diag/review/g1b/stage_obs1/`. `OBS1=HOLD_NO_MEASUREMENT` · `OBS1_HARDWARE=DONE_HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.

## Befund (belegt)

| Punkt | Befund |
|---|---|
| Abbruch | `capture.json`: `failure = uart-unexpected-reset`, `status FAIL/HOLD` ≈ 5 s nach dem Öffnen; `source_proven`/`ready` noch `false`, `expected_initial_reset_lines = 1` |
| Ursache im Werkzeug | Die ersten 76 UART-Zeilen sind **eine** abgeschnittene Heartbeat-Zeile (`p_main: heartbeat … uptime_ms=3209930`), 76-fach identisch, **vor** dem ersten Resetmarker `rst:0x1 (POWERON_RESET)`. Der Supervisor wertet jeden Resetmarker als unerwartet, wenn zuvor schon ein Heartbeat gesehen wurde (`last_uptime` gesetzt, `g1_capture.py`). Der Resetmarker ist der **erwartete Reset durch die Portöffnung** (bekanntes FT232R-Verhalten); er wurde fälschlich als unerwartet gewertet. |
| Herkunft der Altdaten | Die Heartbeat-Zeile stammt aus dem Puffer der Schnittstelle von **vor** dem Reset. Uptime 3 209 930 ms = **53,5 min** = Zeit zwischen dem B2-Testboot (08:59:45 UTC) und der Portöffnung (09:53:17 UTC): das Gerät lief seit B2 **ununterbrochen**. Das stützt die Ownerbestätigung „keine Änderung seit E-FB“. In B1 und B2 traten solche Altdaten vor dem Reset nicht auf. Warum sie hier auftraten, ist **nicht belegt**. |
| Gerät nach dem Reset | normal (`uart_tail.log`, 5 s Nachlauf): `profile: esp32_release`, Quelle `31c3ee48…`, `tcp tx win: 2880`, `real actuators: disabled`, `application: ready`, `HomeConnected`; kein `heap_alloc_failed`, kein Panic |
| Aktueller Zustand | Gerät läuft mit Image B im Leerlauf (frisch gebootet); kein Capture/Pcap/Lasttreiber aktiv |

Das ist **kein** Befund zu O1 oder zur Stillstandsphase und **kein** Ergebnis der Klassen O-A bis O-D; die Klasse O-D (anderes) trifft formal zu, ohne Aussage über das Gerät. Der Testboot-Reset des Gates ist verbraucht; ein neuer Versuch bräuchte ein neues Gate (neuer Reset).

## Minimaler Korrekturvorschlag (nur Vorschlag, nichts umgesetzt)

Ausschliesslich in `g1_capture.py`: Der **erste** Resetmarker innerhalb der ersten 10 s nach dem Öffnen gilt als der erwartete Portöffnungsreset; Heartbeat-/Uptime-Zustand vor diesem Marker (Altdaten) wird dabei **verworfen** (`last_uptime = None`, `last_heartbeat` neu). Jeder weitere Resetmarker, ein Resetmarker nach den ersten 10 s oder ein Uptime-Rückgang **nach** dem erwarteten Reset bleiben unveränderte Hard-Stops (auch im Beobachtungsmodus). Pre-Reset-Zeilen bleiben unverändert im Log, es wird nichts verworfen. Hosttests: Altdaten vor dem Reset werden toleriert, ein zweiter Reset und ein Reset nach 10 s stoppen, Uptime-Regression nach dem Reset stoppt, fehlende Resetmarker (kein Reset beim Öffnen) behalten das bestehende Verhalten. Danach neue Werkzeughashes, Manifest und ein neues Gate (neuer Reset, ein Lauf). Keine Firmware-, Image- oder Konfigurationsänderung.

**Keine Umsetzung, kein weiterer Lauf, kein Reset, kein Flash ohne neue Ownerentscheidung.**
