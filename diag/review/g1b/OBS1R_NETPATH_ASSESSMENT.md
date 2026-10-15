# OBS1R – einmalige Offline-Netzpfadprüfung und Entscheidungsvorlage

Basis: `OBS1R_RESULT.md` @ `5a1ad29f19383f3ce946f7664e4b6bf95dab22ee`; vorhandene Evidence (R1, R1b, B1, B2, OBS1R: Pcap, UART, `load.json`), Image-Konfiguration (aufgelöstes `sdkconfig`), ESP-IDF-6.1-Quelltext. **Nur Offline-Analyse und Dokumentation**: keine Produkt- oder Werkzeugänderung, keine Portöffnung, kein Reset/Flash/Hardwarelauf, keine neue Berechtigung. `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `ACTUATOR_RELEASE=NO`.

## 1. Befunde (belegt) getrennt von Hypothesen

| # | Befund (Quelle) | Aussage |
|---|---|---|
| F1 | OBS1R: Stillstandsphase beginnt bei Lastsekunde ≈ 115 bei grösstem freiem Block 38 912 B; kein Shell-Handler zwischen 100,8 und 127,8 s; Hauptloop-Heartbeats laufen weiter (Lücke ≤ 1,3 s); Geräte-Paketrate fällt von 13–18/s auf 1–9/s, danach Nachholsalve (33/s bei 126 s). | Auslöser ist weder Speichermangel noch eine laufende Shell-Antwort (Head-of-Line). Das Gerät war gedrosselt, nicht dunkel. |
| **F2** | **Das Gerät bestätigt Anfragen im Stillstand selbst spät** (Pcap, je Verbindung, erstes Gerätepaket – ACK oder Daten – nach der ersten Anfragenpaket): OBS1R vor dem Stillstand (100–114 s) p50 167 ms / p95 356 ms (n = 34); **im Stillstand (115–131 s) p50 792 ms / p95 3 024 ms (n = 29)**; Antwort-Daten p50 883 / p95 3 049 ms; **5 Host-Retransmits von Anfragedaten**. B1 (755–762 s): ACK/erstes Paket p50 637 / p95 2 021 ms, 3 Host-Retransmits (vorher p50 87 ms). | Die Verzögerung liegt **nicht in der Anwendung/`esp_http_server`** (die ACK der Anfrage erzeugt lwIP, nicht der HTTP-Task): Empfang der Host-Frames, Wi-Fi-RX-Pfad oder `tcpip`-Task. **B2 weicht ab:** dort kam das erste Gerätepaket zügig (p50 317 ms), die Antwort blieb mitten im Senden stehen (7 200 von ≈ 8 155 B) – ein Sendepfad-Ereignis. |
| F3 | SYN/ACK-Verzögerungen bis 5,0 s, 11 Verbindungen ohne SYN/ACK; kleine 339-B-Antwort braucht 6,8 s (OBS1R). | Auch die von lwIP erzeugte SYN/ACK ist verzögert: **unterhalb** der Anwendung. |
| F4 | **R1 (Leerlauf):** erstes Login nach 3 min Idle, SYN/ACK nach 55,6 ms, danach Login-Daten ≥ 4 s unbestätigt (Host-Retransmits 0,32/0,86/1,92/4,10 s); freier Heap 41 KB, keine Last, keine Shell. | Gleiche Symptomklasse **ohne Last und ohne Speicherdruck**: Das Phänomen ist nicht lastgebunden. |
| F5 | Im Teilstück vor dem Stillstand ist die Qualität aus Hostsicht in allen vier Pcap-Läufen praktisch gleich und gut (Lastsekunden 5–100, je 243 Verbindungen): SYN-Wiederholungen 0 % (R1b, B1, OBS1R), 0,4 % (B2); SYN→SYN/ACK p50 82–101 ms, p95 135–222 ms; Anfrage→erste Daten p95 141–357 ms. | Der Stillstand setzt **abrupt** ein, ohne vorherige Verschlechterung (aus Hostsicht). |
| F6 | Device-Log aller Läufe: Kanal 1, BW20, WPA2, **gleicher AP**, RSSI beim Verbinden −44 / −48 / −49 / −51 / −49 / −50 dBm (G1, R1, R1b, B1, B2, OBS1R; schwach driftend, alle gut); in keinem Lauf nach dem Boot ein Trennungs-/Neuverbindungs-/Beacon-Timeout-Eintrag (nur die drei Boot-Zustandszeilen; Wi-Fi-Log auf INFO); Block-Ack-Sitzungen wechseln normal (`ADDBA/DELBA`). | **Keine** beobachtete Reassoziation oder Roaming; ein Funkausfall mit Reassoziation ist ausgeschlossen, **Funkstörungen ohne Reassoziation sind nicht ausgeschlossen**. RSSI nur beim Verbinden, nie zur Laufzeit gemessen. |
| F7 | Host: Ethernet `enp2s0` (kabelgebunden), RX/TX-Fehler und Drops 0, `carrier_changes` 2 (seit Hoststart), keine Hostlast-Indikatoren aus den Läufen; Pcap-Zeitbasis und Lasttreiber stimmen auf 0,2 ms überein. | Der Host-Link ist unauffällig; die einzige drahtlose Teilstrecke ist AP ↔ Gerät. |

**Nicht mit vorhandenen, zugänglichen Daten belegbar (`NOT_PROVEN`):** Zustand und Logs von Router/AP (Retries, Airtime, Powersave-Pufferung, DTIM/TIM, Kanalauslastung, Reassoziation, andere Clients) – kein Zugriff; Host-Systemlog/`dmesg` für das OBS1R-Fenster – ohne zusätzliche Berechtigung nicht lesbar (`journalctl`: keine Einträge sichtbar, `dmesg`: nicht erlaubt), es werden keine Rechte angefordert; Gerätezähler des Wi-Fi-Treibers oder von lwIP (`CONFIG_LWIP_STATS` aus; keine Instrumentierung); Funkumgebung zur Messzeit; CPU-Auslastung des Geräts. Daraus folgt: **Eine Ursache auf AP, Funkweg, Wi-Fi-Treiber oder lwIP wird nicht behauptet; eine Zeitkorrelation allein begründet keine Kausalität.**

## 2. Tatsächliche Konfiguration (aufgelöst, belegt)

| Punkt | Wert / Quelle |
|---|---|
| Powersave | Das Produkt ruft **kein** `esp_wifi_set_ps` auf (Quellsuche `lib/`, `main/`); damit gilt der ESP-IDF-Default `WIFI_PS_MIN_MODEM` (`esp_wifi.h`: „Default power save type is WIFI_PS_MIN_MODEM“); Boot-Log aller Läufe: `pm start, type: 1`, `dp: 1`, Beacon-Intervall 102 400 µs, `li: 3` (Listen-Intervall 3 Beacons ≈ 307 ms, „scale … from 307200 us to 307200 us“). Wartezeit bis zur Zustellung eines gepufferten Frames im Normalfall höchstens im Bereich von 0,1–0,3 s – Sekunden-Stillstände sind damit **nicht** die Regel-Latenz der Betriebsart. |
| Verbindungsparameter | Station, Kanal 1, BW20, WPA2-PSK, Sicherheitsmodus aus dem Boot-Log; Produktcode setzt Station-Konfiguration über `esp_wifi_set_config` (`esp_idf_network_lifecycle.cpp`), keine eigene Scan-/Sortier-/Schwellenlogik gefunden; `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=y`, `CONFIG_PM_ENABLE` aus (kein Light-Sleep/DFS), CPU 160 MHz, `CONFIG_FREERTOS_HZ=100`. |
| Wi-Fi/Treiber | `CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0`, `ESP_WIFI_IRAM_OPT`/`RX_IRAM_OPT` an, AMPDU RX/TX an (BA-Fenster 6), RX-Puffer 10 statisch + 32 dynamisch, TX 32 dynamisch, `ESP_COEX_ENABLED=y` (Koexistenz-Software, kein Bluetooth). |
| Task-Zuordnung | Hauptaufgabe `CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0` (Priorität niedrig, `ESP_TASK_MAIN_PRIO`), `esp_timer`-Task Kern 0; lwIP-`tcpip`-Task Priorität **18**, ohne Kernbindung (`0x7FFFFFFF`), `CONFIG_LWIP_TCPIP_CORE_LOCKING` aus; `esp_http_server` Priorität 5 (Default, `HTTPD_DEFAULT_CONFIG`); LVGL-Task Priorität 4, ohne Kernbindung. |
| lwIP/HTTPD | `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880` (O1, B-Image), `TCP_WND 5760`, `MAX_ACTIVE_TCP 16`, `TCP_ACCEPTMBOX_SIZE 6`, `LWIP_ICMP=y`; `esp_http_server`: 7 Sitzungen, Backlog 5, `lru_purge` aus (unverändert). |

**Auswirkung auf die Hypothesen (belegbare Schlüsse, keine Ursache):** Der Hauptloop (niedrige Priorität, Kern 0) lief in der Stillstandsphase **weiter** (Heartbeats ohne Lücke). Eine CPU-Aushungerung durch einen höherpriorisierten Task auf Kern 0 würde zuerst diesen niedrig priorisierten Task treffen – das ist **nicht** beobachtet und macht eine reine CPU-Blockade des Geräts **unwahrscheinlicher**; eine **Sperre/Blockade im `tcpip`-Task ohne CPU-Last** oder ein **Zustellproblem davor** ist damit nicht ausgeschlossen.

## 3. Hypothesen mit Vorhersagen (keine behauptet)

| Hyp. | Beschreibung | Vorhersage für einen Leerlauf-ICMP-Test (Abschnitt 4) |
|---|---|---|
| **H-A** AP/Funkweg | Airtime, Retries, Interferenz oder AP-seitige Pufferung (inkl. Powersave-Zustellung) verzögern Frames zum Gerät | Verzögerungen/Verluste auch im Leerlauf, zufällig verteilt; zeitgleich in AP-Logs erkennbar |
| **H-B** Geräte-Wi-Fi/PS | Das Gerät verpasst im `WIFI_PS_MIN_MODEM` die Zustellung (TIM/Beacon, Listen-Intervall 3) oder der Treiber verzögert RX/TX | Verzögerungen vor allem beim **ersten Kontakt nach Leerlauf** (R1-Muster, F4), im dichten Betrieb seltener |
| **H-C** Geräte-lwIP/`tcpip` | `tcpip`-Task/Mbox/Sperre verzögert Eingang und SYN/ACK **lastabhängig** | Leerlauf **unauffällig**; Verzögerungen erst unter HTTP-Last; ICMP unter Last mit betroffen |
| **H-D** CPU-Aushungerung | höherpriorisierter Task blockiert | durch F1/Heartbeats **unwahrscheinlich** |

F4 (Stillstand-Muster im Leerlauf, R1) spricht gegen eine rein lastabhängige Erklärung (H-C allein) und für H-A/H-B; F2/F3 sprechen gegen Anwendung/HOL als Auslöser. Eine Unterscheidung **H-A gegen H-B** ist mit den vorhandenen Daten **nicht** möglich.

## 4. Ein einziges priorisiertes nächstes Experiment (Empfehlung)

**Leerlauf-ICMP-Langzeitprobe auf das unveränderte Gerät (Image B), ausschliesslich hostseitig – keine Firmware-/Konfigurations-/Werkzeugänderung, kein Reset, kein Flash, keine Last, kein HTTP.** Standardwerkzeug `ping` mit Zeitstempel (`-D`), vorhandene lwIP-ICMP-Funktion (`LWIP_ICMP=y`), Host-Pcap kann zeitgleich (Header-only) mitlaufen. Ablauf in zwei Phasen:

1. **Phase 1 – Erstkontakt nach Leerlauf (R1-Muster):** alle **180 s** ein Echo-Burst von 3 Paketen (20 Ereignisse in 60 min; bei der beobachteten Rate eines R1-artigen Ereignisses auf etwa sieben Erstkontakte werden ≈ 3 Ereignisse erwartet).
2. **Phase 2 – dichte Probe:** **5 Echo/s** über 30 min (1 800 s) zur Erfassung von Verlust- und Verzögerungsstössen.

Auswertung: Antwortzeit und Verlust je Paket, Stösse ≥ 1 s, Erstkontaktverteilung; Korrelation nur mit Zeitpunkten, die der Owner aus AP-Logs liefert.

| Ergebnis | Folge |
|---|---|
| **Phase 1 zeigt Erstkontakt-Verzögerungen/Verluste ≥ 1 s (R1-Muster)** | Last-unabhängiges Zustellproblem **vor** lwIP/HTTP bestätigt (H-A/H-B); nächstes Experiment: **eine** Einflussgrösse am Gerät – `WIFI_PS_NONE` (`esp_wifi_set_ps`, bestehende ESP-IDF-Funktion) mit derselben Probe |
| **Nur Phase 2 zeigt Stösse (zufällig, ohne Erstkontaktbezug)** | eher AP/Funkweg (H-A); Eigentümer-AP-Logs/Kanalwechsel prüfen, bevor Firmware angefasst wird |
| **Beide Phasen sauber** | Last-gebunden (H-C-Richtung); dann ein **einziger** Beobachtungslauf mit paralleler ICMP-Probe (zeigt, ob ICMP während eines TCP-Stillstands mitverzögert wird) |

Unterscheidungskraft: gut für „lastunabhängig gegenüber lastgebunden“ und für R1-artige Erstkontakt-Ereignisse; **nicht** trennbar: AP gegenüber Wi-Fi-Treiber (dafür AP-Logs oder der PS-Folgeschritt). Aufwand: etwa 90 min Hostzeit, Auswertung offline; Risiko: sehr gering (≈ 5 Paket/s ICMP an ein aktorfreies Devboard, keine Zustandsänderung, kein Reset). **Keine** Änderung an Heap-, HTTPD- oder WLAN-Puffern, kein neues Diagnoseframework.

**Parallel ohne jedes Risiko (Owner, falls Daten vorhanden):** AP-/Controller-Export für das Gerät und die Läufe (UTC 04:4x, 07:5x, 09:0x, **10:22–10:27**): Verbindungs-/Reassoziations-Ereignisse, Sende-Retries, Airtime/Kanalauslastung, Signal/Datenrate, DTIM-Periode und Powersave-Pufferung, benachbarte Störer auf Kanal 1, Firmware. Diese Daten würden H-A direkt prüfen.

## 5. Entscheidungsvorlage

| Klasse | Inhalt |
|---|---|
| **BLOCKER** | `RES-01` bleibt Blocker; Ursache der Stillstandsphase offen; `O1_QUALIFICATION=NOT_PROVEN`; kein weiterer identischer A- oder B-Lauf und kein umfassendes A/B-Programm, solange kein diskriminierender Messzweck steht |
| **FOLLOW-UP** | (1) **Ownergate ICMP-Probe** (Abschnitt 4), host-only, 90 min, keine Reset-/Flash-/Lastaktion; (2) Owner exportiert AP-/Controller-Daten (nur lesend, Abschnitt 4); (3) bedingt: ein Einflussgrössen-Experiment `WIFI_PS_NONE`, nur nach Phase-1-Befund; (4) bedingt: Beobachtungslauf mit paralleler ICMP-Probe, nur bei sauberen Phasen |
| **NO-ACTION** | O1, HTTPD, WLAN-/lwIP-Puffer, Lastprofil und Werkzeuge bleiben unverändert; keine O2-/O3-Umsetzung; kein Rückflash |

**Konkretes nächstes Owner-Gate (Skizze, noch nicht freigegeben):** `NETPROBE1` – host-only: ICMP-Probe wie Abschnitt 4 gegen das unveränderte, aktorfreie Gerät; Werkzeug `ping`/optional Header-only-Pcap; **keine** Portöffnung, kein Reset, kein Flash, kein HTTP, keine Last; Stopp bei Gerätereset (UART nicht nötig: der Host beobachtet nur Netzantworten) oder Ownerabbruch; Evidence: abgeleitete Antwortzeit-/Verlustverteilung und Stösse mit UTC-Zeiten, keine Adressen. Freigabe, Beisein und aktorfreier Aufbau wie in früheren Gates; **keine** automatische Folgeaktion. F2-D4 Bewertungsgrösse, F2-D5 offen.
