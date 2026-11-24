import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:91040b89 */
import augurContract_3316bcad from './contracts/served-profile.contract.mjs'; /* augur-inject:contract-predicate:4ee8141e */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_plugins)
export function verifyStudioServedProfile(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioServedProfile = contract(verifyStudioServedProfile, { ...augurContract_3316bcad, contractId: 'C-8', mode: 'observe', sample: 1, where: 'tests/studio/verify_served_profile.mjs:4', rule: 'contract-wrap', id: '3316bcad' }); /* augur-inject:contract-wrap:3316bcad */

if (!process.argv[2]) throw new Error('test_studio_plugins executable path is required');
const result = verifyStudioServedProfile(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
