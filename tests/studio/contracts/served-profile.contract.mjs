export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] ProfileSource.TheServedProjectIsTheOnlyProfileThePluginsSee'))
    || 'test_studio_plugins: every C-8 regression must execute and pass',
};
