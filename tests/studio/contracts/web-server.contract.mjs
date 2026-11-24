export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] StudioWebRoutes.OnlyTheTableReachesAHandler')
    && result.stdout.includes('[       OK ] StudioWebRoutes.TheRemoteGateRefusesLoopbackOnlyOperations')
    && result.stdout.includes('[       OK ] StudioWebRoutes.TheEntranceComesBeforeTheTable')
    && result.stdout.includes('[       OK ] StudioWebRoutes.WebSocketsNeedTheEntranceAndAnAcceptor')
    && result.stdout.includes('[       OK ] StudioWebServer.TheRemoteGateIsOffByDefault')
    && result.stdout.includes('[       OK ] StudioWebServer.AnswersTheTableAnd404sTheRest')
    && result.stdout.includes('[       OK ] StudioWebServer.ABodyOverTheLimitIsRefusedBeforeTheHandler')
    && result.stdout.includes('[       OK ] StudioWebServer.TheGateIsThePortNotAHeader')
    && result.stdout.includes('[       OK ] StudioWebServer.EngineWsClientConnectsAndEchoes'))
    || 'test_studio_web_server: every C-6 regression must execute and pass',
};
