# Terminal lifetimes and error policy

Consumers depend on `//terminal:api` and use the
[supported headers](../../include/terminal/). [The target](../../BUILD.bazel)
composes checked `//terminal_io:api` operations, decoder input, and a private FD
adapter. Core editor and document code do not perform terminal I/O.

## Scoped ownership

`RawMode` borrows an open terminal descriptor and saves its exact attributes.
The descriptor must outlive the scope. Each scope restores its own saved state;
nested scopes for one terminal restore in LIFO order. Construction reports
attribute acquisition/application failures as `std::system_error`, attempting
rollback if applying raw mode fails.

`InputProtocol` borrows an active `Output` and owns keyboard protocol state.
It pushes escape-code disambiguation without a support query or input read;
legacy decoding remains available. It pops the previous mode on restoration.
The output must outlive the session; nested sessions restore in LIFO order.

`Output` borrows its descriptor and owns buffered bytes plus scoped process-wide
SIGPIPE suppression. Output scopes restore signal handling in LIFO order. The
application nests protocol ownership inside raw-mode and output ownership, and
explicitly restores protocol, terminal attributes, then signal handling on normal
exit. None of these scopes owns or closes the borrowed descriptor.

## Checked shutdown and unwinding

Explicit restoration reports failures. Successful restoration is idempotent;
failed restoration leaves cleanup pending. Destructors attempt restoration
without throwing, so cleanup cannot replace a primary exception. Destructor
restoration is best effort; it cannot promise success after descriptor failure.
Call restoration explicitly when shutdown errors must be reported.

`Flush()` preserves command order and handles partial/interrupted writes.
Write failures throw `std::system_error` and discard pending bytes, preventing
cleanup from replaying a partially delivered frame. SIGPIPE suppression makes a
broken pipe reportable through this exception path. `RestoreSignal()` flushes
before restoring the saved signal disposition. After successful teardown,
output requests throw `std::logic_error`; an empty flush is harmless. The output
destructor restores signal handling without flushing pending bytes.

## Input results

`ReadKey()` distinguishes a key, `NoKey`, and EOF. Timeouts and discarded
unsupported/incomplete sequences yield `NoKey` unless acquisition reached EOF.
EOF during a sequence remains EOF. Acquisition failures propagate as system
errors. The private FD adapter implements the
[decoder-owned byte-source contract](../../../key_decoder/ports/byte_source/docs/specs/byte_source.md);
it does not make the decoder depend on terminal or POSIX I/O.

## Verification

[Features](../../features/) cover restoration, protocol scope, input, and output.
[API and adapter tests](../../tests/) use isolated PTYs and pipes. Run
`bazel test //terminal/tests:all` and its `-c dbg` variant, plus
`bazel test //terminal_io/tests:syscall_tests` for checked syscall behavior.
Preserve static output-test linking and signal/syscall linker wrapping.
Run `python3 tools/verify.py` from the repository root for acceptance bindings.
After application or terminal implementation changes, smoke-test startup,
editing, legacy/kitty exit, protocol push/pop, and broken output pipes, comparing
saved terminal attributes after normal and failed sessions.
