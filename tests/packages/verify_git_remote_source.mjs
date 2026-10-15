import { spawnSync } from 'node:child_process';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:965d1c69 */
import augurContract_12441f84 from './contracts/git-remote-source.contract.mjs'; /* augur-inject:contract-predicate:01d141b9 */

// Actio: actio:5b01b9e1-28ea-4082-86cb-05d379489fcd
// The native regression uses a capture spy: it never launches git.
export function verifyGitRemoteSource(executable) {
  return spawnSync(executable, [], { encoding: 'utf8', shell: false });
}
// @ts-expect-error augur-inject
verifyGitRemoteSource = contract(verifyGitRemoteSource, { ...augurContract_12441f84, contractId: 'C-1', mode: 'observe', sample: 1, where: 'tests/packages/verify_git_remote_source.mjs:5', rule: 'contract-wrap', id: '12441f84' }); /* augur-inject:contract-wrap:12441f84 */

if (!process.argv[2]) throw new Error('regression executable path is required');
const result = verifyGitRemoteSource(process.argv[2]);
if (result.error) throw result.error;
process.stdout.write(result.stdout ?? '');
process.stderr.write(result.stderr ?? '');
process.exitCode = result.status ?? 1;
