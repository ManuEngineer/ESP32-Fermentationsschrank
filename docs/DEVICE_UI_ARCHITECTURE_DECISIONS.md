# Device-UI-Architekturentscheidungen

**Projekt:** ESP32-Fermentationsschrank / wiederverwendbare Device Platform
**Status:** Vom Owner bestätigt; Roadmap- und Spezifikationsintegration in PR #80 umgesetzt, Ownerreview ausstehend
**Zweck:** Verbindliche Grundlage für die spätere Prüfung und Anpassung der Issues #25, #26 und #31 sowie für die Implementierungsplanung
**Hinweis:** Dieses Dokument enthält Architektur- und Produktentscheide. Es ist noch kein Implementierungsauftrag und greift der vorgesehenen Issue-Reihenfolge nicht vor.

---

## UI-01 – Feste Device-Shell

Die lokale Benutzeroberfläche verwendet eine feste Device-Shell.

Der Header bleibt auf allen normalen Ansichten sichtbar und enthält:

- konfiguriertes Logo und Branding
- Sprachwahl als kompakte Auswahl, initial `DE`, `EN`, `ES`
- WLAN-Status
- Uhrzeit

Der untere Navigationsbereich bleibt ebenfalls an einer festen Position, ist jedoch kontextabhängig.

Auf der Hauptseite enthält er die primären App- und Plattformbereiche. Auf Unterseiten wird er durch lokale Navigation, Tabs, Zurück und/oder Home ersetzt.

Jede normale Ansicht muss eine eindeutige Rückkehrmöglichkeit besitzen. Vollbilddarstellungen sind nur für ausdrücklich definierte Spezialabläufe zulässig, insbesondere Splashscreen, Touchkalibrierung, kritischer Recoverybildschirm sowie nicht unterbrechbare Sicherheits- oder Bestätigungsabläufe.

Der mittlere Arbeitsbereich wird vollständig für die jeweils aktive Anwendung oder den gewählten Plattformbereich genutzt.

## UI-02 – Navigation Home und Zurück

- Auf der ersten Navigationsebene unterhalb der Hauptansicht wird ein Home-Button angezeigt.
- Auf tieferen Ebenen wird dieser durch einen Zurück-Button ersetzt.
- Sobald der Benutzer wieder die erste Ebene unterhalb der Hauptansicht erreicht, wird erneut der Home-Button angezeigt.
- Zurück navigiert genau eine Ebene nach oben.
- Home führt direkt zur Hauptansicht der aktiven Anwendung.
- Ungespeicherte Änderungen müssen vor dem Verlassen bestätigt werden.
- Sicherheitskritische Abläufe dürfen durch Navigation nicht umgangen werden.

## UI-03 – Vier feste Navigationsslots und vertikales Blättern

- Immer genau vier feste Navigationsslots am unteren Bildschirmrand.
- Position und Breite bleiben unabhängig von der Belegung gleich.
- Nicht benötigte Slots bleiben sichtbar und leer.
- Andere Slots werden nicht verbreitert.
- Kein horizontales Scrollen.
- Kein freies Wischscrollen.
- Längere Listen und Informationsseiten werden mit sichtbaren Auf- und Ab-Schaltflächen am rechten Rand des Inhaltsbereichs navigiert.
- Die Pfeile bewegen bevorzugt seitenweise oder um vollständige Einträge.
- Bei geführten Abläufen dürfen die vier unteren Slots für Zurück, Vorherige, Weiter, Abbrechen oder Speichern verwendet werden.

## UI-04 – Status und Service als Erweiterungspunkte

Status und Service sind feste Bestandteile der Device-UI-Shell.

- Apps dürfen beide Bereiche um eigene Unterseiten erweitern.
- App-spezifische Statusinformationen gehören unter Status.
- App-spezifische Diagnose-, Kalibrier- und Wartungsfunktionen gehören unter Service.
- Plattformseiten dürfen nicht ersetzt oder entfernt werden.
- Erweiterungen werden über definierte, rendererunabhängige UI-Verträge registriert.
- Keine direkten Hardware-, Treiber- oder fremden Fachzugriffe.
- Plattformseiten werden vor App-Erweiterungen dargestellt.
- Schutz- und Freigaberegeln bleiben plattformseitig verbindlich.
- Eine fehlerhafte App darf Plattform-Status und Plattform-Service nicht unbrauchbar machen.

## UI-05 – Branding und Sprachpakete

Standardbranding: **ManuEngineer**.

Branding wird zentral und anwendungsunabhängig konfiguriert und nicht in Screens oder Fachlogik hart codiert.

Dazu gehören mindestens Markenname, Headerlogo, Bootlogo, Theme-Grundwerte sowie Web-Favicon/Web-Branding.

Die konkrete rendererunabhaengige R1-Definition fuer ManuEngineer-Assets,
Headergeometrie, Boot-Splash und semantische Theme-Tokens steht in
[`DEVICE_UI_VISUAL_DESIGN.md`](DEVICE_UI_VISUAL_DESIGN.md). Renderer, Zielformat
und reale Ressourcenpruefung bleiben davon unberuehrt.

Für Release 1 wird Branding zur Build-Zeit festgelegt. Die erzeugte Firmware besitzt ein festes Branding; die Plattform bleibt für andere Projekte konfigurierbar. Laufzeitänderbares Branding ist nicht Bestandteil von Release 1.

Initiale Sprachen:

- Deutsch
- Englisch
- Spanisch

Regeln:

- Alle Texte werden über stabile, sprachunabhängige Textschlüssel aufgelöst.
- Plattform und Anwendung besitzen getrennte Namensräume.
- Weitere Sprachpakete müssen ohne Änderung bestehender Screens oder Fachlogik ergänzt werden können.
- Jede Firmware legt zur Build-Zeit fest, welche Sprachpakete enthalten sind.
- Die aktive Sprache ist zur Laufzeit auswählbar und persistent gespeichert.
- Fehlende Übersetzungen fallen zuerst auf Englisch und danach auf einen sichtbaren technischen Schlüssel zurück.
- Sprachpakete deklarieren benötigte Zeichen, Font-Unterstützung, Textlängenprüfung und Eignung für 320 × 240.
- Es wird kein vollständiger grosser Unicode-Font eingebunden.
- Fonts werden gezielt für die enthaltenen Sprachpakete erzeugt.

## UI-06 – Themes

Release 1 enthält ausschliesslich das ManuEngineer Dark Theme.

- Die Architektur unterstützt von Anfang an mehrere parallel eingebundene Themes.
- Die Build-Konfiguration legt fest, welche Themes enthalten sind.
- Der Benutzer kann zur Laufzeit zwischen enthaltenen Themes wechseln.
- Die Auswahl wird persistent gespeichert.
- Die Auswahl ist über einen plattformseitigen Einstellungs- oder Servicebereich erreichbar.
- Screens und Apps verwenden ausschliesslich semantische Theme-Tokens.
- Ein Light Theme, z. B. für Release 1.1, muss ohne strukturellen Screen-Umbau ergänzt werden können.
- Ungültige oder unvollständige Themes fallen sicher auf das Standardtheme zurück.

## UI-07 – UI-Technologie und Renderergrenze

LVGL ist der bevorzugte Kandidat, aber noch nicht endgültig gewählt.

Vor der Auswahl sind mindestens zu prüfen:

- Lizenz und Lizenzkompatibilität
- ESP-IDF-6.0.2-Kompatibilität
- Flash-, RAM- und Laufzeitbedarf
- 320 × 240 Querformat
- ILI9341-Anbindung
- XPT2046-Anbindung
- Font- und Asset-Workflow
- Testbarkeit ohne reale Hardware
- Integrationsaufwand mit bestehenden Plattformadaptern

UI-Architektur, View-Modelle, Navigation, Branding, Sprachpakete und Theme-Verträge bleiben rendererunabhängig. Der Renderer darf weder Fachlogik noch Plattformkern bestimmen.

## UI-08 – Gemeinsame View-Modelle und Commands

Touch- und Weboberfläche verwenden gemeinsame, darstellungsunabhängige View-Modelle und Commands.

View-Modelle enthalten keine LVGL-, HTML-, Treiber-, Hardware- oder GPIO-Typen.

- Benutzeraktionen werden als typisierte Commands übergeben.
- Renderer verändern keine Fachzustände direkt.
- Plattform und Apps verwenden getrennte Namensräume.
- Command-Ergebnisse unterscheiden mindestens angenommen, abgelehnt, Bestätigung erforderlich, beschäftigt und nicht verfügbar.
- Bestätigungsdialoge entstehen aus strukturierten Anforderungen.
- Der fachliche Zustand bleibt die einzige Wahrheit.
- Bei Änderungen wird die UI benachrichtigt und liest danach ein vollständiges aktuelles View-Modell.
- Touch und Web lösen dieselben fachlichen Commands aus.
- Zugriffsschutz und Transportvalidierung bleiben Aufgabe des jeweiligen Adapters.

Empfohlene Aktualisierung:

- Uhrzeit etwa jede Minute
- Restzeit je nach Ansicht jede Sekunde oder Minute
- Temperaturen bei neuem Messwert
- WLAN bei Zustandsänderung
- Warnungen und Fehler sofort
- Diagnosewerte nur auf den entsprechenden Seiten
- Verlauf bei neuem Datenpunkt oder langsam zyklisch

## UI-09 – Leichtgewichtiges Komponentensystem

Die Plattform stellt ein kleines, verbindliches Komponenten- und Layoutsystem für 320 × 240 bereit.

Ziele: KISS, einheitliches Aussehen, geringer Ressourcenbedarf, Wartbarkeit, Wiederverwendung, Theme-Unterstützung und Testbarkeit.

Vorgesehene Kandidaten:

- DeviceHeader
- BottomNavigation
- PageTitle
- StatusBadge
- TemperatureValue
- ProgressRing
- ListRow
- ActionButton
- ConfirmationDialog
- MessageBanner
- VerticalPager
- TabIndicator

App-spezifische Komponenten sind erlaubt, müssen aber Plattformregeln, Theme-Tokens und Touchgrössen einhalten und rendererunabhängige View-Modelle konsumieren.

## UI-10 – Touch-Bedienregeln

- Auslegung für resistive Touchbedienung.
- Primäre Elemente erhalten ausreichend grosse Touchflächen; Richtwert etwa 44 × 40 px.
- Kleine Textlinks und Mikrobuttons werden vermieden.
- Selten genutzte Headeraktionen dürfen optisch kleiner sein, besitzen aber grössere unsichtbare Touchflächen.
- Touchflächen dürfen sich nicht überschneiden.
- Wischgesten, Doppeltippen und Long-Press sind keine zwingende Voraussetzung.
- Kritische Aktionen benötigen Bestätigung.
- Schutz gegen Prellen und Doppelauslösung.
- Deaktivierte Funktionen bleiben sichtbar und zeigen bei Bedarf den Grund.
- Beim Drücken muss Interaktivität sichtbar werden.
- Im Ruhezustand werden Headeraktionen nicht wie klassische Buttons hervorgehoben.

## UI-11 – Headerinteraktionen

Der Header bleibt optisch ruhig und integriert.

- Sprachcode mit Dropdown öffnet die enthaltenen Sprachpakete.
- WLAN-Symbol führt direkt zu Netzwerkeinstellungen.
- Uhrzeit führt zu Zeit-/Zeitzonen-/Synchronisationseinstellungen, sofern vorhanden; sonst reine Anzeige.
- Logo und Markenname sind nicht interaktiv.
- Kritische Abläufe dürfen Headeraktionen sperren.
- Beim Antippen erfolgt eine kurze visuelle Rückmeldung.
- Keine dauerhafte Buttonumrandung.

## UI-12 – Splashscreen und Startstatus

Nach Verfügbarkeit des Renderers erscheint ein ManuEngineer-Splashscreen mit Bootlogo und Branding.

- Reguläre Dauer etwa drei Sekunden.
- Ist die Plattform bis dahin sicher bereit, folgt direkt Home.
- Dauert der Start länger, folgt ein kompakter Startstatus mit aktuellem Schritt und relevanten Warn-/Fehlerhinweisen.
- Nichtkritische Einschränkungen blockieren Home nicht.
- Kritische Fehler führen in Recovery/Service.
- Kein separater früher Grafikpfad vor dem Renderer in Release 1.
- Bis dahin darf das Display kurz schwarz bleiben.
- Früher Bootscreen nur neu bewerten, wenn die Technologiewahl ihn einfach und robust ermöglicht.
- Keine blockierende Verzögerung; Initialisierung läuft parallel weiter.

## UI-13 – Fehler-, Warn- und Meldungsdarstellung

Darstellungsarten:

- temporäre Statushinweise
- nicht blockierende Warnhinweise
- Bestätigungsdialoge
- blockierende Fehler-/Recoverybildschirme

Schweregrade: Info, Warning, Error, Critical.

- Zustände, die bereits durch Shell-Elemente sichtbar sind, erzeugen nicht automatisch zusätzliche Warnungen.
- WLAN wird primär über das Header-Symbol dargestellt; offline z. B. durchgestrichen.
- Zusätzliche WLAN-Meldung nur bei konkreter Funktionseinschränkung.
- Nichtkritische Meldungen blockieren Betrieb und Navigation nicht unnötig.
- Recovery darf den Zugang zu Diagnose und Service nicht verhindern, sofern technisch sicher möglich.
- Quittieren bestätigt nur Kenntnisnahme, behebt nichts und löst keine Sperre.
- Nach Quittierung darf eine Meldung kompakter bleiben.
- Mehrere Meldungen werden auf Home zusammengefasst; Details unter Status > Aktive Meldungen.
- Quittieren, Beheben und Zurücksetzen sind getrennt.
- Bedeutung immer über Farbe plus Symbol plus Text.
- Touch und Web verwenden dieselben strukturierten Modelle.
- Keine eigene parallele UI-Meldungspersistenz.

## UI-14 – Priorisierung des Home-Screens

Reihenfolge:

1. kritischer Zustand oder Warnungszusammenfassung
2. aktives Rezept/Programm
3. Restzeit/Fortschritt
4. Produkt- und Schranktemperatur
5. Prozesszustand
6. Zieltemperaturen
7. Phase/Programmschritt
8. dekorative Elemente

Bei Platzmangel verschwinden zuerst dekorative und danach weniger wichtige Zusatztexte. Sicherheits-, Zustands- und Bedieninformationen bleiben erhalten. Dies gilt in allen unterstützten Sprachen bei 320 × 240.

## UI-15 – Zustandsabhängige Hauptansicht

Mindestens folgende Modi:

- bereit, kein aktiver Lauf
- Lauf aktiv
- wartet auf Bestätigung oder Voraussetzung
- Lauf abgeschlossen
- eingeschränkter Betrieb
- App nicht verfügbar/kritischer Recoveryzustand

Die App liefert einen typisierten Anzeigemodus. Der Renderer leitet ihn nicht aus vielen Einzelwerten ab. Nicht relevante Elemente erscheinen nicht als leere oder irreführende Platzhalter. Ohne aktiven Lauf gibt es keinen bedeutungslosen Restzeit- oder Fortschrittswert. Kritische Zustände verwenden den Plattform-Recoverypfad.

## UI-16 – Zustandsabhängige Primäraktion

Der erste app-spezifische Slot ist die primäre Aktion.

Die App liefert Label, Icon, Command, Aktivierungszustand, Hervorhebung und optionalen Sperrgrund.

Für die Fermentations-App mindestens:

- Start
- Stop
- Fortsetzen/Bestätigen
- Abschluss bestätigen
- deaktiviert mit sichtbarem Grund

Kritische oder irreversible Aktionen verwenden strukturierte Bestätigungsdialoge.

### Vorheizen

Ist Vorheizen im Programm aktiviert, lautet die Primäraktion zuerst `Vorheizen` statt `Start`.

- Während Vorheizen: Stop oder Abbrechen gemäss Fachlogik.
- Danach Start, falls manueller Start vorgesehen.
- Bei automatischem Übergang kein zusätzlicher Start.
- Die App liefert den vollständigen Zustand; der Renderer leitet ihn nicht aus Temperaturen ab.

## UI-17 – Zweiter App-Slot

Der zweite Slot ist ein app-spezifischer Hauptbereich.

Die Plattform schreibt Label und Bedeutung nicht fest. Die App liefert Label, Icon, Zielseite, Aktivierungszustand, Auswahlzustand und optionalen Sperrgrund.

In der Fermentations-App: `Rezepte`.

- Rezepte bleiben grundsätzlich auch während eines aktiven Laufs erreichbar.
- Änderungen verändern keinen laufenden unveränderlichen Snapshot.
- Änderungen am verwendeten Rezept erzeugen eine neue Revision oder sind gesperrt.
- Bei Recovery darf der Slot deaktiviert oder durch eine zulässige Recoveryfunktion ersetzt werden.

## UI-18 – Status- und Service-Navigation

Status und Service verwenden ebenfalls genau vier Slots.

Beispiel Status: Home | Übersicht | Meldungen | App
Beispiel Service: Home | Gerät | Netzwerk | App

App-spezifische Unterbereiche liegen unter dem jeweiligen App-Einstieg. Auf tieferen Ebenen wird Home gemäss UI-02 durch Zurück ersetzt. Nicht benötigte Slots bleiben leer. Längere Inhalte werden rechts mit Auf/Ab geblättert.

## UI-19 – Servicezugang und Schutzstufen

### Frei zugänglich

- Firmwareversion
- Gerätestatus
- Netzwerkstatus
- Uhrzeit und Zeitzone
- Anzeigehelligkeit
- Logs und Diagnose ansehen

### Bestätigung erforderlich

- Neustart
- Netzwerk zurücksetzen
- Touchkalibrierung starten
- gespeicherte Daten exportieren oder löschen

### Service-PIN erforderlich

- Werkseinstellungen
- Aktortest
- Sensorkalibrierung
- Reglerparameter
- Hardwarezuordnung
- sicherheitsrelevante Servicefunktionen

Release 1 besitzt normalen Zugriff und geschützten Servicezugriff, aber kein komplexes Rollenmodell. Apps verwenden denselben Schutzmechanismus. Eine gültige PIN ersetzt keine Sicherheitsprüfung.

## UI-20 – Service-PIN und Build-Parameter

- Numerische PIN.
- Bildschirmtastatur für 320 × 240 mit grossen Tasten.
- Keine Klartextanzeige.
- Begrenzte zunehmende Wartezeit nach Fehlversuchen.
- Keine dauerhafte Gerätesperre.
- Gemeinsamer Authentifizierungsdienst, getrennte Touch- und Websitzungen.
- Zeiten und Grenzwerte zentral als Build-/Produktparameter, nicht in normalen Einstellungen.
- Initialer Richtwert der Inaktivitätsdauer: zehn Minuten.

## UI-21 – Inaktivitätsbasierte Servicesitzung

### Lokale Touch-Servicefreigabe

- Ablauf nach 10 Minuten Inaktivität, nicht nach einer starren Gesamtdauer.
- Die 10 Minuten sind zentraler Build-/Produktparameter und nicht über die UI
  änderbar.
- Relevante Bedienhandlungen im geschützten Bereich setzen den Timer zurück;
  Hintergrundaktivität und automatische Updates nicht.
- Release 1 besitzt keine absolute Maximaldauer für die lokale Freigabe.
- Die Freigabe wird bei Inaktivitätsablauf, Gerätestart, explizitem Abmelden und
  definierten sicherheitsrelevanten Zustandswechseln gesperrt.
- Frei zugängliche Informationen bleiben sichtbar; geschützte Aktionen verlangen
  erneut die PIN.
- Eingaben in Editoren dürfen bei Ablauf nicht kommentarlos verloren gehen;
  Speichern verlangt erneut PIN.

### Getrennte Web-Servicepolicy

- Touch und Web verwalten getrennte Sitzungen.
- Die Web-Servicefreigabe bleibt die bewusst strengere netzwerkseitige Grenze:
  5 Minuten Inaktivität und 15 Minuten absolute Maximaldauer.
- Diese Web-Policy wird nicht mit der lokalen Touch-Servicefreigabe vereinheitlicht.

## UI-22 – Displayhelligkeit und Ruhezustand

- Plattform steuert Helligkeit und Ruhezustand zentral.
- Nach Inaktivität dimmen, optional später ausschalten.
- Erster Touch weckt nur und löst keine Aktion aus.
- Helligkeit als plattformseitige Benutzereinstellung.
- Dimm-/Ausschaltzeiten innerhalb zentraler Grenzen konfigurierbar.
- Hintergrundereignisse zählen nicht als Benutzeraktivität.
- Laufende Prozesse verhindern Dimmen nicht grundsätzlich.
- Warnungen/kritische Fehler dürfen aufwecken, Mindesthelligkeit verlangen oder Ausschalten verhindern.
- Richtwerte erst nach Hardware- und Praxisevaluation.

## UI-23 – Akustische und visuelle Rückmeldung

### Touchfeedback

- kurzer Ton optional
- standardmässig aus
- benutzerseitig ein-/ausschaltbar
- nie einzige Rückmeldung

### Visuelles Feedback

- immer aktiv
- kurz und dezent
- keine dauerhafte Hervorhebung ausserhalb aktiver Zustände

### Weitere Signale

- wichtige erfolgreiche Aktion: kurzer Bestätigungston
- Warnung: eigenes Warnsignal
- kritischer Fehler: deutliches Muster
- Lauf abgeschlossen: eigenes Abschlussmuster

Muster werden zentral durch die Plattform definiert. Apps wählen semantische Signaltypen. Konfiguration trennt Touchton, allgemeine Hinweistöne und Warn-/Fehlersignale. Sicherheitsrelevante Signale nur deaktivierbar, wenn die Sicherheitsbewertung dies erlaubt. Lautstärke nur, wenn Hardware dies unterstützt.

## UI-24 – Ausschliessliche lokale Bedienung per Touch

Verbindlicher Hardwarestand:

- Touchdisplay vorhanden
- Pieper vorhanden
- keine physischen Taster
- kein Encoder
- kein Programmwahlschalter
- keine Status-LEDs

Der Pieper ist nur Ausgabekanal.

Alle lokalen Benutzeraktionen erfolgen über das Touchdisplay und typisierte Commands. Die UI muss sämtliche Bedienpfade bereitstellen, insbesondere Start, Stop, Vorheizen, Bestätigungen, Navigation, Servicezugang, Recovery, Quittierung, Einstellungen und Touchkalibrierung. Keine Funktion darf ein physisches Bedienelement als Fallback voraussetzen.

---

## Noch offene technische Punkte

1. endgültige Wahl von LVGL oder anderem Renderer
2. konkreter Display-/Touch-Treiberstack
3. Flash-, RAM-, DMA- und Pufferstrategie
4. endgültige Fonts und Assetformate
5. konkrete Helligkeits-, Dimm- und Ausschaltwerte
6. konkrete Pieper-Signalmuster
7. genaue Seiten- und Komponentenliste nach Abgleich mit den bestehenden Spezifikationen

## Erledigte Roadmap- und Dokumentationspunkte

- Der Live-Abgleich, die Konfliktmatrix, der Issue-Schnitt und die Anpassung
  von #25, #26 und #31 sind in PR #80 umgesetzt.
- Die repositoryweite Prüfung physischer Bedienelementannahmen ist erfolgt;
  verbindlich ausgeschlossene Taster, Encoder, Programmwahlschalter und
  Status-LEDs sind in Spezifikation, Hardware- und Roadmapdokumentation
  bereinigt beziehungsweise als Ausschluss dokumentiert.

## Nächster verbindlicher Schritt

PR #80 wartet auf unabhängiges Ownerreview. Nach seinem Abschluss startet jedes
einzelne Implementierungs-Issue weiterhin nur mit einem eigenen Plan-first-
Draft-PR und einer commitgebundenen Ownerfreigabe. Insbesondere bleiben
Rendererwahl, Treiberstack, Ressourcenmessung, Font-/Assets, Helligkeitswerte
und reale Hardwareverifikation vor ihrer jeweiligen Freigabe offen.
