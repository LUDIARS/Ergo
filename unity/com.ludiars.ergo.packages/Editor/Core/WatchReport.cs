// The report `ergo_pkg watch` writes to <project>/.ergo/watch-report.json (spec/data/ergo-watch-report.md, schema 1).
// Plain data; WatchReportReader fills it.
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    // One update: an entry of `updates`, `events` or `pending`. Event / pending only fields stay "" or null otherwise.
    public sealed class WatchUpdate
    {
        public string Id = "";            // "<host>/<dependency>/<kind>/<subject>" (events, pending): the read-state key
        public string Host = "";
        public string Dependency = "";    // for a UPM host: the UPM package name in manifest.json
        public string Code = "";
        public string Owner = "";         // "upm" for a Unity project; "host" / "ergo" for CMake hosts
        public bool Private;
        public string Kind = "";          // tag / track / off-main (W4 adds run / review / merged)
        public string Subject = "";
        public string FromRev = "";
        public string FromTag = "";
        public string ToRev = "";
        public string ToTag = "";
        public int? Behind;               // null = distance unknown
        public bool Warning;
        public bool StrictOk;
        public string Next = "";
        public string AsOf = "";
        public string At = "";
        public int? SinceNotified;
        public string FirstSeen = "";
        public string LastNotified = "";
    }

    // One dependency as observed in this run.
    public sealed class WatchDependencyStatus
    {
        public string Name = "";
        public string Code = "";
        public string Owner = "";
        public bool Private;
        public bool Observed;             // false is NOT "no update": the source could not be read
        public string Reason = "";
        public string AsOf = "";          // "ls-remote" / "sibling-last-fetch" / ""
        public string PinRev = "";
        public string PinTag = "";
        public string TrackBranch = "";
        public string TrackHead = "";
        public int? TrackBehind;
        public bool? OnTrack;
        public int? SiblingBehind;
        public List<WatchUpdate> Updates = new List<WatchUpdate>();
    }

    public sealed class WatchReport
    {
        public const int SupportedSchema = 1;

        public int Schema;
        public string Host = "";
        public string GeneratedAt = "";   // "YYYY-MM-DDTHH:MM:SSZ"
        public List<WatchDependencyStatus> Dependencies = new List<WatchDependencyStatus>();
        public List<WatchUpdate> Events = new List<WatchUpdate>();
        public List<WatchUpdate> Pending = new List<WatchUpdate>();

        public WatchDependencyStatus FindDependency(string name)
        {
            foreach (WatchDependencyStatus d in Dependencies)
            {
                if (d.Name == name) return d;
            }
            return null;
        }

        // Known open updates: pending (which already contains this run's events), then any event not in it.
        public List<WatchUpdate> OpenUpdates()
        {
            var seen = new HashSet<string>();
            var result = new List<WatchUpdate>();
            foreach (WatchUpdate u in Pending)
            {
                if (seen.Add(u.Id)) result.Add(u);
            }
            foreach (WatchUpdate u in Events)
            {
                if (seen.Add(u.Id)) result.Add(u);
            }
            return result;
        }
    }
}
