#include "lesson_view.h"

namespace ergo::studio::views {
namespace {

namespace lesson = plugins::lesson;

const char* role_label(lesson::Role r) {
    switch (r) {
    case lesson::Role::Student: return "生徒";
    case lesson::Role::Presenter: return "実演";
    case lesson::Role::None: break;
    }
    return "";
}

const char* call_label(lesson::LessonCall c) {
    switch (c) {
    case lesson::LessonCall::Pair: return "接続";
    case lesson::LessonCall::State: return "授業の状態の取得";
    case lesson::LessonCall::Progress: return "進み具合の報告";
    case lesson::LessonCall::None: break;
    }
    return "通信";
}

/// What the known server codes mean for the user; empty for the others.
const char* code_hint(const std::string& code) {
    if (code == "link_not_found") return "接続文字列が見つかりません。授業ページで取り直してください";
    if (code == "rate_limited") return "試した回数が多すぎます。少し待ってからやり直してください";
    if (code == "invalid_request") return "授業のサーバが要求を受け付けませんでした";
    return "";
}

std::string failure_text(lesson::LessonCall call, const lesson::CallFailure& f) {
    const std::string what = std::string(call_label(call)) + "に失敗しました: ";
    switch (f.kind) {
    case lesson::FailureKind::Transport:
        return what + "サーバにつながりません (" + f.detail + ")";
    case lesson::FailureKind::Timeout:
        return what + "時間内に応答がありません";
    case lesson::FailureKind::Http: {
        std::string s = what + "HTTP " + std::to_string(f.status);
        if (!f.code.empty()) s += " (" + f.code + ")";
        if (const char* hint = code_hint(f.code); *hint) s += std::string(" ") + hint;
        return s;
    }
    case lesson::FailureKind::Malformed:
        return what + "サーバの応答が取り決めと違います (" + f.detail + ")";
    case lesson::FailureKind::None: break;
    }
    return {};
}

std::string status_text(const lesson::LessonShown& s) {
    if (s.connecting) return "接続中";
    if (s.session.linked) return std::string("接続済み (") + role_label(s.session.role) + ")";
    return "未接続";
}

std::string demo_text(const lesson::DemoEntry& e) {
    const std::string name = e.feature + (e.variable.empty() ? "" : "  [" + e.variable + "]");
    const std::string want = e.on ? (*e.on ? "ON" : "OFF") : "";
    switch (e.status) {
    case lesson::DemoStatus::NotCued: return name + "  —";
    case lesson::DemoStatus::Applied: return name + "  " + want;
    case lesson::DemoStatus::WaitingForGame: return name + "  " + want + "  未反映 (ゲームがこの変数を公開するのを待っています)";
    case lesson::DemoStatus::NotBool: return name + "  " + want + "  未反映 (ゲームの変数が bool ではありません)";
    case lesson::DemoStatus::NoVariable: return name + "  " + want + "  対応する変数がありません";
    }
    return name;
}

void build_assignment(ui::ViewBuilder& b, const lesson::AssignmentRows& a) {
    ui::ViewBuilder panel = b.panel(4);
    panel.label("課題: " + a.title);
    if (!a.active) panel.muted("この課題は今は受け付けていません");
    for (const auto& step : a.steps) {
        const std::string mark = step.done ? "✓ " : "・ ";
        panel.label(mark + step.label + (step.automatic ? "  [自動]" : ""), step.done ? &panel.theme().ok : nullptr);
    }
    panel.muted("[自動] の手順は、動かしているゲームの変数が条件を満たすと自動で報告します。ほかの手順は授業ページで印を付けてください");
}

void build_demo(ui::ViewBuilder& b, const std::vector<lesson::DemoEntry>& demo) {
    ui::ViewBuilder panel = b.panel(4);
    panel.label("実演の切り替え");
    if (demo.empty()) panel.muted("この授業には切り替える機能がありません");
    for (const auto& e : demo) {
        const bool waiting = e.status != lesson::DemoStatus::Applied && e.status != lesson::DemoStatus::NotCued;
        panel.label(demo_text(e), waiting ? &panel.theme().warn : nullptr);
    }
}

} // namespace

LessonView::LessonView(lesson::LessonPlugin& plugin, std::shared_ptr<platform::IClipboard> clipboard)
    : plugin_(plugin), clipboard_(std::move(clipboard)) {}

void LessonView::paste_and_connect() {
    ++local_revision_;
    const platform::ClipboardText pasted = clipboard_->read_text();
    if (!pasted.ok) {
        clipboard_error_ = "クリップボードを読めませんでした (" + pasted.error + ")";
        return;
    }
    clipboard_error_.clear();
    plugin_.connect(pasted.text);
}

void LessonView::build(ui::ViewBuilder& b) {
    const lesson::LessonShown   shown = plugin_.shown();
    const lesson::SessionView&  s     = shown.session;

    b.title("授業");
    b.muted("授業ページで接続文字列をコピーしてから「貼り付けてつなぐ」を押してください");
    {
        ui::ViewBuilder row = b.row();
        row.button("貼り付けてつなぐ", [this] { paste_and_connect(); }, !s.linked && !shown.connecting);
        row.button("切る", [this] {
            clipboard_error_.clear();
            ++local_revision_;
            plugin_.disconnect();
        });
    }

    ui::ViewBuilder status = b.panel(4);
    status.status(s.linked, status_text(shown));
    if (!s.host.empty()) status.field("接続先", s.host);
    if (!s.class_title.empty()) status.field("授業", s.class_title);
    if (!clipboard_error_.empty()) status.label(clipboard_error_, &status.theme().error);
    if (!shown.link_error.empty()) status.label(shown.link_error, &status.theme().error);
    if (s.end == lesson::LinkEnd::Unlinked) {
        status.label("接続が切れました。授業ページで接続文字列を取り直してください", &status.theme().error);
    } else if (s.end == lesson::LinkEnd::Disconnected) {
        status.muted("切りました");
    }
    if (s.failure.failed()) status.label(failure_text(s.failed_call, s.failure), &status.theme().error);

    if (!s.linked) return;
    if (s.role == lesson::Role::Student) {
        if (s.assignment) build_assignment(b, *s.assignment);
        else b.muted("今は課題が出ていません");
    } else if (s.role == lesson::Role::Presenter) {
        build_demo(b, s.demo);
    }
}

} // namespace ergo::studio::views
