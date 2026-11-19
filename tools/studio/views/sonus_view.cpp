#include "sonus_view.h"

#include <algorithm>
#include <cstdio>

namespace ergo::studio::views {
namespace {

namespace sn = plugins::sonus;

constexpr float kBarWidth  = 160.0f;
constexpr float kBarHeight = 10.0f;
constexpr float kBarFloorDb = -60.0f;   // the left end of a meter bar
constexpr std::size_t kMaxVoices = 32;
constexpr std::size_t kMaxLogLines = 20;

std::string db_text(float db) {
    if (db <= sn::kFloorDb) return "-inf dB";
    char b[32];
    std::snprintf(b, sizeof b, "%+.1f dB", static_cast<double>(db));
    return b;
}

std::string number(double v, const char* fmt = "%.1f") {
    char b[48];
    std::snprintf(b, sizeof b, fmt, v);
    return b;
}

std::string bytes_text(uint64_t n) {
    if (n >= (1ull << 20)) return number(static_cast<double>(n) / (1024.0 * 1024.0), "%.1f MiB");
    if (n >= (1ull << 10)) return number(static_cast<double>(n) / 1024.0, "%.1f KiB");
    return std::to_string(n) + " B";
}

const char* state_label(sn::LinkState s) {
    switch (s) {
    case sn::LinkState::Offline: return "未接続";
    case sn::LinkState::Connecting: return "接続を試しています";
    case sn::LinkState::Handshake: return "接続の確認中";
    case sn::LinkState::Online: return "接続済み";
    case sn::LinkState::Rejected: return "ゲームに拒否されました (版が合いません)";
    }
    return "";
}

const char* recording_label(const std::string& state) {
    if (state == "idle") return "録画していません";
    if (state == "starting") return "開始中";
    if (state == "recording") return "録画中";
    if (state == "stopping") return "停止中";
    if (state == "ready") return "録画済み (保存できます)";
    if (state == "failed") return "失敗しました";
    return "";
}

/// A meter bar: a track and a fill as wide as the peak on a kBarFloorDb..0 dB scale.
void meter_bar(ui::ViewBuilder& row, const sn::MeterShown& m) {
    const ui::Theme& t = row.theme();
    ergo::ui_kit::Node track;
    track.name                = "meter";
    track.kind                = ergo::ui_kit::NodeKind::Frame;
    track.size                = {kBarWidth, kBarHeight};
    track.style.fill          = t.panel_alt;
    track.style.corner_radius = 2;
    track.layout.mode         = ergo::ui_kit::LayoutMode::Horizontal;
    const float fraction = m.present ? std::clamp((m.peak_db - kBarFloorDb) / -kBarFloorDb, 0.0f, 1.0f) : 0.0f;
    if (fraction > 0.0f) {
        ergo::ui_kit::Node fill;
        fill.name                = "level";
        fill.kind                = ergo::ui_kit::NodeKind::Shape;
        fill.size                = {kBarWidth * fraction, kBarHeight};
        fill.style.fill          = m.clipped ? t.error : m.stale ? t.muted : m.peak_db > -6.0f ? t.warn : t.ok;
        fill.style.corner_radius = 2;
        track.children.push_back(std::move(fill));
    }
    row.node().children.push_back(std::move(track));
}

std::string meter_text(const sn::MeterShown& m) {
    if (!m.present) return "メーターなし";
    std::string s = "peak " + db_text(m.peak_db) + "  rms " + db_text(m.rms_db) + "  hold " + db_text(m.hold_db);
    if (m.clipped) s += "  クリップ";
    if (m.stale) s += "  (更新なし)";
    return s;
}

} // namespace

void SonusView::show(Page p) {
    page_ = p;
    ++local_revision_;
}

void SonusView::build(ui::ViewBuilder& b) {
    const sn::SonusShown s   = plugin_.shown();
    const sn::SonusConfig& c = plugin_.config();

    b.title("サウンド (Sonus)");
    {
        ui::ViewBuilder row = b.row();
        row.button("接続", [this] {
            const std::string why = plugin_.connect();
            notice_ = why.empty() ? "" : "接続できません: " + why;
            ++local_revision_;
        }, s.state == sn::LinkState::Offline);
        row.button("切断", [this] {
            plugin_.disconnect();
            notice_.clear();
            ++local_revision_;
        });
    }
    ui::ViewBuilder status = b.panel(4);
    status.status(s.state == sn::LinkState::Online, state_label(s.state));
    status.field("接続先", c.host + ":" + std::to_string(c.port) + "  (ERGO_SONUS_HOST / ERGO_SONUS_PORT)");
    if (s.state == sn::LinkState::Online) {
        status.field("ゲーム", std::to_string(s.sample_rate) + " Hz / " + std::to_string(s.block_size) +
                                   " frames, RTT " + std::to_string(s.round_trip_us) + " us");
        if (s.mismatch) {
            status.label("ゲームのバンクはプロジェクトと違います。Studio の値をゲームへ送っています", &status.theme().warn);
        }
    }
    if (s.project_loaded || !s.project_errors.empty()) {
        status.field("プロジェクト", s.project_name.empty() ? c.project_dir : s.project_name);
    } else {
        status.muted("プロジェクトなし (ERGO_SONUS_PROJECT)。ミキサーにはバスが出ません");
    }
    for (const auto& e : s.project_errors) status.label(e, &status.theme().error);
    if (!notice_.empty()) status.label(notice_, &status.theme().error);
    if (!s.messages.empty()) status.muted(s.messages.back());

    {
        ui::ViewBuilder tabs = b.row();
        tabs.button("ミキサー", [this] { show(Page::Mixer); }, page_ == Page::Mixer);
        tabs.button("プロファイラ", [this] { show(Page::Profiler); }, page_ == Page::Profiler);
        tabs.button("上書き (" + std::to_string(s.overrides.size()) + ")", [this] { show(Page::Overrides); },
                    page_ == Page::Overrides);
        tabs.button("録画", [this] { show(Page::Recording); }, page_ == Page::Recording);
    }
    switch (page_) {
    case Page::Mixer: build_mixer(b, s); break;
    case Page::Profiler: build_profiler(b, s); break;
    case Page::Overrides: build_overrides(b, s); break;
    case Page::Recording: build_recording(b, s); break;
    }
}

void SonusView::build_mixer(ui::ViewBuilder& b, const sn::SonusShown& s) {
    ui::ViewBuilder panel = b.panel(6);
    {
        ui::ViewBuilder row = panel.row();
        row.button("元に戻す (" + std::to_string(s.undo_count) + ")", [this] { plugin_.undo(); });
        row.button("やり直す (" + std::to_string(s.redo_count) + ")", [this] { plugin_.redo(); });
        if (s.cells_pending + s.cells_failed + s.cells_no_answer > 0) {
            row.label("送信中 " + std::to_string(s.cells_pending) + " / 拒否 " + std::to_string(s.cells_failed) +
                          " / 応答なし " + std::to_string(s.cells_no_answer),
                      s.cells_failed + s.cells_no_answer > 0 ? &row.theme().error : &row.theme().muted);
        }
    }
    if (s.buses.empty()) panel.muted("バスがありません (プロジェクトを ERGO_SONUS_PROJECT で指定してください)");
    for (const sn::BusShown& bus : s.buses) {
        ui::ViewBuilder row = panel.row(6);
        row.label(std::string(static_cast<std::size_t>(bus.depth) * 2, ' ') + bus.name);
        meter_bar(row, bus.meter);
        const ergo::ui_kit::Color* tone = bus.muted || bus.solo_muted ? &row.theme().muted : nullptr;
        row.label(db_text(bus.fader_db), tone);
        const std::string key = bus.key;
        row.button("-6", [this, key] { plugin_.nudge_bus_fader(key, -6.0f); });
        row.button("-1", [this, key] { plugin_.nudge_bus_fader(key, -1.0f); });
        row.button("+1", [this, key] { plugin_.nudge_bus_fader(key, 1.0f); });
        row.button("+6", [this, key] { plugin_.nudge_bus_fader(key, 6.0f); });
        row.button("0 dB", [this, key] { plugin_.set_bus_fader(key, 0.0f); });
        row.button("切", [this, key] { plugin_.set_bus_fader(key, sn::kFloorDb); });
        const bool muted = bus.muted, soloed = bus.soloed;
        row.button("M", [this, key, muted] { plugin_.set_bus_mute(key, !muted); }, muted);
        row.button("S", [this, key, soloed] { plugin_.set_bus_solo(key, !soloed); }, soloed);
        if (bus.solo_muted) row.muted("ソロで消音");
    }
}

void SonusView::build_profiler(ui::ViewBuilder& b, const sn::SonusShown& s) {
    const sn::TimingShown& t = s.timing;
    ui::ViewBuilder timing = b.panel(4);
    timing.label("処理時間");
    if (t.samples == 0) {
        timing.muted("まだ届いていません");
    } else {
        timing.field("ブロック", std::to_string(t.last_block) + " (" + std::to_string(t.samples) + " 件)");
        timing.field("描画", number(t.last_render_us) + " us (平均 " + number(t.avg_render_us) + ", 最大 " +
                                 number(t.max_render_us) + ")");
        timing.field("ロジック", number(t.last_logic_us) + " us (平均 " + number(t.avg_logic_us) + ")");
        timing.field("負荷", number(t.load_percent) + " % (予算 " + number(t.budget_us) + " us、超過 " +
                                 std::to_string(t.over_budget) + " 回)",
                     t.load_percent > 100.0f ? &timing.theme().error : nullptr);
        timing.field("LOD / 遅れ / アンダーラン", std::to_string(t.lod) + " / " + std::to_string(t.late_chunks) + " / " +
                                                   std::to_string(t.underruns));
        timing.field("ストリームの停滞 / LOD の変化", std::to_string(s.stalls) + " / " + std::to_string(s.lod_changes));
    }

    ui::ViewBuilder meters = b.panel(4);
    meters.label("メーター");
    if (s.meters.empty()) meters.muted("まだ届いていません");
    for (const sn::MeterRow& m : s.meters) {
        ui::ViewBuilder row = meters.row(6);
        row.label(m.name);
        meter_bar(row, m.meter);
        row.muted(meter_text(m.meter));
    }

    ui::ViewBuilder voices = b.panel(4);
    voices.label("ボイス (実 " + std::to_string(s.real_voices) + " / 全 " + std::to_string(s.voices.size()) + ")");
    for (std::size_t i = 0; i < s.voices.size() && i < kMaxVoices; ++i) {
        const sn::VoiceShown& v = s.voices[i];
        char id[16];
        std::snprintf(id, sizeof id, "%08x", static_cast<unsigned>(v.event_id));
        voices.label(std::string("event ") + id + "  " + v.bus + "  " + v.state + "  " + v.source + "  gain " +
                         number(v.gain, "%.3f") + (v.is_virtual ? "  (仮想)" : ""),
                     v.is_virtual ? &voices.theme().muted : nullptr);
    }
    if (s.voices.size() > kMaxVoices) voices.muted("ほか " + std::to_string(s.voices.size() - kMaxVoices) + " 件");

    ui::ViewBuilder memory = b.panel(4);
    memory.label("メモリ");
    if (!s.has_memory) memory.muted("まだ届いていません");
    for (const sn::MemoryShown& m : s.memory) {
        memory.field(m.tag, bytes_text(m.used) + (m.budget ? " / " + bytes_text(m.budget) : ""),
                     m.budget && m.used > m.budget ? &memory.theme().error : nullptr);
    }

    ui::ViewBuilder log = b.panel(4);
    log.label("ログ");
    if (s.log.empty()) log.muted("まだありません");
    const std::size_t first = s.log.size() > kMaxLogLines ? s.log.size() - kMaxLogLines : 0;
    for (std::size_t i = first; i < s.log.size(); ++i) {
        const sn::LogShown& l = s.log[i];
        char id[16];
        std::snprintf(id, sizeof id, "%08x", static_cast<unsigned>(l.id));
        log.muted(std::to_string(l.clock_sample) + "  " + l.kind + "  " + id + "  " + std::to_string(l.detail));
    }
}

void SonusView::build_overrides(ui::ViewBuilder& b, const sn::SonusShown& s) {
    ui::ViewBuilder panel = b.panel(6);
    panel.muted("プロジェクト (バンク) の値と違う、Studio がゲームに送っている値");
    panel.button("全部戻す", [this] { plugin_.revert_all(); }, !s.overrides.empty());
    if (s.overrides.empty()) panel.muted("上書きはありません");
    for (const sn::OverrideShown& o : s.overrides) {
        ui::ViewBuilder row = panel.row(8);
        row.label(o.label);
        row.muted(number(o.bank_value, "%.2f") + " -> " + (o.muted ? std::string("消音") : number(o.value, "%.2f")));
        const std::string key = o.key;
        row.button("戻す", [this, key] { plugin_.revert(key); });
    }
}

void SonusView::build_recording(ui::ViewBuilder& b, const sn::SonusShown& s) {
    const sn::RecordingShown& r = s.recording;
    ui::ViewBuilder panel = b.panel(6);
    panel.field("状態", recording_label(r.state), r.state == "failed" ? &panel.theme().error : nullptr);
    if (r.bytes > 0) panel.field("記録", bytes_text(r.bytes));
    if (!r.error_name.empty()) panel.label("ゲームが拒否しました: " + r.error_name, &panel.theme().error);
    {
        ui::ViewBuilder row = panel.row();
        row.button("開始", [this] {
            notice_ = plugin_.start_recording() ? "" : "録画を始められません (ゲームにつながっていないか、録画中です)";
            ++local_revision_;
        }, s.state == sn::LinkState::Online && r.state != "recording");
        row.button("停止", [this] {
            notice_ = plugin_.stop_recording() ? "" : "止める録画がありません";
            ++local_revision_;
        }, r.state == "recording");
        row.button("保存", [this] {
            std::string why;
            const std::string path = plugin_.save_recording(why);
            notice_ = path.empty() ? "保存できません: " + why : "";
            ++local_revision_;
        }, r.can_save);
    }
    const std::string& dir = plugin_.config().recording_dir;
    if (dir.empty()) panel.muted("保存は無効です (ERGO_SONUS_RECORDING_DIR に保存先を指定してください)");
    else panel.field("保存先", dir);
    const std::string saved = plugin_.last_saved();
    if (!saved.empty()) panel.field("最後に保存したファイル", saved, &panel.theme().ok);
}

} // namespace ergo::studio::views
