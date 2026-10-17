# Invariant result types

`//contracts:api` defines the diagnostic value types used by document
invariant checks. Its visibility is limited to the document component and its
internal storage packages. Include
[`contracts/invariant_result.h`](include/contracts/invariant_result.h).

`InvariantViolation` carries the failed expression, optional details, and the
source location where the invariant was defined. `InvariantResult` is an
optional violation: an empty result means no violation was reported. The
reporting and exception policy belongs to the document checks, not these value
types; see the [document contract](../document/README.md).
