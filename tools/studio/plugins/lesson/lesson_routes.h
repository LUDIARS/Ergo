#pragma once
/// HTTP routes of the "lesson" plugin on the Studio's own (loopback) server,
/// for scripts and setups without the window (spec/feature/tool/lesson.md,
/// LS-INV-06):
///   GET  /lesson/api/state        what the tab shows (lesson_shown_json)
///   POST /lesson/api/connect      {"link":"<connection string>"}: same as 貼り付けてつなぐ (409 while linked)
///   POST /lesson/api/disconnect   {}: same as 切る
/// Mutating calls pass the Studio's mutation guard (ST-INV-03: loopback Host,
/// same-origin Origin if any, JSON body). The connection string is never
/// logged or echoed.
#include "lesson_plugin.h"

#include "ergo/studio/net/http_message.h"

namespace ergo::studio::plugins::lesson {

net::HttpResponse handle_lesson_route(LessonPlugin& plugin, const net::HttpRequest& req, const std::string& sub,
                                      const host::LogFn& log);

} // namespace ergo::studio::plugins::lesson
