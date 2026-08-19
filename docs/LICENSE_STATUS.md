# Vorlaeufiger Lizenzstatus

Status: `PROJECT_LICENSE_UNDECIDED`

## Entscheidung

Fuer die vom Projekt selbst erstellte Software, Dokumentation, Grafiken und
spaetere eigene Hardwareentwuerfe ist derzeit keine allgemeine Projektlizenz
festgelegt. Diese Offenheit ist eine bewusste vorlaeufige
Governance-Entscheidung und keine Open-Source-Freigabe.

Soweit fuer eine Datei oder einen Pfad nicht ausdruecklich etwas anderes
angegeben ist, behaelt der jeweilige Rechteinhaber alle Rechte am
projektverfassten Inhalt. Durch die oeffentliche Sichtbarkeit des Repositorys
erteilt der Projekteigentuemer keine allgemeine Erlaubnis zur Nutzung,
Veraenderung, Weitergabe, Veroeffentlichung oder kommerziellen Verwertung.
Rechte, die sich zwingend aus dem anwendbaren Recht oder aus den Bedingungen
der verwendeten Hostingplattform ergeben, bleiben unberuehrt.

Dieses Dokument ist keine Lizenz und erteilt selbst keine zusaetzlichen
Nutzungsrechte.

## Drittanbieterinhalte

Drittanbieterbibliotheken, Frameworkbestandteile, Herstellerunterlagen,
Referenzarchive und sonstige fremde Inhalte bleiben ausschliesslich unter ihren
jeweiligen Originallizenzen, Nutzungsbedingungen und Rechten. Dieser
vorlaeufige Projektstatus schraenkt diese Rechte weder ein noch erweitert oder
uebertraegt er sie auf andere Projektteile.

Die Herkunft und der jeweilige Lizenzstatus gepruefter Komponenten werden im
[`Third-Party-Komponentenregister`](THIRD_PARTY_COMPONENTS.md) sowie in den
dazugehoerigen Auditdokumenten gepflegt.

## Issue #90: konkret verwendeter ESP-IDF-Dateisatz

Der #90-Adapter verwendet keine kopierten Herstellerdateien. Er bindet gegen
die gepinnte ESP-IDF-Basis
`v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e` und nutzt daraus die
oeffentlichen NVS-/Partition-APIs; der private Hosttest nutzt zusaetzlich die
gepinnten BDL-/NVS-Implementierungsquellen. Fuer den Herkunfts-, Lizenz- und
Notice-Nachweis sind mindestens diese tatsaechlich entscheidenden Dateien und
deren mitgelieferte Lizenz-/Noticeabdeckung zu pruefen:

| Verwendung | ESP-IDF-Dateisatz | Dokumentationspflicht |
|---|---|---|
| Produktionsadapter | `components/nvs_flash/include/nvs.h`, `components/nvs_flash/include/nvs_flash.h`, `components/esp_partition/include/esp_partition.h` | Version/Commit, Apache-2.0 und die im ESP-IDF-Distributionsweg zugehoerigen Drittbestandteile/Notices erfassen |
| Herstellervertrag und Kapazitaet | `components/nvs_flash/src/nvs_api.cpp`, `components/nvs_flash/src/nvs_storage.cpp`, `components/nvs_flash/private_include/nvs_constants.h`, `components/nvs_flash/src/nvs_pagemanager.cpp`, `components/nvs_flash/include/nvs_flash.h` | die fuer `nvs_set_blob`, No-op-`nvs_commit`, Chunk-/Entry-/GC-/Erase- und BDL-Vertraege verwendeten Quellen auf dem exakten Commit referenzieren |
| Host-Recoverytest | `components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.cpp` sowie die zugehoerigen `esp_blockdev`-Header | BDL-Hosttest bleibt ein Testartefakt; Lizenz-/Noticeabdeckung des vollstaendigen tatsaechlich gebauten Komponentenimports pruefen |

Der Repository-Nachweis besteht aus dieser Tabelle, dem Third-Party-Register,
dem Auditregister und dem Release-/Hostartefakt-Notice-Check. Die #90-
Implementierung behauptet weder aktivierte NVS-Verschluesselung noch Schutz
gespeicherter Secrets; das separate Security-/Releasegate bleibt unveraendert.

## Projektbezeichnung

Bis zu einem ausdruecklichen spaeteren Lizenzentscheid wird das Projekt nicht
als Open-Source-Projekt oder als Open-Source-Hardware bezeichnet. Eine passende
Kurzbezeichnung ist:

> Public development repository – license decision pending

## Spaetere Entscheidung

Der Projekteigentuemer behaelt sich vor, projektverfasste Inhalte spaeter unter
einer ausdruecklich gewaehlten Lizenz zu veroeffentlichen. Als moegliche Option
bleibt insbesondere MPL-2.0 offen; dies ist jedoch keine Vorentscheidung.

Der Lizenzentscheid muss spaetestens vor einem der folgenden Schritte erneut
und separat getroffen werden:

- offizielle Veroeffentlichung einer Release-Firmware;
- Weitergabe von Geraeten mit installierter Projektfirmware;
- Annahme externer urheberrechtlich relevanter Beitraege;
- Veroeffentlichung eigener Fertigungs-, Platinen- oder CAD-Unterlagen;
- kommerzielle Zusammenarbeit oder Lizenzanfrage.

Eine spaetere Lizenzwahl erfolgt als eigener dokumentierter
Governance-Entscheid und nicht beilaufig in einem fachlichen
Implementierungs-PR.
