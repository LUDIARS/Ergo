// Window > LUDIARS > Ergo Packages: the LUDIARS packages of this project (declared pin, resolved hash, branch-pin
// warnings), the updates from the last `ergo_pkg watch` report (with read state) and "use this version".
// The window only reads; a pin changes only through UpmApplier after a confirmation dialog.
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

namespace Ludiars.Ergo.Packages.Editor
{
    internal sealed class ErgoPackagesWindow : EditorWindow
    {
        private const string Title = "Ergo Packages";

        private WatchReport _report;
        private string _reportError = "";
        private string _manifestError = "";
        private List<PackageRow> _rows = new List<PackageRow>();
        private List<WatchUpdate> _open = new List<WatchUpdate>();
        private EventInbox _inbox;
        private Vector2 _scroll;

        [MenuItem("Window/LUDIARS/Ergo Packages")]
        public static void Open()
        {
            GetWindow<ErgoPackagesWindow>(Title).Show();
        }

        // The open window, if any (never opens one).
        public static ErgoPackagesWindow OpenInstance()
        {
            ErgoPackagesWindow[] all = Resources.FindObjectsOfTypeAll<ErgoPackagesWindow>();
            return all.Length > 0 ? all[0] : null;
        }

        private void OnEnable()
        {
            UpdateNotifier.ReportChanged += Reload;
            Reload();
        }

        private void OnDisable()
        {
            UpdateNotifier.ReportChanged -= Reload;
        }

        private void Reload()
        {
            _report = ErgoProjectFiles.LoadReport(out _reportError);
            List<UpmDependencyEntry> entries = ErgoProjectFiles.LoadManifest(out _manifestError);
            _rows = PackageOverview.Build(entries, ErgoProjectFiles.LoadLock(), _report, DateTime.UtcNow,
                                          UpdateStatus.DefaultStaleAfter);
            _open = _report != null ? _report.OpenUpdates() : new List<WatchUpdate>();
            _inbox = InboxStore.Load();
            UpdateTitle();
            Repaint();
        }

        private void UpdateTitle()
        {
            int unread = _inbox.UnreadCount(_open);
            titleContent = new GUIContent(unread > 0 ? Title + " (" + unread + ")" : Title);
        }

        private void OnGUI()
        {
            DrawToolbar();
            _scroll = EditorGUILayout.BeginScrollView(_scroll);
            DrawReportHeader();
            EditorGUILayout.Space();
            DrawPackages();
            EditorGUILayout.Space();
            DrawUpdates();
            EditorGUILayout.EndScrollView();
        }

        private void DrawToolbar()
        {
            using (new EditorGUILayout.HorizontalScope(EditorStyles.toolbar))
            {
                if (GUILayout.Button("Refresh", EditorStyles.toolbarButton, GUILayout.Width(70)))
                {
                    UpdateNotifier.CheckNow();
                    Reload();
                }
                GUILayout.FlexibleSpace();
                using (new EditorGUI.DisabledScope(_inbox.UnreadCount(_open) == 0))
                {
                    if (GUILayout.Button("Mark all read", EditorStyles.toolbarButton, GUILayout.Width(100)))
                    {
                        _inbox.MarkAllRead(_open);
                        InboxStore.Save(_inbox);
                        UpdateTitle();
                    }
                }
            }
        }

        private void DrawReportHeader()
        {
            if (_report == null)
            {
                string why = string.IsNullOrEmpty(_reportError)
                    ? "No .ergo/watch-report.json yet. It is written by `ergo_pkg watch --host <this project>`; " +
                      "until then nothing is known about updates (this is not \"up to date\")."
                    : "The watch report cannot be read: " + _reportError;
                EditorGUILayout.HelpBox(why, MessageType.Info);
                return;
            }
            EditorGUILayout.LabelField("Host", _report.Host);
            EditorGUILayout.LabelField("Report", _report.GeneratedAt + " (UTC)");
            if (UpdateStatus.IsStale(_report, DateTime.UtcNow, UpdateStatus.DefaultStaleAfter))
            {
                EditorGUILayout.HelpBox("The report is older than a day; packages without updates are shown as unknown.",
                                        MessageType.Warning);
            }
        }

        private void DrawPackages()
        {
            EditorGUILayout.LabelField("Packages", EditorStyles.boldLabel);
            if (!string.IsNullOrEmpty(_manifestError)) EditorGUILayout.HelpBox(_manifestError, MessageType.Error);
            foreach (PackageRow row in _rows)
            {
                using (new EditorGUILayout.VerticalScope(EditorStyles.helpBox))
                {
                    UpmDependencyEntry e = row.Entry;
                    string code = string.IsNullOrEmpty(row.Code) ? "" : "  [" + row.Code + "]";
                    EditorGUILayout.LabelField(e.Name + code, EditorStyles.boldLabel);
                    if (e.Kind == UpmSourceKind.Git)
                    {
                        EditorGUILayout.LabelField("Declared", string.IsNullOrEmpty(e.Rev) ? "(none)" : "#" + Short(e.Rev));
                        if (!string.IsNullOrEmpty(row.LockedHash)) EditorGUILayout.LabelField("Resolved", Short(row.LockedHash));
                        EditorGUILayout.LabelField("Status", UpdateStatus.Label(row.State));
                    }
                    else
                    {
                        EditorGUILayout.LabelField("Version", e.Value);
                    }
                    if (!string.IsNullOrEmpty(row.Reason))
                    {
                        EditorGUILayout.HelpBox(row.Reason, e.HasPinWarning && e.IsLudiars ? MessageType.Warning : MessageType.None);
                    }
                }
            }
        }

        private void DrawUpdates()
        {
            EditorGUILayout.LabelField("Updates", EditorStyles.boldLabel);
            if (_open.Count == 0)
            {
                EditorGUILayout.LabelField(_report == null ? "—" : "No known updates in the report.");
                return;
            }
            foreach (WatchUpdate u in _open)
            {
                using (new EditorGUILayout.VerticalScope(EditorStyles.helpBox))
                {
                    bool unread = !_inbox.IsRead(u);
                    string to = string.IsNullOrEmpty(u.ToTag) ? Short(u.ToRev) : u.ToTag + " " + Short(u.ToRev);
                    string behind = u.Behind.HasValue ? " (+" + u.Behind.Value + ")" : "";
                    EditorGUILayout.LabelField((unread ? "● " : "") + u.Dependency + ": " + u.Kind + " → " + to + behind,
                                               unread ? EditorStyles.boldLabel : EditorStyles.label);
                    EditorGUILayout.LabelField("Pin", Short(u.FromRev) + (string.IsNullOrEmpty(u.FromTag) ? "" : " " + u.FromTag));
                    if (u.Warning) EditorGUILayout.HelpBox("The pin is not on the tracked branch (off-main).", MessageType.Warning);
                    if (!string.IsNullOrEmpty(u.Next)) EditorGUILayout.LabelField("Next", u.Next, EditorStyles.wordWrappedMiniLabel);
                    using (new EditorGUILayout.HorizontalScope())
                    {
                        GUILayout.FlexibleSpace();
                        if (unread && GUILayout.Button("Mark read", GUILayout.Width(90)))
                        {
                            _inbox.MarkRead(u.Id);
                            InboxStore.Save(_inbox);
                            UpdateTitle();
                        }
                        using (new EditorGUI.DisabledScope(u.Owner != "upm" || !PinRules.IsFullSha(u.ToRev)))
                        {
                            if (GUILayout.Button("Use this version", GUILayout.Width(130)) && UpmApplier.Apply(u))
                            {
                                _inbox.MarkRead(u.Id);
                                InboxStore.Save(_inbox);
                                Reload();
                                GUIUtility.ExitGUI();
                            }
                        }
                    }
                }
            }
        }

        private static string Short(string sha)
        {
            return string.IsNullOrEmpty(sha) ? "—" : sha.Length > 7 ? sha.Substring(0, 7) : sha;
        }
    }
}
