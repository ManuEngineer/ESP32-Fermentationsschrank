#!/usr/bin/env python3
"""Beweist, dass die Qualitaetspruefungen absichtlich fehlerhafte Faelle erkennen.

Alle Fixtures werden in einem temporaeren Verzeichnis erzeugt und wieder
entfernt. Dadurch bleibt `main` immer gruen; kein absichtlich fehlerhafter Fall
wird jemals in dieses Repository eingecheckt.

Ergebnis je Teilpruefung: PASS oder FAILED. Wird ein Werkzeug selbst nicht
gefunden (z. B. clang-format/clang-tidy lokal nicht installiert), ist das
Ergebnis BLOCKED statt eines falschen PASS oder FAILED.
"""

import copy
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

BLOCKED = "BLOCKED"
PASS = "PASS"
FAILED = "FAILED"


def report(name: str, status: str) -> None:
    print(f"{status}: {name}")


def selftest_format() -> str:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        return BLOCKED

    with tempfile.TemporaryDirectory() as tmp:
        bad_file = Path(tmp) / "badly_formatted.cpp"
        bad_file.write_text("int f(int a,int b){return a+b;}\n")

        result = subprocess.run(
            [clang_format, "--dry-run", "--Werror", str(bad_file)],
            capture_output=True,
            text=True,
        )
        return FAILED if result.returncode == 0 else PASS


def selftest_static_analysis() -> str:
    clang_tidy = shutil.which("clang-tidy")
    if clang_tidy is None:
        return BLOCKED

    with tempfile.TemporaryDirectory() as tmp:
        bad_file = Path(tmp) / "missing_braces.cpp"
        bad_file.write_text(
            "bool f(bool b) {\n    if (b) return true;\n    return false;\n}\n"
        )

        result = subprocess.run(
            [
                clang_tidy,
                "--checks=-*,readability-braces-around-statements",
                "--warnings-as-errors=*",
                str(bad_file),
                "--",
                "-std=c++17",
            ],
            capture_output=True,
            text=True,
        )
        return FAILED if result.returncode == 0 else PASS


def run_script_selftest(repo_root: Path, script_name: str) -> str:
    result = subprocess.run(
        [sys.executable, str(repo_root / "scripts" / script_name), "--selftest"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return PASS if result.returncode == 0 else FAILED


# --- Pre-Ready-Attestierungs-Wrapper (Issue #184) -----------------------------
# Fixture-Test mit lokalem Bare-origin, Stub-Runner und Stub-gh. Der echte
# Runner und das echte gh werden nie aufgerufen; kein Netzwerkzugriff. Die
# Temp-Repos isolieren die Git-Konfiguration und setzen Identitaet und
# Default-Branch explizit.

STUB_GH = """#!/usr/bin/env bash
case "$1" in
auth) [[ -z "${GH_FAIL_AUTH:-}" ]]; exit ;;
repo) printf '%s\\n' "${GH_REPO_NAME:-owner/repo}"; exit 0 ;;
api)
    state=
    for arg in "$@"; do
        [[ "$arg" != state=* ]] || state=${arg#state=}
    done
    status=OK
    [[ "$state" != "${GH_FAIL_STATE:-}" ]] || status=FAILED
    { printf '%s' "$status"; printf '\x1f%s' "$@"; printf '\n'; } >>"$GH_LOG"
    [[ "$status" == OK ]] || exit 1
    if [[ "$state" == pending && -n "${GH_HOOK_ON_PENDING:-}" ]]; then
        bash -c "$GH_HOOK_ON_PENDING"
    fi
    exit 0 ;;
esac
exit 1
"""

STUB_RUNNER = """#!/usr/bin/env bash
printf '%s|%s\\n' "$1" "${PRE_READY_EXPECTED_HEAD:-}" >>"$RUN_LOG"
if [[ -n "${STUB_HOOK:-}" && "$1" == "${STUB_HOOK_PHASE:-}" ]]; then
    bash -c "$STUB_HOOK"
fi
[[ "$1" != "${STUB_FAIL_PHASE:-}" ]]
"""

PUSH_OTHER_HOOK = (
    'bash -c \'set -e; cd "$FIXTURE_ROOT"; '
    '[ -d other ] || git clone -q "$ORIGIN" other; cd other; git fetch -q; '
    'git switch -q -C "$1" "origin/$1"; echo x > o.txt; git add -A; '
    'git commit -q -m other; git push -q origin "HEAD:refs/heads/$1"\' _ '
)


class WrapperFixture:
    def __init__(self, root, branch_files, branch_removes):
        self.root = root
        self.origin = root / "origin.git"
        self.work = root / "work"
        self.gh_log = root / "gh.log"
        self.run_log = root / "run.log"
        bin_dir = root / "bin"
        self.env = {
            "PATH": f"{bin_dir}:{os.environ['PATH']}",
            "HOME": str(root),
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_AUTHOR_NAME": "t",
            "GIT_AUTHOR_EMAIL": "t@example.invalid",
            "GIT_COMMITTER_NAME": "t",
            "GIT_COMMITTER_EMAIL": "t@example.invalid",
            "GH_LOG": str(self.gh_log),
            "RUN_LOG": str(self.run_log),
            "ORIGIN": str(self.origin),
            "FIXTURE_ROOT": str(root),
            "IDF_PATH": str(root / "idf"),
            "IDF_TOOLS_PATH": str(root / "idf_tools"),
        }
        bin_dir.mkdir()
        (bin_dir / "gh").write_text(STUB_GH)
        (bin_dir / "gh").chmod(0o755)
        (root / "idf").mkdir()
        (root / "idf" / "export.sh").write_text(":\n")
        (root / "idf_tools").mkdir()
        self.git("init", "-q", "--bare", "-b", "main", str(self.origin), cwd=root)
        self.git("init", "-q", "-b", "main", str(self.work), cwd=root)
        self.git("config", f"url.{self.origin}.insteadOf",
                 "https://github.com/owner/repo.git")
        self.git("remote", "add", "origin", "https://github.com/owner/repo.git")
        scripts = self.work / "scripts"
        scripts.mkdir()
        shutil.copy(
            Path(__file__).resolve().parent / "run_pre_ready_and_publish.sh", scripts
        )
        (scripts / "run_pre_ready_gates.sh").write_text(STUB_RUNNER)
        for name, text in {
            "docs/a.md": "# base\n", "tool.sh": "echo 0\n", "README.md": "r\n",
        }.items():
            self.write(name, text)
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "base")
        self.git("push", "-q", "origin", "main")
        self.git("switch", "-q", "-c", "feat")
        for name, text in branch_files.items():
            self.write(name, text)
        for name in branch_removes:
            self.git("rm", "-q", name)
        self.git("add", "-A")
        if self.git("status", "--porcelain").stdout.strip():
            self.git("commit", "-q", "-m", "feature")
        self.git("push", "-q", "-u", "origin", "feat")

    def git(self, *args, cwd=None):
        return subprocess.run(
            ["git", *args], cwd=cwd or self.work, env=self.env,
            capture_output=True, text=True, check=True,
        )

    def write(self, name, text):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def head(self):
        return self.git("rev-parse", "HEAD").stdout.strip()

    def push_other(self, ref):
        subprocess.run(
            ["bash", "-c", PUSH_OTHER_HOOK + ref], env=self.env, check=True,
            capture_output=True,
        )

    def minimal_path(self):
        # Nur git, bash und dirname: gh ist garantiert nicht auffindbar.
        minimal = self.root / "minimal"
        minimal.mkdir()
        for tool in ("git", "bash", "dirname"):
            (minimal / tool).symlink_to(shutil.which(tool))
        return str(minimal)

    def run(self, env_overrides, args=()):
        env = dict(self.env)
        for key, value in env_overrides.items():
            if value is None:
                env.pop(key, None)
            elif value == "@MINIMAL@":
                env[key] = self.minimal_path()
            else:
                env[key] = str(value)
        self.tested_head = self.head()
        result = subprocess.run(
            ["bash", "scripts/run_pre_ready_and_publish.sh", *args],
            cwd=self.work, env=env, capture_output=True, text=True,
        )
        posts = []
        if self.gh_log.exists():
            for line in self.gh_log.read_text().splitlines():
                outcome, *call = line.split("\x1f")
                fields = dict(
                    item.split("=", 1) for item in call[4:] if "=" in item
                )
                posts.append({
                    "outcome": outcome,
                    "prefix": call[:4],
                    "endpoint": call[3] if len(call) > 3 else "",
                    "keys": sorted(fields),
                    "state": fields.get("state"),
                    "context": fields.get("context"),
                    "description": fields.get("description"),
                })
        phases = []
        if self.run_log.exists():
            phases = [line.split("|") for line in self.run_log.read_text().splitlines()]
        return result, posts, phases


def wrapper_scenarios():
    md = {"docs/a.md": "# a\n"}
    code = {"tool.sh": "echo 1\n"}
    hook_main = PUSH_OTHER_HOOK + "main"
    hook_feat = PUSH_OTHER_HOOK + "feat"

    def states(posts):
        return [post["state"] for post in posts if post["outcome"] == "OK"]

    def passed(fx, result, posts, phases, description, ran):
        return (
            result.returncode == 0
            and states(posts) == ["pending", "success"]
            and posts[-1]["description"] == f"{fx.tested_head[:12]} {description}"
            and [phase[0] for phase in phases] == ran
            and all(phase[1] == fx.tested_head for phase in phases)
        )

    def full_pass(fx, result, posts, phases):
        return passed(fx, result, posts, phases, "host+esp PASS", ["host", "esp"]) \
            and "PRE_READY_LOCAL_GATES=PASS" in result.stdout

    def markdown_only(fx, result, posts, phases):
        return passed(fx, result, posts, phases, "MARKDOWN_ONLY_NOT_REQUIRED", []) \
            and "PRE_READY_LOCAL_GATES=NOT_REQUIRED_MARKDOWN_ONLY" in result.stdout \
            and "PRE_READY_LOCAL_GATES=PASS" not in result.stdout

    def failed_after_pending(ran, kind="FAILED"):
        def check(fx, result, posts, phases):
            return (
                result.returncode != 0
                and states(posts) == ["pending", "failure"]
                and posts[-1]["description"].startswith(
                    f"{fx.tested_head[:12]} {kind}: ")
                and f"PRE_READY_LOCAL_GATES={kind}" in result.stdout
                and "PRE_READY_LOCAL_GATES=PASS" not in result.stdout
                and [phase[0] for phase in phases] == ran
            )
        return check

    def blocked_after_pending(fx, result, posts, phases):
        return failed_after_pending([], "BLOCKED")(fx, result, posts, phases)

    def rejected_early(fx, result, posts, phases):
        return result.returncode != 0 and posts == [] and phases == []

    def api_error_on_pending(fx, result, posts, phases):
        return (
            result.returncode != 0 and phases == [] and states(posts) == []
            and [post["outcome"] for post in posts] == ["FAILED"]
        )

    def api_error_on_success(fx, result, posts, phases):
        return (
            result.returncode != 0
            and "success" not in states(posts)
            and "PRE_READY_LOCAL_GATES=PASS" not in result.stdout
        )

    def gh_missing(fx, result, posts, phases):
        return (
            result.returncode != 0 and posts == [] and phases == []
            and "BLOCKED: gh fehlt" in result.stderr
        )

    def usage_error(fx, result, posts, phases):
        return result.returncode == 2 and posts == [] and phases == []

    def make_dirty(fx):
        fx.write("untracked.txt", "x\n")

    def make_unpushed(fx):
        fx.git("commit", "-q", "--allow-empty", "-m", "local only")

    def make_main_ahead(fx):
        fx.push_other("main")

    def drop_upstream(fx):
        fx.git("branch", "--unset-upstream")

    def detach(fx):
        fx.git("checkout", "-q", "--detach")

    def switch_main(fx):
        fx.git("switch", "-q", "main")

    mid_run = {"STUB_HOOK_PHASE": "esp"}
    # (Name, Branch-Dateien, Branch-Loeschungen, Vorbereitung, Env, Args, Pruefung)
    return [
        ("Diff mit Nicht-Markdown: host+esp PASS", code, (), None, {}, (), full_pass),
        ("Markdown-only: Runner nicht aufgerufen, keine ESP-Umgebung", md, (), None,
         {"IDF_PATH": None, "IDF_TOOLS_PATH": None}, (), markdown_only),
        ("Mischdiff .md + .sh: kein Bypass", {**md, **code}, (), None, {}, (),
         full_pass),
        ("Mischdiff .md + .cpp/.yml/.toml: kein Bypass",
         {**md, "a.cpp": "1\n", "b.yml": "1\n", "c.toml": "1\n"}, (), None, {}, (),
         full_pass),
        ("Loeschung einer Nicht-Markdown-Datei + .md: kein Bypass", md,
         ("tool.sh",), None, {}, (), full_pass),
        ("Umbenennung Nicht-Markdown -> .md (gleicher Inhalt): kein Bypass",
         {"tool.md": "echo 0\n"}, ("tool.sh",), None, {}, (), full_pass),
        ("Umbenennung .md -> Nicht-Markdown: kein Bypass", {"docs/a.txt": "# base\n"},
         ("docs/a.md",), None, {}, (), full_pass),
        ("Grossgeschriebenes .MD gilt nicht als Markdown", {"X.MD": "x\n"}, (), None,
         {}, (), full_pass),
        ("leerer Diff: FULL, kein Bypass", {}, (), None, {}, (), full_pass),
        ("host FAIL: failure, esp nicht gestartet", code, (), None,
         {"STUB_FAIL_PHASE": "host"}, (), failed_after_pending(["host"])),
        ("esp FAIL: failure", code, (), None, {"STUB_FAIL_PHASE": "esp"}, (),
         failed_after_pending(["host", "esp"])),
        ("FULL ohne IDF_PATH: BLOCKED ohne Gatelauf", code, (), None,
         {"IDF_PATH": None}, (), blocked_after_pending),
        ("FULL ohne IDF_TOOLS_PATH: BLOCKED ohne Gatelauf", code, (), None,
         {"IDF_TOOLS_PATH": None}, (), blocked_after_pending),
        ("gh fehlt: BLOCKED, kein Gatelauf, kein Status", code, (),
         lambda fx: None, {"PATH": "@MINIMAL@"}, (), gh_missing),
        ("HEAD aendert sich waehrend des Laufs", code, (), None,
         {**mid_run, "STUB_HOOK": "git commit -q --allow-empty -m moved"}, (),
         failed_after_pending(["host", "esp"])),
        ("Arbeitsbaum wird waehrend des Laufs schmutzig", code, (), None,
         {**mid_run, "STUB_HOOK": "echo x > scratch.txt"}, (),
         failed_after_pending(["host", "esp"])),
        ("main laeuft waehrend des Laufs weiter", code, (), None,
         {**mid_run, "STUB_HOOK": hook_main}, (),
         failed_after_pending(["host", "esp"])),
        ("Remote-Branch wird waehrend des Laufs weitergeschoben", code, (), None,
         {**mid_run, "STUB_HOOK": hook_feat}, (),
         failed_after_pending(["host", "esp"])),
        ("Markdown-only: main laeuft nach pending weiter", md, (), None,
         {"GH_HOOK_ON_PENDING": hook_main}, (), failed_after_pending([])),
        ("Arbeitsbaum vorher schmutzig: kein Gatelauf", code, (), make_dirty, {}, (),
         rejected_early),
        ("Upstream-Abweichung (ungepushter Commit): kein Gatelauf", code, (),
         make_unpushed, {}, (), rejected_early),
        ("origin/main nicht Vorfahre von HEAD: kein Gatelauf", code, (),
         make_main_ahead, {}, (), rejected_early),
        ("kein Upstream: BLOCKED", code, (), drop_upstream, {}, (), rejected_early),
        ("detached HEAD: BLOCKED", code, (), detach, {}, (), rejected_early),
        ("Branch main: BLOCKED", code, (), switch_main, {}, (), rejected_early),
        ("gh nicht authentifiziert: BLOCKED", code, (), None, {"GH_FAIL_AUTH": 1},
         (), rejected_early),
        ("falsches gh-Repository: BLOCKED", code, (), None,
         {"GH_REPO_NAME": "someone/else"}, (), rejected_early),
        ("API-Fehler bei pending: kein Gatelauf", code, (), None,
         {"GH_FAIL_STATE": "pending"}, (), api_error_on_pending),
        ("API-Fehler bei success: kein stilles PASS", code, (), None,
         {"GH_FAIL_STATE": "success"}, (), api_error_on_success),
        ("Aufruf mit Argument: Usage, Exit 2", code, (), None, {}, ("x",),
         usage_error),
    ]


def valid_posts(fixture, posts) -> bool:
    """Jeder publizierte Status: exakt Context und Ziel-SHA, nur erlaubte
    States und Felder, kein target_url."""
    endpoint = f"repos/owner/repo/statuses/{fixture.tested_head}"
    return all(
        post["prefix"] == ["api", "-X", "POST", endpoint]
        and post["context"] == "pre-ready/local"
        and post["state"] in ("pending", "success", "failure")
        and post["keys"] == ["context", "description", "state"]
        and len(post["description"]) <= 140
        for post in posts
    )


def selftest_pre_ready_attestation() -> str:
    if shutil.which("git") is None or shutil.which("bash") is None:
        return BLOCKED
    script = Path(__file__).resolve().parent / "run_pre_ready_and_publish.sh"
    if subprocess.run(["bash", "-n", str(script)]).returncode != 0:
        return FAILED

    for name, files, removes, prepare, env, args, check in wrapper_scenarios():
        with tempfile.TemporaryDirectory() as tmp:
            fixture = WrapperFixture(Path(tmp), files, removes)
            if prepare is not None:
                prepare(fixture)
            result, posts, phases = fixture.run(env, args)
            if not check(fixture, result, posts, phases) or not valid_posts(
                fixture, posts
            ):
                print(
                    f"  Wrapper-Szenario FAILED: {name}\n"
                    f"    rc={result.returncode} posts={posts} phases={phases}\n"
                    f"    stderr={result.stderr.strip()[-300:]}"
                )
                return FAILED
    return PASS


# --- Heavy-CI-Trigger-Vertrag (Issue #184) ------------------------------------
# Die positive Pfadliste in .github/workflows/build.yml steuert, wann die
# schwere Clean-Room-CI automatisch laeuft. Der Check leitet die Pflichtpfade
# aus dem Runner (SSOT), dem Wrapper, den Profilen und sdkconfig.defaults ab
# und haelt zusaetzlich eine bewusst kleine, redundante Schutzliste der
# direkten Produktions-Buildinputs, damit keiner davon unbemerkt aus dem
# Workflow entfernt werden kann.

PROTECTED_BUILD_CONTRACT_FILES = (
    "CMakeLists.txt",
    "main/CMakeLists.txt",
    "main/Kconfig.projbuild",
    "lib/device_platform/CMakeLists.txt",
    "lib/device_platform_esp_idf/CMakeLists.txt",
    "lib/fermentation_app/CMakeLists.txt",
    "sdkconfig.defaults",
)


def workflow_trigger_problems(workflow, runner_text, sdkconfig_text, profiles, exists):
    """Liefert eine Liste von Vertragsverletzungen (leer = Vertrag erfuellt)."""
    problems = []
    triggers = workflow.get("on", workflow.get(True)) or {}
    if "workflow_dispatch" not in triggers:
        problems.append("workflow_dispatch fehlt")
    if "push" in triggers:
        problems.append("push-Trigger ist nicht zulaessig")
    pull_request = triggers.get("pull_request") or {}
    if "paths-ignore" in pull_request:
        problems.append("paths-ignore ist nicht zulaessig")
    paths = pull_request.get("paths")
    if not paths:
        problems.append("pull_request.paths fehlt")
        paths = []

    for path in paths:
        if not exists(path):
            problems.append(f"Pfadfilter nennt nicht vorhandene Datei: {path}")

    required = {
        ".github/workflows/build.yml",
        "scripts/run_pre_ready_gates.sh",
        "scripts/run_pre_ready_and_publish.sh",
        "scripts/selftest_quality_gates.py",
    }
    required.update(re.findall(r"scripts/[A-Za-z0-9_]+\.(?:py|sh)", runner_text))
    required.update(f"sdkconfig.defaults.{profile}" for profile in profiles)
    required.update(PROTECTED_BUILD_CONTRACT_FILES)
    partition = re.search(
        r'^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="([^"]+)"', sdkconfig_text, re.M
    )
    if partition is None:
        problems.append("Partitionstabelle in sdkconfig.defaults nicht ableitbar")
    else:
        required.add(partition.group(1))
    for path in sorted(required - set(paths)):
        problems.append(f"Pfadfilter deckt Gate-/Buildinput nicht ab: {path}")

    job = (workflow.get("jobs") or {}).get("firmware") or {}
    if "workflow_dispatch" not in str(job.get("if", "")):
        problems.append("Job-if laesst workflow_dispatch nicht ausdruecklich zu")
    if "||" not in str((workflow.get("concurrency") or {}).get("group", "")):
        problems.append("concurrency.group ohne Fallback fuer workflow_dispatch")
    if "||" not in str((job.get("env") or {}).get("SOURCE_GIT_SHA", "")):
        problems.append("SOURCE_GIT_SHA ohne Fallback fuer workflow_dispatch")
    return problems


def selftest_workflow_trigger_contract(repo_root: Path) -> str:
    try:
        import yaml
    except ImportError:
        return BLOCKED
    import esp_idf_contract

    workflow = yaml.safe_load((repo_root / ".github/workflows/build.yml").read_text())
    runner_text = (repo_root / "scripts/run_pre_ready_gates.sh").read_text()
    sdkconfig_text = (repo_root / "sdkconfig.defaults").read_text()

    def problems(wf, runner=runner_text, sdkconfig=sdkconfig_text):
        return workflow_trigger_problems(
            wf, runner, sdkconfig, esp_idf_contract.PROFILES,
            lambda path: (repo_root / path).exists(),
        )

    if problems(workflow):
        for problem in problems(workflow):
            print(f"  Trigger-Vertrag verletzt: {problem}")
        return FAILED

    def mutated(change):
        wf = copy.deepcopy(workflow)
        change(wf)
        return wf

    def pr(wf):
        return wf[True]["pull_request"] if True in wf else wf["on"]["pull_request"]

    def trigs(wf):
        return wf[True] if True in wf else wf["on"]

    def drop(path):
        return lambda wf: pr(wf)["paths"].remove(path)

    def drop_any(prefix):
        return lambda wf: pr(wf).update(
            paths=[p for p in pr(wf)["paths"] if not p.startswith(prefix)]
        )

    bad_cases = [
        ("workflow_dispatch fehlt", mutated(lambda wf: trigs(wf).pop("workflow_dispatch")), {}),
        ("push-Trigger", mutated(lambda wf: trigs(wf).update(push={})), {}),
        ("paths-ignore", mutated(lambda wf: pr(wf).update({"paths-ignore": ["**/*.md"]})), {}),
        ("nicht vorhandener Pfad", mutated(lambda wf: pr(wf)["paths"].append("nope/x.txt")), {}),
        ("Partitionstabelle entfernt", mutated(drop("partitions/issue_90_state_store.csv")), {}),
        ("Kconfig.projbuild entfernt", mutated(drop("main/Kconfig.projbuild")), {}),
        ("Komponenten-CMakeLists entfernt", mutated(drop("lib/fermentation_app/CMakeLists.txt")), {}),
        ("Wrapper entfernt", mutated(drop("scripts/run_pre_ready_and_publish.sh")), {}),
        ("Runner-Skript nicht abgedeckt", mutated(drop("scripts/check_secrets.py")), {}),
        ("Profil-Overlay entfernt", mutated(drop("sdkconfig.defaults.release")), {}),
        ("Dispatch-Guard fehlt",
         mutated(lambda wf: wf["jobs"]["firmware"].update({"if": "github.event.pull_request.draft == false"})), {}),
        ("concurrency ohne Fallback",
         mutated(lambda wf: wf["concurrency"].update(group="x-${{ github.event.pull_request.number }}")), {}),
        ("SOURCE_GIT_SHA ohne Fallback",
         mutated(lambda wf: wf["jobs"]["firmware"]["env"].update(SOURCE_GIT_SHA="${{ github.event.pull_request.head.sha }}")), {}),
        ("neues Runner-Skript ohne Pfadfilter", workflow,
         {"runner": runner_text + "\npython3 scripts/new_gate_tool.py\n"}),
        ("Partitionstabelle nicht ableitbar", workflow, {"sdkconfig": "# leer\n"}),
    ]
    for name, wf, kwargs in bad_cases:
        if not problems(wf, **kwargs):
            print(f"  Trigger-Vertrag erkennt Fall nicht: {name}")
            return FAILED
    return PASS


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent

    results = {
        "Format-Pruefung erkennt absichtlich fehlerhaften Fall": selftest_format(),
        "Static-Analysis erkennt absichtlich fehlerhaften Fall": selftest_static_analysis(),
        "Pre-Ready-Wrapper publiziert success nur nach bestandenen Pruefungen "
        "(Fixture-Szenarien)": selftest_pre_ready_attestation(),
        "Heavy-CI-Trigger-/Buildinput-Vertrag der build.yml wird eingehalten "
        "und erkennt absichtlich fehlerhafte Faelle": selftest_workflow_trigger_contract(
            repo_root
        ),
        "Geheimnispruefung erkennt absichtlich fehlerhaften Fall": run_script_selftest(
            repo_root, "check_secrets.py"
        ),
        "Architekturpruefung erkennt absichtliche Grenzverletzung": run_script_selftest(
            repo_root, "check_architecture_boundaries.py"
        ),
        "Profil-/Driftpruefung erkennt absichtlich fehlerhafte Faelle": run_script_selftest(
            repo_root, "check_build_profiles.py"
        ),
        "Buildtreiber meldet Erfolg erst nach bestandenem Guard": run_script_selftest(
            repo_root, "build_esp_idf_profiles.py"
        ),
        "Ressourcenbericht kombiniert nativen Host- und ESP-IDF-Bericht, "
        "erkennt fehlende Artefakte": run_script_selftest(repo_root, "build_report.py"),
        "CI-Artefakt-Scanabdeckung erkennt ungescannt hochgeladene "
        "Textartefakte": run_script_selftest(
            repo_root, "check_ci_artifact_scan_coverage.py"
        ),
        "ESP-IDF-Static-Analysis-Treiber (esp-clang) erkennt fehlerhafte "
        "Werkzeug-/Dateiauswahlfaelle": run_script_selftest(
            repo_root, "run_esp_idf_static_analysis.py"
        ),
    }

    for name, status in results.items():
        report(name, status)

    if FAILED in results.values():
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
