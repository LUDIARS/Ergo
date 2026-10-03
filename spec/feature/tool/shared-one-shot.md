# Shared one-shot VFX generator launch

The foundation supporting domain owns the developer-tool transport adapter.
Its VfxGenerator CLI implementation uses Lapilli's `@ludiars/one-shot` from the
pinned `lib/lapilli` submodule, linked as a local file dependency.

Lapilli resolves executables and model roles and isolates the subscription CLI
environment. The plugin retains prompt generation, stdin, JSON envelopes,
preset validation and failure reporting. The default CLI model is the central
haiku role; explicit ERGO_VFX_MODEL values remain supported. Health reports the
resolved model. The optional API SDK backend retains its existing behavior.

Initialize the submodule before installing tools/ergo. Build validation uses
`npm run build` there; no service or live inference is launched. Rollback
restores the adapter, dependency and gitlink together.
