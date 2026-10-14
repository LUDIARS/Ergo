using System;
using System.Collections.Generic;
using NUnit.Framework;

namespace Ludiars.Ergo.Packages.Tests
{
    public class OverviewAndInboxTests
    {
        private static readonly DateTime ReportTime = new DateTime(2026, 10, 15, 0, 0, 0, DateTimeKind.Utc);

        [Test]
        public void LockReaderReadsHashes()
        {
            Dictionary<string, PackagesLockEntry> locks = PackagesLockReader.Read(Fixtures.Read("upm-packages-lock.json"));
            Assert.AreEqual(3, locks.Count);
            Assert.AreEqual("git", locks["com.ludiars.tela"].Source);
            Assert.AreEqual("3333333333333333333333333333333333333333", locks["com.ludiars.tela"].Hash);
            Assert.AreEqual("", locks["com.unity.modules.jsonserialize"].Hash);
        }

        [Test]
        public void OverviewPutsLudiarsFirstAndJoinsTheReport()
        {
            List<PackageRow> rows = PackageOverview.Build(
                UpmManifestReader.Read(Fixtures.Read("upm-manifest.json")),
                PackagesLockReader.Read(Fixtures.Read("upm-packages-lock.json")),
                WatchReportReader.Read(Fixtures.Read("watch-report.json")),
                ReportTime, UpdateStatus.DefaultStaleAfter);
            Assert.AreEqual(5, rows.Count);

            Assert.AreEqual("com.ludiars.ergo.packages", rows[0].Entry.Name);
            Assert.AreEqual("Eg", rows[0].Code);
            Assert.AreEqual(UpdateState.UpdatesAvailable, rows[0].State);
            Assert.AreEqual(2, rows[0].Updates.Count);
            Assert.AreEqual("b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", rows[0].LockedHash);

            Assert.AreEqual("com.ludiars.pictor", rows[1].Entry.Name);
            Assert.AreEqual(UpdateState.NotObserved, rows[1].State);
            StringAssert.Contains("ls-remote", rows[1].Reason);

            Assert.AreEqual("com.ludiars.tela", rows[2].Entry.Name);
            Assert.AreEqual(UpdateState.NotInReport, rows[2].State);  // #main is not judged
            StringAssert.Contains("#main", rows[2].Reason);

            Assert.AreEqual(UpmSourceKind.File, rows[3].Entry.Kind);
            Assert.AreEqual(UpmSourceKind.Registry, rows[4].Entry.Kind);
        }

        [Test]
        public void OverviewWithoutReportSaysNoReport()
        {
            List<PackageRow> rows = PackageOverview.Build(
                UpmManifestReader.Read(Fixtures.Read("upm-manifest.json")), null, null, ReportTime, UpdateStatus.DefaultStaleAfter);
            Assert.AreEqual(UpdateState.NoReport, rows[0].State);
        }

        [Test]
        public void InboxCountsUnreadAndNotifiesOnce()
        {
            List<WatchUpdate> open = WatchReportReader.Read(Fixtures.Read("watch-report.json")).OpenUpdates();
            var inbox = new EventInbox(null, null);
            Assert.AreEqual(2, inbox.UnreadCount(open));
            Assert.AreEqual(2, inbox.TakeNewForNotification(open).Count);
            Assert.AreEqual(0, inbox.TakeNewForNotification(open).Count);

            inbox.MarkRead(open[0].Id);
            Assert.AreEqual(1, inbox.UnreadCount(open));
            inbox.MarkAllRead(open);
            Assert.AreEqual(0, inbox.UnreadCount(open));
        }

        [Test]
        public void InboxRestoresFromStoredIdsAndPrunesResolved()
        {
            List<WatchUpdate> open = WatchReportReader.Read(Fixtures.Read("watch-report.json")).OpenUpdates();
            var inbox = new EventInbox(new[] { open[0].Id, "Example/gone/tag/v1.0.0" }, new[] { open[0].Id });
            Assert.AreEqual(1, inbox.UnreadCount(open));
            Assert.AreEqual(1, inbox.TakeNewForNotification(open).Count);
            inbox.Prune(open);
            CollectionAssert.DoesNotContain(new List<string>(inbox.ReadIds), "Example/gone/tag/v1.0.0");
        }

        [Test]
        public void JsonParserRecordsSpans()
        {
            const string text = "{ \"a\" : [1, \"x\", null, true] }";
            JsonNode root = JsonParser.Parse(text);
            JsonNode a = root.Get("a");
            Assert.AreEqual(JsonKind.Array, a.Kind);
            Assert.AreEqual("[1, \"x\", null, true]", text.Substring(a.Start, a.End - a.Start));
            Assert.AreEqual("\"x\"", text.Substring(a.Items[1].Start, a.Items[1].End - a.Items[1].Start));
            Assert.Throws<FormatException>(() => JsonParser.Parse("{\"a\":1} x"));
            Assert.Throws<FormatException>(() => JsonParser.Parse("{\"a\":}"));
        }
    }
}
