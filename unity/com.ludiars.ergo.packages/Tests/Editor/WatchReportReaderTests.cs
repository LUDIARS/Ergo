using System;
using System.Collections.Generic;
using NUnit.Framework;

namespace Ludiars.Ergo.Packages.Tests
{
    public class WatchReportReaderTests
    {
        private static readonly DateTime ReportTime = new DateTime(2026, 10, 15, 0, 0, 0, DateTimeKind.Utc);

        private static WatchReport Report()
        {
            return WatchReportReader.Read(Fixtures.Read("watch-report.json"));
        }

        [Test]
        public void ReadsTheReport()
        {
            WatchReport r = Report();
            Assert.AreEqual(1, r.Schema);
            Assert.AreEqual("Example", r.Host);
            Assert.AreEqual("2026-10-15T00:00:00Z", r.GeneratedAt);
            Assert.AreEqual(2, r.Dependencies.Count);
            Assert.AreEqual(1, r.Events.Count);
            Assert.AreEqual(2, r.Pending.Count);
        }

        [Test]
        public void ReadsDependencyFields()
        {
            WatchDependencyStatus d = Report().FindDependency("com.ludiars.ergo.packages");
            Assert.IsNotNull(d);
            Assert.AreEqual("Eg", d.Code);
            Assert.AreEqual("upm", d.Owner);
            Assert.IsTrue(d.Observed);
            Assert.AreEqual("ls-remote", d.AsOf);
            Assert.AreEqual("b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", d.PinRev);
            Assert.AreEqual(18, d.TrackBehind);
            Assert.AreEqual(true, d.OnTrack);
            Assert.IsNull(d.SiblingBehind);
            Assert.AreEqual(2, d.Updates.Count);
            Assert.AreEqual("tag", d.Updates[0].Kind);
            Assert.AreEqual("v0.10.0", d.Updates[0].ToTag);
            Assert.AreEqual(12, d.Updates[0].Behind);
        }

        [Test]
        public void NullStaysNull()
        {
            WatchDependencyStatus d = Report().FindDependency("com.ludiars.pictor");
            Assert.IsNull(d.TrackBehind);
            Assert.IsNull(d.OnTrack);
            Assert.IsNull(Report().Events[0].SinceNotified);
        }

        [Test]
        public void ReadsEventFields()
        {
            WatchUpdate e = Report().Events[0];
            Assert.AreEqual("Example/com.ludiars.ergo.packages/tag/v0.10.0", e.Id);
            Assert.AreEqual("Example", e.Host);
            Assert.AreEqual("com.ludiars.ergo.packages", e.Dependency);
            Assert.AreEqual("upm", e.Owner);
            Assert.AreEqual("9c2d4e7ccccccccccccccccccccccccccccccccc", e.ToRev);
            Assert.IsTrue(e.StrictOk);
            Assert.AreEqual("2026-10-15T00:00:00Z", e.At);
        }

        [Test]
        public void OpenUpdatesMergePendingAndEventsById()
        {
            List<WatchUpdate> open = Report().OpenUpdates();
            Assert.AreEqual(2, open.Count);
            Assert.AreEqual("tag", open[0].Kind);
            Assert.AreEqual("track", open[1].Kind);
        }

        [Test]
        public void NotObservedIsNeverUpToDate()
        {
            WatchReport r = Report();
            UpdateState s = UpdateStatus.Of(r, "com.ludiars.pictor", ReportTime, UpdateStatus.DefaultStaleAfter);
            Assert.AreEqual(UpdateState.NotObserved, s);
            Assert.AreNotEqual(UpdateStatus.Label(UpdateState.UpToDate), UpdateStatus.Label(s));
        }

        [Test]
        public void UpdatesAvailable()
        {
            Assert.AreEqual(UpdateState.UpdatesAvailable,
                UpdateStatus.Of(Report(), "com.ludiars.ergo.packages", ReportTime, UpdateStatus.DefaultStaleAfter));
        }

        [Test]
        public void ObservedWithoutUpdatesIsUpToDateOnlyWhileFresh()
        {
            WatchReport r = Report();
            r.FindDependency("com.ludiars.ergo.packages").Updates.Clear();
            Assert.AreEqual(UpdateState.UpToDate,
                UpdateStatus.Of(r, "com.ludiars.ergo.packages", ReportTime.AddHours(1), UpdateStatus.DefaultStaleAfter));
            Assert.AreEqual(UpdateState.ReportStale,
                UpdateStatus.Of(r, "com.ludiars.ergo.packages", ReportTime.AddDays(2), UpdateStatus.DefaultStaleAfter));
        }

        [Test]
        public void MissingReportOrDependencyIsNotUpToDate()
        {
            Assert.AreEqual(UpdateState.NoReport, UpdateStatus.Of(null, "x", ReportTime, UpdateStatus.DefaultStaleAfter));
            Assert.AreEqual(UpdateState.NotInReport, UpdateStatus.Of(Report(), "com.ludiars.tela", ReportTime, UpdateStatus.DefaultStaleAfter));
        }

        [Test]
        public void OtherSchemaIsRejected()
        {
            Assert.Throws<FormatException>(() => WatchReportReader.Read("{\"schema\":2,\"host\":\"Example\"}"));
            Assert.Throws<FormatException>(() => WatchReportReader.Read("{\"host\":\"Example\"}"));
            Assert.Throws<FormatException>(() => WatchReportReader.Read("not json"));
        }

        [Test]
        public void ParsesUtc()
        {
            Assert.AreEqual(ReportTime, WatchReportReader.ParseUtc("2026-10-15T00:00:00Z"));
            Assert.IsNull(WatchReportReader.ParseUtc("2026-10-15 00:00:00"));
        }
    }
}
