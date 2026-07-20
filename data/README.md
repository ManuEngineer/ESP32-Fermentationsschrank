# LittleFS- und Webdateien

Dieser Ordner ist fuer statische Dateien vorgesehen, die spaeter in LittleFS
geladen werden, beispielsweise:

```text
data/
├── index.html
├── app.js
├── style.css
└── favicon.svg
```

Upload, sobald LittleFS im Projekt konfiguriert ist:

```bash
pio run --target uploadfs
```
