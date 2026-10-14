// What the window may say about one dependency, from the report alone. A dependency is shown as "up to date" only
// when the report is fresh and the dependency was observed with no update; an unobserved or stale state is never
// shown as "no update" (spec/feature/package-manager.md §6.2).
using System;

namespace Ludiars.Ergo.Packages
{
    public enum UpdateState
    {
        NoReport,          // no .ergo/watch-report.json (or unreadable)
        NotInReport,       // the report does not list this dependency (not judged: branch pin, registry, ...)
        NotObserved,       // observed:false — the source could not be read
        ReportStale,       // observed with no update, but the report is too old to say so
        UpdatesAvailable,
        UpToDate,
    }

    public static class UpdateStatus
    {
        // Default: the watch job runs every 15 minutes; a report older than a day says nothing about "now".
        public static readonly TimeSpan DefaultStaleAfter = TimeSpan.FromHours(24);

        public static UpdateState Of(WatchReport report, string dependencyName, DateTime nowUtc, TimeSpan staleAfter)
        {
            if (report == null) return UpdateState.NoReport;
            WatchDependencyStatus d = report.FindDependency(dependencyName);
            if (d == null) return UpdateState.NotInReport;
            if (!d.Observed) return UpdateState.NotObserved;
            if (d.Updates.Count > 0) return UpdateState.UpdatesAvailable;
            return IsStale(report, nowUtc, staleAfter) ? UpdateState.ReportStale : UpdateState.UpToDate;
        }

        // A report whose generated_at cannot be read is stale.
        public static bool IsStale(WatchReport report, DateTime nowUtc, TimeSpan staleAfter)
        {
            DateTime? at = WatchReportReader.ParseUtc(report.GeneratedAt);
            return !at.HasValue || nowUtc - at.Value > staleAfter;
        }

        public static string Label(UpdateState state)
        {
            switch (state)
            {
                case UpdateState.NoReport: return "no report (run ergo_pkg watch)";
                case UpdateState.NotInReport: return "not checked";
                case UpdateState.NotObserved: return "not observed";
                case UpdateState.ReportStale: return "unknown (report is old)";
                case UpdateState.UpdatesAvailable: return "update available";
                case UpdateState.UpToDate: return "up to date";
            }
            return "unknown";
        }
    }
}
