"""Small private loading-phase facts; these do not describe a configured build."""

from dataclasses import dataclass


class OperationalError(Exception):
    """A required input or tool failed, so the audit cannot be trusted."""


@dataclass(frozen=True, order=True)
class Location:
    path: str
    line: int = 1
    column: int = 1


Value = tuple[str, ...] | str | bool


@dataclass(frozen=True)
class Choice:
    condition: str
    value: Value | None
    # Bazel uses this for select branches whose value is None (attribute default).
    uses_attribute_default: bool = False


@dataclass(frozen=True)
class Selector:
    choices: tuple[Choice, ...]
    has_default: bool
    no_match_error: str


@dataclass(frozen=True)
class Attribute:
    name: str
    type: str
    explicitly_specified: bool
    value: Value | None = None
    # Concatenated list segments stay separate, in declaration order.
    selectors: tuple[Selector, ...] = ()
    # Unsupported facts are retained for future not-checked diagnostics.
    limitation: str | None = None


@dataclass(frozen=True)
class Target:
    label: str
    kind: str
    location: Location
    attributes: tuple[Attribute, ...]

    def attribute(self, name):
        return next((a for a in self.attributes if a.name == name), None)


@dataclass(frozen=True)
class File:
    label: str
    location: Location
    generated_by: str | None = None


@dataclass(frozen=True)
class Limitation:
    location: Location
    reason: str


@dataclass(frozen=True)
class BuildFacts:
    targets: tuple[Target, ...]
    files: tuple[File, ...]
    limitations: tuple[Limitation, ...] = ()
