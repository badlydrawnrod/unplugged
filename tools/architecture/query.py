"""Read only the Bazel loading-phase facts needed by the architecture rules."""

import json
import os
from pathlib import Path
import re
import subprocess

from facts import (Attribute, BuildFacts, Choice, File, Limitation, Location,
                   OperationalError, Selector, Target)


# Never use ruleInput: it merges code, data, tools and configuration conditions.
ATTRIBUTES = {
    "srcs", "hdrs", "textual_hdrs", "deps", "implementation_deps", "testonly",
    "visibility", "include_prefix", "strip_include_prefix", "includes",
    "local_includes", "actual",
}


def _run(command, root, env):
    try:
        result = subprocess.run(command, cwd=root, env=env, capture_output=True,
                                text=True, encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise OperationalError(f"cannot run Bazel: {error}") from error
    if result.returncode:
        raise OperationalError(
            f"Bazel exited {result.returncode}: {result.stderr.strip()}")
    return result.stdout


def _location(value, root):
    match = re.fullmatch(r"(.+):(\d+):(\d+)", value)
    if match is None:
        raise ValueError(f"invalid Bazel location: {value!r}")
    path = Path(match[1])
    if path.is_absolute():
        path = path.relative_to(root)
    if ".." in path.parts:
        raise ValueError(f"location outside repository: {value!r}")
    return Location(path.as_posix(), int(match[2]), int(match[3]))


def _value(data, kind):
    if kind in {"LABEL_LIST", "STRING_LIST"}:
        value = data.get("stringListValue", [])
        if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
            raise ValueError(f"invalid {kind} value")
        return tuple(value)
    if kind in {"STRING", "LABEL"}:
        value = data.get("stringValue", "")
        if not isinstance(value, str):
            raise ValueError(f"invalid {kind} value")
        return value
    if kind == "BOOLEAN":
        value = data.get("booleanValue", False)
        if not isinstance(value, bool):
            raise ValueError("invalid BOOLEAN value")
        return value
    raise ValueError(f"unsupported attribute type {kind}")


def _attribute(data):
    name, kind = data["name"], data["type"]
    explicit = data.get("explicitlySpecified", False)
    supported = {"LABEL_LIST", "STRING_LIST", "STRING", "LABEL", "BOOLEAN"}
    if kind != "SELECTOR_LIST":
        if kind not in supported:
            return Attribute(name, kind, explicit,
                             limitation=f"unsupported attribute type {kind}")
        return Attribute(name, kind, explicit, value=_value(data, kind))
    selection = data["selectorList"]
    value_type = selection["type"]
    if value_type not in supported:
        return Attribute(name, value_type, explicit,
                         limitation=f"unsupported selector type {value_type}")
    selectors = []
    for element in selection["elements"]:
        choices = []
        for entry in element["entries"]:
            uses_default = entry.get("isDefaultValue", False)
            choices.append(Choice(entry["label"],
                                  None if uses_default else _value(entry, value_type),
                                  uses_default))
        if not choices:
            raise ValueError("selector has no entries")
        selectors.append(Selector(tuple(sorted(choices, key=lambda c: c.condition)),
                                  element.get("hasDefaultValue", False),
                                  element.get("noMatchError", "")))
    if not selectors:
        raise ValueError("selector list has no elements")
    return Attribute(name, value_type, explicit, selectors=tuple(selectors))


def parse_query(output, root):
    """Decode streamed_jsonproto; corrupt tool output is an operational error.

    Unknown attribute types are coverage limitations. Recognized types with
    malformed payloads are failed tool input, rather than empty facts.
    """
    targets, files, limitations = {}, {}, []
    root = Path(root).resolve()
    try:
        for line in output.splitlines():
            if not line.strip():
                continue
            record = json.loads(line)
            kind = record["type"]
            if kind == "RULE":
                rule = record["rule"]
                label = rule["name"]
                attributes = tuple(sorted(
                    (_attribute(a) for a in rule.get("attribute", [])
                     if a["name"] in ATTRIBUTES), key=lambda a: a.name))
                target = Target(label, rule["ruleClass"],
                                _location(rule["location"], root), attributes)
                if label in targets:
                    raise ValueError(f"duplicate target {label}")
                targets[label] = target
            elif kind in {"SOURCE_FILE", "GENERATED_FILE"}:
                file = record["sourceFile" if kind == "SOURCE_FILE" else "generatedFile"]
                label = file["name"]
                generated_by = file.get("generatingRule")
                files[label] = File(label, _location(file["location"], root), generated_by)
            elif kind not in {"PACKAGE_GROUP", "ENVIRONMENT_GROUP"}:
                limitations.append(Limitation(
                    Location("."), f"unsupported Bazel query record type {kind!r}"))
    except (ValueError, TypeError, KeyError, AttributeError) as error:
        raise OperationalError(f"cannot decode Bazel query output: {error}") from error
    return BuildFacts(tuple(targets[k] for k in sorted(targets)),
                      tuple(files[k] for k in sorted(files)),
                      tuple(sorted(set(limitations),
                                   key=lambda gap: (gap.location, gap.reason))))


def load_facts(root, inventory, *, bazel=("bazel",)):
    """Query explicit active packages, retaining selects and all target tags.

    The repository pin is passed to Bazelisk and checked against the executable.
    No configuration, C++ compiler, or platform SDK is involved.
    """
    root = Path(root).resolve()
    try:
        pinned = (root / ".bazelversion").read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError) as error:
        raise OperationalError(f"cannot read .bazelversion: {error}") from error
    if not re.fullmatch(r"\d+\.\d+\.\d+", pinned):
        raise OperationalError(f"unsupported .bazelversion pin: {pinned!r}")
    env = dict(os.environ, USE_BAZEL_VERSION=pinned)
    version = _run([*bazel, "version", "--gnu_format"], root, env)
    if f"bazel {pinned}" not in version.splitlines():
        raise OperationalError(f"expected Bazel {pinned}; got {version.strip()}")
    if not inventory.packages:
        raise OperationalError("no active Bazel packages found")
    # Quoted patterns are literals in Bazel's query language, not shell input.
    patterns = " ".join(json.dumps(f"//{package}:*") for package in inventory.packages)
    output = _run([*bazel, "query", f"set({patterns})",
                   "--output=streamed_jsonproto", "--proto:flatten_selects=false"],
                  root, env)
    facts = parse_query(output, root)
    if not facts.targets and not facts.files and not facts.limitations:
        raise OperationalError("Bazel query returned no facts for active packages")
    return facts
