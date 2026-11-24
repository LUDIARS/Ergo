import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:8bdb8225 */
import augurContract_9fc26efc from './contracts/plugin-registry-1.contract.mjs'; /* augur-inject:contract-predicate:1eb6f288 */
import augurContract_3609420e from './contracts/plugin-registry-2.contract.mjs'; /* augur-inject:contract-predicate:be69277f */
import augurContract_6f7ec6ae from './contracts/plugin-registry-3.contract.mjs'; /* augur-inject:contract-predicate:9dd6b265 */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b
// The project's mini-gtest runs every case in the executable.
const runs = new Map();
function run(executable) {
  if (!runs.has(executable)) runs.set(executable, spawnSync(executable, [], { encoding: 'utf8', shell: false }));
  return runs.get(executable);
}

export function verifyPluginRegistrySignatures(executable) {
  return run(executable);
}
// @ts-expect-error augur-inject
verifyPluginRegistrySignatures = contract(verifyPluginRegistrySignatures, { ...augurContract_9fc26efc, contractId: 'C-12', mode: 'observe', sample: 1, where: 'tests/studio/verify_plugin_registry.mjs:4', rule: 'contract-wrap', id: '9fc26efc' }); /* augur-inject:contract-wrap:9fc26efc */

export function verifyPluginRegistryRevocations(executable) {
  return run(executable);
}
// @ts-expect-error augur-inject
verifyPluginRegistryRevocations = contract(verifyPluginRegistryRevocations, { ...augurContract_3609420e, contractId: 'C-13', mode: 'observe', sample: 1, where: 'tests/studio/verify_plugin_registry.mjs:11', rule: 'contract-wrap', id: '3609420e' }); /* augur-inject:contract-wrap:3609420e */

export function verifyPluginRegistryBoundary(executable) {
  return run(executable);
}
// @ts-expect-error augur-inject
verifyPluginRegistryBoundary = contract(verifyPluginRegistryBoundary, { ...augurContract_6f7ec6ae, contractId: 'C-14', mode: 'observe', sample: 1, where: 'tests/studio/verify_plugin_registry.mjs:18', rule: 'contract-wrap', id: '6f7ec6ae' }); /* augur-inject:contract-wrap:6f7ec6ae */

const executable = process.argv[2];
if (!executable) throw new Error('test_studio_registry executable path is required');
const result = verifyPluginRegistrySignatures(executable);
verifyPluginRegistryRevocations(executable);
verifyPluginRegistryBoundary(executable);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
