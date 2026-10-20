# Sprachwahl – Budgetmessung (Plan `issue-27-r1-web-language-plan.md`)

- `measure.py`: Basisabgleich (Suffix 9 619 B, Gesamt 11 016 B) und Tabelle aller Kandidaten.
- `candidates/*__*.txt`: Zeile 1 = zusaetzliches Kopf-Markup (`<select>`), ab Zeile 2 der
  vollstaendige Suffix; `candidates/BASE.suffix.txt` = Suffix auf `origin/main` `43f0338`.
- Funktionspruefung (Fake-DOM, kein Browser): den Suffix eines Kandidaten (ab Zeile 2)
  anstelle des aus `web_application_routes.cpp` extrahierten Literals in eine Kopie von
  `scripts/test_web_shell_polling.mjs` einsetzen. Ergebnis fuer A2/Endonyme: 56/56 PASS
  (bestehende Faelle, inklusive Pfad ohne `localStorage`) plus ein Ad-hoc-Wechseltest
  (`sw.value='es'; sw.onchange()` → `T.login` spanisch, `lc`/`localStorage.lang`/`<html lang>`
  = `es`; `xx` → `en`; `de` zurueck). Kein Produkt- oder Testcode des Repositorys wurde
  veraendert.
