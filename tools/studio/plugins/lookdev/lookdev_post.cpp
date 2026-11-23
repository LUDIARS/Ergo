#include "lookdev_post.h"

namespace ergo::studio::plugins::lookdev {

namespace {

using ergo::common::jsonm::JsonValue;

/// Cuts `text` to at most `limit` bytes without splitting a UTF-8 sequence.
std::string utf8_prefix(const std::string& text, std::size_t limit) {
    if (text.size() <= limit) return text;
    std::size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
    return text.substr(0, end);
}

} // namespace

std::string compose_post_text(const std::string& app, const std::string& comment,
                              const std::vector<std::string>& changes) {
    std::string text = "\xE3\x83\xAB\xE3\x83\x83\xE3\x82\xAF\xE3\x83\x87\xE3\x83\xB4: " + (app.empty() ? std::string("(app)") : app);   // ルックデヴ: 
    if (!comment.empty()) text += "\n" + comment;
    if (changes.empty()) {
        text += "\n\xE5\xA4\x89\xE6\x9B\xB4\xE3\x81\xAA\xE3\x81\x97 (\xE6\x8E\xA5\xE7\xB6\x9A\xE3\x81\x97\xE3\x81\x9F\xE3\x81\xA8\xE3\x81\x8D\xE3\x81\xAE\xE8\xA8\xAD\xE5\xAE\x9A\xE3\x81\xAE\xE3\x81\xBE\xE3\x81\xBE)";   // \n変更なし (接続したときの設定のまま)
    } else {
        text += "\n\xE5\xA4\x89\xE6\x9B\xB4\xE3\x81\x97\xE3\x81\x9F\xE9\xA0\x85\xE7\x9B\xAE:";   // \n変更した項目:
        for (const auto& change : changes) text += "\n- " + change;
    }
    text += "\n\xE6\xB7\xBB\xE4\xBB\x98: \xE7\x94\xBB\xE9\x9D\xA2 (PNG) \xE3\x81\xA8\xE8\xA8\xAD\xE5\xAE\x9A (.look\xE3\x80\x81--look-file \xE3\x81\xA7\xE8\xAA\xAD\xE3\x82\x81\xE3\x82\x8B)";   // \n添付: 画面 (PNG) と設定 (.look、--look-file で読める)
    return utf8_prefix(text, kMaxPostText);
}

studio::ServiceResult LookdevPoster::post(const std::string& text, const std::vector<std::string>& attachment_paths) const {
    JsonValue body = JsonValue::make_object();
    body.set("channel", JsonValue::make_string("\xE5\xA0\xB1\xE5\x91\x8A"));   // 報告
    body.set("text", JsonValue::make_string(text));
    body.set("author_label", JsonValue::make_string("Ergo Studio"));
    JsonValue files = JsonValue::make_array();
    for (const auto& path : attachment_paths) files.push(JsonValue::make_string(path));
    body.set("attachment_paths", std::move(files));
    return caller_.call(endpoints_.url(studio::Service::Concordia), "/v1/chat", "POST", body);
}

} // namespace ergo::studio::plugins::lookdev
