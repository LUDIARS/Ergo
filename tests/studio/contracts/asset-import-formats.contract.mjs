export default {
  post(result) {
    const lines = result.stdout?.split(/\r?\n/) ?? [];
    return (result.status === 0 && [
      'AssetImport.SourceFamilies',
      'AssetImport.SourcePriority',
    ].every((name) => {
      const passed = '[       OK ] ' + name;
      return lines.some((line) => line === passed || line.startsWith(passed + ' ('));
    }))
      || 'Source families and selection regressions must execute and pass';
  },
};
