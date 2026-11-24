export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] StudioEntrance.TheLocalGateTakesLoopbackNamesWithASameOrigin')
    && result.stdout.includes('[       OK ] StudioEntrance.TheLocalGateRefusesPublishedNames')
    && result.stdout.includes('[       OK ] StudioEntrance.APublicNameNeedsTheAccessAssertion')
    && result.stdout.includes('[       OK ] StudioEntrance.ATailnetNameNeedsTailscaleUserLogin')
    && result.stdout.includes('[       OK ] StudioEntrance.TheRemoteGateRefusesOtherHostsAndCrossOrigins')
    && result.stdout.includes('[       OK ] StudioEntrance.TailnetNamesAreTheTsNetOnes')
    && result.stdout.includes('[       OK ] StudioRemoteScope.LoopbackOnlyOperationsAreRefusedAndReadingIsNot'))
    || 'test_studio_ingress: every C-5 regression must execute and pass',
};
