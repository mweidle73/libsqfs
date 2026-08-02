#!/usr/bin/env python3

"""Check the archived Gitorious snapshot used as libsqfs provenance."""

import json
import sys
import urllib.request

SNAPSHOT = "48285f2d8cf74bc14b1c5d300ed5bfccbd2fbc4f"
MASTER = "c5c464b1a6d5c74447b9d5daa0815425bb5642bc"
URL = f"https://archive.softwareheritage.org/api/1/snapshot/{SNAPSHOT}/"


def main() -> int:
    with urllib.request.urlopen(URL, timeout=30) as response:
        payload = json.load(response)

    branch = payload.get("branches", {}).get("refs/heads/master")
    if payload.get("id") != SNAPSHOT:
        print("Software Heritage returned a different snapshot", file=sys.stderr)
        return 1
    if (
        not branch
        or branch.get("target_type") != "revision"
        or branch.get("target") != MASTER
    ):
        print("Archived master does not match the pinned revision", file=sys.stderr)
        return 1

    print(f"snapshot: {SNAPSHOT}")
    print(f"master:   {MASTER}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
