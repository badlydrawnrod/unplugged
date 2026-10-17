# Editor application

## Start and open a file

Run `bazel run //:editor -- path/to/existing-file` from the repository root.
The application opens a readable regular file into memory and displays its
contents. An empty file opens as an empty document. The editor does not save
changes. A missing or unreadable file produces an error on standard error and
exits before changing terminal mode.

## Exit and restore the terminal

Press **Alt+Escape** to exit normally. The application clears its display, then
restores the keyboard protocol, terminal attributes, and signal handling in
that order. The [terminal contract](../../terminal/README.md) describes the
scoped resource behavior.

The [application acceptance scenario](features/application.feature) checks
startup, file display, normal exit, and terminal-attribute restoration through
the real process.
