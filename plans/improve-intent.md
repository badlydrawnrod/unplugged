# Improve the path from intent to verified behaviour

Inspect the current HEAD and `AGENTS.md` before making changes.

## Goal

Make documentation links reliable, clarify file-opening behaviour, state the acceptance-first change process, and give the editor application a small supported-behaviour contract and automated acceptance check. Preserve the repository’s narrow component boundaries and low-maintenance conventions.

Save this plan as a temporary cross-cutting plan under `plans/`. After each step, update its checkbox and a brief checkpoint: commit or working-tree state, decisions made, checks run and their results, and the next action. On resumption, inspect that checkpoint and the working tree before continuing. Remove the temporary plan when the work is complete and its durable decisions live in contracts, scenarios, tests, and guidance.

## Steps

- [x] **1. Establish the baseline.** Read the root README, contributor and component guidance, relevant component READMEs, features, tests, and CI configuration. Confirm whether the findings below still apply. Preserve unrelated changes. Record any changed assumptions in the checkpoint.

- [x] **2. Repair documentation navigation.** At the starting reference, `key/README.md` links to absent `plans/enhance-discoverability.md`, and the root README links to a root `plans/` directory that was absent. Remove or correct stale links without creating a permanent document solely to satisfy a link. Because this temporary plan will create root `plans/`, check the root README again when closing the plan; its link must remain valid or become plain text. Review links affected by the recent spec and plan moves.

- [x] **3. Add a proportionate local-link check.** Add a small check for repository-relative Markdown file and directory links, with focused fixtures, and include it in `tools/verify.py`. It should catch the failures in step 2. Do not fetch external URLs, build a documentation crawler, or maintain a link registry. Document the syntax it checks and its limits. If accurate checking requires substantially more machinery than this benefit warrants, record that finding in the checkpoint and keep step 2 as a manual repair.

- [x] **4. State the acceptance-first workflow.** Make one concise change to the contributor route, linking to detail rather than repeating it across documents: agree on a concrete observable example; add or revise its scenario and boundary test; observe the test fail for the expected reason; implement; then refactor with tests passing. Preserve the distinction between component acceptance behaviour and implementation tests. Do not add a Gherkin runner. Keep the existing traceability guide’s warning that a scenario binding does not prove the test asserts the scenario.

- [x] **5. Clarify file-opening behaviour through an example.** The root README describes opening an existing file, while `file_loader::Load()` currently returns an empty buffer for some open or read failures, indistinguishable from a valid empty file. First look for an established intended policy. If none conflicts, use this working decision: a valid empty file loads successfully; a missing or unreadable file reports a load failure. Record the decision, write a failing scenario and supported-API test, then implement it and update `file_loader/README.md` and any affected application diagnostics. Avoid relying on permission tests that behave differently when run with elevated privileges. If existing requirements conflict with this decision, leave the behaviour unchanged, record the alternatives, and ask the user for the missing product decision while continuing independent steps. Do not silently canonise the current implementation.

- [x] **6. Describe and check application composition.** Add a short `applications/editor/README.md` covering supported startup, file opening, normal exit, and terminal restoration. Add one Linux process-level acceptance test using a pseudo-terminal and a temporary existing file. Exercise startup and Alt+Escape exit through the real application; assert the observable outcome and restored terminal settings. Use bounded timeouts and event-driven coordination rather than fixed sleeps or a full terminal-screen snapshot. Keep detailed editor and terminal cases in their existing component tests. If this test requires a large new framework, document the obstacle and stop at the smallest stable test.

- [ ] **7. Validate and close.** Run `python3 tools/verify.py` and focused tests for changed code. Let CI perform the full build and default/debug test suites; do not repeat a full local build unless diagnosing a failure. Inspect the final diff for unrelated changes, broken relative links, duplicated guidance, and claims of behaviour not established by tests. Remove this temporary plan after transferring durable decisions to their owning files. Report completed steps, checks and CI results, remaining decisions, and any deliberately deferred work.

## Constraints and completion criteria

Follow the repository’s documentation guidance, including its practical use of ISO 24495-1 and Diátaxis. Write for a reader’s task, with direct language and descriptive headings. Keep current contracts in component READMEs, concrete behaviour in adjacent features and tests, and unfinished proposals in plans. Do not add a central component registry, a broad architecture framework, or unrelated product changes.

The work is complete when a reader can follow valid links from the root to the relevant contract and checks; the change process states when and how to start with an acceptance example; file-opening outcomes are either specified and tested or explicitly awaiting a recorded product decision; and application-level behaviour has a concise entry point and the smallest practical automated check.

## Checkpoint

After step 1, at HEAD `9ccd2b4`, the working tree was clean before this checkpoint update. The root README, contributor guidance, component-creation guide, C++ and Bazel guidance, acceptance traceability guide, relevant component contracts, APIs, features, tests, and CI workflow have been reviewed.

- Confirmed: `key/README.md` links to the absent `plans/enhance-discoverability.md`. The root README's `plans/` link currently resolves because this temporary plan is present and tracked at HEAD; recheck it when closing the plan.
- Confirmed: `file_loader::Load()` returns an empty buffer for some open/read failures, but `file_loader/README.md` explicitly leaves that behavior unspecified. Existing loader coverage only specifies oversized-file rejection; no conflicting intended policy was found in the reviewed contracts or scenarios.
- Confirmed: the root README documents opening an existing readable file and Alt+Escape exit. `editor_core` specifies the exit key behavior, and `terminal` specifies normal restoration order. There is no editor application README or process-level application test; existing terminal tests use PTYs for component behavior.
- Checks run: read-only inspection of repository files, Git status, and HEAD. No build or tests run during this baseline step.
- Working-tree state now: only this plan's step-1 checkbox and checkpoint are modified; no commit made. Next action: step 2, repair documentation navigation and review links affected by spec/plan moves.

After step 2, at HEAD `f7bede2`, the working tree was clean before this step. A manual scan of repository-relative Markdown file and directory links found one missing target: `key/README.md` referenced `plans/enhance-discoverability.md`. Removed that stale proposal link while retaining the direct statement that the valid input range for `Key::MakeText()` is unspecified. The root README's `plans/` link resolves at this HEAD. The changes since `9ccd2b4` only added the step-1 checkpoint to this plan; no spec or plan moves were present in that range to repair.

- Checks run: manual relative-link scan across Markdown files; confirmed the only missing repository-relative target was the key README link. No build or tests run in this documentation step.
- Working-tree state now: only `key/README.md` and this plan are modified; no commit made. Next action: step 3, evaluate and add a proportionate local-link check if it remains low-maintenance.

After step 3, at HEAD `a84f942`, the working tree was clean before this step. Added `tools/check_markdown_links.py` and focused fixtures, and placed the fixture suite immediately before the scan in `tools/verify.py`. The check uses Git's active tracked/untracked Markdown inventory, checks relative targets from inline links, images, and reference definitions, ignores external/root-absolute/anchor-only destinations, and does not fetch URLs or validate anchors or rendering. The repository guide documents the supported syntax and limits. This remains a small local check; no crawler or link registry was needed.

- Checks run: `python3 tools/check_markdown_links_tests.py` passed (5 tests); `python3 tools/check_markdown_links.py` passed with no missing repository-relative targets. The complete `tools/verify.py` entry point remains for step 7.
- Working-tree state now: `guidance/repository.md`, `tools/verify.py`, the two new Markdown-link checker files, and this plan are modified; no commit made. Next action: step 4, state the acceptance-first workflow once in the contributor route and link to the traceability detail.

After step 4, at HEAD `4c2e922`, the working tree was clean before this step. Updated the root README contributor route to start with an agreed observable example, a persisted component scenario and supported-API boundary test, and confirmation that the test fails for the expected reason. The route then calls for implementation and refactoring with tests passing, while reserving implementation tests for non-acceptance properties. It links to the existing acceptance traceability guide; that guide's warning that a binding does not prove assertions remains unchanged. No Gherkin runner or duplicated workflow text was added.

- Checks run: reviewed the README diff and ran `git diff --check`. No tests or builds run for this documentation-only step.
- Working-tree state now: only `README.md` and this plan are modified; no commit made. Next action: step 5, resolve the file-opening policy from existing requirements, then add and implement the agreed behavior unless a conflict appears.

After step 5, at HEAD `82b42e6`, the working tree was clean before this step. No existing contract or scenario conflicted with the plan's policy, so `file_loader::Load()` now returns an empty buffer for an empty regular file and throws `std::runtime_error` for open, size-query, seek, or read failures; oversized regular files still throw `std::length_error`. Added three persisted file-opening scenarios and supported-API tests. The read-failure boundary test uses a directory path rather than permissions, so it behaves consistently under elevated test users. The application already reports caught exception details to stderr and performs loading before terminal setup; process-level verification is step 6.

- Checks run: confirmed the new tests failed before implementation (missing file returned normally; directory was misreported as oversized), then `bazel test //file_loader/tests:load_tests` passed. `bazel build //:editor`, `python3 tools/check_format.py`, and `python3 tools/acceptance/check_traceability.py` passed.
- Working-tree state now: `file_loader/README.md`, `file_loader/features/file_opening.feature`, `file_loader/include/file_loader/load.h`, `file_loader/load.cpp`, `file_loader/tests/load_tests.cpp`, and this plan are modified; no commit made. Next action: step 6, document supported editor application behavior and implement the smallest practical Linux PTY process-level acceptance test.

After step 6, at HEAD `12342c5`, the working tree was clean before this step. Added the editor application contract and one acceptance scenario/test that starts the real editor on a PTY with a temporary file, waits for its content to appear, sends Alt+Escape, observes process exit via Linux `pidfd`, and compares all saved terminal attributes after exit. Coordination is event-driven with bounded timeouts; the test does not snapshot the screen. Reused the existing PTY helper and exposed it to the application test package through its test-only Bazel visibility. No additional test framework was needed.

- Checks run: uncached `bazel test //applications/editor:editor_process_tests` passed; `python3 tools/acceptance/check_traceability.py` passed (48 scenarios, 52 bindings); `python3 tools/check_markdown_links.py` and `python3 tools/check_format.py` passed. The default Bazel output cache was not writable in this turn, so Bazel ran with an output root under `/tmp`.
- Working-tree state now: `applications/editor/BUILD.bazel`, the new application README, feature, and process test, `test_support/BUILD.bazel`, and this plan are modified; no commit made. Next action: step 7, run the repository verification entry point and focused tests, inspect the final diff and links, then remove this temporary plan if all durable decisions have been transferred.
