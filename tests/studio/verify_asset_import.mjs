import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs';
import augurContract_bd8013da from './contracts/asset-import-formats.contract.mjs'; /* augur-inject:contract-predicate:98046b30 */
import augurContract_3fbca996 from './contracts/asset-import.contract.mjs'; /* augur-inject:contract-predicate:2e86fb2d */ /* augur-inject:import:51d483d6 */

// Actio: actio:3ce4ecf9-5b7c-4630-8453-c4c69ae941f3
export function verifyAssetImportFormats(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyAssetImportFormats = contract(verifyAssetImportFormats, { ...augurContract_bd8013da, contractId: 'C-16', mode: 'observe', sample: 1, where: 'tests/studio/verify_asset_import.mjs:5', rule: 'contract-wrap', id: 'bd8013da' }); /* augur-inject:contract-wrap:bd8013da */

export function verifyAssetImport(executable) {
  return verifyAssetImportFormats(executable);
}
// @ts-expect-error augur-inject
verifyAssetImport = contract(verifyAssetImport, { ...augurContract_3fbca996, contractId: 'C-15', mode: 'observe', sample: 1, where: 'tests/studio/verify_asset_import.mjs:12', rule: 'contract-wrap', id: '3fbca996' }); /* augur-inject:contract-wrap:3fbca996 */

if (!process.argv[2]) throw new Error('test executable path is required');
const result = verifyAssetImport(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
