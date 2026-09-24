#!/usr/bin/env python3
"""Generiert das produktive R1-Headerlogo als echtes LVGL-9.6-Image-Asset aus
dem verbindlichen Master-SVG (Issue #31 Branding-FOLLOW-UP).

Verbindliche Masterquelle, niemals ueberschrieben:

    assets/branding/manuengineer/ManuEngineer.svg

Reproduzierbare Pipeline, alle Schritte deterministisch und ausschliesslich
auf dem Entwicklungsrechner ausgefuehrt (kein SVG-Rendering auf dem ESP32):

    Master-SVG
    -> rsvg-convert -> 168x24 PNG mit Alphakanal
    -> gepinntes managed_components/lvgl__lvgl/scripts/LVGLImage.py
    -> LVGL-C-Image-Descriptor (RGB565A8, sofern die reale Darstellung
       korrekt ist - siehe --cf)

Das generierte Asset (main/generated/manuengineer_logo_168x24.cpp/.h) wird
eingecheckt: ein normaler Firmware-/CI-Build braucht weder rsvg-convert
noch LVGLImage.py und dessen Python-Abhaengigkeiten (pypng, lz4). Dieses
Skript laeuft nur beim bewussten Regenerieren.

Ausgabeformat .cpp statt LVGLImage.py's eigenem .c: main/CMakeLists.txt
kompiliert die main-Component einheitlich mit "-std=gnu++17"; eine .c-Datei
wuerde denselben, komponentenweiten C++-Dialektflag erhalten und fehlschlagen
("-std=gnu++17 is valid for C++/ObjC++ but not for C"). Der unveraenderte
Dateiinhalt (C99-Bezeichnungsinitialisierer) ist unter gnu++17 als
GNU-Erweiterung gueltiges C++.

Kein eigener SVG-Renderer wird gebaut: die Rasterisierung ist vollstaendig
rsvg-convert (librsvg) delegiert, die Bin-/C-Kodierung vollstaendig dem
gepinnten LVGL-eigenen Konverter.

Provenienz (Eingabe-SHA256, rsvg-convert-Version, LVGL-Version/
Converterpfad, Konvertierungsargumente, generierte Groesse,
clang-format-Version und Ausgabe-SHA256) wird als Kommentarblock in die
generierte .c-Datei
geschrieben und zusaetzlich auf stdout ausgegeben, damit sie unabhaengig
vom generierten Artefakt nachvollziehbar bleibt.
"""

import argparse
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MASTER_SVG = REPO_ROOT / "assets" / "branding" / "manuengineer" / "ManuEngineer.svg"
LVGL_IMAGE_PY = (
    REPO_ROOT
    / "managed_components"
    / "lvgl__lvgl"
    / "scripts"
    / "LVGLImage.py"
)
OUTPUT_DIR = REPO_ROOT / "main" / "generated"
ASSET_NAME = "manuengineer_logo_168x24"
TARGET_WIDTH = 168
TARGET_HEIGHT = 24
COLOR_FORMAT = "RGB565A8"


class BrandingAssetError(RuntimeError):
    pass


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def run_capture(cmd: list) -> str:
    result = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if result.returncode != 0:
        raise BrandingAssetError(
            f"command failed ({' '.join(cmd)}):\n{result.stderr}"
        )
    return result.stdout


def format_generated_sources(*paths: Path) -> str:
    formatter = shutil.which("clang-format-18")
    if formatter is None:
        raise BrandingAssetError(
            "clang-format-18 not found on PATH; generated branding output "
            "must pass the repository's existing format gate"
        )
    version = run_capture([formatter, "--version"]).strip()
    result = subprocess.run(
        [formatter, "-i", *(str(path) for path in paths)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise BrandingAssetError(
            f"clang-format-18 failed for generated branding output:\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return version


def clang_format_version() -> str:
    formatter = shutil.which("clang-format-18")
    if formatter is None:
        raise BrandingAssetError(
            "clang-format-18 not found on PATH; generated branding output "
            "must pass the repository's existing format gate"
        )
    return run_capture([formatter, "--version"]).strip()


def rsvg_convert_version() -> str:
    try:
        output = run_capture(["rsvg-convert", "--version"])
    except FileNotFoundError as error:
        raise BrandingAssetError(
            "rsvg-convert not found on PATH. Install it (e.g. "
            "'sudo apt-get install librsvg2-bin') before regenerating the "
            "branding asset. No SVG rasterizer is hand-rolled by this "
            "script."
        ) from error
    return output.strip()


def lvgl_version() -> str:
    idf_component_yml = (
        REPO_ROOT / "managed_components" / "lvgl__lvgl" / "idf_component.yml"
    )
    if idf_component_yml.exists():
        match = re.search(
            r'^version:\s*"?([^"\n]+)"?',
            idf_component_yml.read_text(encoding="utf-8"),
            re.MULTILINE,
        )
        if match:
            return match.group(1).strip()
    return "unknown (idf_component.yml not found/parseable)"


def convert_svg_to_png(svg_path: Path, png_path: Path) -> None:
    cmd = [
        "rsvg-convert",
        "-w",
        str(TARGET_WIDTH),
        "-h",
        str(TARGET_HEIGHT),
        "--format=png",
        str(svg_path),
        "-o",
        str(png_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise BrandingAssetError(f"rsvg-convert failed:\n{result.stderr}")
    if not png_path.exists():
        raise BrandingAssetError("rsvg-convert did not produce an output file")


def convert_png_to_lvgl_c(png_path: Path, out_dir: Path, name: str) -> Path:
    cmd = [
        sys.executable,
        str(LVGL_IMAGE_PY),
        "--ofmt",
        "C",
        "--cf",
        COLOR_FORMAT,
        "-o",
        str(out_dir),
        "--name",
        name,
        str(png_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise BrandingAssetError(
            f"LVGLImage.py failed:\n{result.stdout}\n{result.stderr}"
        )
    generated = out_dir / f"{name}.c"
    if not generated.exists():
        raise BrandingAssetError(
            f"LVGLImage.py did not produce {generated} (looked for exactly "
            "this filename)"
        )
    return generated


def patch_missing_reserved_2_field(c_contents: str) -> str:
    """The pinned LVGLImage.py's C template does not initialize
    lv_image_dsc_t::reserved_2 (a field this LVGL 9.6 version's struct
    genuinely has - see managed_components/lvgl__lvgl/include/lvgl/draw/
    lv_image_dsc.h - that the converter's own template predates). C
    silently zero-initializes the omitted trailing member, but this
    project's C++ build treats -Wmissing-field-initializers as an error,
    so the descriptor fails to compile as emitted. This appends the
    missing initializer to the one line the converter always emits
    (`.reserved = NULL,`), rather than reimplementing any part of the
    conversion itself.
    """
    patched, count = re.subn(
        r"(\.reserved\s*=\s*NULL,)",
        r"\1\n  .reserved_2 = NULL,",
        c_contents,
        count=1,
    )
    if count != 1:
        raise BrandingAssetError(
            "expected LVGLImage.py output to contain exactly one "
            "'.reserved = NULL,' line to patch with '.reserved_2 = NULL,' "
            "- the converter's template may have changed; update this "
            "patch function accordingly instead of silently skipping it"
        )
    return patched


def patch_const_definition_linkage(c_contents: str, name: str) -> str:
    """LVGLImage.py's C template defines the descriptor as
    `const lv_image_dsc_t <name> = {...};` with no explicit `extern`. In C,
    that already has external linkage; in C++ (this file is compiled as
    C++ - see the module docstring), a namespace-scope `const` without
    `extern` has *internal* linkage by default, so the header's
    `extern const lv_image_dsc_t <name>;` declaration used elsewhere would
    silently resolve to nothing at link time. Adding the explicit `extern`
    keyword to the definition restores external linkage without changing
    any value.
    """
    pattern = re.compile(
        rf"(?<!extern )(const lv_image_dsc_t {re.escape(name)} = \{{)"
    )
    patched, count = pattern.subn(rf"extern \1", c_contents, count=1)
    if count != 1:
        raise BrandingAssetError(
            f"expected LVGLImage.py output to contain exactly one "
            f"'const lv_image_dsc_t {name} = {{' definition to patch with "
            "an explicit 'extern' - the converter's template may have "
            "changed; update this patch function accordingly instead of "
            "silently skipping it"
        )
    return patched


def render_header(name: str) -> str:
    return f"""// GENERATED FILE - DO NOT EDIT BY HAND.
//
// Regenerate with:
//   python3 scripts/generate_branding_asset.py
//
// The productive R1 header logo as a real LVGL image asset, converted
// from the master SVG (never overwritten):
//   assets/branding/manuengineer/ManuEngineer.svg
// See {name}.cpp for full provenance (input SHA-256, rsvg-convert version,
// LVGL version, conversion arguments, output size and SHA-256).
#pragma once

#ifdef __has_include
    #if __has_include("lvgl.h")
        #if !defined(LV_LVGL_H_INCLUDE_SIMPLE) && !defined(LV_LVGL_H_INCLUDE_SYSTEM) && !defined(LV_BUILD_TEST)
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_LVGL_H_INCLUDE_SYSTEM)
#include <lvgl.h>
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

extern const lv_image_dsc_t {name};
"""


def generate(*, keep_intermediate_png: Path | None = None) -> dict:
    if not MASTER_SVG.exists():
        raise BrandingAssetError(f"master SVG not found: {MASTER_SVG}")
    if not LVGL_IMAGE_PY.exists():
        raise BrandingAssetError(
            f"pinned LVGLImage.py not found: {LVGL_IMAGE_PY}"
        )

    input_sha256 = sha256_of(MASTER_SVG)
    rsvg_version = rsvg_convert_version()
    lvgl_ver = lvgl_version()
    clang_format_ver = clang_format_version()

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        png_path = tmp_path / f"{ASSET_NAME}.png"
        convert_svg_to_png(MASTER_SVG, png_path)
        if keep_intermediate_png is not None:
            shutil.copy2(png_path, keep_intermediate_png)

        generated_c = convert_png_to_lvgl_c(png_path, tmp_path, ASSET_NAME)
        c_contents = generated_c.read_text(encoding="utf-8")
        c_contents = patch_missing_reserved_2_field(c_contents)
        c_contents = patch_const_definition_linkage(c_contents, ASSET_NAME)

    args_display = (
        f"rsvg-convert -w {TARGET_WIDTH} -h {TARGET_HEIGHT} --format=png "
        f"<ManuEngineer.svg> -o <tmp.png>  |  LVGLImage.py --ofmt C "
        f"--cf {COLOR_FORMAT} --name {ASSET_NAME} <tmp.png>"
    )

    provenance_lines = [
        "// GENERATED FILE - DO NOT EDIT BY HAND.",
        "//",
        "// Regenerate with:",
        "//   python3 scripts/generate_branding_asset.py",
        "//",
        "// Provenance:",
        f"//   input: assets/branding/manuengineer/ManuEngineer.svg",
        f"//   input SHA-256: {input_sha256}",
        f"//   rsvg-convert version: {rsvg_version}",
        f"//   LVGL version (managed_components/lvgl__lvgl): {lvgl_ver}",
        "//   LVGL converter: managed_components/lvgl__lvgl/scripts/LVGLImage.py",
        f"//   clang-format version: {clang_format_ver}",
        f"//   conversion: {args_display}",
        f"//   output size: {TARGET_WIDTH}x{TARGET_HEIGHT}, color format {COLOR_FORMAT}",
        "//",
    ]

    output_c_path = OUTPUT_DIR / f"{ASSET_NAME}.cpp"
    output_h_path = OUTPUT_DIR / f"{ASSET_NAME}.h"
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    full_c_contents = "\n".join(provenance_lines) + "\n" + c_contents
    # The output SHA-256 covers the final, checked-in file including the
    # provenance header, so a reader can verify the exact bytes without a
    # second, separately-hashed artifact.
    output_c_path.write_text(full_c_contents, encoding="utf-8")
    output_sha256 = sha256_of(output_c_path)
    output_size_bytes = output_c_path.stat().st_size

    # Re-write with the output hash/size appended, then re-hash is not
    # meaningful (self-referential); instead the output hash/size are
    # reported on stdout and in a small sidecar provenance note, not
    # inside the hashed file itself.
    output_h_path.write_text(render_header(ASSET_NAME), encoding="utf-8")
    clang_format_ver = format_generated_sources(output_c_path, output_h_path)
    output_sha256 = sha256_of(output_c_path)
    output_size_bytes = output_c_path.stat().st_size

    provenance_md_path = OUTPUT_DIR / f"{ASSET_NAME}.PROVENANCE.md"
    provenance_md_path.write_text(
        "\n".join(
            [
                f"# Provenance: {ASSET_NAME}",
                "",
                "Generated file, do not edit by hand. Regenerate with:",
                "",
                "```",
                "python3 scripts/generate_branding_asset.py",
                "```",
                "",
                f"- Input: `assets/branding/manuengineer/ManuEngineer.svg`",
                f"- Input SHA-256: `{input_sha256}`",
                f"- rsvg-convert version: `{rsvg_version}`",
                f"- LVGL version (managed_components/lvgl__lvgl): `{lvgl_ver}`",
                "- LVGL converter: `managed_components/lvgl__lvgl/scripts/LVGLImage.py`",
                f"- clang-format version: `{clang_format_ver}`",
                f"- Conversion: `{args_display}`",
                f"- Output size: {TARGET_WIDTH}x{TARGET_HEIGHT}, color format {COLOR_FORMAT}",
                f"- Output file: `{output_c_path.relative_to(REPO_ROOT)}`",
                f"- Output file size: {output_size_bytes} bytes",
                f"- Output file SHA-256: `{output_sha256}`",
                "",
            ]
        ),
        encoding="utf-8",
    )

    provenance = {
        "input_path": str(MASTER_SVG.relative_to(REPO_ROOT)),
        "input_sha256": input_sha256,
        "rsvg_convert_version": rsvg_version,
        "lvgl_version": lvgl_ver,
        "clang_format_version": clang_format_ver,
        "conversion_args": args_display,
        "output_width": TARGET_WIDTH,
        "output_height": TARGET_HEIGHT,
        "color_format": COLOR_FORMAT,
        "output_c_path": str(output_c_path.relative_to(REPO_ROOT)),
        "output_c_size_bytes": output_size_bytes,
        "output_c_sha256": output_sha256,
        "provenance_doc_path": str(provenance_md_path.relative_to(REPO_ROOT)),
    }
    return provenance


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--keep-png",
        type=Path,
        default=None,
        help="Optional: die generierte 168x24-PNG-Zwischendatei zusaetzlich hierhin kopieren (Debug/Layoutpruefung)",
    )
    args = parser.parse_args()

    try:
        provenance = generate(keep_intermediate_png=args.keep_png)
    except BrandingAssetError as error:
        print(f"FAILED: {error}", file=sys.stderr)
        return 1

    print("PASS: generated branding asset")
    for key, value in provenance.items():
        print(f"  {key}: {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
