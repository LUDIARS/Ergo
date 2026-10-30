# com.ludiars.ergo.packages

Editor-only Unity package of the Ergo package manager (Ergo `spec/feature/package-manager.md` §6).
It reads and writes files only; whether an update exists is decided by the native `ergo_pkg watch`.

## Install

Add to `Packages/manifest.json` (pin a 40-digit commit SHA, not a branch):

```json
"com.ludiars.ergo.packages": "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#<40-digit SHA>"
```

The package version (`0.x`) is independent of the Ergo tags.

## Set up the project as a UPM host

1. Run `ergo_pkg init --upm --code <project code> --host <project>` once. It writes only `.ergo/host.json`
   (commit it). Add `.ergo/watch-report.json` to the project's `.gitignore` (the report is not tracked).
2. Run `ergo_pkg watch --host <project>` (the scheduled job does this). It reads `Packages/manifest.json`, checks the
   LUDIARS git dependencies pinned to `#<sha>` and writes `<project>/.ergo/watch-report.json`.

## Window > LUDIARS > Ergo Packages

| Section | Shows |
|---|---|
| Packages | Every dependency of `manifest.json`: declared pin, the hash in `packages-lock.json`, status. Branch pins (`#main`) are warned; registry and `file:` dependencies are listed but not checked |
| Updates | Open updates of the report (tag / track / off-main) with read state. A dependency that could not be observed, or an old report, is never shown as "up to date" |
| Use this version | Shows the change in a dialog, rewrites that one value of `manifest.json` (other bytes unchanged) and calls `Client.Resolve()`. Commit and open a pull request yourself |

The tab shows the unread count; a new update is notified once (`ShowNotification`).

## Layout

| Assembly | Folder | Notes |
|---|---|---|
| `Ergo.Packages.Core` | `Editor/Core` | Plain C# (`noEngineReferences`): JSON reader, manifest / lock / report readers, `PinPlanner`, `ManifestJsonEditor`, read state |
| `Ergo.Packages.Editor` | `Editor` | Window, notifier, applier |
| `Ergo.Packages.Tests.Editor` | `Tests/Editor` | NUnit tests of Core. `Fixtures/` are byte-identical copies of the `*.json` directly in Ergo's `tests/package_watch/fixtures` (its `provenance/` holds Revisor / Concordia API samples only the native tests read) |

To run the tests in Unity, add `"testables": ["com.ludiars.ergo.packages"]` to `manifest.json` and use the Test
Runner (EditMode). Without Unity: `dotnet test tests/unity_packages` in the Ergo checkout.
