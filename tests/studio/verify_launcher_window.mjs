import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:761fcb7d */
import augurContract_08dac5e7 from './contracts/launcher-window.contract.mjs'; /* augur-inject:contract-predicate:f643ef18 */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_ui)
export function verifyStudioLauncherWindow(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioLauncherWindow = contract(verifyStudioLauncherWindow, { ...augurContract_08dac5e7, contractId: 'C-9', mode: 'observe', sample: 1, where: 'tests/studio/verify_launcher_window.mjs:4', rule: 'contract-wrap', id: '08dac5e7' }); /* augur-inject:contract-wrap:08dac5e7 */

if (!process.argv[2]) throw new Error('test_studio_ui executable path is required');
const result = verifyStudioLauncherWindow(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
