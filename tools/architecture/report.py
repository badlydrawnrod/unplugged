"""Deterministic findings and small per-rule coverage counts."""

from collections import Counter
from dataclasses import dataclass, field

from facts import Location


RULES = ("includes", "private-access", "entry-targets", "cycles")


@dataclass(frozen=True)
class Finding:
    rule: str
    location: Location
    reason: str
    not_checked: bool


@dataclass
class Report:
    findings: set[Finding] = field(default_factory=set)
    counts: Counter = field(default_factory=Counter)

    def checked(self, rule):
        self.counts[rule] += 1

    def skip(self, rule, location, reason):
        self.findings.add(Finding(rule, location, reason, True))

    def violation(self, rule, location, reason):
        self.findings.add(Finding(rule, location, reason, False))

    def ordered(self, *, not_checked):
        return sorted((f for f in self.findings if f.not_checked == not_checked),
                      key=lambda f: (f.location.path, f.location.line, f.rule,
                                     f.location.column, f.reason))

    @property
    def failed(self):
        return any(not f.not_checked for f in self.findings)

    def render(self):
        lines = []
        for label, limited in (("Violations", False), ("Not checked", True)):
            findings = self.ordered(not_checked=limited)
            lines.append(f"{label}: {len(findings)}")
            for f in findings:
                lines.append(f"  {f.location.path}:{f.location.line}: "
                             f"[{f.rule}] {f.reason}")
        lines.append("Coverage (checked items / not-checked findings):")
        for rule in RULES:
            skipped = sum(f.not_checked and f.rule == rule for f in self.findings)
            lines.append(f"  {rule}: {self.counts[rule]} / {skipped}")
        lines.append("No supported-rule violation found." if not self.failed
                     else "Supported-rule violations found.")
        lines.append("This check does not prove architectural correctness.")
        return "\n".join(lines)
