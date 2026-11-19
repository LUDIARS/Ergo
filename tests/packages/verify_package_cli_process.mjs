import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:a49db701 */
import augurContract_aca2a23e from './contracts/package-cli-process.contract.mjs'; /* augur-inject:contract-predicate:5d34fb4b */

export function verifyPackageCliProcess(executable, child) {
  return spawnSync(executable, [child], { encoding: 'utf8', shell: false });
}
// @ts-expect-error augur-inject
verifyPackageCliProcess = contract(verifyPackageCliProcess, { ...augurContract_aca2a23e, contractId: 'C-4', mode: 'observe', sample: 1, where: 'tests/packages/verify_package_cli_process.mjs:5', rule: 'contract-wrap', id: 'aca2a23e' }); /* augur-inject:contract-wrap:aca2a23e */

const [executable, child] = process.argv.slice(2);
if (!executable || !child) throw new Error('Expected the process test and child executable paths');
const result = verifyPackageCliProcess(executable, child);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
