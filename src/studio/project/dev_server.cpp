#include "ergo/studio/project/dev_server.h"

#include <utility>

namespace ergo::studio::project {

DevServer::DevServer(Launcher& launcher, ServeFn serve, DevServerOptions options)
    : launcher_(launcher), serve_(std::move(serve)), options_(std::move(options)) {}

DevServer::~DevServer() {
    std::lock_guard<std::mutex> lk(mutex_);
    serving_.reset();
}

bool DevServer::start(const std::string& source, std::string& err) {
    std::lock_guard<std::mutex> lk(mutex_);
    const std::string key     = Launcher::normalize(source);
    const auto        current = launcher_.project().current();
    if (serving_ && current && current->source == key) return true;
    if (serving_) stop_locked();   // one project at a time: stop the current one, then switch
    if (!launcher_.open(source, err)) {
        ++revision_;
        return false;
    }
    serving_ = serve_ ? serve_(err) : nullptr;
    if (!serving_) {
        if (err.empty()) err = "the web server could not start";
        launcher_.close();
        ++revision_;
        return false;
    }
    ++revision_;
    return true;
}

bool DevServer::serve_without_project(std::string& err) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (serving_) return true;
    serving_ = serve_ ? serve_(err) : nullptr;
    if (!serving_ && err.empty()) err = "the web server could not start";
    ++revision_;
    return serving_ != nullptr;
}

void DevServer::stop() {
    std::lock_guard<std::mutex> lk(mutex_);
    stop_locked();
}

void DevServer::stop_locked() {
    const bool was_serving = serving_ != nullptr;
    serving_.reset();   // joins the server's threads before the project closes
    if (was_serving) launcher_.close();
    ++revision_;
}

DevServerStatus DevServer::status() const {
    std::lock_guard<std::mutex> lk(mutex_);
    DevServerStatus s;
    if (!serving_) return s;
    s.running = true;
    // The project the pages see: a browser page may also have opened or closed one.
    if (const auto current = launcher_.project().current()) s.source = current->source;
    s.local_url = "http://" + options_.bind_host + ":" + std::to_string(serving_->local_port()) + options_.page;
    if (const auto remote = serving_->remote_port()) {
        s.remote_target = options_.bind_host + ":" + std::to_string(*remote);
        for (const std::string& name : options_.published_names) s.remote_urls.push_back("https://" + name + options_.page);
    }
    return s;
}

} // namespace ergo::studio::project
