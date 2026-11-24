import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:e6501a36 */
import augurContract_652ac9f6 from './contracts/web-server.contract.mjs'; /* augur-inject:contract-predicate:b2df6e63 */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_web_server)
export function verifyStudioWebServer(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioWebServer = contract(verifyStudioWebServer, { ...augurContract_652ac9f6, contractId: 'C-6', mode: 'observe', sample: 1, where: 'tests/studio/verify_web_server.mjs:4', rule: 'contract-wrap', id: '652ac9f6' }); /* augur-inject:contract-wrap:652ac9f6 */

if (!process.argv[2]) throw new Error('test_studio_web_server executable path is required');
const result = verifyStudioWebServer(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
