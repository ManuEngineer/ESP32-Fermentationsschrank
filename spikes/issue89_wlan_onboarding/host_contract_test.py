#!/usr/bin/env python3
"""Candidates-neutral Issue #89 Phase A/B host oracle.

This is a test oracle only.  It intentionally models a volatile browser
candidate and never implements a production credential record, slot policy,
fallback policy, storage epoch, DNS server, reconnect manager, or web server.
"""

from __future__ import annotations

import json
import re
import unittest
from dataclasses import dataclass


SSID_MAX_BYTES = 32
WPA2_PASSWORD_MIN_BYTES = 8
WPA2_PASSWORD_MAX_BYTES = 63


def validate_candidate(ssid: bytes, password: bytes, auth_mode: str) -> bool:
    """Validate only the volatile candidate against IDF/802.11 bounds."""

    if not isinstance(ssid, bytes) or not isinstance(password, bytes):
        return False
    if not 1 <= len(ssid) <= SSID_MAX_BYTES or b"\x00" in ssid:
        return False
    if auth_mode != "WPA2_PSK":
        return False
    return WPA2_PASSWORD_MIN_BYTES <= len(password) <= WPA2_PASSWORD_MAX_BYTES


def redact(text: str, secrets: tuple[str, ...]) -> str:
    """Redact secrets before a diagnostic or evidence string is emitted."""

    result = text
    for secret in secrets:
        if secret:
            result = result.replace(secret, "<redacted>")
    result = re.sub(r"([?&](?:password|passphrase|token|secret)=)[^&\s]*", r"\1<redacted>", result, flags=re.I)
    return result


def wifi_qr_payload(ssid: str, password: str) -> str:
    """Create the standard payload in RAM; callers must not persist or log it."""

    def escape(value: str) -> str:
        return value.replace("\\", "\\\\").replace(";", "\\;").replace(",", "\\,").replace(":", "\\:")

    return f"WIFI:T:WPA;S:{escape(ssid)};P:{escape(password)};;"


@dataclass
class VolatileCandidate:
    """A browser candidate used only until the owner-selected commit boundary."""

    ssid: bytes
    password: bytes
    tested: bool = False
    confirmed: bool = False


class VolatileCommitOracle:
    """Common test oracle; it has no persistence and no fallback selection."""

    def __init__(self, current_ssid: bytes) -> None:
        self.current_ssid = current_ssid
        self.applied_ssid = current_ssid

    def commit(self, candidate: VolatileCandidate) -> str:
        if not candidate.tested or not candidate.confirmed:
            return "NOT_COMMITTED"
        self.applied_ssid = candidate.ssid
        return "COMMITTED_BY_SELECTED_OWNER"


class Issue89HostOracleTests(unittest.TestCase):
    def test_validation_uses_bytes_and_rejects_unknown_modes(self) -> None:
        self.assertTrue(validate_candidate(b"home", b"synthetic-pass", "WPA2_PSK"))
        self.assertFalse(validate_candidate(b"", b"synthetic-pass", "WPA2_PSK"))
        self.assertFalse(validate_candidate(b"home", b"short", "WPA2_PSK"))
        self.assertFalse(validate_candidate(b"home", b"synthetic-pass", "UNKNOWN"))

    def test_failed_or_unconfirmed_candidate_preserves_current_network(self) -> None:
        oracle = VolatileCommitOracle(b"existing-home")
        candidate = VolatileCandidate(b"new-home", b"synthetic-pass", tested=False, confirmed=False)
        self.assertEqual(oracle.commit(candidate), "NOT_COMMITTED")
        self.assertEqual(oracle.applied_ssid, b"existing-home")

    def test_only_tested_and_confirmed_candidate_reaches_oracle_boundary(self) -> None:
        oracle = VolatileCommitOracle(b"existing-home")
        candidate = VolatileCandidate(b"new-home", b"synthetic-pass", tested=True, confirmed=True)
        self.assertEqual(oracle.commit(candidate), "COMMITTED_BY_SELECTED_OWNER")
        self.assertEqual(oracle.applied_ssid, b"new-home")

    def test_redaction_covers_logs_urls_and_json_diagnostics(self) -> None:
        secrets = ("synthetic-home-password", "synthetic-ap-password", "synthetic-token")
        raw = json.dumps(
            {
                "url": "/connect?ssid=home&password=synthetic-home-password",
                "log": "AP synthetic-ap-password token=synthetic-token",
            }
        )
        safe = redact(raw, secrets)
        for secret in secrets:
            self.assertNotIn(secret, safe)
        self.assertIn("password=<redacted>", safe)

    def test_qr_payload_is_volatile_and_escaped(self) -> None:
        payload = wifi_qr_payload("home;ssid", "synthetic-home-password")
        self.assertIn(r"S:home\;ssid", payload)
        self.assertIn("P:synthetic-home-password", payload)
        self.assertNotIn("synthetic-home-password", redact(payload, ("synthetic-home-password",)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
