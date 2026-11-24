export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] DevServer.StartOpensTheProjectAndServesItStopClosesIt')
    && result.stdout.includes('[       OK ] DevServer.AnotherProjectStopsTheCurrentOneFirst')
    && result.stdout.includes('[       OK ] DevServer.AFailedStartLeavesNothingOpen')
    && result.stdout.includes('[       OK ] DevServer.TheRemoteGateShowsItsTargetAndTheNames')
    && result.stdout.includes('[       OK ] DevServer.ServesWithoutAProjectForAHeadlessStart')
    && result.stdout.includes('[       OK ] LauncherRoutes.AddAndRemoveAreRefusedOnTheRemoteGate'))
    || 'test_studio_project: every C-7 regression must execute and pass',
};
