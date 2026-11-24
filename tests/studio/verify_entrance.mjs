import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:aa97b3eb */
import augurContract_50b1786d from './contracts/entrance.contract.mjs'; /* augur-inject:contract-predicate:22c474be */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_ingress)
export function verifyStudioEntrance(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioEntrance = contract(verifyStudioEntrance, { ...augurContract_50b1786d, contractId: 'C-5', mode: 'observe', sample: 1, where: 'tests/studio/verify_entrance.mjs:4', rule: 'contract-wrap', id: '50b1786d' }); /* augur-inject:contract-wrap:50b1786d */

if (!process.argv[2]) throw new Error('test_studio_ingress executable path is required');
const result = verifyStudioEntrance(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
