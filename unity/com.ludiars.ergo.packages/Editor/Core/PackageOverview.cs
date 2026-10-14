// One row per manifest dependency for the window's package list: the declared pin, what UPM resolved
// (packages-lock.json) and what the report says. LUDIARS git dependencies come first, then the rest.
using System;
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public sealed class PackageRow
    {
        public UpmDependencyEntry Entry;
        public string Code = "";        // from the report (the repository-to-code map lives on the native side)
        public string LockedHash = "";  // packages-lock.json "hash"
        public UpdateState State;
        public string Reason = "";      // report reason for NotObserved, the entry note otherwise
        public List<WatchUpdate> Updates = new List<WatchUpdate>();
    }

    public static class PackageOverview
    {
        public static List<PackageRow> Build(IList<UpmDependencyEntry> entries,
                                             IDictionary<string, PackagesLockEntry> locks,
                                             WatchReport report, DateTime nowUtc, TimeSpan staleAfter)
        {
            var judged = new List<PackageRow>();
            var others = new List<PackageRow>();
            foreach (UpmDependencyEntry e in entries)
            {
                var row = new PackageRow { Entry = e, Reason = e.Note };
                PackagesLockEntry lockEntry;
                if (locks != null && locks.TryGetValue(e.Name, out lockEntry)) row.LockedHash = lockEntry.Hash ?? "";

                bool watched = e.Kind == UpmSourceKind.Git && e.IsLudiars && e.PinState == UpmPinState.Sha;
                row.State = watched ? UpdateStatus.Of(report, e.Name, nowUtc, staleAfter) : UpdateState.NotInReport;
                WatchDependencyStatus d = report != null && watched ? report.FindDependency(e.Name) : null;
                if (d != null)
                {
                    row.Code = d.Code;
                    row.Updates.AddRange(d.Updates);
                    if (!d.Observed) row.Reason = d.Reason;
                }
                (e.Kind == UpmSourceKind.Git && e.IsLudiars ? judged : others).Add(row);
            }
            judged.AddRange(others);
            return judged;
        }
    }
}
