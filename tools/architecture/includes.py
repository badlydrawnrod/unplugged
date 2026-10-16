"""Syntactic first-party include traversal and direct private-header access."""

from dataclasses import dataclass
from pathlib import Path
import posixpath
import re

from facts import Location, OperationalError
from ownership import (CODE_KINDS, explicit_grant, label_path, package_of,
                       strings, test_context, within)


@dataclass(frozen=True)
class Directive:
    line: int
    spelling: str | None
    angled: bool
    conditional: bool


def directives(text):
    """Read directives after line splicing, ignoring comments and C++ strings.

    Ordinary whole-file #ifndef/#define guards do not make their bodies
    conditional. All other branches remain unresolved, including #if 0/1.
    """
    chars, lines = [], []
    line, index = 1, 0
    while index < len(text):
        splice = re.match(r"\\\r?\n", text[index:index + 3])
        if splice:
            index += len(splice.group())
            line += 1
            continue
        chars.append(text[index])
        lines.append(line)
        line += text[index] == "\n"
        index += 1
    text = "".join(chars)
    clean, code = list(text), list(text)

    def mask(start, end, comments=False):
        for offset in range(start, end):
            if text[offset] != "\n":
                code[offset] = " "
                if comments:
                    clean[offset] = " "

    index = 0
    while index < len(text):
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = len(text) if end < 0 else end
            mask(index, end, True)
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = len(text) if end < 0 else end + 2
            mask(index, end, True)
        elif text.startswith('R"', index):
            raw = re.match(r'R"([^\s()\\]{0,16})\(', text[index:])
            if not raw:
                index += 1
                continue
            close = ")" + raw.group(1) + '"'
            end = text.find(close, index + len(raw.group()))
            end = len(text) if end < 0 else end + len(close)
            mask(index, end)
        elif text[index] in "\"'":
            quote, end = text[index], index + 1
            while end < len(text):
                if text[end] == "\\":
                    end += 2
                elif text[end] == quote:
                    end += 1
                    break
                elif text[end] == "\n":
                    break
                else:
                    end += 1
            mask(index, min(end, len(text)))
        else:
            index += 1
            continue
        index = end
    clean, code = "".join(clean), "".join(code)
    found = list(re.finditer(r"^[ \t]*(?:#|%:)[ \t]*(\w+)([^\n]*)", code, re.M))
    guard = None
    depth, outer_end = 0, None
    for match in found:
        if match.group(1) in ("if", "ifdef", "ifndef"):
            depth += 1
        elif match.group(1) == "endif":
            depth -= 1
            if depth == 0 and outer_end is None:
                outer_end = match.start()
    if len(found) >= 3 and found[0].group(1) == "ifndef":
        name = found[0].group(2).strip()
        if (re.fullmatch(r"\w+", name) and found[1].group(1) == "define"
                and found[1].group(2).strip() == name
                and found[-1].group(1) == "endif"
                and outer_end == found[-1].start()
                and not code[:found[0].start()].strip()
                and not code[found[-1].end():].strip()):
            guard = found[0].start()
    stack, result = [], []
    for match in found:
        name = match.group(1)
        if name in ("if", "ifdef", "ifndef"):
            stack.append(match.start() != guard)
        elif name in ("else", "elif", "elifdef", "elifndef"):
            if stack:
                stack[-1] = True
        elif name == "endif":
            if stack:
                stack.pop()
        elif name in ("include", "include_next", "import"):
            argument = clean[match.start(2):match.end(2)].strip()
            literal = (re.fullmatch(r'"([^"\n]+)"|<([^>\n]+)>', argument)
                       if name == "include" and not match.group().lstrip().startswith("%:")
                       else None)
            result.append(Directive(lines[match.start()],
                                    (literal.group(1) or literal.group(2)) if literal else None,
                                    bool(literal and literal.group(2)), any(stack)))
    return tuple(result)


class _Resolver:
    def __init__(self, inventory, facts, ownership, report):
        self.active = set(inventory.sources)
        self.ownership = ownership
        self.aliases = {}
        self.known = set()
        self.external_prefixes = {"external"}
        self.first_parts = {p.split("/", 1)[0] for p in inventory.packages if p}
        file_facts = {f.label: f for f in facts.files}
        for target in facts.targets:
            if target.kind not in CODE_KINDS:
                continue
            sources, source_gaps = strings(target, "srcs")
            for gap in source_gaps:
                for rule in ("includes", "private-access"):
                    report.skip(rule, target.location, gap)
            for source in sources:
                path = label_path(source.value)
                if path is not None and path not in self.active:
                    for rule in ("includes", "private-access"):
                        report.skip(rule, target.location,
                                    f"source membership is indirect or unavailable: {source.value}")
            for name in ("deps", "implementation_deps"):
                dependencies, _ = strings(target, name)
                for dependency in dependencies:
                    label = dependency.value
                    if label.startswith("@") and "//" in label:
                        repository, rest = label.split("//", 1)
                        self.external_prefixes.add(repository.lstrip("@"))
                        self.external_prefixes.add(rest.split("/", 1)[0].split(":", 1)[0])
                        if ":" in rest:
                            self.external_prefixes.add(rest.split(":", 1)[1].split("/", 1)[0])
            headers = []
            for name in ("hdrs", "textual_hdrs"):
                values, gaps = strings(target, name)
                for gap in gaps:
                    report.skip("includes", target.location, gap)
                for value in values:
                    path = label_path(value.value)
                    if path is None:
                        continue
                    file = file_facts.get(value.value)
                    if path not in self.active:
                        reason = ("generated header is unavailable" if file and file.generated_by
                                  else "header membership is indirect or unavailable")
                        report.skip("includes", target.location, f"{reason}: {value.value}")
                    headers.append((path, value.conditional))
            strips, sg = strings(target, "strip_include_prefix")
            prefixes, pg = strings(target, "include_prefix")
            dirs, dg = strings(target, "includes")
            local, lg = strings(target, "local_includes")
            for gap in (*sg, *pg, *dg, *lg):
                report.skip("includes", target.location, gap)
            package = package_of(target.label)
            if package is None:
                continue
            # rules_cc treats an empty strip as unset, even when explicit.
            # include_prefix alone instead strips the rule's package prefix.
            strip_values = [(r.value or None, r.conditional) for r in strips] or [(None, False)]
            prefix_values = [(r.value, r.conditional) for r in prefixes] or [("", False)]
            for path, conditional in (() if sg or pg else headers):
                for strip, sc in strip_values:
                    if strip and (".." in strip.split("/") or strip != posixpath.normpath(strip)):
                        report.skip("includes", target.location,
                                    f"unsupported strip_include_prefix: {strip}")
                        continue
                    for prefix, pc in prefix_values:
                        if prefix and (prefix.startswith("/") or ".." in prefix.split("/")
                                       or prefix != posixpath.normpath(prefix)):
                            report.skip("includes", target.location,
                                        f"unsupported include_prefix: {prefix}")
                            continue
                        effective_strip = strip if strip is not None else ("." if prefix else None)
                        base = (posixpath.normpath(effective_strip.lstrip("/"))
                                if effective_strip and effective_strip.startswith("/")
                                else posixpath.normpath(posixpath.join(package, effective_strip or "")))
                        if base == ".":
                            base = ""
                        relative = path
                        if effective_strip is not None:
                            if not within(path, base):
                                report.skip("includes", target.location,
                                            f"strip_include_prefix does not contain {path}")
                                continue
                            relative = posixpath.relpath(path, base or ".")
                        self._add(posixpath.join(prefix, relative), path, conditional or sc or pc)
            for directory in (*dirs, *local):
                base = posixpath.normpath(posixpath.join(package, directory.value))
                if base.startswith("/") or base == ".." or base.startswith("../"):
                    report.skip("includes", target.location,
                                f"include directory outside repository: {directory.value}")
                    continue
                for path in sorted(self.active):
                    if within(path, base):
                        self._add(posixpath.relpath(path, base), path, directory.conditional)

    def _add(self, spelling, path, conditional):
        self.known.add(spelling)
        self.aliases.setdefault(spelling, set()).add((path, conditional))
        if "/" in spelling:
            self.first_parts.add(spelling.split("/", 1)[0])

    def resolve(self, source, directive):
        spelling = directive.spelling
        if spelling is None:
            return None, False, "computed or unsupported include directive"
        # Quoted includes first search the including file's directory.
        relative = posixpath.normpath(posixpath.join(posixpath.dirname(source), spelling))
        if not directive.angled and relative in self.active:
            candidates = {(relative, False)}
        else:
            candidates = set(self.aliases.get(spelling, ()))
            if spelling in self.active:
                candidates.add((spelling, False))
        paths = {path for path, _ in candidates}
        if len(paths) > 1:
            return None, False, f"ambiguous include mapping for {spelling}"
        if paths:
            path = next(iter(paths))
            if path not in self.active:
                return None, False, f"generated or unavailable first-party include: {spelling}"
            members = self.ownership.members.get(path, ())
            conditional = (all(c for _, c in candidates)
                           or bool(members) and all(c for _, c in members))
            return path, conditional, None
        if spelling.split("/", 1)[0] in self.external_prefixes:
            return None, False, None
        if (not directive.angled or spelling in self.known
                or spelling.split("/", 1)[0] in self.first_parts):
            return None, False, f"unresolved first-party include: {spelling}"
        return None, False, None  # Standard/platform/external headers are outside traversal.


def _private_access(source, destination, location, conditional, ownership, report):
    if not {"internal", "detail"}.intersection(destination.split("/")):
        return
    source_owner = ownership.owner_path(source)
    destination_owner = ownership.owner_path(destination)
    source_uncertain = source_owner is None and (
        ownership.package_for_path(source) is None
        or any(within(source, p) for p in ownership.components)
        or any(within(source, p) for p in ownership.uncertain_boundaries))
    if source_uncertain or destination_owner is None:
        report.skip("private-access", location, "private include ownership is uncertain")
        return
    report.checked("private-access")
    if source_owner == destination_owner:
        return
    clients = ownership.members.get(source, ())
    if source_owner is not None and within(destination_owner, source_owner) and (
            ownership.package_for_path(source) == source_owner
            or clients and all(test_context(t) is False for t, _ in clients)):
        return
    providers = ownership.members.get(destination, ())
    if not clients or not providers:
        report.skip("private-access", location, "private include lacks declared membership evidence")
        return
    uncertain, denied = conditional, False
    for client, cc in clients:
        context = test_context(client)
        if context is None:
            uncertain = True
            continue
        dependencies = []
        gaps = []
        for name in ("deps", "implementation_deps"):
            refs, missing = strings(client, name)
            dependencies.extend(refs)
            gaps.extend(missing)
        allowed, potential = False, bool(gaps)
        for provider, pc in providers:
            grant = explicit_grant(provider, package_of(client.label))
            for dependency in dependencies:
                if dependency.value != provider.label:
                    continue
                if grant is True:
                    if cc or pc or dependency.conditional:
                        potential = True
                    else:
                        allowed = True
                elif grant is None:
                    potential = True
        if not allowed:
            if potential or cc:
                uncertain = True
            else:
                denied = True
    if denied:
        reason = f"direct include of another component's private header: {destination}"
        (report.skip if conditional else report.violation)("private-access", location, reason)
    elif uncertain:
        report.skip("private-access", location,
                    f"selected private-header access is conditional or unsupported: {destination}")


def check_includes(root: Path, inventory, facts, ownership, report):
    resolver = _Resolver(inventory, facts, ownership, report)
    edges = {}
    for source in inventory.sources:
        try:
            text = (root / source).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            raise OperationalError(f"Cannot read required source {source}: {error}") from error
        edges[source] = []
        for directive in directives(text):
            location = Location(source, directive.line)
            destination, conditional, gap = resolver.resolve(source, directive)
            if gap:
                report.skip("includes", location, gap)
                report.skip("private-access", location, gap)
                continue
            if destination is None:
                continue
            report.checked("includes")
            source_members = ownership.members.get(source, ())
            conditional |= (directive.conditional or bool(source_members)
                            and all(c for _, c in source_members))
            edges[source].append((destination, conditional, location))
            _private_access(source, destination, location, conditional, ownership, report)
    roots = set()
    for target in facts.targets:
        entry = ownership.entry_status(target)
        if entry is None and target.kind == "cc_library":
            report.skip("includes", target.location, "API variant or ownership is uncertain")
        if entry is not True:
            continue
        for name in ("hdrs", "textual_hdrs"):
            headers, _ = strings(target, name)
            for header in headers:
                path = label_path(header.value)
                if path in edges:
                    roots.add((path, header.conditional))
    for start, conditional in sorted(roots):
        if "impl" in start.split("/"):
            (report.skip if conditional else report.violation)(
                "includes", Location(start), f"API header is in impl/: {start}")
        pending = [(start, conditional, (start,))]
        visited = set()
        while pending:
            source, conditional, chain = pending.pop()
            if (source, conditional) in visited:
                continue
            visited.add((source, conditional))
            for destination, ec, location in edges[source]:
                potential = conditional or ec
                path = (*chain, destination)
                if "impl" in destination.split("/"):
                    reason = "API include closure reaches impl/: " + " -> ".join(path)
                    (report.skip if potential else report.violation)("includes", location, reason)
                elif destination not in chain:
                    pending.append((destination, potential, path))
