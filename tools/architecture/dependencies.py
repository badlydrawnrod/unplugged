"""Direct entry-target checks and cycles in the unconditional component graph."""

from collections import defaultdict

from ownership import CODE_KINDS, explicit_grant, package_of, strings, test_context, within


def _uncertain_owner(target, ownership):
    package = package_of(target.label)
    return package is None or (ownership.owner_target(target) is None and any(
        within(package, p) for p in ownership.uncertain_boundaries))


def _entry(client, dependency, conditional, ownership, report):
    rule, location = "entry-targets", client.location
    owner = ownership.owner_target(dependency)
    client_owner = ownership.owner_target(client)
    if _uncertain_owner(client, ownership) or _uncertain_owner(dependency, ownership):
        report.skip(rule, location, f"{client.label} -> {dependency.label}: uncertain component boundary")
        return
    if owner is not None and owner == client_owner:
        report.checked(rule)
        return
    client_test, dependency_test = test_context(client), test_context(dependency)
    if dependency_test is True and client_test is False:
        report.skip(rule, location, f"{client.label} -> {dependency.label}: production dependency on testonly target is left to Bazel")
        return
    if owner is None:
        if dependency_test is True and client_test is True:
            report.checked(rule)
        else:
            report.skip(rule, location, f"{client.label} -> {dependency.label}: dependency has no supported component boundary")
        return
    entry = ownership.entry_status(dependency)
    if entry is True:
        report.checked(rule)
        return
    grant = explicit_grant(dependency, package_of(client.label))
    if grant is True:
        report.checked(rule)
    elif entry is None or grant is None or client_test is None or dependency_test is None:
        report.skip(rule, location, f"{client.label} -> {dependency.label}: entry target, test context, or implementation grant is uncertain")
    elif conditional:
        report.skip(rule, location, f"{client.label} -> {dependency.label}: conditional dependency may bypass //{owner}:api")
    else:
        report.checked(rule)
        report.violation(rule, location, f"{client.label} -> {dependency.label} bypasses //{owner}:api without an explicit package grant")


def _cycle_witness(graph, component):
    """One deterministic cycle witness per strongly connected component."""
    start = min(component)
    path, active = [start], {start}

    def visit(node):
        for successor in sorted(graph[node]):
            if successor not in component:
                continue
            if successor == start:
                return (*path, start)
            if successor not in active:
                path.append(successor)
                active.add(successor)
                result = visit(successor)
                if result:
                    return result
                active.remove(successor)
                path.pop()
        return None

    return visit(start)


def _cycles(graph, edge_locations, report):
    # Tarjan's algorithm finds maximal cyclic groups without enumerating every
    # possible cycle; only a deterministic witness is needed for a diagnosis.
    indices, low, stack, active = {}, {}, [], set()

    def visit(node):
        indices[node] = low[node] = len(indices)
        stack.append(node)
        active.add(node)
        for successor in sorted(graph[node]):
            if successor not in indices:
                visit(successor)
                low[node] = min(low[node], low[successor])
            elif successor in active:
                low[node] = min(low[node], indices[successor])
        if low[node] != indices[node]:
            return
        component = set()
        while True:
            member = stack.pop()
            active.remove(member)
            component.add(member)
            if member == node:
                break
        if len(component) > 1:
            witness = _cycle_witness(graph, component)
            locations = [edge_locations[a, b] for a, b in zip(witness, witness[1:])]
            report.violation("cycles", min(locations), "component cycle: " +
                             " -> ".join(f"//{p}:api" for p in witness))

    for node in sorted(set(graph) | {v for values in graph.values() for v in values}):
        if node not in indices:
            visit(node)


def check_dependencies(facts, ownership, report):
    """Inspect only direct C++ deps/implementation_deps, without alias expansion."""
    graph, edge_locations = defaultdict(set), {}
    for client in sorted(facts.targets, key=lambda t: t.label):
        for name in ("deps", "implementation_deps"):
            references, gaps = strings(client, name)
            for gap in gaps:
                for rule in ("entry-targets", "cycles"):
                    report.skip(rule, client.location, f"{client.label}: {gap}")
            for reference in references:
                label = reference.value
                # External dependencies have no first-party component role.
                if label.startswith("@"):
                    continue
                dependency = ownership.targets.get(label)
                if client.kind not in CODE_KINDS or dependency is None or dependency.kind not in CODE_KINDS:
                    reason = (f"{client.label} -> {label}: unsupported client kind {client.kind}"
                              if client.kind not in CODE_KINDS else
                              f"{client.label} -> {label}: missing local target or unsupported dependency kind")
                    for rule in ("entry-targets", "cycles"):
                        report.skip(rule, client.location, reason)
                    continue
                _entry(client, dependency, reference.conditional, ownership, report)
                origin, destination = ownership.owner_target(client), ownership.owner_target(dependency)
                if origin is None or destination is None:
                    report.skip("cycles", client.location,
                                f"{client.label} -> {label}: component ownership is unavailable")
                elif origin == destination:
                    report.checked("cycles")
                elif reference.conditional:
                    report.skip("cycles", client.location,
                                f"{client.label} -> {label}: conditional component edge is excluded from definite cycles")
                else:
                    report.checked("cycles")
                    graph[origin].add(destination)
                    edge = origin, destination
                    edge_locations[edge] = min(edge_locations.get(edge, client.location), client.location)
    _cycles(graph, edge_locations, report)
