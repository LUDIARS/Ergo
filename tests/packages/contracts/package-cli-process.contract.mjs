export default {
  post: (result) =>
    (result.status === 0 && result.signal === null
      && result.stdout.replaceAll('\r\n', '\n') === '[run-ok]\ntest_package_cli_process: ok\n'
      && result.stderr.replaceAll('\r\n', '\n') === 'run-stderr\n')
    || 'The native argument and stream regression suite must pass',
};
