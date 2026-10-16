"""Shared syntactic ownership and the few supported access-evidence forms."""

from dataclasses import dataclass
import posixpath


CODE_KINDS = frozenset({"cc_library", "cc_binary", "cc_test"})


def package_of(label):
    if not label.startswith("//") or ":" not in label:
        return None
    return label[2:].split(":", 1)[0]


def label_path(label):
    package = package_of(label)
    if package is None:
        return None
    path = posixpath.normpath(posixpath.join(package, label.split(":", 1)[1]))
    return None if path.startswith("../") or path.startswith("/") else path


def within(path, parent):
    return not parent or path == parent or path.startswith(parent + "/")


@dataclass(frozen=True)
class Reference:
    value: str
    conditional: bool = False


def strings(target, name):
    """Return literal string references and reasons for unsupported values.

    A default-only selector is a constant concatenation segment. Multiple
    choices stay conditional, including their //conditions:default branch.
    """
    attribute = target.attribute(name)
    if attribute is None:
        return (), ()
    if attribute.limitation:
        return (), (f"{name}: {attribute.limitation}",)
    references, gaps = set(), []

    def append(value, conditional):
        if isinstance(value, str):
            references.add(Reference(value, conditional))
        elif isinstance(value, tuple) and all(isinstance(v, str) for v in value):
            references.update(Reference(v, conditional) for v in value)
        else:
            gaps.append(f"{name}: unsupported or defaulted value")

    if attribute.selectors:
        for selector in attribute.selectors:
            conditional = (len(selector.choices) != 1
                           or selector.choices[0].condition != "//conditions:default")
            for choice in selector.choices:
                append(choice.value, conditional)
    else:
        append(attribute.value, False)
    # A reference with a supported unconditional occurrence remains definite.
    definite = {r.value for r in references if not r.conditional}
    return tuple(sorted((r for r in references
                         if not r.conditional or r.value not in definite),
                        key=lambda r: (r.value, r.conditional))), tuple(sorted(set(gaps)))


def test_context(target):
    if target.kind == "cc_test":
        return True
    attribute = target.attribute("testonly")
    if attribute is None:
        return False
    if attribute.limitation or attribute.selectors or not isinstance(attribute.value, bool):
        return None
    return attribute.value


def explicit_grant(target, client_package):
    """Recognize only literal, target-level finite lists of exact package grants.

    Public/private declarations do not grant an implementation exception.
    Package groups, subtree grants and configurable visibility remain uncertain.
    """
    attribute = target.attribute("visibility")
    if attribute is None:
        return False
    values, gaps = strings(target, "visibility")
    if gaps or any(v.conditional for v in values):
        return None
    grants = {v.value for v in values}
    if "//visibility:public" in grants or grants == {"//visibility:private"}:
        return False
    if grants and not attribute.explicitly_specified:
        return None  # An inherited grant's implementation-access intent is unknown.
    if any(not v.endswith(":__pkg__") or package_of(v) is None for v in grants):
        return None
    return f"//{client_package}:__pkg__" in grants


class Ownership:
    def __init__(self, facts, inventory):
        self.targets = {t.label: t for t in facts.targets}
        self.packages = inventory.packages
        self.components = {package_of(t.label): t for t in facts.targets
                           if t.kind == "cc_library" and t.label.endswith(":api")
                           and package_of(t.label) is not None}
        self.uncertain_boundaries = {package_of(t.label) for t in facts.targets
                                     if t.label.endswith(":api")
                                     and t.kind != "cc_library"
                                     and package_of(t.label) is not None}
        self.members = {}
        for target in facts.targets:
            if target.kind not in CODE_KINDS:
                continue
            for name in ("hdrs", "textual_hdrs", "srcs"):
                values, _ = strings(target, name)
                for value in values:
                    path = label_path(value.value)
                    if path is not None:
                        self.members.setdefault(path, []).append((target, value.conditional))

    def package_for_path(self, path):
        return max((p for p in self.packages if within(posixpath.dirname(path), p)),
                   key=len, default=None)

    def owner_path(self, path):
        owner = max((p for p in self.components if within(path, p)),
                    key=len, default=None)
        if any(within(path, p) and (owner is None or len(p) >= len(owner))
               for p in self.uncertain_boundaries):
            return None
        # A header exported by an API in a different component has conflicting
        # layout/declaration evidence. Do not assign it a confident owner.
        for target, _ in self.members.get(path, ()):
            package = package_of(target.label)
            if target.label.endswith(":api") and package in self.components and package != owner:
                return None
        return owner

    def owner_target(self, target):
        package = package_of(target.label)
        return None if package is None else self.owner_path(package)

    def entry_status(self, target):
        """Recognize :api and same-package library variants exposing identical APIs."""
        owner = self.owner_target(target)
        if owner is None:
            return None
        api = self.components[owner]
        if target.label == api.label:
            return True
        if target.kind != "cc_library" or package_of(target.label) != owner:
            return False
        for name in ("hdrs", "textual_hdrs", "include_prefix", "strip_include_prefix"):
            left, lg = strings(target, name)
            right, rg = strings(api, name)
            if lg or rg or any(r.conditional for r in (*left, *right)):
                return None
            if {r.value for r in left} != {r.value for r in right}:
                return False
        headers, _ = strings(api, "hdrs")
        return bool(headers)
