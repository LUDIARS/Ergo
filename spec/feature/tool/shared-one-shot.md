# Shared one-shot VFX generator launch

> Historical note (2026-11-06): the Node / Electron developer tool `tools/ergo`
> that hosted this adapter was removed and replaced by the native
> `ergo_studio` (`tools/studio`). The `vfx-suggest` / `vfx-breed` plugins now
> call the Claude CLI through `tools/studio/plugins/common/claude_cli.*` with
> `ERGO_VFX_BACKEND=cli` only; the API SDK backend and the Lapilli
> `@ludiars/one-shot` dependency described below no longer apply. The text is
> kept as the record of the former design.

The foundation supporting domain owns the developer-tool transport adapter.
Its VfxGenerator CLI implementation uses Lapilli's `@ludiars/one-shot` from the
pinned `lib/lapilli` submodule, linked as a local file dependency.

Lapilli resolves executables and model roles and isolates the subscription CLI
environment. The plugin retains prompt generation, stdin, JSON envelopes,
preset validation and failure reporting. The default CLI model is the central
haiku role; explicit ERGO_VFX_MODEL values remain supported. Health reports the
resolved model. The optional API SDK backend retains its existing behavior.

Initialize the submodule before installing the former tools/ergo. Build
validation in the former tool used `npm run build`; no service or live
inference is launched. Rollback restores the adapter, dependency and gitlink
together.
