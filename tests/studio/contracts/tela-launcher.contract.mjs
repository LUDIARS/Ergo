export default {
  post: (result) => (result.status === 0
    && result.stdout.includes('[       OK ] StudioTelaLauncher.DeclaresTheLauncherAloneUntilAProjectIsOpen')
    && result.stdout.includes('[       OK ] StudioTelaLauncher.TheWindowIsTheLauncherAndItsRowsStartAndStopTheServer')
    && result.stdout.includes('[       OK ] StudioTelaLauncher.APageOpensTheBandOverTheTabsAndBackReturns')
    && result.stdout.includes('[       OK ] StudioTela.ShellRoutesServeThePageAndOpenPages'))
    || 'test_studio_tela: every C-10 regression must execute and pass',
};
