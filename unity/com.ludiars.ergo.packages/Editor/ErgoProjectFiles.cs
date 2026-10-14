// Where the files of this Unity project are, and reading them without throwing (a missing or broken file is shown
// in the window, not raised). Paths are built at run time from the project; none is stored.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

namespace Ludiars.Ergo.Packages.Editor
{
    internal static class ErgoProjectFiles
    {
        public static string ProjectRoot
        {
            get { return Directory.GetParent(Application.dataPath).FullName; }
        }

        public static string ManifestPath { get { return Path.Combine(ProjectRoot, "Packages", "manifest.json"); } }
        public static string LockPath { get { return Path.Combine(ProjectRoot, "Packages", "packages-lock.json"); } }
        public static string ReportPath { get { return Path.Combine(ProjectRoot, ".ergo", "watch-report.json"); } }

        public static DateTime ReportWriteTimeUtc()
        {
            return File.Exists(ReportPath) ? File.GetLastWriteTimeUtc(ReportPath) : DateTime.MinValue;
        }

        public static string ReadText(string path)
        {
            return File.Exists(path) ? File.ReadAllText(path, Encoding.UTF8) : null;
        }

        // null when the report is missing or cannot be read; error says why (empty when simply missing).
        public static WatchReport LoadReport(out string error)
        {
            error = "";
            string text = ReadText(ReportPath);
            if (text == null) return null;
            try
            {
                return WatchReportReader.Read(text);
            }
            catch (FormatException e)
            {
                error = e.Message;
                return null;
            }
        }

        public static List<UpmDependencyEntry> LoadManifest(out string error)
        {
            error = "";
            string text = ReadText(ManifestPath);
            if (text == null)
            {
                error = "Packages/manifest.json not found";
                return new List<UpmDependencyEntry>();
            }
            try
            {
                return UpmManifestReader.Read(text);
            }
            catch (FormatException e)
            {
                error = e.Message;
                return new List<UpmDependencyEntry>();
            }
        }

        public static Dictionary<string, PackagesLockEntry> LoadLock()
        {
            string text = ReadText(LockPath);
            if (text == null) return new Dictionary<string, PackagesLockEntry>();
            try
            {
                return PackagesLockReader.Read(text);
            }
            catch (FormatException)
            {
                return new Dictionary<string, PackagesLockEntry>();
            }
        }
    }
}
