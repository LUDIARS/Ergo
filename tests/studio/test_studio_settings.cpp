// ergo_studio's command line and environment (tools/studio/studio_settings.h).
#include "studio_settings.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

using namespace ergo::studio::app;

namespace {

EnvFn env_of(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](const std::string& k) {
        const auto it = vars.find(k);
        return it == vars.end() ? std::string() : it->second;
    };
}

/// read_settings over a literal command line (argv[0] is the program).
StudioSettings read(std::vector<std::string> args, std::map<std::string, std::string> vars = {}) {
    args.insert(args.begin(), "ergo_studio");
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(a.data());
    return read_settings(env_of(std::move(vars)), "/opt/ergo", static_cast<int>(argv.size()), argv.data());
}

} // namespace

TEST(StudioSettings, NoArgumentsStartsTheWindowOnTheDefaultPort) {
    const auto s = read({});
    EXPECT_TRUE(s.error.empty());
    EXPECT_FALSE(s.headless);
    EXPECT_FALSE(s.show_help);
    EXPECT_EQ(s.port, kDefaultPort);
    EXPECT_EQ(s.bind_host, "127.0.0.1");
}

TEST(StudioSettings, HelpInBothSpellings) {
    for (const char* a : {"--help", "-h"}) {
        const auto s = read({a});
        EXPECT_TRUE(s.show_help);
        EXPECT_TRUE(s.error.empty());
    }
}

TEST(StudioSettings, HeadlessWithPortAndHost) {
    const auto s = read({"--headless"}, {{"PORT", "5491"}, {"ERGO_HOST", " 127.0.0.2 "}});
    EXPECT_TRUE(s.error.empty());
    EXPECT_TRUE(s.headless);
    EXPECT_FALSE(s.show_help);
    EXPECT_EQ(s.port, 5491);
    EXPECT_EQ(s.bind_host, "127.0.0.2");
}

TEST(StudioSettings, HeadlessAndHelpTogether) {
    const auto s = read({"--headless", "--help"});
    EXPECT_TRUE(s.error.empty());
    EXPECT_TRUE(s.headless);
    EXPECT_TRUE(s.show_help);
}

TEST(StudioSettings, UnknownOptionIsAnErrorNamingItAndHelp) {
    const auto s = read({"--headless", "--no-such-option"});
    EXPECT_NE(s.error.find("unknown option \"--no-such-option\""), std::string::npos);
    EXPECT_NE(s.error.find("--help"), std::string::npos);
}

TEST(StudioSettings, UnknownOptionWinsOverHelpAndIsReportedBeforeABadPort) {
    EXPECT_FALSE(read({"--help", "--port=1"}).error.empty());
    const auto s = read({"-x"}, {{"PORT", "abc"}});
    EXPECT_NE(s.error.find("\"-x\""), std::string::npos);
}

TEST(StudioSettings, PositionalArgumentIsAnError) {
    const auto s = read({"5170"});
    EXPECT_NE(s.error.find("unexpected argument \"5170\""), std::string::npos);
}

TEST(StudioSettings, BadPortIsAnError) {
    for (const char* p : {"abc", "0", "65536", "80x"}) {
        EXPECT_NE(read({}, {{"PORT", p}}).error.find("PORT must be 1..65535"), std::string::npos);
    }
}

TEST(StudioSettings, UsageListsEveryOptionAndVariableReadSettingsReads) {
    const std::string u = studio_usage();
    for (const char* word : {"--headless", "--help", "-h", "PORT", "ERGO_HOST", "ERGO_PLUGIN_DIR", "ERGO_STUDIO_HOME",
                             "LUDIARS_ROOT", "EXCUBITOR_URL", "ERGO_ACTIO_TOKEN", "ERGO_CURARE_URL", "ERGO_CURARE_TOKEN",
                             "ERGO_PLUGIN_OWNERS", "ERGO_STUDIO_TIMEOUT_MS", "VISUS_PROJECT_ROOTS",
                             "ERGO_UI_LAYOUT_ROOT", "ERGO_PICTOR_PROFILE_DIR", "ERGO_VFX_BACKEND", "ERGO_VFX_MODEL",
                             "CLAUDE_BIN", "ERGO_STUDIO_FONT", "WINDIR", "ERGO_SONUS_HOST", "ERGO_SONUS_PORT",
                             "ERGO_SONUS_PROJECT", "ERGO_SONUS_BUILD_DIR", "ERGO_SONUS_RECORDING_DIR",
                             "ERGO_SONUS_TICK_MS"}) {
        EXPECT_NE(u.find(word), std::string::npos);
    }
    EXPECT_NE(u.find("default " + std::to_string(kDefaultPort)), std::string::npos);
#ifdef _WIN32
    EXPECT_NE(u.find("USERPROFILE"), std::string::npos);
#else
    EXPECT_NE(u.find("HOME "), std::string::npos);
#endif
}

TEST(StudioSettings, SonusPluginSettings) {
    const auto d = read({});
    EXPECT_TRUE(d.error.empty());
    EXPECT_EQ(d.sonus.host, "127.0.0.1");
    EXPECT_EQ(d.sonus.port, ergo::studio::plugins::sonus::kSonusDefaultPort);
    EXPECT_TRUE(d.sonus.project_dir.empty());
    EXPECT_TRUE(d.sonus.recording_dir.empty());
    EXPECT_EQ(d.sonus.tick_ms, 50);

    const auto s = read({}, {{"ERGO_SONUS_HOST", " 192.168.0.7 "}, {"ERGO_SONUS_PORT", "31000"},
                             {"ERGO_SONUS_PROJECT", "/p"}, {"ERGO_SONUS_BUILD_DIR", "/b"},
                             {"ERGO_SONUS_RECORDING_DIR", "/r"}, {"ERGO_SONUS_TICK_MS", "20"}});
    EXPECT_TRUE(s.error.empty());
    EXPECT_EQ(s.sonus.host, "192.168.0.7");
    EXPECT_EQ(s.sonus.port, 31000);
    EXPECT_EQ(s.sonus.project_dir, "/p");
    EXPECT_EQ(s.sonus.build_dir, "/b");
    EXPECT_EQ(s.sonus.recording_dir, "/r");
    EXPECT_EQ(s.sonus.tick_ms, 20);

    for (const char* p : {"abc", "0", "70000"}) {
        const auto bad = read({}, {{"ERGO_SONUS_PORT", p}});
        EXPECT_NE(bad.error.find("ERGO_SONUS_PORT must be 1..65535"), std::string::npos);
        EXPECT_EQ(bad.sonus.port, ergo::studio::plugins::sonus::kSonusDefaultPort);
    }
    EXPECT_NE(read({}, {{"ERGO_SONUS_TICK_MS", "5"}}).error.find("ERGO_SONUS_TICK_MS"), std::string::npos);
    EXPECT_NE(read({}, {{"ERGO_SONUS_BUILD_DIR", "/b"}}).error.find("needs ERGO_SONUS_PROJECT"), std::string::npos);
    // A bad PORT is reported before a bad sonus setting.
    EXPECT_NE(read({}, {{"PORT", "x"}, {"ERGO_SONUS_PORT", "x"}}).error.find("PORT must be"), std::string::npos);
}
