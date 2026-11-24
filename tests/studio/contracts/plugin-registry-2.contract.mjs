export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] PluginRegistry.RevocationFailsClosed')
    && result.stdout.includes('[       OK ] PluginRegistry.MissingRootKeepsUntrustedEntries')
    && result.stdout.includes('[       OK ] PluginRegistry.PersistsSerialAndFiltersInstalledPacks'))
    || 'PluginRegistry C-13: every regression must execute and pass',
};
