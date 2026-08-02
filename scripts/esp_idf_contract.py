#!/usr/bin/env python3
"""Gemeinsame ESP-IDF-Vertragswerte fuer Issue #74 (DRY).

Einzige Quelle fuer den gepinnten ESP-IDF-Tag/-Commit und die kanonischen
Profilnamen, damit `scripts/build_esp_idf_profiles.py` und
`scripts/check_build_profiles.py` diese Werte nicht unabhaengig
duplizieren (siehe docs/tasks/issue-74-implementation-plan.md,
Abschnitt 7.1/7.4/7.5.2).

Bewusst nur Konstanten und kleine, reine Namenskonventions-Funktionen -
keine Klassenhierarchie, kein generisches Toolchain-Framework, kein
Dependency-Injection-Container.
"""

ESP_IDF_TAG = "v6.0.2"
ESP_IDF_COMMIT = "7101770dc6db2667b3c477cc31365dd1acd6db4e"

PROFILES = ("bringup", "release")

# ESP-IDF-Static-Analysis-Werkzeugvertrag (Issue #74, Commit 4, Plan
# Abschnitt 7.8.1/7.8.4). `clang --version` meldet den vollstaendigen
# Toolpaketnamen (ESP_CLANG_TOOL_VERSION); `clang-tidy --version` meldet
# nachweislich nur die LLVM-Version (ESP_CLANG_LLVM_VERSION), nicht den
# Toolpaketnamen - deshalb zwei getrennte Konstanten statt einer.
ESP_CLANG_TOOL_VERSION = "esp-20.1.1_20250829"
ESP_CLANG_LLVM_VERSION = "20.1.1"
ESP_CLANG_LINUX_AMD64_SHA256 = (
    "88910c21350c06a521f243304d1a3adbdb78447123b3f8e27493aab75e3cc07f"
)

# Von ESP-IDF selbst nicht ueber ein Constraints-File fixiert (siehe Plan
# Abschnitt 7.8.1); wird deshalb hier projekteigen fail-closed erzwungen.
PYCLANG_VERSION = "0.7.0"


def sdkconfig_overlay(profile: str) -> str:
    """Overlay-Dateiname fuer ein Profil, z. B. 'sdkconfig.defaults.bringup'."""
    return f"sdkconfig.defaults.{profile}"


def build_dir_name(profile: str) -> str:
    """Buildordnername fuer ein Profil, z. B. 'esp32_bringup'."""
    return f"esp32_{profile}"


def profile_kconfig_option(profile: str) -> str:
    """Kconfig-Optionsname, z. B. 'CONFIG_APP_PROFILE_ESP32_BRINGUP'."""
    return f"CONFIG_APP_PROFILE_ESP32_{profile.upper()}"


def profile_define(profile: str) -> str:
    """Compile-Definitionsname, z. B. 'APP_PROFILE_ESP32_BRINGUP'."""
    return f"APP_PROFILE_ESP32_{profile.upper()}"


def other_profile(profile: str) -> str:
    """Das jeweils andere der genau zwei Profile."""
    others = [candidate for candidate in PROFILES if candidate != profile]
    assert len(others) == 1, f"unerwartete Profilanzahl: {PROFILES}"
    return others[0]
