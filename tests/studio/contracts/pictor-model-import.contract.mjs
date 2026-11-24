export default {
  post(result) {
    const lines = result.stdout?.split(/\r?\n/) ?? [];
    return (result.status === 0 && [
      'AssetImport.PictorModelLoaders',
      'AssetImport.BrokenPreferredModel',
    ].every((name) => {
      const passed = '[       OK ] ' + name;
      return lines.some((line) => line === passed || line.startsWith(passed + ' ('));
    })) || 'Pictor import and invalid preferred source regressions must execute and pass';
  },
};
