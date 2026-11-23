#include "assets_view.h"

#include "json_render.h"

#include <cstdio>
#include <ctime>

namespace ergo::studio::views {

namespace {

constexpr std::size_t kMaxEntries      = 300;   // lines drawn at most (a huge folder must not stall the UI)
constexpr std::size_t kMaxPreviewLines = 400;

std::string modified_text(double seconds) {
    if (seconds <= 0) return {};
    const std::time_t t = static_cast<std::time_t>(seconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

} // namespace

std::string human_size(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int u = 0;
    while (bytes >= 1024 && u < 3) {
        bytes /= 1024;
        ++u;
    }
    char buf[32];
    if (u == 0) std::snprintf(buf, sizeof(buf), "%.0f %s", bytes, units[u]);
    else std::snprintf(buf, sizeof(buf), "%.1f %s", bytes, units[u]);
    return buf;
}

std::string parent_dir(const std::string& rel) {
    const auto slash = rel.find_last_of('/');
    return slash == std::string::npos ? std::string() : rel.substr(0, slash);
}

void AssetsView::on_first_show() {
    get("roots", "/api/roots");
    if (root_ >= 0) open_dir(dir_);
}

void AssetsView::open_root(int index) {
    root_ = index;
    file_.clear();
    open_dir("");
}

void AssetsView::open_dir(const std::string& dir) {
    dir_ = dir;
    file_.clear();
    get("list", "/api/list?root=" + std::to_string(root_) + "&dir=" + url_encode(dir));
    touch();
}

void AssetsView::open_file(const std::string& path, bool text) {
    file_      = path;
    file_text_ = text;
    if (text) get("file", "/api/file?root=" + std::to_string(root_) + "&path=" + url_encode(path));
    touch();
}

void AssetsView::render(ui::ViewBuilder& b) {
    b.title(plugin_.info().title);
    b.muted("ワークスペースのプロフィールにある assets のフォルダ (読み取りのみ)");
    {
        ui::ViewBuilder r = b.row(6);
        r.button("更新", [this] { on_first_show(); });
    }
    if (!slot_state(b, "roots")) return;
    const JsonValue& roots = slot("roots").result.json;
    if (!roots.is_array() || roots.a->empty()) {
        b.paragraph("アセットのフォルダがありません。プロフィール (<studio home>/profiles) の assets を確認してください。");
        return;
    }
    {
        ui::ViewBuilder list = b.panel(2);
        for (const auto& r : *roots.a) {
            const int index = r.find("index") ? static_cast<int>(r.find("index")->as_number(-1)) : -1;
            const bool exists = r.find("exists") && r.find("exists")->as_bool();
            list.list_item(text_of(r, "name") + "  (" + text_of(r, "profile") + ")", index == root_,
                           [this, index] { open_root(index); }, exists ? text_of(r, "path") : "見つかりません");
        }
    }
    if (root_ < 0) return;
    render_listing(b);
    render_preview(b);
}

void AssetsView::render_listing(ui::ViewBuilder& b) {
    b.separator();
    {
        ui::ViewBuilder r = b.row(8);
        r.label("/" + dir_);
        if (!dir_.empty()) r.button("上へ", [this] { open_dir(parent_dir(dir_)); });
    }
    if (!slot_state(b, "list")) return;
    const JsonValue& listing = slot("list").result.json;
    const JsonValue* entries = listing.find("entries");
    if (!entries || !entries->is_array() || entries->a->empty()) {
        b.muted("空のフォルダです");
        return;
    }
    ui::ViewBuilder list = b.panel(2);
    std::size_t n = 0;
    for (const auto& e : *entries->a) {
        if (++n > kMaxEntries) {
            list.muted("… (" + std::to_string(entries->a->size() - kMaxEntries) + " 件は省略)");
            break;
        }
        const std::string path = text_of(e, "path");
        const bool dir = text_of(e, "type") == "dir";
        if (dir) {
            list.list_item("\xE2\x96\xB8 " + text_of(e, "name"), false, [this, path] { open_dir(path); }, "フォルダ");   // ▸
            continue;
        }
        const double size     = e.find("size") ? e.find("size")->as_number(0) : 0;
        const double modified = e.find("modified") ? e.find("modified")->as_number(0) : 0;
        const std::string kind = text_of(e, "kind");
        const bool preview = e.find("preview") && e.find("preview")->as_bool();
        const bool image   = kind == "texture";   // previewable textures are png / jpeg / gif / webp / bmp
        const std::string detail = (kind.empty() ? std::string() : kind + "  ") + human_size(size) + "  " + modified_text(modified);
        ui::ClickFn open;
        if (preview) open = [this, path, image] { open_file(path, !image); };
        list.list_item("  " + text_of(e, "name"), path == file_, std::move(open), detail);
    }
    if (listing.find("truncated") && listing.find("truncated")->as_bool()) list.muted("一覧は途中までです (件数の上限)");
}

void AssetsView::render_preview(ui::ViewBuilder& b) {
    if (file_.empty()) return;
    b.separator();
    b.label(file_);
    if (!file_text_) {
        b.muted("画像はこのタブではまだ表示できません (一覧の大きさと更新時刻を参照)");
        return;
    }
    if (!slot_state(b, "file")) return;
    ui::ViewBuilder text = b.panel(1);
    const std::string& body = slot("file").result.body;
    std::size_t start = 0, lines = 0;
    while (start <= body.size() && lines < kMaxPreviewLines) {
        const auto end = body.find('\n', start);
        std::string line = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        text.clipped_label(line, 1100, &b.theme().muted);
        ++lines;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (lines >= kMaxPreviewLines) text.muted("… (以降は省略)");
}

} // namespace ergo::studio::views
