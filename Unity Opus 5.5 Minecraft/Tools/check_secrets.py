"""Fail when files contain common secrets or local machine identifiers."""

from __future__ import annotations

import argparse
import re
import subprocess
from collections import defaultdict
from pathlib import Path


ROOT = Path(subprocess.check_output(
    ["git", "rev-parse", "--show-toplevel"], cwd=Path(__file__).resolve().parent, text=True
).strip())
SCRIPT_RELATIVE = Path(__file__).resolve().relative_to(ROOT).as_posix()
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


def scan_content(relative: str, content: bytes, failures: list[str], scope: str = "") -> None:
    if relative == SCRIPT_RELATIVE or Path(relative).suffix.lower() in BINARY_SUFFIXES:
        return
    if len(content) > 10 * 1024 * 1024:
        return
    text = content.decode("utf-8", errors="ignore")
    for label, pattern in PATTERNS.items():
        if pattern.search(text):
            failures.append(f"{scope}{label}: {relative}")


def scan_current(failures: list[str]) -> None:
    for path in tracked_files():
        relative = path.relative_to(ROOT).as_posix()
        if SUSPICIOUS_NAMES.search(relative):
            failures.append(f"sensitive filename: {relative}")
        if path.is_file():
            scan_content(relative, path.read_bytes(), failures)


def history_blobs(failures: list[str]) -> dict[str, set[str]]:
    """Return every unique blob reachable from HEAD and the paths that used it."""
    blobs: dict[str, set[str]] = defaultdict(set)
    commits = subprocess.check_output(["git", "rev-list", "HEAD"], cwd=ROOT, text=True).splitlines()
    for commit in commits:
        tree = subprocess.check_output(["git", "ls-tree", "-r", "-z", commit], cwd=ROOT)
        for entry in tree.split(b"\0"):
            if not entry:
                continue
            metadata, raw_path = entry.split(b"\t", 1)
            _mode, kind, object_id = metadata.decode("ascii").split()
            if kind != "blob":
                continue
            relative = raw_path.decode("utf-8", errors="surrogateescape")
            if SUSPICIOUS_NAMES.search(relative):
                failures.append(f"history sensitive filename: {relative}")
            if relative == SCRIPT_RELATIVE or Path(relative).suffix.lower() in BINARY_SUFFIXES:
                continue
            blobs[object_id].add(relative)
    return blobs


def scan_history(failures: list[str]) -> None:
    blobs = history_blobs(failures)
    if not blobs:
        return

    object_ids = list(blobs)
    request = "".join(f"{object_id}\n" for object_id in object_ids)
    metadata = subprocess.check_output(
        ["git", "cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize)"],
        cwd=ROOT,
        input=request,
        text=True,
    ).splitlines()
    eligible = [
        object_id
        for object_id, kind, size in (line.split() for line in metadata)
        if kind == "blob" and int(size) <= 10 * 1024 * 1024
    ]

    payload = subprocess.check_output(
        ["git", "cat-file", "--batch"],
        cwd=ROOT,
        input="".join(f"{object_id}\n" for object_id in eligible).encode("ascii"),
    )
    position = 0
    for expected_id in eligible:
        header_end = payload.index(b"\n", position)
        object_id, kind, raw_size = payload[position:header_end].decode("ascii").split()
        if object_id != expected_id or kind != "blob":
            raise RuntimeError("Unexpected response from git cat-file --batch")
        size = int(raw_size)
        content_start = header_end + 1
        content_end = content_start + size
        content = payload[content_start:content_end]
        position = content_end + 1
        for relative in blobs[object_id]:
            scan_content(relative, content, failures, scope="history ")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--history", action="store_true", help="scan every file version reachable from HEAD")
    args = parser.parse_args()

    failures: list[str] = []
    if args.history:
        scan_history(failures)
    else:
        scan_current(failures)
    if failures:
        print("Potential sensitive data found:")
        for failure in sorted(set(failures)):
            print(f"  - {failure}")
        return 1
    target = "all file versions reachable from HEAD" if args.history else "all tracked files"
    print(f"Secret check passed for {target}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
