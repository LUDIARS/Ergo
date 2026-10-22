export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] CustosModule.content_length_invalid_returns_400_and_server_survives')
    && result.stdout.includes('[       OK ] CustosModule.content_length_valid_preserves_body_and_zero_length'))
    || 'Custos Content-Length regressions must both execute and pass',
};
