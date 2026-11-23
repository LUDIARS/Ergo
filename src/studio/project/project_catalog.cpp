#include "ergo/studio/project/project_catalog.h"

#include <fstream>
#include <sstream>

namespace ergo::studio::project {

namespace fs = std::filesystem;

ProjectCatalog::ProjectCatalog(fs::path file) : file_(std::move(file)) {}

bool ProjectCatalog::load(std::string& err) {
    projects_.clear();
    broken_ = false;
    if (file_.empty()) return true;
    std::error_code ec;
    if (!fs::exists(file_, ec)) return true;   // first run
    std::ifstream in(file_, std::ios::binary);
    if (!in) {
        broken_ = true;
        err     = file_.u8string() + ": cannot be read";
        return false;
    }
    std::ostringstream text;
    text << in.rdbuf();
    std::string why;
    auto list = parse_recent_projects(text.str(), why);
    if (!list) {
        broken_ = true;
        err     = file_.u8string() + ": " + why + " (treated as empty; the file is left as it is)";
        return false;
    }
    projects_ = std::move(*list);
    return true;
}

bool ProjectCatalog::save(std::string& err) const {
    if (file_.empty()) return true;
    if (broken_) {
        err = file_.u8string() + " could not be read; it is not overwritten";
        return false;
    }
    std::error_code ec;
    fs::create_directories(file_.parent_path(), ec);
    fs::path temp = file_;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << serialize_recent_projects(projects_);
        if (!out.flush()) {
            err = temp.u8string() + ": cannot be written";
            return false;
        }
    }
    fs::rename(temp, file_, ec);   // replaces the original (MoveFileEx REPLACE_EXISTING on Windows)
    if (ec) {
        fs::remove(temp, ec);
        err = file_.u8string() + ": cannot be replaced";
        return false;
    }
    return true;
}

} // namespace ergo::studio::project
