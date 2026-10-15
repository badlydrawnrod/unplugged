# Bazel guidance

Use Bazel targets and `visibility` to express architectural boundaries.
Declare dependencies in `MODULE.bazel` and keep the lockfile in sync when
module resolution changes.

For an internal subcomponent:

- Expose its supported boundary through a `:api` target.
- Make consumers depend on `:api`, not implementation targets.
- Keep API headers at the subcomponent root and private implementation headers
  under `impl/`.
- Restrict implementation targets to their legitimate consumers.
- If consumers repeatedly need an implementation target, reconsider the
  boundary before widening visibility.
- Do not expose `impl/` headers through an API header.

Example layout:

```text
messaging/internal/spsc_queue/
  BUILD
  spsc_queue.h
  spsc_queue.cpp
  impl/
    ring_storage.h
    sequence.h
```

The supported target can be `//messaging/internal/spsc_queue:api`; consumers
include `messaging/internal/spsc_queue/spsc_queue.h`. A restricted target such
as `:ring_storage` can compile private implementation, included by its owning
implementation as `messaging/internal/spsc_queue/impl/ring_storage.h`.

Visibility declarations are enforced by Bazel. Other architectural principles
remain design guidance unless a specific build rule or checker enforces them.
