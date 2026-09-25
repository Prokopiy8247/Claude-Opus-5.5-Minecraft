"""Sums the real API token usage recorded in this project's Claude Code session transcripts, prices it at the
Claude Opus 5.5 API rates and measures the active working time.

Claude Code writes one JSON line per event; assistant messages carry the API "usage" block of the request that
produced them. A single API response can be split over several lines (one per content block), all sharing the
same message id, so usage is counted once per message id (the last record wins). Subagent transcripts live in
<session>/subagents/*.jsonl and are included. Synthetic records (API errors written by the client) carry no usage.

Rates: Claude Opus 5.5 on the Claude API (platform.claude.com/docs/en/about-claude/pricing, checked 2026-09-25),
USD per 1M tokens: input 4.00, 5-minute cache writes 5.00, 1-hour cache writes 8.00, cache hits 0.20, output 20.00.
The 1M context window is billed at standard rates (no long-context premium). input_tokens already excludes cache
reads and cache writes, so the classes are disjoint.

Active time: the model responses and tool results of the main session and the subagents on one timeline; gaps of
up to PAUSE_MIN minutes are continuous work (tool runs such as Unity builds, long generations), longer silences and
the stalls around API errors (rate limits, outages) are pauses.

Usage: python Tools/token_usage.py <session_jsonl> [--until 2026-09-25T03:33:00Z] [--json out.json]
"""
import glob, json, os, sys
from datetime import datetime

RATES = {"input": 4.00, "cache_write_5m": 5.00, "cache_write_1h": 8.00, "cache_read": 0.20, "output": 20.00}
PAUSE_MIN = 30


def when(s):
    return datetime.fromisoformat(s.replace("Z", "+00:00"))


def scan(path, until):
    """Usage per message id and the activity timeline (time, is_api_error) of one transcript."""
    usage, events = {}, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            try:
                e = json.loads(line)
            except ValueError:
                continue
            t = e.get("timestamp")
            if not t or (until and when(t) > until):
                continue
            m = e.get("message") if isinstance(e.get("message"), dict) else {}
            if e.get("type") == "assistant":
                synthetic = m.get("model") == "<synthetic>"
                events.append((when(t), synthetic))
                if not synthetic and isinstance(m.get("usage"), dict):
                    usage[m.get("id") or e.get("uuid")] = m["usage"]
            elif e.get("type") == "user":
                c = m.get("content")
                if isinstance(c, list) and any(isinstance(b, dict) and b.get("type") == "tool_result" for b in c):
                    events.append((when(t), False))
    return usage, events


def tally(usage):
    t = dict.fromkeys(RATES, 0)
    for u in usage.values():
        t["input"] += int(u.get("input_tokens") or 0)
        t["cache_read"] += int(u.get("cache_read_input_tokens") or 0)
        t["output"] += int(u.get("output_tokens") or 0)
        written = int(u.get("cache_creation_input_tokens") or 0)
        split = u.get("cache_creation") if isinstance(u.get("cache_creation"), dict) else {}
        hour = int(split.get("ephemeral_1h_input_tokens") or 0)
        t["cache_write_1h"] += hour
        t["cache_write_5m"] += written - hour  # writes without a TTL split count as the 5-minute cache
    return t


def active_seconds(events):
    events = sorted(events)
    total = 0.0
    for (a, a_error), (b, b_error) in zip(events, events[1:]):
        gap = (b - a).total_seconds()
        if gap <= PAUSE_MIN * 60 and not a_error and not b_error:
            total += gap
    return total


def hms(seconds):
    s = int(seconds)
    return f"{s // 3600:02d}:{s % 3600 // 60:02d}:{s % 60:02d}"


def main():
    argv = sys.argv[1:]
    opts = {}
    for flag in ("--json", "--until"):
        if flag in argv:
            i = argv.index(flag)
            opts[flag] = argv[i + 1] if i + 1 < len(argv) else None
            argv = argv[:i] + argv[i + 2:]
    path = argv[0] if argv else os.environ.get("CLAUDE_SESSION_JSONL")
    if not path:
        print("Pass the Claude session .jsonl path or set CLAUDE_SESSION_JSONL.", file=sys.stderr)
        return 2
    until = when(opts["--until"]) if opts.get("--until") else None

    files = [path] + sorted(glob.glob(os.path.join(os.path.splitext(path)[0], "subagents", "*.jsonl")))
    usage, events, per_file = {}, [], []
    for p in files:
        u, ev = scan(p, until)
        usage.update(u)
        events += ev
        per_file.append({"file": os.path.basename(p), "requests": len(u), "active_time": hms(active_seconds(ev)), **tally(u)})
    tokens = tally(usage)
    cost = {k: tokens[k] / 1e6 * RATES[k] for k in RATES}
    active = active_seconds(events)
    report = {
        "requests": len(usage),
        "tokens": tokens,
        "total_tokens": sum(tokens.values()),
        "cost_usd_claude_opus_5_5": {**{k: round(v, 2) for k, v in cost.items()}, "total": round(sum(cost.values()), 2)},
        "active_time": hms(active),
        "active_minutes": round(active / 60, 1),
        "first_activity": min(e[0] for e in events).isoformat() if events else None,
        "last_activity": max(e[0] for e in events).isoformat() if events else None,
        "files": per_file,
    }
    print(f"requests: {report['requests']:,}")
    for k, v in tokens.items():
        print(f"  {k:15s} {v:>15,} tokens  x ${RATES[k]:.2f}/M = ${cost[k]:,.2f}")
    print(f"  {'total':15s} {report['total_tokens']:>15,} tokens            ${sum(cost.values()):,.2f} (Claude Opus 5.5 API equivalent)")
    print(f"active time: {report['active_time']} ({report['active_minutes']} min), activity {report['first_activity']} -> {report['last_activity']}")
    if opts.get("--json"):
        with open(opts["--json"], "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
