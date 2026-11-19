// SonusLink over snd::tool::Session (Sonus::tool_core, CONTRACTS.md D-101).
// The only Sonus-including file of the sonus plugin; compiled as C++20 in
// ergo_studio_sonus_link (Sonus::tool_core carries cxx_std_20 to it alone).
#include "sonus_link.h"

#include "sonus_config.h"

#include "snd/tool/names.hpp"
#include "snd/tool/session.hpp"

#include <algorithm>
#include <cctype>

namespace ergo::studio::plugins::sonus {

static_assert(kSonusDefaultPort == snd::livelink::kDefaultPort,
              "sonus_config.h kSonusDefaultPort must be Sonus' live link default port");

namespace tool = snd::tool;

static_assert(static_cast<int>(LinkState::Offline) == static_cast<int>(tool::LinkState::Offline) &&
                  static_cast<int>(LinkState::Connecting) == static_cast<int>(tool::LinkState::Connecting) &&
                  static_cast<int>(LinkState::Handshake) == static_cast<int>(tool::LinkState::Handshake) &&
                  static_cast<int>(LinkState::Online) == static_cast<int>(tool::LinkState::Online) &&
                  static_cast<int>(LinkState::Rejected) == static_cast<int>(tool::LinkState::Rejected),
              "sonus_shown.h LinkState must follow snd::tool::LinkState");

namespace {

std::string lower(const char* s) {
    std::string out = s ? s : "";
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

MeterShown meter_of(const tool::BusMeter* m) {
    MeterShown s;
    if (m == nullptr) return s;
    s.present = true;
    s.clipped = m->clipped;
    s.stale   = m->stale;
    float hold = kFloorDb;
    for (std::uint32_t c = 0; c < m->channels && c < SND_MAX_CHANNELS; ++c) {
        s.peak = std::max(s.peak, m->peak[c]);
        s.rms  = std::max(s.rms, m->rms[c]);
        hold   = std::max(hold, m->holdDb[c]);
    }
    s.peak_db = to_db(s.peak);
    s.rms_db  = to_db(s.rms);
    s.hold_db = std::max(hold, kFloorDb);
    return s;
}

float floor_db(float db) { return db < kFloorDb ? kFloorDb : db; }

} // namespace

struct SonusLink::Impl {
    tool::Session session;
    bool          project_loaded = false;

    std::string bus_name(std::uint32_t id) const {
        if (id == 0) return "output";
        const std::int32_t i = session.project().FindBus(id);
        if (i >= 0) return session.project().buses()[static_cast<std::size_t>(i)].name;
        return tool::Hex32(id);
    }
    const tool::BusMeter* bus_meter(const tool::BusDesc& b) const {
        // The output (bus id 0 in the protocol) is the master's signal.
        const tool::BusMeter* m = b.master ? session.profiler().FindMeter(0u) : nullptr;
        return m != nullptr ? m : session.profiler().FindMeter(b.id);
    }
    bool valid_bus(int bus) const {
        return bus >= 0 && static_cast<std::size_t>(bus) < session.project().buses().size();
    }
};

SonusLink::SonusLink() : impl_(std::make_unique<Impl>()) {}
SonusLink::~SonusLink() { impl_->session.Disconnect(); }

bool SonusLink::load_project(const std::string& dir, const std::string& build_dir) {
    impl_->project_loaded = impl_->session.LoadProject(dir, build_dir);
    return impl_->project_loaded;
}

std::string SonusLink::connect(const std::string& host, uint16_t port) {
    tool::ConnectionDesc d;
    d.address       = host;
    d.port          = port;
    d.name          = "ergo-studio";
    d.policy        = tool::MismatchPolicy::SendDiffs;   // the Studio's (project's) values are the truth
    d.autoReconnect = true;
    const int r = impl_->session.Connect(d);
    return r == SND_OK ? std::string() : std::string(tool::ResultName(r));
}

void SonusLink::disconnect() { impl_->session.Disconnect(); }
bool SonusLink::is_open() const { return impl_->session.connection().IsOpen(); }
void SonusLink::tick() { impl_->session.Tick(); }

bool SonusLink::set_bus_fader(int bus, float db) {
    if (!impl_->valid_bus(bus)) return false;
    return impl_->session.mixer().Set(tool::Target{tool::Control::BusFader, bus, 0, 0}, db);
}
bool SonusLink::set_bus_mute(int bus, bool on) {
    if (!impl_->valid_bus(bus)) return false;
    return impl_->session.mixer().Set(tool::Target{tool::Control::BusMute, bus, 0, 0}, on ? 1.0f : 0.0f);
}
bool SonusLink::set_bus_solo(int bus, bool on) {
    if (!impl_->valid_bus(bus)) return false;
    return impl_->session.mixer().Set(tool::Target{tool::Control::BusSolo, bus, 0, 0}, on ? 1.0f : 0.0f);
}
bool SonusLink::undo() { return impl_->session.mixer().Undo(); }
bool SonusLink::redo() { return impl_->session.mixer().Redo(); }

int SonusLink::find_bus(const std::string& key) const {
    const auto& buses = impl_->session.project().buses();
    for (std::size_t i = 0; i < buses.size(); ++i) {
        if (buses[i].key == key) return static_cast<int>(i);
    }
    return -1;
}

bool SonusLink::revert(const std::string& key) {
    for (const tool::OverrideItem& o : impl_->session.mixer().Overrides()) {
        if (o.key == key) return impl_->session.mixer().Revert(o);
    }
    return false;
}
bool SonusLink::revert_all() { return impl_->session.mixer().RevertAll(); }

bool SonusLink::start_recording() { return impl_->session.StartRecording(); }
bool SonusLink::stop_recording() { return impl_->session.StopRecording(); }

std::string SonusLink::save_recording(const std::string& path) {
    tool::Session& s = impl_->session;
    if (s.profiler().recordingState() != tool::RecordingState::Ready) {
        return std::string("the recording is not ready (state ") +
               lower(tool::RecordingStateName(s.profiler().recordingState())) + ")";
    }
    std::string err;
    if (!s.profiler().SaveRecording(s.connection().client(), s.CurrentRecordingInfo(), path, &err)) {
        return err.empty() ? std::string("saving failed") : err;
    }
    s.Message("recording saved: " + path);
    return {};
}

SonusShown SonusLink::shown() const {
    const tool::Session&    s   = impl_->session;
    const tool::Connection& c   = s.connection();
    const tool::Project&    p   = s.project();
    const tool::MixerModel& mix = s.mixer();
    const tool::Profiler&   pr  = s.profiler();
    SonusShown out;

    out.state         = static_cast<LinkState>(c.state());
    out.target        = c.IsOpen() ? c.Target() : std::string();
    out.mismatch      = c.mismatch();
    out.sessions      = c.sessions();
    out.sample_rate   = c.hello().sampleRate;
    out.block_size    = c.hello().blockSize;
    out.round_trip_us = c.roundTripUs();

    out.project_loaded = impl_->project_loaded;
    out.project_name   = p.name();
    for (const tool::ProjectIssue& i : p.issues()) {
        if (i.error) out.project_errors.push_back((i.where.empty() ? std::string() : i.where + ": ") + i.message);
    }

    const auto& strips = mix.buses();
    for (std::size_t i = 0; i < p.buses().size(); ++i) {
        const tool::BusDesc& b = p.buses()[i];
        BusShown             r;
        r.index  = static_cast<int>(i);
        r.key    = b.key;
        r.name   = b.name;
        r.id     = b.id;
        r.depth  = static_cast<int>(b.depth);
        r.master = b.master;
        const tool::Target fader{tool::Control::BusFader, static_cast<std::int32_t>(i), 0, 0};
        r.fader_db     = floor_db(mix.Get(fader));
        r.bank_db      = floor_db(mix.BankValue(fader));
        r.effective_db = floor_db(mix.EffectiveBusDb(static_cast<std::int32_t>(i)));
        if (i < strips.size()) {
            r.muted  = strips[i].muted;
            r.soloed = strips[i].soloed;
        }
        r.solo_muted = mix.SoloMuted(static_cast<std::int32_t>(i));
        r.meter      = meter_of(impl_->bus_meter(b));
        out.buses.push_back(std::move(r));
    }
    out.undo_count      = mix.history().UndoCount();
    out.redo_count      = mix.history().RedoCount();
    out.cells_pending   = s.sync().Count(tool::CellState::Pending);
    out.cells_failed    = s.sync().Count(tool::CellState::Failed);
    out.cells_no_answer = s.sync().NoAnswer(tool::Session::NowNs());
    for (const tool::OverrideItem& o : mix.Overrides()) {
        out.overrides.push_back({o.key, o.label, o.bankValue, o.value, o.muted});
    }

    const tool::TimingSummary t = pr.Summary();
    out.timing = {t.samples,      t.lastBlock,   t.lastRenderUs, t.avgRenderUs, t.maxRenderUs, t.lastLogicUs, t.avgLogicUs,
                  t.budgetUs,     t.loadPercent, t.lod,          t.lateChunks,  t.underruns,   t.overBudget};
    for (const tool::BusMeter& m : pr.meters()) out.meters.push_back({m.busId, impl_->bus_name(m.busId), meter_of(&m)});
    out.real_voices = pr.realVoices();
    for (const snd::livelink::VoiceInfo& v : pr.voices()) {
        out.voices.push_back({v.eventId, impl_->bus_name(v.busId), lower(tool::PlaybackStateName(v.playbackState)),
                              lower(tool::SourceKindName(v.sourceKind)), v.audibility, v.gain, v.isVirtual != 0u});
    }
    out.has_memory = pr.hasMemory();
    if (out.has_memory) {
        const snd::livelink::MemoryUsage& m = pr.memory();
        for (std::uint32_t tag = 0; tag < SND_MEM_TAG_COUNT; ++tag) {
            if (m.used[tag] == 0 && m.budget[tag] == 0) continue;
            out.memory.push_back({tool::MemTagName(tag), m.used[tag], m.budget[tag]});
        }
    }
    const auto& log   = pr.log();
    const std::size_t first = log.size() > kMaxLog ? log.size() - kMaxLog : 0;
    for (std::size_t i = first; i < log.size(); ++i) {
        const snd::livelink::LogEntry& e = log[i].entry;
        out.log.push_back({e.clockSample, tool::LogKindName(e.kind), e.id, e.detail});
    }
    out.stalls      = pr.stalls();
    out.lod_changes = pr.lodChanges();

    out.recording.state      = lower(tool::RecordingStateName(pr.recordingState()));
    out.recording.bytes      = pr.recordedBytes();
    out.recording.error      = pr.recordingError();
    out.recording.error_name = pr.recordingError() != 0 ? tool::ResultName(pr.recordingError()) : "";
    out.recording.can_save   = pr.recordingState() == tool::RecordingState::Ready;

    out.messages = s.messages();
    out.ticks    = s.ticks();
    return out;
}

} // namespace ergo::studio::plugins::sonus
