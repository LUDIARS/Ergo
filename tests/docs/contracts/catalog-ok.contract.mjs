export default {
  post: (result) => result.ok || `test_module_catalog did not print OK (exit ${result.status})`,
};
