#!/usr/bin/env python3
"""Verify Issue-90 build provenance against the reachable GitHub PR history.

The local Git object store is deliberately not sufficient: an unpushed local
commit can satisfy ``git cat-file`` while being unavailable to reviewers.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path


SHA = re.compile(r"^[0-9a-f]{40}$")
DEFAULT_REPOSITORY = "ManuEngineer/ESP32-Fermentationsschrank"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def git(*arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments], cwd=repo_root(), text=True,
        capture_output=True, check=False,
    )
    if result.returncode != 0:
        raise SystemExit(result.stderr.strip() or f"git {' '.join(arguments)} failed")
    return result.stdout.strip()


def github_json(url: str) -> object:
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "issue-90-build-provenance-check",
            **({"Authorization": f"Bearer {token}"} if (token :=
                os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN"))
               else {}),
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            return json.load(response)
    except (urllib.error.URLError, urllib.error.HTTPError) as error:
        raise SystemExit(f"GitHub remote provenance lookup failed: {error}") from error


def require_sha(value: str, name: str) -> str:
    if not SHA.fullmatch(value):
        raise SystemExit(f"{name} must be a full 40-character commit SHA: {value}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-git-sha", required=True)
    parser.add_argument("--build-commit", required=True)
    parser.add_argument("--pr", type=int, default=117)
    parser.add_argument("--repository", default=DEFAULT_REPOSITORY)
    parser.add_argument(
        "--ci-merge-commit", action="store_true",
        help="allow Build-Commit to differ from Source-Git-SHA only as an "
             "explicit GitHub Actions merge commit",
    )
    args = parser.parse_args()

    source = require_sha(args.source_git_sha, "Source-Git-SHA")
    build = require_sha(args.build_commit, "Build-Commit")
    status = git("status", "--porcelain", "--untracked-files=all")
    if status:
        raise SystemExit("FAIL build provenance: checkout is not clean")
    head = require_sha(git("rev-parse", "HEAD"), "HEAD")
    if head != build:
        raise SystemExit(
            f"FAIL build provenance: Build-Commit {build} is not the checked-out HEAD {head}"
        )
    if not args.ci_merge_commit and build != source:
        raise SystemExit(
            "FAIL build provenance: local Build-Commit must equal Source-Git-SHA; "
            "a local temporary commit is not a valid substitute"
        )

    api_root = f"https://api.github.com/repos/{args.repository}"
    remote_commit = github_json(f"{api_root}/commits/{source}")
    if not isinstance(remote_commit, dict) or remote_commit.get("sha") != source:
        raise SystemExit("FAIL build provenance: Source-Git-SHA is not GitHub-resolvable")

    pr = github_json(f"{api_root}/pulls/{args.pr}")
    if not isinstance(pr, dict) or pr.get("state") != "open":
        raise SystemExit("FAIL build provenance: target PR is not open")
    pr_head = pr.get("head", {}).get("sha")
    require_sha(str(pr_head), "PR head")

    page = 1
    pr_commits: set[str] = set()
    while True:
        commits = github_json(
            f"{api_root}/pulls/{args.pr}/commits?per_page=100&page={page}"
        )
        if not isinstance(commits, list):
            raise SystemExit("FAIL build provenance: invalid GitHub PR commits response")
        pr_commits.update(
            item.get("sha") for item in commits
            if isinstance(item, dict) and isinstance(item.get("sha"), str)
        )
        if len(commits) < 100:
            break
        page += 1
    if source not in pr_commits:
        raise SystemExit(
            "FAIL build provenance: Source-Git-SHA is not an ancestor in the PR commit list"
        )
    if args.ci_merge_commit and build == str(pr_head):
        raise SystemExit(
            "FAIL build provenance: an explicitly marked CI merge commit must remain "
            "distinct from the PR Source-Git-SHA"
        )
    print(
        "PASS build-provenance "
        f"source={source} build={build} pr={args.repository}#{args.pr} "
        f"pr_head={pr_head} ci_merge={args.ci_merge_commit}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
