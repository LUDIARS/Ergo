# Changelog

## 0.1.0

- First version (Ergo package manager stage W7).
- Package list from `Packages/manifest.json` and `Packages/packages-lock.json`, with a warning for branch pins (`#main`).
- Updates from `<project>/.ergo/watch-report.json` (written by `ergo_pkg watch`), unread count and a one-time notification per update.
- "Use this version": rewrites one value of `Packages/manifest.json` to `#<40-digit SHA>` after a confirmation dialog and calls `Client.Resolve()`. Never commits.
