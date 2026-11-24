export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] PluginRegistry.SignatureAndCanonicalJson')
    && result.stdout.includes('[       OK ] PluginRegistry.RejectsWrongRootAndExpiredSigner')
    && result.stdout.includes('[       OK ] PluginRegistry.RejectsTamperingAndRollback'))
    || 'PluginRegistry C-12: every regression must execute and pass',
};
