#include "sonus_shown_json.h"

namespace ergo::studio::plugins::sonus {
namespace {

JsonValue meter_json(const MeterShown& m) {
    if (!m.present) return JsonValue::make_null();
    return jobj({{"peak", jnum(m.peak)},
                 {"rms", jnum(m.rms)},
                 {"peak_db", jnum(m.peak_db)},
                 {"rms_db", jnum(m.rms_db)},
                 {"hold_db", jnum(m.hold_db)},
                 {"clipped", jbool(m.clipped)},
                 {"stale", jbool(m.stale)}});
}

JsonValue strings_json(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::make_array();
    for (const auto& s : v) a.push(jstr(s));
    return a;
}

JsonValue mixer_json(const SonusShown& s) {
    JsonValue buses = JsonValue::make_array();
    for (const BusShown& b : s.buses) {
        buses.push(jobj({{"key", jstr(b.key)},
                         {"name", jstr(b.name)},
                         {"depth", jnum(b.depth)},
                         {"master", jbool(b.master)},
                         {"fader_db", jnum(b.fader_db)},
                         {"bank_db", jnum(b.bank_db)},
                         {"effective_db", jnum(b.effective_db)},
                         {"muted", jbool(b.muted)},
                         {"soloed", jbool(b.soloed)},
                         {"solo_muted", jbool(b.solo_muted)},
                         {"meter", meter_json(b.meter)}}));
    }
    return jobj({{"buses", buses},
                 {"undo", jnum(static_cast<double>(s.undo_count))},
                 {"redo", jnum(static_cast<double>(s.redo_count))},
                 {"pending", jnum(s.cells_pending)},
                 {"failed", jnum(s.cells_failed)},
                 {"no_answer", jnum(s.cells_no_answer)}});
}

JsonValue overrides_json(const SonusShown& s) {
    JsonValue a = JsonValue::make_array();
    for (const OverrideShown& o : s.overrides) {
        a.push(jobj({{"key", jstr(o.key)},
                     {"label", jstr(o.label)},
                     {"bank_value", jnum(o.bank_value)},
                     {"value", jnum(o.value)},
                     {"muted", jbool(o.muted)}}));
    }
    return a;
}

JsonValue profiler_json(const SonusShown& s) {
    const TimingShown& t = s.timing;
    JsonValue timing = jobj({{"samples", jnum(t.samples)},
                             {"last_block", jnum(static_cast<double>(t.last_block))},
                             {"last_render_us", jnum(t.last_render_us)},
                             {"avg_render_us", jnum(t.avg_render_us)},
                             {"max_render_us", jnum(t.max_render_us)},
                             {"last_logic_us", jnum(t.last_logic_us)},
                             {"avg_logic_us", jnum(t.avg_logic_us)},
                             {"budget_us", jnum(t.budget_us)},
                             {"load_percent", jnum(t.load_percent)},
                             {"lod", jnum(t.lod)},
                             {"late_chunks", jnum(t.late_chunks)},
                             {"underruns", jnum(t.underruns)},
                             {"over_budget", jnum(t.over_budget)}});
    JsonValue meters = JsonValue::make_array();
    for (const MeterRow& m : s.meters) {
        meters.push(jobj({{"bus_id", jnum(m.bus_id)}, {"name", jstr(m.name)}, {"meter", meter_json(m.meter)}}));
    }
    JsonValue voices = JsonValue::make_array();
    for (const VoiceShown& v : s.voices) {
        voices.push(jobj({{"event_id", jnum(v.event_id)},
                          {"bus", jstr(v.bus)},
                          {"state", jstr(v.state)},
                          {"source", jstr(v.source)},
                          {"audibility", jnum(v.audibility)},
                          {"gain", jnum(v.gain)},
                          {"virtual", jbool(v.is_virtual)}}));
    }
    JsonValue memory = JsonValue::make_null();
    if (s.has_memory) {
        memory = JsonValue::make_array();
        for (const MemoryShown& m : s.memory) {
            memory.push(jobj({{"tag", jstr(m.tag)},
                              {"used", jnum(static_cast<double>(m.used))},
                              {"budget", jnum(static_cast<double>(m.budget))}}));
        }
    }
    JsonValue log = JsonValue::make_array();
    for (const LogShown& l : s.log) {
        log.push(jobj({{"clock_sample", jnum(static_cast<double>(l.clock_sample))},
                       {"kind", jstr(l.kind)},
                       {"id", jnum(l.id)},
                       {"detail", jnum(l.detail)}}));
    }
    return jobj({{"timing", timing},
                 {"meters", meters},
                 {"real_voices", jnum(s.real_voices)},
                 {"voices", voices},
                 {"memory", memory},
                 {"log", log},
                 {"stalls", jnum(s.stalls)},
                 {"lod_changes", jnum(s.lod_changes)}});
}

} // namespace

JsonValue sonus_shown_json(const SonusShown& s) {
    const RecordingShown& r = s.recording;
    return jobj({{"state", jstr(link_state_name(s.state))},
                 {"target", jstr(s.target)},
                 {"mismatch", jbool(s.mismatch)},
                 {"sessions", jnum(s.sessions)},
                 {"sample_rate", jnum(s.sample_rate)},
                 {"block_size", jnum(s.block_size)},
                 {"round_trip_us", jnum(s.round_trip_us)},
                 {"project", jobj({{"loaded", jbool(s.project_loaded)},
                                   {"name", jstr(s.project_name)},
                                   {"errors", strings_json(s.project_errors)}})},
                 {"mixer", mixer_json(s)},
                 {"overrides", overrides_json(s)},
                 {"profiler", profiler_json(s)},
                 {"recording", jobj({{"state", jstr(r.state)},
                                     {"bytes", jnum(static_cast<double>(r.bytes))},
                                     {"error", jstr(r.error_name)},
                                     {"can_save", jbool(r.can_save)}})},
                 {"messages", strings_json(s.messages)}});
}

} // namespace ergo::studio::plugins::sonus
