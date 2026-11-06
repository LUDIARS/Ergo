export default {
  post: (result) => {
    const problems = [];
    if (result.layersStale.length) problems.push(`layers.json still globs ${result.layersStale.join(', ')}`);
    if (result.layersMissing.length) problems.push(`layers.json lacks ${result.layersMissing.join(', ')}`);
    if (result.domainStale.length) problems.push(`dev-tools.domain.json still matches ${result.domainStale.join(', ')}`);
    if (result.domainMissing.length) problems.push(`dev-tools.domain.json lacks ${result.domainMissing.join(', ')}`);
    if (result.domainDescriptionStale) problems.push('dev-tools.domain.json description still describes tools/ergo');
    return problems.length === 0 || problems.join('; ');
  },
};
