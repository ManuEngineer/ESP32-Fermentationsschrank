#!/usr/bin/env bash
# Fuehrt den lokalen Pre-Ready-Lauf auf dem gepushten PR-HEAD aus und
# publiziert das Ergebnis als SHA-gebundenen GitHub-Commit-Status
# `pre-ready/local` (docs/tasks/issue-184-local-pre-ready-github-status-plan.md).
#
# Der Gate-Owner bleibt scripts/run_pre_ready_gates.sh; dieser Wrapper enthaelt
# weder eine Testliste noch eine Toolchain-Wahrheit und ruft nur dessen Phasen
# `host` und `esp` auf. Alle Pruefungen sind fail-closed: ohne vollstaendig
# bestandene Pruefungen wird nie `success` publiziert.

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

readonly STATUS_CONTEXT=pre-ready/local
readonly RUNNER=scripts/run_pre_ready_gates.sh

if [[ $# -ne 0 ]]; then
    printf 'Usage: %s\n' "${BASH_SOURCE[0]}" >&2
    exit 2
fi

BRANCH=
TESTED_HEAD=
TESTED_SHORT=
MAIN_BEFORE=
REPO_NAME=
CLASS=
PENDING_PUBLISHED=false
SUCCESS_PUBLISHED=false
FAIL_KIND=FAILED
FAIL_REASON=
STATE_ERROR=

publish_status() {
    local state=$1
    local description=${2:0:140}
    gh api -X POST "repos/$REPO_NAME/statuses/$TESTED_HEAD" \
        -f state="$state" \
        -f context="$STATUS_CONTEXT" \
        -f description="$description" >/dev/null
}

on_exit() {
    local exit_status=$?
    trap - EXIT
    if [[ "$PENDING_PUBLISHED" == true && "$SUCCESS_PUBLISHED" != true ]]; then
        publish_status failure \
            "$TESTED_SHORT $FAIL_KIND: ${FAIL_REASON:-abgebrochen}" || true
        printf 'PRE_READY_LOCAL_GATES=%s\n' "$FAIL_KIND"
        printf 'PRE_READY_TESTED_HEAD=%s\n' "$TESTED_HEAD"
        [[ $exit_status -ne 0 ]] || exit_status=1
    fi
    exit "$exit_status"
}
trap on_exit EXIT

blocked() {
    printf 'PRE_READY_LOCAL_GATES=BLOCKED\n' >&2
    printf 'BLOCKED: %s\n' "$1" >&2
    exit 1
}

# Abbruch nach pending: der Trap publiziert failure (GitHub kennt kein
# `blocked`); FAIL_KIND bestimmt Beschreibung und lokales Ergebnis.
fail() {
    FAIL_REASON=$1
    printf '%s: %s\n' "$FAIL_KIND" "$1" >&2
    exit 1
}

blocked_after_pending() {
    FAIL_KIND=BLOCKED
    fail "$1"
}

fetch_remote_state() {
    git fetch --quiet origin \
        "+refs/heads/main:refs/remotes/origin/main" \
        "+refs/heads/$BRANCH:refs/remotes/origin/$BRANCH"
}

# Setzt STATE_ERROR und liefert 1, wenn der geprueften Basis etwas widerspricht.
verify_repo_state() {
    local expected_head=$1
    local expected_main=$2
    STATE_ERROR=

    if ! fetch_remote_state; then
        STATE_ERROR="git fetch origin fehlgeschlagen"
        return 1
    fi
    if [[ -n "$(git status --porcelain)" ]]; then
        STATE_ERROR="Arbeitsbaum nicht sauber"
        return 1
    fi

    local head remote_branch remote_main
    head=$(git rev-parse HEAD)
    remote_branch=$(git rev-parse "refs/remotes/origin/$BRANCH")
    remote_main=$(git rev-parse refs/remotes/origin/main)

    if [[ -n "$expected_head" && "$head" != "$expected_head" ]]; then
        STATE_ERROR="HEAD hat sich geaendert"
        return 1
    fi
    if [[ "$head" != "$remote_branch" ]]; then
        STATE_ERROR="HEAD entspricht nicht origin/$BRANCH"
        return 1
    fi
    if [[ -n "$expected_main" && "$remote_main" != "$expected_main" ]]; then
        STATE_ERROR="origin/main hat sich waehrend des Laufs geaendert"
        return 1
    fi
    if ! git merge-base --is-ancestor "$remote_main" "$head"; then
        STATE_ERROR="origin/main ist kein Vorfahre von HEAD"
        return 1
    fi
}

# MARKDOWN_ONLY nur bei nicht leerem Diff, dessen Pfade alle auf .md enden;
# jeder andere Fall (auch ein Fehler des Diff-Aufrufs) ist FULL.
classify_diff() {
    local files=() path
    mapfile -d '' files < <(
        git diff -z --name-only --no-renames "$MAIN_BEFORE" "$TESTED_HEAD"
    )
    if ((${#files[@]} == 0)); then
        printf 'FULL\n'
        return
    fi
    for path in "${files[@]}"; do
        if [[ "$path" != *.md ]]; then
            printf 'FULL\n'
            return
        fi
    done
    printf 'MARKDOWN_ONLY\n'
}

# 1. Voraussetzungen
command -v git >/dev/null 2>&1 || blocked "git fehlt"
command -v gh >/dev/null 2>&1 || blocked "gh fehlt"
gh auth status >/dev/null 2>&1 || blocked "gh ist nicht authentifiziert"

# 2. normaler Branch, nicht main
BRANCH=$(git symbolic-ref --quiet --short HEAD) || blocked "detached HEAD"
[[ "$BRANCH" != main ]] || blocked "Branch main ist nicht zulaessig"

# 4. Upstream muss origin/<branch> sein
UPSTREAM=$(git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' \
    2>/dev/null) || blocked "kein Upstream gesetzt"
[[ "$UPSTREAM" == "origin/$BRANCH" ]] ||
    blocked "Upstream ist $UPSTREAM, erwartet origin/$BRANCH"

# Repo-Identitaet: Status nur auf das Repository von origin publizieren.
REPO_NAME=$(gh repo view --json nameWithOwner --jq .nameWithOwner) ||
    blocked "gh repo view fehlgeschlagen"
ORIGIN_URL=$(git config --get remote.origin.url) || blocked "origin fehlt"
ORIGIN_NAME=${ORIGIN_URL#https://github.com/}
ORIGIN_NAME=${ORIGIN_NAME#git@github.com:}
ORIGIN_NAME=${ORIGIN_NAME%.git}
[[ "$ORIGIN_NAME" == "$REPO_NAME" ]] ||
    blocked "gh-Repository $REPO_NAME entspricht nicht origin $ORIGIN_NAME"

# 3., 5.-7. sauberer Baum, Fetch, HEAD == origin/<branch>, main ist Vorfahre
verify_repo_state "" "" || blocked "$STATE_ERROR"

# 8. gepruefter Stand
TESTED_HEAD=$(git rev-parse HEAD)
TESTED_SHORT=${TESTED_HEAD:0:12}
MAIN_BEFORE=$(git rev-parse refs/remotes/origin/main)

# 9. Klassifikation aus dem Diff; kein Flag, kein Umgebungsschalter
CLASS=$(classify_diff)

# 10. pending; ohne publizierbaren Status ist der Lauf wertlos
publish_status pending "pre-ready/local pending $TESTED_SHORT" ||
    blocked "Status pending konnte nicht publiziert werden"
PENDING_PUBLISHED=true

# 11. Gates (nur bei FULL)
if [[ "$CLASS" == FULL ]]; then
    [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/export.sh" ]] ||
        blocked_after_pending "IDF_PATH/export.sh fehlt"
    [[ -n "${IDF_TOOLS_PATH:-}" && -d "$IDF_TOOLS_PATH" ]] ||
        blocked_after_pending "IDF_TOOLS_PATH fehlt"

    export PRE_READY_EXPECTED_HEAD=$TESTED_HEAD
    bash "$RUNNER" host || fail "host-Phase fehlgeschlagen"
    (
        set +eu
        # shellcheck disable=SC1091
        . "$IDF_PATH/export.sh" || exit 1
        set -eu
        bash "$RUNNER" esp
    ) || fail "esp-Phase fehlgeschlagen"
fi

# 12. Abschlusspruefung (beide Klassen)
verify_repo_state "$TESTED_HEAD" "$MAIN_BEFORE" || fail "$STATE_ERROR"
[[ "$(classify_diff)" == "$CLASS" ]] || fail "Diff-Klassifikation hat sich geaendert"

# 13. success
if [[ "$CLASS" == FULL ]]; then
    RESULT=PASS
    SUCCESS_DESCRIPTION="$TESTED_SHORT host+esp PASS"
else
    RESULT=NOT_REQUIRED_MARKDOWN_ONLY
    SUCCESS_DESCRIPTION="$TESTED_SHORT MARKDOWN_ONLY_NOT_REQUIRED"
fi
publish_status success "$SUCCESS_DESCRIPTION" ||
    fail "Status success konnte nicht publiziert werden"
SUCCESS_PUBLISHED=true

printf 'PRE_READY_LOCAL_GATES=%s\n' "$RESULT"
printf 'PRE_READY_TESTED_HEAD=%s\n' "$TESTED_HEAD"
