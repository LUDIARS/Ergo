import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:ef536ccf */
import augurContract_203dcb2c from './contracts/tela-launcher.contract.mjs'; /* augur-inject:contract-predicate:68a5fe12 */

// Actio: actio:c6c6c340-8831-4fc9-b08f-f2054fdfbc1b (test_studio_tela)
export function verifyStudioTelaLauncher(executable) {
  return spawnSync(executable, [], {
    encoding: 'utf8',
    shell: false,
  });
}
// @ts-expect-error augur-inject
verifyStudioTelaLauncher = contract(verifyStudioTelaLauncher, { ...augurContract_203dcb2c, contractId: 'C-10', mode: 'observe', sample: 1, where: 'tests/studio/verify_tela_launcher.mjs:4', rule: 'contract-wrap', id: '203dcb2c' }); /* augur-inject:contract-wrap:203dcb2c */

if (!process.argv[2]) throw new Error('test_studio_tela executable path is required');
const result = verifyStudioTelaLauncher(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
