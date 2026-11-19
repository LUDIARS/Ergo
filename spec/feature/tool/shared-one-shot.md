# Shared one-shot VFX generator launch

The foundation supporting domain owns the developer-tool transport adapter.
Its VfxGenerator CLI implementation uses Lapilli's `@ludiars/one-shot` from the
pinned `lib/lapilli` submodule, linked as a local file dependency.

Lapilli resolves executables and model roles and isolates the subscription CLI
environment. The plugin retains prompt generation, stdin, JSON envelopes,
preset validation and failure reporting. VFX Suggest and VFX Breed default
to the `haiku` role on both backends. The CLI health check reports the resolved
model. SDK health reports the configured role or ID; the SDK resolves the role
through Lapilli when sending each request. If a role has no assigned ID, the SDK
request fails before contacting Anthropic. An explicit `ERGO_VFX_MODEL` model
ID still works without a role map.

Actio: `actio:ec1f0fa8-3c2e-4567-8dee-1a9479d075fb`.

Initialize the submodule before installing tools/ergo. Build validation uses
`npm run build` there; no service or live inference is launched. Rollback
restores the adapter, dependency and gitlink together.
