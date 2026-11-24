export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] StudioUiLauncher.StartsOnTheLauncherAloneWithoutTabs')
    && result.stdout.includes('[       OK ] StudioUiLauncher.StartingTheServerKeepsTheLauncherAndStopCloses')
    && result.stdout.includes('[       OK ] StudioUiLauncher.OpeningFromElsewhereKeepsTheWindowOnTheLauncher'))
    || 'test_studio_ui: every C-9 regression must execute and pass',
};
