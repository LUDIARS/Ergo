import assert from 'node:assert/strict';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';
import { makeAdvisor } from '../../tools/ergo/src/plugins/vfx-breed/advisor.ts';
import { makeGenerator } from '../../tools/ergo/src/plugins/vfx-suggest/generator.ts';
import { DEFAULT_EFFECT } from '../../tools/ergo/src/plugins/vfx-suggest/schema.ts';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:16cb3dab */
import augurContract_916aa173 from './contracts/model-roles.contract.mjs'; /* augur-inject:contract-predicate:6dc47cbe */

const mappedModel = 'claude-fixture-mapped';
const explicitModel = 'claude-fixture-explicit';

export async function captureSdkModels(expectedModel, t, configuredModel) {
  const directory = mkdtempSync(join(tmpdir(), 'ergo-vfx-roles-'));
  const roleMap = join(directory, 'model-roles.json');
  t.after(() => rmSync(directory, { recursive: true, force: true }));
  if (configuredModel === undefined) {
    writeFileSync(roleMap, JSON.stringify({ schemaVersion: 1, roles: { claude: { haiku: mappedModel } } }));
  }

  const env = {
    ...process.env,
    ERGO_VFX_BACKEND: 'sdk',
    ERGO_VFX_MODEL: configuredModel,
    ANTHROPIC_API_KEY: 'ergo-test-key',
    ANTHROPIC_BASE_URL: 'https://anthropic.invalid',
    LUDIARS_MODEL_ROLES_FILE: roleMap,
    LUDIARS_ONESHOT_MODEL_HAIKU: undefined,
  };
  for (const key of [
    'ERGO_VFX_BACKEND', 'ERGO_VFX_MODEL', 'ANTHROPIC_API_KEY', 'ANTHROPIC_BASE_URL',
    'LUDIARS_MODEL_ROLES_FILE', 'LUDIARS_ONESHOT_MODEL_HAIKU',
  ]) {
    const previous = process.env[key];
    t.after(() => {
      if (previous === undefined) delete process.env[key];
      else process.env[key] = previous;
    });
    if (env[key] === undefined) delete process.env[key];
    else process.env[key] = env[key];
  }

  const models = [];
  t.mock.method(globalThis, 'fetch', async (_url, options) => {
    const request = JSON.parse(options.body);
    models.push(request.model);
    const content = request.tools
      ? [{ type: 'tool_use', name: 'output_presets', input: { presets: [] } }]
      : [{ type: 'text', text: '{"weights":{}}' }];
    return Response.json({ content });
  });

  assert.deepEqual(await makeGenerator().generate({ game: 'generic', scene: 'sparks' }, 1), []);
  assert.deepEqual(await makeAdvisor(env).suggest('fewer sparks', DEFAULT_EFFECT), {});
  return models;
}
// @ts-expect-error augur-inject
captureSdkModels = contract(captureSdkModels, { ...augurContract_916aa173, contractId: 'C-3', mode: 'observe', sample: 1, where: 'tests/vfx/verify_vfx_model_roles.mjs:13', rule: 'contract-wrap', id: '916aa173' }); /* augur-inject:contract-wrap:916aa173 */

test('both VFX SDK backends resolve the default Haiku role', async (t) => {
  assert.deepEqual(await captureSdkModels(mappedModel, t), [mappedModel, mappedModel]);
});

test('both VFX SDK backends preserve an explicit ID without the role map', async (t) => {
  assert.deepEqual(await captureSdkModels(explicitModel, t, explicitModel), [explicitModel, explicitModel]);
});
