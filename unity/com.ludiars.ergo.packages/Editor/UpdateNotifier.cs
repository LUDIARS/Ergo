// Watches <project>/.ergo/watch-report.json (written by the scheduled `ergo_pkg watch`). When it changes, new updates
// are notified once (ShowNotification on the Ergo Packages window, or the Scene view) and the window's unread count
// is refreshed. Nothing here contacts the network.
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

namespace Ludiars.Ergo.Packages.Editor
{
    [InitializeOnLoad]
    internal static class UpdateNotifier
    {
        private const double PollSeconds = 30.0;

        private static double _nextPoll;
        private static DateTime _lastWrite = DateTime.MinValue;

        public static event Action ReportChanged;

        static UpdateNotifier()
        {
            EditorApplication.update += Poll;
        }

        // Re-reads the report now (the window's Refresh button).
        public static void CheckNow()
        {
            _lastWrite = DateTime.MinValue;
            _nextPoll = 0;
            Poll();
        }

        private static void Poll()
        {
            if (EditorApplication.timeSinceStartup < _nextPoll) return;
            _nextPoll = EditorApplication.timeSinceStartup + PollSeconds;

            DateTime write = ErgoProjectFiles.ReportWriteTimeUtc();
            if (write == _lastWrite) return;
            _lastWrite = write;

            string error;
            WatchReport report = ErgoProjectFiles.LoadReport(out error);
            if (report != null) Notify(report);
            if (ReportChanged != null) ReportChanged();
        }

        private static void Notify(WatchReport report)
        {
            List<WatchUpdate> open = report.OpenUpdates();
            EventInbox inbox = InboxStore.Load();
            inbox.Prune(open);
            List<WatchUpdate> fresh = inbox.TakeNewForNotification(open);
            InboxStore.Save(inbox);
            if (fresh.Count == 0) return;

            string text = fresh.Count == 1
                ? "Ergo Packages: " + fresh[0].Dependency + " has a new " + fresh[0].Kind + " update"
                : "Ergo Packages: " + fresh.Count + " new updates";
            EditorWindow target = ErgoPackagesWindow.OpenInstance();
            if (target == null) target = SceneView.lastActiveSceneView;
            if (target != null) target.ShowNotification(new GUIContent(text));
            else Debug.Log("[Ergo Packages] " + text);
        }
    }
}
