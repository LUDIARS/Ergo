// Acceptance checks for the docs sweep that replaced the Node / Electron
// `tools/ergo` with the native Ergo Studio (`tools/studio`, 2026-11-06).
// Actio: actio:df416e03-8ef7-4868-ae3a-4bf1fcdafdc8
//
//   node tests/docs/verify_studio_native_sweep.mjs [<repo root>]
//
// Exit code 0 when every check passes. Each exported function is one
// acceptance contract (see augur.contracts.json, C-3 .. C-5).
import { spawnSync } from 'node:child_process';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join, relative, resolve, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { contract } from '../../contracts/contract-runtime.mjs'; /* augur-inject:import:0bc4048f */
import augurContract_f9f23df8 from './contracts/catalog-ok.contract.mjs'; /* augur-inject:contract-predicate:029a29a9 */
import augurContract_9df34ec0 from './contracts/no-stale-tools-ergo.contract.mjs'; /* augur-inject:contract-predicate:7095b0d2 */
import augurContract_58639f62 from './contracts/studio-paths-in-domains.contract.mjs'; /* augur-inject:contract-predicate:13af6d6b */

/** C-3: the catalog test (links, plugin matrix, domains) passes. */
export function verifyCatalogOk(root) {
  const cmake = process.platform === 'win32' ? 'cmake.exe' : 'cmake';
  const result = spawnSync(cmake, ['-DERGO_SOURCE_DIR=.', '-P', 'tests/test_module_catalog.cmake'],
    { cwd: root, encoding: 'utf8', shell: false });
  const output = `${result.stdout ?? ''}${result.stderr ?? ''}`;
  return { status: result.status, ok: /test_module_catalog: OK/.test(output), output };
}
// @ts-expect-error augur-inject
verifyCatalogOk = contract(verifyCatalogOk, { ...augurContract_f9f23df8, contractId: 'C-3', mode: 'observe', sample: 1, where: 'tests/docs/verify_studio_native_sweep.mjs:15', rule: 'contract-wrap', id: 'f9f23df8' }); /* augur-inject:contract-wrap:f9f23df8 */

/** Documents the sweep must leave free of live `tools/ergo` instructions. */
export const SWEEP_DOCS = [
  'README.md', 'CLAUDE.md', 'module_list.md', 'module_list.yaml',
  'spec/domains/README.md', 'spec/feature/package-hub.md', 'spec/architecture/domains-and-ux.md',
];
export const SWEEP_DIRS = ['spec/setup', 'spec/feature/tool', 'spec/feature/module', 'spec/module'];
/** Files kept as historical design records; only a note at the top is required. */
export const HISTORICAL = new Set([
  'spec/feature/tool/ergo-redesign.md',
  'spec/feature/tool/render_pipeline_system_b.md',
]);
/** The reference spec, rewritten before this sweep (its history section names tools/ergo). */
export const REFERENCE = new Set(['spec/feature/tool/ergo.md']);
/** A `tools/ergo` mention is deliberate when the line says it is gone. */
const HISTORY_MARKERS = /旧|削除|廃止|履歴|置き換え|removed|replaced|former|deleted|[Hh]istorical/;
const STALE = /tools\/ergo|npm run|npm start|npm test|npm install|Electron/;

function listMarkdown(dir) {
  return readdirSync(dir).filter((n) => n.endsWith('.md')).map((n) => join(dir, n));
}

/** C-4: every remaining `tools/ergo` / npm / Electron mention is a history note. */
export function verifyNoStaleToolsErgoRefs(root) {
  const files = [
    ...SWEEP_DOCS.map((f) => join(root, f)),
    ...SWEEP_DIRS.flatMap((d) => listMarkdown(join(root, d))),
  ];
  const stale = [];
  const historical = [];
  for (const file of files) {
    const rel = relative(root, file).split(sep).join('/');
    if (REFERENCE.has(rel) || !statSync(file).isFile()) continue;
    const lines = readFileSync(file, 'utf8').split(/\r?\n/);
    const isHistory = HISTORICAL.has(rel);
    lines.forEach((line, i) => {
      if (!STALE.test(line)) return;
      if (isHistory || HISTORY_MARKERS.test(line)) historical.push(`${rel}:${i + 1}`);
      else stale.push(`${rel}:${i + 1}: ${line.trim()}`);
    });
  }
  return { stale, historical, files: files.length };
}
// @ts-expect-error augur-inject
verifyNoStaleToolsErgoRefs = contract(verifyNoStaleToolsErgoRefs, { ...augurContract_9df34ec0, contractId: 'C-4', mode: 'observe', sample: 1, where: 'tests/docs/verify_studio_native_sweep.mjs:43', rule: 'contract-wrap', id: '9df34ec0' }); /* augur-inject:contract-wrap:9df34ec0 */

/** C-5: Anatomia layer globs and the dev-tools domain point at the native tree. */
export function verifyStudioPathsInDomains(root) {
  const layers = JSON.parse(readFileSync(join(root, '.anatomia/layers.json'), 'utf8'));
  const domain = JSON.parse(readFileSync(join(root, 'spec/domains/dev-tools.domain.json'), 'utf8'));
  const globs = layers.layers.map((l) => l.glob);
  const patterns = domain.membership.map((m) => m.pathPattern);
  const wanted = ['tools/studio', 'src/studio', 'include/ergo/studio', 'tests/studio'];
  return {
    layersStale: globs.filter((g) => g.startsWith('tools/ergo/')),
    layersMissing: wanted.filter((w) => !globs.some((g) => g.startsWith(`${w}/`))),
    domainStale: patterns.filter((p) => p.includes('tools/ergo')),
    domainMissing: wanted.filter((w) => !patterns.some((p) => p.includes(w))),
    domainDescriptionStale: /tools\/ergo|Node|Electron/.test(domain.description),
  };
}
// @ts-expect-error augur-inject
verifyStudioPathsInDomains = contract(verifyStudioPathsInDomains, { ...augurContract_58639f62, contractId: 'C-5', mode: 'observe', sample: 1, where: 'tests/docs/verify_studio_native_sweep.mjs:65', rule: 'contract-wrap', id: '58639f62' }); /* augur-inject:contract-wrap:58639f62 */

const invokedDirectly = process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href;
if (invokedDirectly) {
  const root = resolve(process.argv[2] ?? process.cwd());
  const catalog = verifyCatalogOk(root);
  const refs = verifyNoStaleToolsErgoRefs(root);
  const domains = verifyStudioPathsInDomains(root);
  const report = { catalog: { ok: catalog.ok, status: catalog.status }, refs, domains };
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  if (!catalog.ok) process.stdout.write(catalog.output);
  const domainOk = !domains.layersStale.length && !domains.layersMissing.length
    && !domains.domainStale.length && !domains.domainMissing.length && !domains.domainDescriptionStale;
  process.exitCode = catalog.ok && refs.stale.length === 0 && domainOk ? 0 : 1;
}
