import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:979b7a5a */
import augurContract_5d7716b0 from './contracts/dev-server.contract.mjs'; /* augur-inject:contract-predicate:91754b4e */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_project)
export function verifyStudioDevServer(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioDevServer = contract(verifyStudioDevServer, { ...augurContract_5d7716b0, contractId: 'C-7', mode: 'observe', sample: 1, where: 'tests/studio/verify_dev_server.mjs:4', rule: 'contract-wrap', id: '5d7716b0' }); /* augur-inject:contract-wrap:5d7716b0 */

if (!process.argv[2]) throw new Error('test_studio_project executable path is required');
const result = verifyStudioDevServer(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
