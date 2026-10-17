# Invariant result types

## Use the diagnostic types

Depend on `//contracts:api` and include
[`contracts/invariant_result.h`](include/contracts/invariant_result.h). The
target is visible only to the document component and its internal storage
packages.

## Result and ownership

`InvariantViolation` carries the failed expression, optional details, and the
source location where the invariant was defined. `InvariantResult` is an
optional violation: an empty result means no violation was reported. The
reporting and exception policy belongs to the document checks, not these value
types. For how document invariant failures are handled, see the
[document contract](../document/README.md).
