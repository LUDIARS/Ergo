export default {
  post: (models, expectedModel) =>
    (models.length === 2 && models.every((model) => model === expectedModel))
    || 'Both SDK requests must send the expected model ID',
};
