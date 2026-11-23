// A Studio built without Odeum (no ERGO_ODEUM_SOURCE_DIR): building, running
// and logs work; video and input are refused.
#include "remote_play.h"

namespace ergo::studio::plugins::play {
namespace {

class UnavailableRemotePlay final : public IRemotePlay {
public:
    bool      streaming() const override { return false; }
    bool      input() const override { return false; }
    bool      turn() const override { return false; }
    JsonValue ice_servers(std::string* warning) override {
        if (warning != nullptr) *warning = "remote play is not in this build (configure with ERGO_ODEUM_SOURCE_DIR)";
        return JsonValue::make_array();
    }
    bool open(const std::string&, const PlayTarget&, const std::string&, std::function<void()>, std::string&,
              std::string& err) override {
        err = "webrtc_unavailable";
        return false;
    }
    void        close(const std::string&) override {}
    std::size_t viewers(const std::string&) const override { return 0; }
    bool        known_key(const std::string&) const override { return false; }
    bool        key(const std::string&, const std::string&, bool, std::string& err) override { return refuse(err); }
    bool pointer(const std::string&, double, double, int, PointerPhase, std::string& err) override { return refuse(err); }
    bool wheel(const std::string&, double, double, int, std::string& err) override { return refuse(err); }

private:
    static bool refuse(std::string& err) {
        err = "input_unsupported";
        return false;
    }
};

} // namespace

std::unique_ptr<IRemotePlay> make_remote_play(const RemotePlayConfig&, host::LogFn) {
    return std::make_unique<UnavailableRemotePlay>();
}

} // namespace ergo::studio::plugins::play
