export default {
  post: (result) => result.stale.length === 0
    || `live tools/ergo instructions remain: ${result.stale.slice(0, 5).join('; ')}`,
};
