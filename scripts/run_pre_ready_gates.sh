#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

readonly SELF_CHECK_BASE_REF=refs/remotes/origin/main
SELF_CHECK_MERGE_BASE=

readonly CLANG_TIDY_FILES=(
    include/app_config.hpp
    lib/device_platform/src/device_platform.cpp
    lib/device_platform/src/virtual_time_source.cpp
    lib/fermentation_app/src/fermentation_application.cpp
    lib/fermentation_app/src/process_state_machine.cpp
    lib/fermentation_app/src/program_model.cpp
    lib/fermentation_app/src/run_commands.cpp
    lib/fermentation_app/src/run_snapshot.cpp
    lib/fermentation_app/src/standard_program_catalog.cpp
    src/main.cpp
)

usage() {
    printf 'Usage: %s host|esp|self-check\n' "${BASH_SOURCE[0]}" >&2
}

if [[ $# -ne 1 || ( "$1" != "host" && "$1" != "esp" && "$1" != "self-check" ) ]]; then
    usage
    exit 2
fi

phase=$1

if [[ -n "${PRE_READY_EXPECTED_HEAD:-}" ]]; then
    current_head=$(git rev-parse HEAD)
    if [[ "$current_head" != "$PRE_READY_EXPECTED_HEAD" ]]; then
        printf 'FAILED: PRE_READY_EXPECTED_HEAD=%s, aktueller HEAD=%s\n' \
            "$PRE_READY_EXPECTED_HEAD" "$current_head" >&2
        exit 1
    fi
fi

require_command() {
    local command_name=$1
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'BLOCKED: erforderliches Werkzeug fehlt: %s\n' "$command_name" >&2
        exit 1
    fi
}

verify_platformio() {
    require_command pio
    local version_output
    version_output=$(pio --version 2>&1)
    if ! grep -Fqx 'PlatformIO Core, version 6.1.19' <<<"$version_output"; then
        printf 'FAILED: PlatformIO 6.1.19 erwartet, gefunden:\n%s\n' \
            "$version_output" >&2
        exit 1
    fi
}

verify_clang_major() {
    local command_name=$1
    require_command "$command_name"
    local version_line
    version_line=$("$command_name" --version 2>&1 | head -n 1)
    if ! printf '%s\n' "$version_line" | grep -Eq \
        '(^|[^0-9])18(\.[0-9]+)*([^0-9]|$)'; then
        printf 'FAILED: %s aus Major-Linie 18 erwartet, gefunden:\n%s\n' \
            "$command_name" "$version_line" >&2
        exit 1
    fi
}

verify_python3() {
    require_command python3
}

verify_host_toolchain() {
    verify_platformio
    verify_clang_major clang-format
    verify_clang_major clang-tidy
    verify_python3
}

verify_expected_esp_environment() {
    verify_python3
    if [[ -z "${IDF_PATH:-}" || ! -d "$IDF_PATH" ]]; then
        printf 'BLOCKED: IDF_PATH zeigt auf kein ESP-IDF-Verzeichnis.\n' >&2
        exit 1
    fi
    if [[ -z "${IDF_TOOLS_PATH:-}" || ! -d "$IDF_TOOLS_PATH" ]]; then
        printf 'BLOCKED: IDF_TOOLS_PATH zeigt auf kein ESP-IDF-Tools-Verzeichnis.\n' >&2
        exit 1
    fi
    require_command idf.py
}

run_clang_tidy() {
    clang-tidy -p . "$@"
}

verify_self_check_base() {
    if [[ -n "${STATIC_ANALYSIS_BASE_SHA:-}" ]]; then
        printf 'FAILED: STATIC_ANALYSIS_BASE_SHA ist im Self-Check nicht zulaessig; verwendet wird %s mit dessen Merge-Base.\n' \
            "$SELF_CHECK_BASE_REF" >&2
        exit 1
    fi

    if ! git show-ref --verify --quiet "$SELF_CHECK_BASE_REF"; then
        printf 'BLOCKED: erforderliche lokale Base-Referenz fehlt: %s\n' \
            "$SELF_CHECK_BASE_REF" >&2
        exit 1
    fi

    local base_ref_commit
    if ! base_ref_commit=$(git rev-parse --verify "$SELF_CHECK_BASE_REF^{commit}"); then
        printf 'FAILED: lokale Base-Referenz ist kein Commit: %s\n' \
            "$SELF_CHECK_BASE_REF" >&2
        exit 1
    fi

    if ! SELF_CHECK_MERGE_BASE=$(git merge-base HEAD "$SELF_CHECK_BASE_REF"); then
        printf 'FAILED: Merge-Base zwischen HEAD und %s konnte nicht bestimmt werden.\n' \
            "$SELF_CHECK_BASE_REF" >&2
        exit 1
    fi

    if [[ -z "$SELF_CHECK_MERGE_BASE" ]] ||
        ! git merge-base --is-ancestor "$SELF_CHECK_MERGE_BASE" HEAD ||
        ! git merge-base --is-ancestor "$SELF_CHECK_MERGE_BASE" "$SELF_CHECK_BASE_REF"; then
        printf 'FAILED: ungueltige Merge-Base fuer HEAD und %s.\n' \
            "$SELF_CHECK_BASE_REF" >&2
        exit 1
    fi

    printf 'STATIC_ANALYSIS_BASE_REF=%s\n' "$SELF_CHECK_BASE_REF"
    printf 'STATIC_ANALYSIS_BASE_REF_COMMIT=%s\n' "$base_ref_commit"
    printf 'STATIC_ANALYSIS_MERGE_BASE=%s\n' "$SELF_CHECK_MERGE_BASE"
}

collect_changed_c_cpp_files() {
    local scope=$1
    local path
    local changed_files=()

    mapfile -t changed_files < <(
        git diff --name-only --diff-filter=ACMRTUXB \
            "$SELF_CHECK_MERGE_BASE...HEAD"
    )

    for path in "${changed_files[@]}"; do
        [[ -f "$path" ]] || continue
        case "$path" in
            *.cpp|*.hpp|*.h) ;;
            *) continue ;;
        esac

        if [[ "$scope" == "format" ]]; then
            case "$path" in
                src/*|include/*|lib/*|test/*|main/*) printf '%s\n' "$path" ;;
            esac
        else
            case "$path" in
                include/*|lib/device_platform/*|lib/fermentation_app/*|src/main.cpp)
                    printf '%s\n' "$path"
                    ;;
            esac
        fi
    done
}

run_self_check() {
    verify_self_check_base
    verify_host_toolchain

    local format_files=()
    mapfile -t format_files < <(collect_changed_c_cpp_files format)
    if ((${#format_files[@]} > 0)); then
        clang-format --dry-run --Werror "${format_files[@]}"
        printf 'CLANG_FORMAT=PASS\n'
    else
        printf 'CLANG_FORMAT=NOT_REQUIRED\n'
    fi

    local native_production_files=()
    mapfile -t native_production_files < <(collect_changed_c_cpp_files native)
    if ((${#native_production_files[@]} > 0)); then
        pio run -e native -t compiledb
        run_clang_tidy "${CLANG_TIDY_FILES[@]}"
        printf 'CLANG_TIDY=PASS\n'
    else
        printf 'CLANG_TIDY=NOT_REQUIRED\n'
    fi

    printf 'BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS\n'
}

run_host_gates() {
    verify_host_toolchain

    clang-format --dry-run --Werror \
        $(find src include lib test main -type f \( \
            -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \))

    set -o pipefail
    python3 scripts/build_report.py --output build-report.md 2>&1 \
        | tee platformio-build.log

    pio test -e native
    pio run -e native -t compiledb
    run_clang_tidy "${CLANG_TIDY_FILES[@]}"

    python3 scripts/check_architecture_boundaries.py
    python3 scripts/check_secrets.py
    python3 scripts/selftest_quality_gates.py
}

run_esp_gates() {
    verify_expected_esp_environment

    python3 scripts/build_esp_idf_profiles.py all

    local report_arguments=(
        --output build-report.md
        --append
        --esp-idf-profiles bringup release
    )
    if [[ -n "${SOURCE_GIT_SHA:-}" ]]; then
        report_arguments+=(--source-git-sha "$SOURCE_GIT_SHA")
    fi
    python3 scripts/build_report.py "${report_arguments[@]}"

    python3 scripts/run_esp_idf_static_analysis.py all
}

case "$phase" in
    host)
        run_host_gates
        printf 'PRE_READY_HOST_GATES=PASS\n'
        ;;
    esp)
        run_esp_gates
        printf 'PRE_READY_ESP_GATES=PASS\n'
        ;;
    self-check)
        run_self_check
        ;;
esac
