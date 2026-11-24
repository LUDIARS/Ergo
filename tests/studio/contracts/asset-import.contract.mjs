export default {
  post: (result) => (result.status === 0 && [
    'AssetImport.FbxAnalysis',
    'AssetImport.IncrementalCatalog',
    'AssetImport.CaptureLifecycle',
  ].every((name) => result.stdout.includes('[       OK ] ' + name)))
    || 'The FBX, catalog and capture regressions must execute and pass',
};
