export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] PluginRegistry.FetchesOnlyAllowedReleaseAssets')
    && result.stdout.includes('[       OK ] PluginRegistry.RemoteRegistryMutationsAreRefused'))
    || 'PluginRegistry C-14: every regression must execute and pass',
};
