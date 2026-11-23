#include "ergo/studio/project/open_project.h"

namespace ergo::studio::project {

std::optional<ProjectRef> OpenProject::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

void OpenProject::open(ProjectRef project) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::move(project);
    ++revision_;
}

bool OpenProject::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_) return false;
    current_.reset();
    ++revision_;
    return true;
}

} // namespace ergo::studio::project
