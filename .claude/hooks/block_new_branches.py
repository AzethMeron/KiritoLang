#!/usr/bin/env python3
"""PreToolUse hook: block ALL git branch creation. Commit and push to main only."""
from __future__ import annotations
import json
import re
import sys


def _extract_branch_name(cmd: str) -> str | None:
    m = re.match(r"\s*git\s+(\S+)\s+(.*)", cmd, re.DOTALL)
    if not m:
        return None
    subcmd, rest = m.group(1), m.group(2)
    if subcmd == "checkout":
        bm = re.match(r"-[Bb]\s+(\S+)", rest)
        return bm.group(1) if bm else None
    if subcmd == "switch":
        bm = re.match(r"-[Cc]\s+(\S+)", rest)
        return bm.group(1) if bm else None
    if subcmd == "branch":
        bm = re.match(r"(?!--)(?!-)(\S+)", rest)
        return bm.group(1) if bm else None
    return None


try:
    data = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(0)

cmd  = data.get("tool_input", {}).get("command", "")
name = _extract_branch_name(cmd)

if name:
    print(json.dumps({
        "decision": "block",
        "reason": (
            f"Branch creation blocked (attempted: '{name}'). "
            "ALL branch creation is forbidden. Commit and push directly to main."
        ),
    }), flush=True)
    sys.exit(2)
