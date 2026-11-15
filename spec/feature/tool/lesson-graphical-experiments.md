# Graphical action-game lesson and reversible experiments

## Scope and acceptance

Human authorization: neco's Odeum AI lesson request and "全部終わるまで続けて ゴールアンドゴー".
The existing deterministic GameSession remains the only game simulation. Windows --play gains
a native window, mouse aiming, keyboard movement and attack, readable enemy telegraphs, attack
arcs, HP bars, damage numbers and the existing effect toggles/Studio variables. POSIX terminal
play and headless modes remain available. Window focus loss pauses input; resize preserves aspect.

Use the operating system's GDI+ drawing API rather than adding a renderer dependency for this
small 2D lesson. Keep drawing, platform input and application orchestration separate. No new
network listener or install script is added. Existing Studio connection is optional (--no-studio).
Do not execute a window from a worktree; runtime checks require the authorized main-folder path.

Included: opt-in bad-UX presets (long hit stop, excessive knockback, excessive shake), a
baseline reset that clears observations, and an independently built C++ rule module with explicit
reload/rollback. Presets must not masquerade as code rewriting. A failed rebuild/load must retain
the previous working module. Native teacher code is trusted local code, not a sandbox for uploads.

## Teacher workflow (Windows)

- F1 baseline, F2 long stop, F3 excessive knockback, F4 excessive shake. Each selection replaces
  the whole trial, resets observations/HP/wave/effects/combat settings to the same CLI starting
  conditions, and clears the old hit-stop clock. No trap is enabled by default.
- Edit `examples/lesson_action_feel/live_rules/lesson_rules.cpp`. The initial example caps stop
  time at 0.12 seconds; changing this C++ expression is distinct from a preset or variable toggle.
- Build `cmake --build build/lesson --config Release --target lesson_rules`. A build failure is
  reported by the compiler; it does not modify the running snapshot.
- F5 explicitly copies and loads the last successfully built `lesson_rules.dll` beside the game.
  Missing exports, incompatible ABI, load failure or invalid sample outputs retain the current trial.
- F6 restores the previous successful snapshot; after the first load it restores built-in behavior.
  F1 always restores baseline. R starts a fresh trial of the selected experiment.
- O pauses play and opens all ten effect switches (A–J); V shows motion vectors. 4–6 adjust enemy
  flinch/armor/cancel; 7–9 adjust the corresponding player settings. P resumes after focus loss.

The loader never compiles, spawns a process, downloads code or accepts a remote module path.
Only the fixed sibling DLL is eligible. Each load uses a distinct temporary snapshot so rebuilding
does not overwrite a loaded image. Shared ownership retains the image until every core callback
has been released; cleanup unloads it before removing the snapshot. All rule calls and replacement
run on the game thread. Runtime non-finite/out-of-range outputs throw and restore a fresh baseline,
pause play and display an error. Native memory faults/hangs cannot be contained by this mechanism:
only locally reviewed teacher code may be loaded. Core depends on a function interface, not Win32.

Existing optional Studio startup communication is outbound WebSocket to the configured host/port,
path `/variable/ws`: variable metadata, game observations and control writes only, no source files,
module bytes, environment variables or credentials. `--no-studio` disables it completely. The new
renderer uses installed Windows SDK libraries only; no package installer or transitive fetch added.

## Verification

Build and static analysis results are recorded separately from runtime verification. Required visual
checks: movement and aiming, telegraph/active-arc alignment, victory/defeat/restart, all effect toggles,
focus loss, resize/minimize/close, Studio writes and --no-studio. Runtime verification is pending.

2026-11-15 implementation evidence: MSVC 19.52 Release game and rule DLL builds passed with
network fetch disabled. Both existing core/frontend test targets, including new rule-boundary and
independent-preset cases, compiled successfully; they were not executed. Clang's configured
core/unix/security analysis passed for all 11 changed/new C++ translation units without diagnostics.
Anatomia verify reported five gates passing. Its `test-suggestions` subcommand is unavailable;
the registered-project `where` lookup did not identify GameSession correctly, so source inspection
was used for references. LSP executable availability was not established; no LSP result is claimed.

Excubitor `ergo-lesson-game` uses the main Ergo folder, a separate `build/lesson` output and
`cmake/BuildLesson.cmake`. It starts with `--no-studio` for an offline trial and never autostarts.
Connecting a teaching session requires explicitly enabling the existing Studio connection with
its actual configured endpoint; the offline catalog profile does not claim classroom pairing.
No window/service launch, runtime test, merge or deployment is included in the evidence above.

## Ownership

Foundation owns the existing example; core remains pure domain logic. windows/ is presentation,
and graphical_mode.cpp is application composition. No changes to other modules' ownership.
