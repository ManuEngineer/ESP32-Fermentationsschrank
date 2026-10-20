# Sprachwahl – Budgetmessung (Plan `issue-27-r1-web-language-plan.md`)

- `measure.py`: Basisabgleich (Suffix 9 619 B, Gesamt 11 016 B) und Tabelle aller Kandidaten.
- `candidates/*__*.txt`: Zeile 1 = zusaetzliches Kopf-Markup (`<select>`), ab Zeile 2 der
  vollstaendige Suffix; `candidates/BASE.suffix.txt` = Suffix auf `origin/main` `43f0338`.
- Stand Revision 2: Die A2-Kandidaten enthalten die beiden Korrekturen aus dem Planreview
  (geschuetztes Schreiben nach `localStorage`; `ui()` rendert eine aktive Bestaetigung in der
  neuen Sprache). Die R-Kandidaten (Reload) wurden vor der Korrektur gemessen und sind nur
  historische Vergleichswerte.
- `candidate_regression_test.mjs`: abgeleitete Kopie von `scripts/test_web_shell_polling.mjs`
  mit zwei Zusatzfaellen (FIX1 werfender/fehlender Speicher, FIX2 aktive Bestaetigung);
  `<REPO>` durch den Repo-Pfad ersetzen, `CAND=<Datei mit dem Suffix ab Zeile 2>
  node candidate_regression_test.mjs`. Ergebnis fuer A2/Endonyme: 58/58 PASS; der Kandidat vor
  der Korrektur wirft `ReferenceError` (Negativkontrolle).
- Funktionspruefung (Fake-DOM, kein Browser): den Suffix eines Kandidaten (ab Zeile 2)
  anstelle des aus `web_application_routes.cpp` extrahierten Literals in eine Kopie von
  `scripts/test_web_shell_polling.mjs` einsetzen. Ergebnis fuer A2/Endonyme: 56/56 PASS
  (bestehende Faelle, inklusive Pfad ohne `localStorage`) plus ein Ad-hoc-Wechseltest
  (`sw.value='es'; sw.onchange()` → `T.login` spanisch, `lc`/`localStorage.lang`/`<html lang>`
  = `es`; `xx` → `en`; `de` zurueck). Kein Produkt- oder Testcode des Repositorys wurde
  veraendert.
