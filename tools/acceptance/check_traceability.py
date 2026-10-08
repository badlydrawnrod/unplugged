#!/usr/bin/env python3
"""Validate persisted Gherkin scenarios against adjacent GoogleTest bindings.

See README.md in this directory for the supported syntax and limitations.
"""

import argparse
from dataclasses import dataclass, field
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx", ".h", ".hpp"}
ANNOTATION = re.compile(r"\s*// (Feature|Scenario): (\S(?:.*\S)?)\s*")
ANNOTATION_MARKER = re.compile(r"\s*//\s*(Feature|Scenario)\b")
TEST_DEFINITION = re.compile(
    r"[ \t]*TEST(?:_F|_P)?\s*\(\s*[A-Za-z_]\w*\s*,\s*"
    r"[A-Za-z_]\w*\s*\)\s*\{")
# Mask comments and literals before looking for executable test definitions.
# Raw string delimiters are captured so embedded quotes/comments stay inert.
CPP_NONCODE = re.compile(
    r'(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\('
    r'.*?\)(?P=delimiter)")'
    r'|(?P<literal>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')'
    r'|(?P<block>/\*.*?\*/)'
    r'|(?P<line>//(?:\\\r?\n|[^\n])*)', re.DOTALL)


@dataclass(frozen=True)
class Location:
    path: str
    line: int

    def __str__(self):
        return f"{self.path}:{self.line}"


@dataclass
class Report:
    scenarios: dict = field(default_factory=dict)
    bindings: dict = field(default_factory=dict)
    errors: list = field(default_factory=list)

    def reject(self, location, message):
        self.errors.append(f"{location}: {message}")


def discover_files(root):
    """Use Git's inventory, including new files but excluding ignored output."""
    output = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=root)
    paths = {Path(os.fsdecode(value)) for value in output.split(b"\0") if value}
    return sorted(path for path in paths
                  if path.suffix in SOURCE_SUFFIXES | {".feature"}
                  and (root / path).is_file()
                  and not any((root / part).is_symlink()
                              for part in (path, *path.parents)))


def parse_feature(path, text, report):
    feature_seen = False
    scenario_seen = False
    current = None
    steps = 0

    def finish_scenario():
        if current is not None and steps == 0:
            report.reject(current, "scenario must contain at least one step")

    for number, line in enumerate(text.splitlines(), 1):
        value = line.strip()
        location = Location(path, number)
        if re.match(r"#\s*language\s*:", value):
            report.reject(location, "unsupported Gherkin language directive")
        elif not value or value.startswith("#"):
            continue
        elif value.startswith(("@", "|", '"""', "```")) or re.match(
                r"(?:Background|Rule|Scenario Outline|Scenario Template|"
                r"Examples|Example)\b", value):
            report.reject(location, "unsupported Gherkin construct")
        elif value.startswith("Feature:"):
            if feature_seen or scenario_seen or not value.removeprefix("Feature:").strip():
                report.reject(location, "expected one nonempty Feature title before scenarios")
            feature_seen = True
        elif value.startswith("Scenario:"):
            finish_scenario()
            scenario_seen = True
            current = location
            steps = 0
            title = value.removeprefix("Scenario:").strip()
            if not feature_seen or not title:
                report.reject(location, "expected a nonempty Scenario title after Feature")
                continue
            identity = (path, title)
            if identity in report.scenarios:
                report.reject(location, f"duplicate scenario identity; first at {report.scenarios[identity]}")
            else:
                report.scenarios[identity] = location
        elif re.match(r"(?:Given|When|Then|And|But|\*)\s+\S", value):
            if current is None:
                report.reject(location, "step must belong to a Scenario")
            steps += 1
        elif (not feature_seen or current is not None
              or re.match(r"[\w ]+:", value)
              or re.match(r"(?:Feature|Scenario|Given|When|Then|And|But)\b", value)):
            report.reject(location, "unsupported or malformed Gherkin syntax")
        # Free-form feature description is allowed before the first scenario.
    finish_scenario()
    if not feature_seen or not scenario_seen:
        report.reject(Location(path, 1), "feature must contain a Feature and at least one Scenario")


def mask_cpp(text):
    comments = {}
    inline_comments = set()

    def replace(match):
        if match.lastgroup == "line":
            number = text.count("\n", 0, match.start()) + 1
            line_start = text.rfind("\n", 0, match.start()) + 1
            comments[number] = match.group()
            if text[line_start:match.start()].strip():
                inline_comments.add(number)
        return re.sub(r"[^\n]", " ", match.group())

    return CPP_NONCODE.sub(replace, text), comments, inline_comments


def parse_bindings(path, text, feature_paths, report):
    code, comments, inline_comments = mask_cpp(text)
    lines = code.splitlines(keepends=True)
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line))
    consumed = set()
    for number, comment in sorted(comments.items()):
        if number in consumed or not ANNOTATION_MARKER.match(comment):
            continue
        location = Location(path, number)
        if number in inline_comments:
            report.reject(location, "malformed annotations; comments must occupy standalone lines")
            continue
        feature = ANNOTATION.fullmatch(comment)
        scenario = ANNOTATION.fullmatch(comments.get(number + 1, ""))
        if (feature is None or feature[1] != "Feature" or scenario is None
                or scenario[1] != "Scenario" or number + 1 in inline_comments):
            report.reject(location, "malformed annotations; expected adjacent '// Feature: path' and '// Scenario: title'")
            continue
        consumed.add(number + 1)
        # The macro must start on the next physical line, with no blank or
        # intervening comment. Its arguments and opening brace may span lines.
        if number + 2 > len(lines) or not lines[number + 1].strip() or not TEST_DEFINITION.match(
                code, offsets[number + 1]):
            report.reject(location, "detached annotations; expected an immediately adjacent TEST, TEST_F, or TEST_P definition")
            continue
        feature_path, title = feature[2], scenario[2]
        relative = PurePosixPath(feature_path)
        if (relative.is_absolute() or ".." in relative.parts
                or relative.as_posix() != feature_path or "\\" in feature_path
                or relative.suffix != ".feature"):
            report.reject(location, "feature path must be a normalized repository-relative .feature path")
            continue
        if feature_path not in feature_paths:
            report.reject(location, f"nonexistent feature reference: {feature_path}")
            continue
        identity = (feature_path, title)
        if identity not in report.scenarios:
            report.reject(Location(path, number + 1), f"nonexistent scenario reference: {feature_path}: {title}")
            continue
        report.bindings.setdefault(identity, []).append(location)


def check_files(root, paths):
    report = Report()
    feature_paths = {path.as_posix() for path in paths if path.suffix == ".feature"}
    for path in paths:
        if path.suffix == ".feature":
            parse_feature(path.as_posix(), (root / path).read_text(encoding="utf-8"), report)
    for path in paths:
        if path.suffix in SOURCE_SUFFIXES:
            parse_bindings(path.as_posix(), (root / path).read_text(encoding="utf-8"), feature_paths, report)
    for identity, location in report.scenarios.items():
        if identity not in report.bindings:
            report.reject(location, f"unbound scenario: {identity[1]}")
    if not feature_paths:
        report.reject(Location(".", 1), "no feature files found")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    try:
        report = check_files(REPO_ROOT, discover_files(REPO_ROOT))
    except (OSError, UnicodeError, subprocess.CalledProcessError) as error:
        print(f"Traceability check failed: {error}", file=sys.stderr)
        return 1
    if report.errors:
        print("\n".join(report.errors), file=sys.stderr)
        return 1
    count = sum(map(len, report.bindings.values()))
    print(f"Traceability passed: {len(report.scenarios)} scenarios, {count} bindings.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
