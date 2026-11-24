import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:13ce7b49 */
import augurContract_d7680654 from './contracts/start-arguments.contract.mjs'; /* augur-inject:contract-predicate:82e2b406 */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_settings)
export function verifyStudioStartArguments(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioStartArguments = contract(verifyStudioStartArguments, { ...augurContract_d7680654, contractId: 'C-11', mode: 'observe', sample: 1, where: 'tests/studio/verify_start_arguments.mjs:4', rule: 'contract-wrap', id: 'd7680654' }); /* augur-inject:contract-wrap:d7680654 */

if (!process.argv[2]) throw new Error('test_studio_settings executable path is required');
const result = verifyStudioStartArguments(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
