#!/usr/bin/env python3
"""Check a supported subset of C++ component boundaries without a compiler."""

import argparse
from pathlib import Path
import sys

from dependencies import check_dependencies
from discovery import discover_files
from facts import Location, OperationalError
from includes import check_includes
from ownership import Ownership
from query import load_facts
from report import Report, RULES


REPO_ROOT = Path(__file__).resolve().parents[2]


def audit(root, inventory, facts):
    report = Report()
    ownership = Ownership(facts, inventory)
    for gap in facts.limitations:
        for rule in RULES:
            report.skip(rule, gap.location, gap.reason)
    if not ownership.components:
        for rule in RULES:
            report.skip(rule, Location("."), "no supported C++ :api component boundaries found")
    for package in sorted(ownership.uncertain_boundaries):
        target = ownership.targets[f"//{package}:api"]
        for rule in RULES:
            report.skip(rule, target.location,
                        f"unsupported API boundary kind {target.kind}: {target.label}")
    check_includes(root, inventory, facts, ownership, report)
    check_dependencies(facts, ownership, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPO_ROOT,
                        help="repository root (default: relative to this script)")
    args = parser.parse_args(argv)
    root = args.repo.resolve()
    try:
        inventory = discover_files(root)
        facts = load_facts(root, inventory)
        report = audit(root, inventory, facts)
    except OperationalError as error:
        print(f"Operational error: {error}", file=sys.stderr)
        return 2
    print(report.render())
    return int(report.failed)


if __name__ == "__main__":
    sys.exit(main())
