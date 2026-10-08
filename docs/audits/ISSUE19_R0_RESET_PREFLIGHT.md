# Issue #19 – R0-Vorpruefung Werksreset (lesend)

Basis: PR #191, Plan Revision 7 (Ownerfreigabe `6fbf1304566472c10b7cc52439475a5743960c0e`),
Abschnitt 4.5/R0. Nur Lesen und Dokumentieren; kein Produktcode, keine Hardware.
Jeder Befund nennt die Codestelle (Stand `main` `9beb68f` + PR-Branch).

## Ergebnis in Kurzform

| Pruefpunkt (Plan R0) | Befund | Folge |
|---|---|---|
| (a) lokale Service-PIN-Pruefung / PIN-geschuetzter Servicebereich | **Nicht vorhanden als Produktpfad.** `AuthenticationDomain::verifyServicePin` wird nur intern von `changeServicePin` aufgerufen (`authentication_records.cpp`); die Touch-Seite `FermentationUiPage::Pin` ist ein Platzhalter ohne PIN-Eingabe oder Pruefung (`fermentation_touch_workspace.cpp`); `device_platform::PinEntryModel` existiert, wird aber im Produkt nirgends verwendet | Ablauf A (normaler PIN-geschuetzter Reset) braucht eine neue lokale PIN-Pruefung gegen den vorhandenen Auth-Record-Pfad. Das ist die im Plan als **O-R2** vorgesehene Ownerentscheidung (Variante B: `PinEntryModel` + `verifyServicePin`, ohne eigenen Servicebereich) |
| (b) Resetkern ohne geladene Runtime / aus `SAFE_BOOT` | **Geteilt.** (1) `SAFE_BOOT` bei gueltiger Konfiguration (z. B. `RunPersistenceUntrusted`): `acquireRuntime()` gelingt, `storageEpoch_` ist gesetzt, `FermentationApplication::beginAuthorizedFactoryReset` ist nutzbar. (2) **Konfiguration ohne Runtime** (`NoRuntime`/`ResetEligibleNoRuntime`): `beginPersistent` kehrt nach fehlgeschlagenem `acquireRuntime()` zurueck, **bevor `storageEpoch_` gesetzt wird**; `beginAuthorizedFactoryReset` liefert dann `ConfigurationUnavailable`, obwohl `ConfigurationRecoveryService::beginAuthorizedFactoryReset` diesen Fall ausdruecklich unterstuetzt (`isResetEligibleNoRuntimeGraph`) | Fall (1) ist im Plan abgedeckt. Fall (2) erfordert eine Aenderung des Anwendungs-Reset-/Recovery-Vertrags (bisherige Epoche aus dem Bootstrap statt aus `storageEpoch_`, Laufpersistenz-Handoff ohne vorherige Runtime) und ist damit eine **nicht genehmigte Recoveryaenderung = Stoppbefund an den Owner**; keine Ersatzarchitektur |
| (c) Runstart betritt den `ApplicationCallSerializer` | **Ja.** Der bindende Start-/Bestaetigungspfad `applyConfirmedPrepared` und alle `prepare*`-Einstiege betreten `applicationCallSerializer_.enter()`; `beginAuthorizedFactoryReset` ebenso | Vorbedingungen des Resets koennen im selben Guard bindend geprueft werden |
| (d) "Ersteinrichtung" heute | Es gibt keinen eigenen Assistenten: nach dem Reset gilt der Factory-Default (Netzwerkmodus `UNSELECTED` -> `SelectionRequired`), die Netzwerk-Moduswahl ueber `FermentationUiPage::HeaderNetwork` und die lokale Web-Provisionierung (`openWebProvisioningWindow`) | Ersteinrichtung = vorhandene Pfade; kein neuer Assistent |
| (e) Netzwerk/HTTP | `EspIdfNetworkLifecycle::stopWifi()` ignoriert `esp_wifi_stop()`; `stop()` liefert immer `Applied`; `startWifi()` ist bei `wifiStarted_ == true` ein No-op-Erfolg, daher bleibt `start()` nach einem als fehlgeschlagen gemeldeten Stopp nutzbar. `IHttpServerLifecycle::stop()` ruft `httpd_stop()` und setzt den Handle vorher zurueck (`running()` beweist nichts). Alle bisherigen `stop()`-Aufrufer in `fermentation_application.cpp`/`network_configuration_service.cpp` verwerfen das Ergebnis. `FermentationApplication::applyNetworkMode` ruft `httpServerLifecycle_->stop()` bereits auf Fehlerpfaden **unter** dem Guard (bestehendes, hier nicht angefasstes Muster; der Reset wird anders gebaut) | Plan 4.4a ist ohne neue Architektur darstellbar: Resetkern unter dem Guard, Netzwerk-/HTTP-Stopp danach ausserhalb |
| (f) Auth-Writes der PIN-Pruefung | `verifyServicePin` schreibt Fehlversuche/Sperre/Sequenz persistent; vorhandene Regressionen: `test_lockout_is_persisted_and_skips_kdf_while_active_and_after_reboot`, `test_credential_change_reports_denial_and_lockout_separately`, `test_other_credential_mutations_do_not_restart_lockout_duration` | unveraendert wiederzuverwenden |
| (g) Status "Resetgrenze ueberschritten" | Nach `FactoryResetCompleted` liefert der Anwendungseinstieg entweder `FactoryResetCompleted` oder `RunPersistenceHandoffUnavailable`; alle Fehler **vor** der Grenze liefern andere Status (`ConfigurationUnavailable`, `ConfigurationMutationBusy`, `StateTransitionRejected`, `CounterOverflow` u. a.) | die Statusmenge {`FactoryResetCompleted`, `RunPersistenceHandoffUnavailable`} kennzeichnet die ueberschrittene Grenze |

## Stoppbefunde und Ownerentscheide

1. **Stoppbefund S1 – Reset ohne geladene Runtime (Fall 2).** Nicht umgesetzt. Der Ablauf zeigt
   dort ehrlich "Werksreset nicht verfuegbar" (fail-closed). Eine Umsetzung braucht eine
   Aenderung des Anwendungs-Reset-/Recovery-Vertrags (Owner).
2. **O-R2 – lokale PIN-Pruefung fuer Ablauf A.** Nicht umgesetzt, bis der Owner O-R2 entscheidet
   (Plan empfiehlt Variante B). Der Ablauf-Zustandsautomat erhaelt die PIN-Pruefung ueber eine
   Eingabe "PIN verifiziert", ohne die Pruefung selbst zu implementieren.

Die uebrigen Punkte (Ablauf B mit beiden Zugaengen, Netzwerk-/HTTP-Sequenz, ehrliche
`esp_wifi_stop()`-Rueckgabe, Guard-Vorbedingungen) sind von diesen Befunden nicht betroffen.

## Nachtrag zur Umsetzung (R1–R3)

- **Zusatzbefund (R0-h):** Die Touch-Seite `Pin` war im Produkt unerreichbar: der
  Service-Bereich hat keinen Produzenten fuer `service.available` (immer `false`),
  und der `pin`-Slot der Service-Seite war an `service.available` gebunden. Fuer
  O-R1 = B+ ist der `pin`-Slot jetzt immer aktiv; die PIN-Seite bleibt ein
  Platzhalter ohne PIN-Pruefung und bietet nur "PIN vergessen?". Der Service-Bereich
  selbst bleibt gesperrt.
- **SAFE_BOOT-Eintrag:** Auf der Startseite (`Recovery`/`Restricted`, Zustand
  `SafeBoot`) ersetzt der Eintrag "Werksreset" den bisher deaktivierten
  "Recovery"-Slot, solange der Ablauf verfuegbar ist; `PersistentFactoryReset`
  wird dann nicht mehr als nicht verfuegbar gelistet.
- **Stoppbefund S1 und O-R2** bleiben offen und sind nicht umgangen: ohne geladene
  Runtime wird der Ablauf nicht angeboten; Variante A wird nicht angeboten.
- Befund (e) bestaetigt: Netzwerk-`stop()` meldet im Adapter nun `Failed`, wenn
  `esp_wifi_stop()` bei gestartetem Treiber einen anderen Fehler als `ESP_OK`/
  `ESP_ERR_WIFI_NOT_INIT` liefert; `start()` nach einem solchen Stopp bleibt nutzbar
  (`startWifi()` ist bei `wifiStarted_ == true` ein Erfolg). Nicht nativ am echten
  Adapter beweisbar (`HW-19-R02`).

## Nachtrag S1 (Umsetzung nach Ownerfreigabe von Plan Revision 8 `aa665f1`)

Stoppbefund S1 ist per Ownerentscheid S1 = B aufgeloest und umgesetzt: der Reset
wird auch im vom Recoverykern als `ResetEligibleNoRuntime` zugelassenen Zustand
angeboten (Epoche aus dem geprueften Bootstrap, Eintrag auf der eingeschraenkten
Startseite, Ergebnis "Geraet aus- und wieder einschalten"). Nicht zugelassene
Zustaende bleiben ausgeschlossen. Nachweise: `ACCEPTANCE_TESTS.md`
`SIM-19-S1-01..06`; Hardware weiter `NOT_RUN` (#192).

Nachbesserung nach Implementierungsreview: die Zulassung schliesst zusaetzlich einen offenen Run-Epochen-Handoff (`Pending`/`Committed`) aus (einmalige Boot-Auswertung, kein Scan pro UI-Tick); `SIM-19-S1-02/03` siehe `ACCEPTANCE_TESTS.md`.
