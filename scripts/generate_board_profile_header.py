#!/usr/bin/env python3
"""Generiert den deterministischen Composition-Root-Header fuer die R1-
Display-/Touch-GPIO-Zuordnung aus der einzigen kanonischen Wiring-SSOT
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` (Issue #130).

Der freigegebene #31-Plan verlangt: bevor ein produktiver Hardwareadapter
konkrete GPIO-Zahlen verwendet, muessen diese aus derselben SSOT abgeleitet
werden. Eine zweite handgepflegte Pinliste in `main/app_main.cpp` (oder
irgendwo sonst in C++/CMake) ist unzulaessig.

Dieses Skript liest ausschliesslich die acht fuer den produktiven
`EspIdfDisplayTouchConfig`-Consumer benoetigten Pinfunktionen
(spi_sck, spi_mosi, spi_miso, tft_cs, touch_cs, tft_dc_rs,
tft_backlight_pwm, touch_irq) sowie den aktiven Pegel des Backlight-Pins.
Es interpretiert die YAML zur Build-/Generierungszeit auf dem
Entwicklungsrechner, niemals zur Laufzeit auf dem ESP32 (keine
YAML-Laufzeitinterpretation auf dem Zielsystem).

Der generierte Header wird eingecheckt. Der reine Firmwarebuild liest nur die
bereits generierte Datei und benoetigt PyYAML nicht. Der Host-/Pre-Ready-
SSOT-Check ruft dieses Skript mit `--check` auf und benoetigt dafuer PyYAML.
Beim bewussten Regenerieren nach einer SSOT-Aenderung validiert dieses Skript
ebenfalls fail-fast:

- jede der acht benoetigten Funktionen kommt in der SSOT genau einmal vor;
- der zugehoerige GPIO-Schluessel ist syntaktisch `gpio<N>` mit
  ganzzahligem `N`;
- `assignment_status` ist gesetzt und keiner der bekannten
  Platzhalterwerte (`unavailable_not_exposed`,
  `board_fixed_pending_functional_verification`) oder ein
  `TBD_HARDWARE`-Text - `planned` und `confirmed` sind gueltig (die
  Stage-2/3-Evidence des #31-Plans hat diese acht Pins bereits
  funktional bestaetigt; die YAML-eigene `assignment_status`-Spalte ist
  eine separate elektrische Designklassifikation, kein Ersatz dafuer);
- der Backlight-Pin hat einen `active_level` von exakt `high` oder `low`.

Zusaetzlich (Issue #32) werden die drei Onboard-MOSFET-Ausgaenge Innenluefter,
Aussenluefter und Summer ueber ihre SSOT-`application_role` (`internal_fan`,
`external_heatsink_fan`, `active_buzzer`) abgeleitet: GPIO-Nummer und
Ausgangspolaritaet. Hier ist `board_fixed_pending_functional_verification` der
erwartete Status (die funktionale Pruefung steht aus). Die Polaritaet kommt
ausschliesslich aus `active_level`: `high` -> `ActiveHigh`, `low` ->
`ActiveLow`, `TBD_HARDWARE` -> `Unconfirmed`; jeder andere oder fehlende Wert
ist ein Fehler. Es wird keine Polaritaet geraten. Die Reserve (`reserve`,
GPIO27) wird nicht erzeugt.

`width`/`height` sind eine Panel-/Displayeigenschaft, keine GPIO-Zuordnung,
und daher nicht Teil dieser SSOT-Ableitung; sie bleiben ein
composition-root-seitiger Konstantwert.

`--selftest` prueft Erkennung und Fail-fast-Pfade anhand temporaerer
Fixture-YAML-Dateien, ohne dass ein absichtlich fehlerhafter Fall jemals in
dieses Repository eingecheckt werden muss.
"""

import argparse
import re
import sys
import tempfile
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE_PATH = (
    REPO_ROOT / "config" / "board_profiles" / "esp32_32e_quad_mosfet_r1.yaml"
)
DEFAULT_OUTPUT_PATH = REPO_ROOT / "main" / "generated" / "board_profile_r1.hpp"

# Maps the SSOT's `function` value to the generated constant name.
REQUIRED_FUNCTIONS = {
    "spi_sck": "kSpiSckPin",
    "spi_mosi": "kSpiMosiPin",
    "spi_miso": "kSpiMisoPin",
    "tft_cs": "kDisplayChipSelectPin",
    "touch_cs": "kTouchChipSelectPin",
    "tft_dc_rs": "kDisplayDataCommandPin",
    "tft_backlight_pwm": "kBacklightPin",
    "touch_irq": "kTouchInterruptPin",
    "one_wire_internal": "kOneWireInternalPin",
    "one_wire_product": "kOneWireProductPin",
}

# Onboard-MOSFET-Ausgaenge (Issue #32), ueber `application_role` gefunden.
OUTPUT_ROLES = {
    "internal_fan": ("kInternalFanPin", "kInternalFanPolarity"),
    "external_heatsink_fan": ("kOuterFanPin", "kOuterFanPolarity"),
    "active_buzzer": ("kBuzzerPin", "kBuzzerPolarity"),
}

POLARITY_BY_ACTIVE_LEVEL = {
    "high": "ActiveHigh",
    "low": "ActiveLow",
    "TBD_HARDWARE": "Unconfirmed",
}

INVALID_ASSIGNMENT_STATUSES = {
    "unavailable_not_exposed",
    "board_fixed_pending_functional_verification",
}

GPIO_KEY_PATTERN = re.compile(r"^gpio(\d+)$")


class BoardProfileError(RuntimeError):
    pass


def load_profile(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle)
    if not isinstance(data, dict) or "pins" not in data or not isinstance(
        data["pins"], dict
    ):
        raise BoardProfileError(f"{path}: missing or malformed 'pins' map")
    return data


def is_valid_assignment_status(status: object) -> bool:
    if not isinstance(status, str) or not status:
        return False
    if status in INVALID_ASSIGNMENT_STATUSES:
        return False
    if "TBD_HARDWARE" in status:
        return False
    return True


def extract_pins(profile: dict, path_for_errors: str) -> dict:
    pins = profile["pins"]
    by_function: dict[str, tuple[int, dict]] = {}
    for gpio_key, pin_data in pins.items():
        if not isinstance(pin_data, dict):
            continue
        function = pin_data.get("function")
        if function not in REQUIRED_FUNCTIONS:
            continue
        match = GPIO_KEY_PATTERN.match(str(gpio_key))
        if not match:
            raise BoardProfileError(
                f"{path_for_errors}: pin key {gpio_key!r} for function "
                f"{function!r} is not of the form 'gpio<N>'"
            )
        gpio_number = int(match.group(1))
        if function in by_function:
            raise BoardProfileError(
                f"{path_for_errors}: function {function!r} is assigned to "
                f"both gpio{by_function[function][0]} and gpio{gpio_number} "
                "- exactly one assignment is required"
            )
        by_function[function] = (gpio_number, pin_data)

    missing = sorted(set(REQUIRED_FUNCTIONS) - set(by_function))
    if missing:
        raise BoardProfileError(
            f"{path_for_errors}: missing required pin function(s): "
            f"{', '.join(missing)}"
        )

    for function, (gpio_number, pin_data) in by_function.items():
        status = pin_data.get("assignment_status")
        if not is_valid_assignment_status(status):
            raise BoardProfileError(
                f"{path_for_errors}: function {function!r} on gpio"
                f"{gpio_number} has an invalid or placeholder "
                f"assignment_status ({status!r})"
            )

    backlight_gpio, backlight_data = by_function["tft_backlight_pwm"]
    backlight_active_level = backlight_data.get("active_level")
    if backlight_active_level not in ("high", "low"):
        raise BoardProfileError(
            f"{path_for_errors}: tft_backlight_pwm (gpio{backlight_gpio}) "
            f"has no valid active_level (got {backlight_active_level!r}); "
            "expected exactly 'high' or 'low'"
        )

    return {
        "pins": {
            function: gpio_number
            for function, (gpio_number, _pin_data) in by_function.items()
        },
        "backlight_active_high": backlight_active_level == "high",
        "outputs": extract_outputs(profile, path_for_errors),
    }


def extract_outputs(profile: dict, path_for_errors: str) -> dict:
    by_role: dict[str, tuple[int, str]] = {}
    for gpio_key, pin_data in profile["pins"].items():
        if not isinstance(pin_data, dict):
            continue
        role = pin_data.get("application_role")
        if role not in OUTPUT_ROLES:
            continue
        match = GPIO_KEY_PATTERN.match(str(gpio_key))
        if not match:
            raise BoardProfileError(
                f"{path_for_errors}: pin key {gpio_key!r} for output role "
                f"{role!r} is not of the form 'gpio<N>'"
            )
        gpio_number = int(match.group(1))
        if role in by_role:
            raise BoardProfileError(
                f"{path_for_errors}: output role {role!r} is assigned to "
                f"both gpio{by_role[role][0]} and gpio{gpio_number} "
                "- exactly one assignment is required"
            )
        status = pin_data.get("assignment_status")
        if (
            not isinstance(status, str)
            or not status
            or status == "unavailable_not_exposed"
        ):
            raise BoardProfileError(
                f"{path_for_errors}: output role {role!r} on gpio"
                f"{gpio_number} has an invalid assignment_status "
                f"({status!r})"
            )
        active_level = pin_data.get("active_level")
        if active_level not in POLARITY_BY_ACTIVE_LEVEL:
            raise BoardProfileError(
                f"{path_for_errors}: output role {role!r} on gpio"
                f"{gpio_number} has no valid active_level (got "
                f"{active_level!r}); expected 'high', 'low' or "
                "'TBD_HARDWARE'"
            )
        by_role[role] = (gpio_number, POLARITY_BY_ACTIVE_LEVEL[active_level])

    missing = sorted(set(OUTPUT_ROLES) - set(by_role))
    if missing:
        raise BoardProfileError(
            f"{path_for_errors}: missing required output role(s): "
            f"{', '.join(missing)}"
        )
    return by_role


def render_header(resolved: dict, source_path_display: str) -> str:
    pins = resolved["pins"]
    lines = [
        "// GENERATED FILE - DO NOT EDIT BY HAND.",
        "//",
        f"// Generated by scripts/generate_board_profile_header.py from",
        f"// {source_path_display}",
        "// Regenerate with:",
        "//   python3 scripts/generate_board_profile_header.py",
        "//",
        "// This is the single deterministic build-time derivation of the",
        "// R1 display/touch/sensor GPIO assignment and onboard MOSFET output",
        "// assignment and polarity from the canonical wiring SSOT",
        "// (Issue #130). No second hand-maintained pin list is permitted in",
        "// main/app_main.cpp, CMake, or any other configuration file.",
        "// The R1 display rotation below is the single shared candidate used",
        "// by the productive renderer and the Issue #31 capture harness; its",
        "// physical selection is accepted only after the actor-free probe.",
        "#pragma once",
        "",
        "#include \"device_ui_hardware_ports.hpp\"",
        "#include \"output_polarity.hpp\"",
        "",
        "namespace board_profile::esp32_32e_quad_mosfet_r1 {",
        "",
        f"inline constexpr int kSpiSckPin = {pins['spi_sck']};",
        f"inline constexpr int kSpiMosiPin = {pins['spi_mosi']};",
        f"inline constexpr int kSpiMisoPin = {pins['spi_miso']};",
        f"inline constexpr int kDisplayChipSelectPin = {pins['tft_cs']};",
        f"inline constexpr int kTouchChipSelectPin = {pins['touch_cs']};",
        f"inline constexpr int kDisplayDataCommandPin = {pins['tft_dc_rs']};",
        f"inline constexpr int kBacklightPin = {pins['tft_backlight_pwm']};",
        "inline constexpr bool kBacklightActiveHigh = "
        + ("true;" if resolved["backlight_active_high"] else "false;"),
        f"inline constexpr int kTouchInterruptPin = {pins['touch_irq']};",
        f"inline constexpr int kOneWireInternalPin = {pins['one_wire_internal']};",
        f"inline constexpr int kOneWireProductPin = {pins['one_wire_product']};",
        "inline constexpr device_platform::DisplayRotation kR1DisplayRotation =",
        "    device_platform::DisplayRotation::Rotate90;",
        "",
        "// Onboard-MOSFET outputs (Issue #32). A polarity of `Unconfirmed` means",
        "// the SSOT active_level is still TBD_HARDWARE: the output adapter then",
        "// performs no GPIO operation. The reserve channel is not generated.",
    ]
    for role, (pin_name, polarity_name) in OUTPUT_ROLES.items():
        gpio_number, polarity = resolved["outputs"][role]
        lines.append(f"inline constexpr int {pin_name} = {gpio_number};")
        lines.append(
            f"inline constexpr device_platform::OutputPolarity {polarity_name} ="
        )
        lines.append(f"    device_platform::OutputPolarity::{polarity};")
    lines += [
        "",
        "}  // namespace board_profile::esp32_32e_quad_mosfet_r1",
        "",
    ]
    return "\n".join(lines)


def generate(profile_path: Path, output_path: Path) -> str:
    profile = load_profile(profile_path)
    try:
        relative_source = profile_path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        relative_source = str(profile_path)
    resolved = extract_pins(profile, relative_source)
    header = render_header(resolved, relative_source)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header, encoding="utf-8")
    return header


def run_selftest() -> int:
    def write_profile(directory: Path, pins_yaml: str) -> Path:
        path = directory / "profile.yaml"
        path.write_text(
            "profile:\n  id: test\npins:\n" + pins_yaml, encoding="utf-8"
        )
        return path

    valid_pins = """
  gpio18:
    function: spi_sck
    assignment_status: planned
  gpio23:
    function: spi_mosi
    assignment_status: planned
  gpio19:
    function: spi_miso
    assignment_status: planned
  gpio5:
    function: tft_cs
    assignment_status: planned
  gpio15:
    function: touch_cs
    assignment_status: planned
  gpio2:
    function: tft_dc_rs
    assignment_status: planned
  gpio4:
    function: tft_backlight_pwm
    active_level: high
    assignment_status: planned
  gpio39:
    function: touch_irq
    assignment_status: planned
  gpio32:
    function: one_wire_internal
    assignment_status: planned
  gpio33:
    function: one_wire_product
    assignment_status: planned
  gpio16:
    function: onboard_mosfet_1
    application_role: internal_fan
    active_level: TBD_HARDWARE
    assignment_status: board_fixed_pending_functional_verification
  gpio17:
    function: onboard_mosfet_2
    application_role: external_heatsink_fan
    active_level: TBD_HARDWARE
    assignment_status: board_fixed_pending_functional_verification
  gpio26:
    function: onboard_mosfet_3
    application_role: active_buzzer
    active_level: TBD_HARDWARE
    assignment_status: board_fixed_pending_functional_verification
  gpio27:
    function: onboard_mosfet_4
    application_role: reserve
    active_level: TBD_HARDWARE
    assignment_status: board_fixed_pending_functional_verification
"""

    checks: dict[str, bool] = {}

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)

        # A fully valid fixture must generate successfully with the exact
        # expected pin numbers.
        valid_path = write_profile(tmp_path, valid_pins)
        output_path = tmp_path / "board_profile_r1.hpp"
        try:
            header = generate(valid_path, output_path)
            checks["valid fixture generates"] = (
                "kSpiSckPin = 18" in header
                and "kOneWireInternalPin = 32" in header
                and "kOneWireProductPin = 33" in header
                and "kBacklightActiveHigh = true;" in header
                and "kInternalFanPin = 16;" in header
                and "kOuterFanPin = 17;" in header
                and "kBuzzerPin = 26;" in header
                and header.count("OutputPolarity::Unconfirmed;") == 3
                and "27" not in header.split("kOneWireProductPin")[1]
                and output_path.exists()
            )
        except BoardProfileError:
            checks["valid fixture generates"] = False

        # Missing a required function must fail fast.
        missing_pins = valid_pins.replace(
            "    function: touch_irq\n", "    function: something_else\n"
        )
        missing_path = write_profile(tmp_path, missing_pins)
        try:
            generate(missing_path, tmp_path / "out2.hpp")
            checks["missing function rejected"] = False
        except BoardProfileError as error:
            checks["missing function rejected"] = "touch_irq" in str(error)

        # A duplicate function assignment must fail fast.
        duplicate_pins = valid_pins.replace(
            "    function: touch_irq\n",
            "    function: spi_sck\n",
        )
        duplicate_path = write_profile(tmp_path, duplicate_pins)
        try:
            generate(duplicate_path, tmp_path / "out3.hpp")
            checks["duplicate function rejected"] = False
        except BoardProfileError as error:
            checks["duplicate function rejected"] = "spi_sck" in str(error)

        # A placeholder assignment_status must fail fast.
        placeholder_pins = valid_pins.replace(
            "  gpio39:\n    function: touch_irq\n    assignment_status: planned\n",
            "  gpio39:\n    function: touch_irq\n"
            "    assignment_status: unavailable_not_exposed\n",
        )
        placeholder_path = write_profile(tmp_path, placeholder_pins)
        try:
            generate(placeholder_path, tmp_path / "out4.hpp")
            checks["placeholder assignment_status rejected"] = False
        except BoardProfileError as error:
            checks["placeholder assignment_status rejected"] = (
                "touch_irq" in str(error)
            )

        # TBD_HARDWARE assignment_status must fail fast.
        tbd_pins = valid_pins.replace(
            "  gpio4:\n    function: tft_backlight_pwm\n    active_level: high\n"
            "    assignment_status: planned\n",
            "  gpio4:\n    function: tft_backlight_pwm\n    active_level: high\n"
            "    assignment_status: TBD_HARDWARE\n",
        )
        tbd_path = write_profile(tmp_path, tbd_pins)
        try:
            generate(tbd_path, tmp_path / "out5.hpp")
            checks["TBD_HARDWARE assignment_status rejected"] = False
        except BoardProfileError as error:
            checks["TBD_HARDWARE assignment_status rejected"] = (
                "tft_backlight_pwm" in str(error)
            )

        # A malformed pin key must fail fast.
        malformed_pins = valid_pins.replace("  gpio18:\n", "  spi_clock_pin:\n")
        malformed_path = write_profile(tmp_path, malformed_pins)
        try:
            generate(malformed_path, tmp_path / "out6.hpp")
            checks["malformed pin key rejected"] = False
        except BoardProfileError as error:
            checks["malformed pin key rejected"] = "spi_clock_pin" in str(error)

        # A missing backlight active_level must fail fast.
        no_level_pins = valid_pins.replace(
            "  gpio4:\n    function: tft_backlight_pwm\n    active_level: high\n"
            "    assignment_status: planned\n",
            "  gpio4:\n    function: tft_backlight_pwm\n"
            "    assignment_status: planned\n",
        )
        no_level_path = write_profile(tmp_path, no_level_pins)
        try:
            generate(no_level_path, tmp_path / "out7.hpp")
            checks["missing backlight active_level rejected"] = False
        except BoardProfileError as error:
            checks["missing backlight active_level rejected"] = (
                "active_level" in str(error)
            )

        def output_case(name: str, pins_yaml: str, expect) -> None:
            # `expect` is a predicate on the generated header (success case)
            # or a substring that must appear in the fail-fast error.
            path = write_profile(tmp_path, pins_yaml)
            try:
                header_text = generate(path, tmp_path / "out_case.hpp")
                checks[name] = callable(expect) and bool(expect(header_text))
            except BoardProfileError as error:
                checks[name] = isinstance(expect, str) and expect in str(error)

        # Confirmed polarities map exactly; nothing is guessed.
        output_case(
            "active_level high/low map to ActiveHigh/ActiveLow",
            valid_pins.replace(
                "    application_role: internal_fan\n    active_level: TBD_HARDWARE\n",
                "    application_role: internal_fan\n    active_level: high\n",
            ).replace(
                "    application_role: active_buzzer\n    active_level: TBD_HARDWARE\n",
                "    application_role: active_buzzer\n    active_level: low\n",
            ),
            lambda h: "kInternalFanPolarity =\n    device_platform::OutputPolarity::ActiveHigh;" in h
            and "kBuzzerPolarity =\n    device_platform::OutputPolarity::ActiveLow;" in h
            and "kOuterFanPolarity =\n    device_platform::OutputPolarity::Unconfirmed;" in h,
        )
        # An unknown or missing output active_level must fail fast.
        output_case(
            "invalid output active_level rejected",
            valid_pins.replace(
                "    application_role: internal_fan\n    active_level: TBD_HARDWARE\n",
                "    application_role: internal_fan\n    active_level: maybe\n",
            ),
            "internal_fan",
        )
        output_case(
            "missing output active_level rejected",
            valid_pins.replace(
                "    application_role: external_heatsink_fan\n    active_level: TBD_HARDWARE\n",
                "    application_role: external_heatsink_fan\n",
            ),
            "external_heatsink_fan",
        )
        # A missing or duplicated output role must fail fast.
        output_case(
            "missing output role rejected",
            valid_pins.replace(
                "application_role: active_buzzer", "application_role: other"
            ),
            "active_buzzer",
        )
        output_case(
            "duplicate output role rejected",
            valid_pins.replace(
                "application_role: reserve", "application_role: internal_fan"
            ),
            "internal_fan",
        )
        output_case(
            "unavailable output assignment_status rejected",
            valid_pins.replace(
                "    application_role: internal_fan\n    active_level: TBD_HARDWARE\n"
                "    assignment_status: board_fixed_pending_functional_verification\n",
                "    application_role: internal_fan\n    active_level: TBD_HARDWARE\n"
                "    assignment_status: unavailable_not_exposed\n",
            ),
            "internal_fan",
        )

    ok = True
    for name, passed in checks.items():
        status = "PASS" if passed else "FAILED"
        print(f"{status}: {name}")
        ok = ok and passed
    return 0 if ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--profile",
        type=Path,
        default=DEFAULT_PROFILE_PATH,
        help="Pfad zur Board-Profil-YAML (Standard: R1-SSOT)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT_PATH,
        help="Zielpfad fuer den generierten Header",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=(
            "Nur pruefen, ob die eingecheckte Datei mit einer frischen "
            "Generierung aus der SSOT uebereinstimmt (kein Schreibzugriff); "
            "fuer CI/Pre-Ready, damit der Header nie unbemerkt von der "
            "SSOT abweicht"
        ),
    )
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return run_selftest()

    if args.check:
        if not args.output.exists():
            print(f"FAILED: {args.output} does not exist", file=sys.stderr)
            return 1
        committed = args.output.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            fresh_path = Path(tmp) / args.output.name
            try:
                fresh = generate(args.profile, fresh_path)
            except BoardProfileError as error:
                print(f"FAILED: {error}", file=sys.stderr)
                return 1
        if committed != fresh:
            print(
                f"FAILED: {args.output} is out of date relative to "
                f"{args.profile}; regenerate with "
                "'python3 scripts/generate_board_profile_header.py'",
                file=sys.stderr,
            )
            return 1
        print(f"PASS: {args.output} matches the current SSOT")
        return 0

    try:
        generate(args.profile, args.output)
    except BoardProfileError as error:
        print(f"FAILED: {error}", file=sys.stderr)
        return 1
    print(f"PASS: generated {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
