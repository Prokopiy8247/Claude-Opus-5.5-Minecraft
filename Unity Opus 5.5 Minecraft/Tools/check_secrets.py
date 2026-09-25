"""Fail when tracked files contain common secrets or local machine identifiers."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path


ROOT = Path(subprocess.check_output(
    ["git", "rev-parse", "--show-toplevel"], cwd=Path(__file__).resolve().parent, text=True
).strip())
SKIP = {Path(__file__).resolve()}
BINARY_SUFFIXES = {
    ".blend", ".dll", ".exe", ".fbx", ".gif", ".ico", ".icns",
    ".jpg", ".jpeg", ".mp3", ".ogg", ".pdf", ".png", ".psd", ".so", ".zip",
}
SUSPICIOUS_NAMES = re.compile(
    r"(^|/)(\.env($|\.)|id_(rsa|dsa|ecdsa|ed25519)(\.pub)?$|credentials?\.(json|ya?ml)$|"
    r"service[-_]?account.*\.json$|.*\.(p12|pfx|jks|keystore|pem|key)$)", re.I
)
PATTERNS = {
    "private key": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"),
    "AWS access key": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
    "Google API key": re.compile(r"\bAIza[0-9A-Za-z_-]{35}\b"),
    "GitHub token": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b"),
    "OpenAI-style key": re.compile(r"\bsk-[A-Za-z0-9_-]{20,}\b"),
    "Slack token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]+\b"),
    "local user path": re.compile(r"[A-Za-z]:[\\/]Users[\\/][^\\/\s]+", re.I),
    "Unity cloud project ID": re.compile(r"cloudProjectId:[ \t]*[0-9a-f]{8}-[0-9a-f-]{27,}", re.I),
    "Unity organization ID": re.compile(r"organizationId:[ \t]*\S+", re.I),
    "assigned secret": re.compile(
        r"(?i)(?:api[_-]?key|client[_-]?secret|access[_-]?token|auth[_-]?token|password)"
        r"[ \t]*[:=][ \t]*['\"][^'\"]{8,}['\"]"
    ),
}


def tracked_files() -> list[Path]:
    raw = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT)
    return [ROOT / item.decode("utf-8") for item in raw.split(b"\0") if item]


def main() -> int:
    failures: list[str] = []
    for path in tracked_files():
        relative = path.relative_to(ROOT).as_posix()
        if SUSPICIOUS_NAMES.search(relative):
            failures.append(f"sensitive filename: {relative}")
        if path.resolve() in SKIP or path.suffix.lower() in BINARY_SUFFIXES or not path.is_file():
            continue
        if path.stat().st_size > 10 * 1024 * 1024:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for label, pattern in PATTERNS.items():
            if pattern.search(text):
                failures.append(f"{label}: {relative}")
    if failures:
        print("Potential sensitive data found:")
        for failure in sorted(set(failures)):
            print(f"  - {failure}")
        return 1
    print("Secret check passed for all tracked files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
