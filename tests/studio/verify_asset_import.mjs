import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:51d483d6 */
import augurContract_232bdde5 from './contracts/asset-import.contract.mjs'; /* augur-inject:contract-predicate:7840c5dc */

// Actio: actio:3ce4ecf9-5b7c-4630-8453-c4c69ae941f3
export function verifyAssetImport(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyAssetImport = contract(verifyAssetImport, { ...augurContract_232bdde5, contractId: 'C-12', mode: 'observe', sample: 1, where: 'tests/studio/verify_asset_import.mjs:4', rule: 'contract-wrap', id: '232bdde5' }); /* augur-inject:contract-wrap:232bdde5 */

if (!process.argv[2]) throw new Error('test executable path is required');
const result = verifyAssetImport(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
