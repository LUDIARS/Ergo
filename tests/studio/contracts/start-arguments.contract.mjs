export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] StudioSettings.TheRemoteGateIsOffUnlessAPortIsGiven')
    && result.stdout.includes('[       OK ] StudioSettings.HeadlessProjectServesThatProfile')
    && result.stdout.includes('[       OK ] StudioSettings.TheRemotePortComesFromTheArgumentOrTheEnvironment')
    && result.stdout.includes('[       OK ] StudioSettings.PublicAndTailnetNamesAreRead')
    && result.stdout.includes('[       OK ] StudioSettings.UsageListsEveryOptionAndVariableReadSettingsReads'))
    || 'test_studio_settings: every C-11 regression must execute and pass',
};
