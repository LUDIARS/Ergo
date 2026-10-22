import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:6622f5bb */
import augurContract_a56b239a from './contracts/content-length.contract.mjs'; /* augur-inject:contract-predicate:538a3a69 */

// Actio: actio:45d56235-e75d-4b58-b8db-0e0c87fb54fe
export function verifyCustosContentLength(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyCustosContentLength = contract(verifyCustosContentLength, { ...augurContract_a56b239a, contractId: 'C-2', mode: 'observe', sample: 1, where: 'tests/custos/verify_content_length.mjs:4', rule: 'contract-wrap', id: 'a56b239a' }); /* augur-inject:contract-wrap:a56b239a */

if (!process.argv[2]) throw new Error('regression executable path is required');
const result = verifyCustosContentLength(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
