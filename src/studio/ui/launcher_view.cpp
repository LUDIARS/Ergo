#include "ergo/studio/ui/launcher_view.h"

#include <utility>

namespace ergo::studio::ui {

namespace {

/// Runs `op` (which may set `err`), records the failure for the next build, then `changed`.
struct Operation {
    LauncherInput*                    input;
    std::function<void()>             changed;
    std::function<bool(std::string&)> op;

    void operator()() const {
        std::string err;
        input->error = op(err) ? std::string() : err;
        if (changed) changed();
    }
};

ClickFn operation(LauncherInput& input, const std::function<void()>& changed, std::function<bool(std::string&)> op) {
    return ClickFn(Operation{&input, changed, std::move(op)});
}

/// The window's line about the remote dev server: stopped, or what it serves and where.
void build_server_status(ViewBuilder& b, const project::DevServerStatus& s) {
    if (!s.running) {
        b.muted("リモート開発サーバは停止中です。プロジェクトの「リモート開発サーバ起動」で始めます。");
        return;
    }
    ViewBuilder p = b.panel(4);
    p.label("リモート開発サーバ稼働中", &p.theme().ok);
    p.muted("手元: " + s.local_url);
    if (s.remote_target.empty()) {
        p.muted("遠隔: 無効 (ERGO_STUDIO_REMOTE_PORT で有効にします)");
        return;
    }
    p.muted("遠隔: " + s.remote_target + " (cloudflared / tailscale serve の転送先)");
    for (const std::string& url : s.remote_urls) p.muted("    " + url);
}

/// The window's buttons of a row: start the remote dev server, or stop it.
void build_server_actions(ViewBuilder& actions, project::IDevServerControl& devserver, LauncherInput& input,
                          const std::function<void()>& changed, const project::LauncherRow& row,
                          const project::DevServerStatus& status) {
    const std::string source = row.source;
    if (status.running && status.source == row.source) {
        actions.name_next("devserver-stop");
        actions.button("停止", operation(input, changed, [&devserver](std::string&) {
                           devserver.stop();
                           return true;
                       }));
        return;
    }
    actions.name_next("devserver");
    if (row.missing) {
        actions.button("リモート開発サーバ起動", {});   // nothing to serve: shown, not usable
        return;
    }
    actions.button("リモート開発サーバ起動", operation(input, changed, [&devserver, source](std::string& err) {
                       return devserver.start(source, err);   // another project being served is stopped first
                   }), true);
}

void build_row(ViewBuilder& list, project::Launcher& launcher, LauncherInput& input, const LauncherScreen& screen,
               const std::function<void()>& changed, const project::LauncherRow& row,
               const project::DevServerStatus& status) {
    ViewBuilder p = list.panel(4);
    {
        ViewBuilder head = p.row(8);
        head.label(row.name.empty() ? row.source : row.name);
        if (screen.devserver != nullptr && status.running && status.source == row.source) {
            head.label("(配信中)", &p.theme().ok);
        } else if (screen.devserver == nullptr && row.open) {
            head.label("(開いています)", &p.theme().ok);
        }
        if (row.missing) head.label("プロフィールが見つかりません", &p.theme().warn);
    }
    p.muted((row.root.empty() ? row.source : row.root) + "    様式: " + (row.style.empty() ? "不明" : row.style) +
            "    用途: " + (row.domain.empty() ? "未設定" : row.domain));
    p.muted(row.last_opened.empty() ? "未起動" : "最後に起動: " + row.last_opened);
    ViewBuilder actions = p.row(8);
    const std::string source = row.source;
    if (screen.devserver != nullptr) {
        build_server_actions(actions, *screen.devserver, input, changed, row, status);
    } else if (!row.missing) {
        actions.name_next("open");
        actions.button("開く", operation(input, changed, [&launcher, source](std::string& err) {
                           return launcher.open(source, err);
                       }), true);
    }
    if (screen.local && row.recent) {
        actions.name_next("remove");
        actions.button("一覧から外す", operation(input, changed, [&launcher, source](std::string& err) {
                           return launcher.remove(source, err);
                       }));
    }
}

} // namespace

void build_launcher(ViewBuilder& b, project::Launcher& launcher, LauncherInput& input, const LauncherScreen& screen,
                    const std::function<void()>& changed) {
    b.title("Ergo Studio");
    b.muted("プロジェクトを選んでください (最近起動した順、続けて見つかったプロフィール)。");
    const project::DevServerStatus status = screen.devserver ? screen.devserver->status() : project::DevServerStatus{};
    if (screen.devserver != nullptr) build_server_status(b, status);
    const std::string notice = launcher.notice();
    if (!notice.empty()) b.label(notice, &b.theme().warn);
    if (!input.error.empty()) b.label(input.error, &b.theme().error);
    const auto rows = launcher.rows();
    if (rows.empty()) b.muted("プロジェクトがありません。プロフィール (*.json) のパスを入れて追加してください。");
    for (const project::LauncherRow& row : rows) build_row(b, launcher, input, screen, changed, row, status);
    if (!screen.local) return;
    b.separator();
    ViewBuilder add = b.row(8);
    add.name_next("add-path");
    const ClickFn add_now = operation(input, changed, [&launcher, &input](std::string& err) {
        if (!launcher.add(input.add_path, err)) return false;
        input.add_path.clear();
        return true;
    });
    add.text_field(input.add_path, "プロフィール (.json) のパス", 420, add_now);
    add.name_next("add");
    add.button("追加", add_now);
}

void build_project_bar(ViewBuilder& b, const project::ProjectRef& project, ClickFn back) {
    ViewBuilder bar = b.row(10);
    bar.label(project.name.empty() ? project.source : project.name, nullptr, b.theme().title_size);
    bar.muted(project.root);
    bar.name_next("back");
    bar.button("ランチャーへ戻る", std::move(back));
}

} // namespace ergo::studio::ui
