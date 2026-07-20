# Projektspezifische Bibliotheken

In diesem Ordner liegt testbare Fachlogik, die nicht direkt in `src/main.cpp`
gehoert.

Empfohlene Struktur:

```text
lib/
└── Controller/
    ├── src/
    │   ├── Controller.cpp
    │   └── Controller.hpp
    └── library.json
```

Hardwareunabhaengige Logik sollte bevorzugt hier liegen, damit sie mit der
nativen PlatformIO-Umgebung getestet werden kann.
