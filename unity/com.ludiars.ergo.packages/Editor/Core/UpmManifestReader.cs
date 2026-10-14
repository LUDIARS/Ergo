// Reads Packages/manifest.json: every dependency, classified. Git dependencies on LUDIARS repositories are the ones
// the native `ergo_pkg watch` judges (it reads the same file as a UPM host); registry and file: dependencies are
// listed with the reason they are not judged.
using System;
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public enum UpmSourceKind
    {
        Git,
        Registry,
        File,
    }

    public enum UpmPinState
    {
        Sha,            // #<40-digit SHA>
        Branch,         // #main, #v1.0.0, #abc1234 ... (moves or is not unique)
        None,           // a git URL without '#'
        NotApplicable,  // registry / file:
    }

    public sealed class UpmDependencyEntry
    {
        public string Name;
        public string Value;       // the manifest value as written
        public UpmSourceKind Kind;
        public string Url;         // before '#'
        public string Repository;  // Url without a leading "git+" and without "?path=..."
        public string Rev;         // after '#', "" when absent
        public UpmPinState PinState;
        public bool IsLudiars;     // a git dependency on a LUDIARS repository (judged by ergo_pkg watch)
        public string Note;        // why it is not judged, or the pin warning; "" when fine

        public bool HasPinWarning { get { return Kind == UpmSourceKind.Git && PinState != UpmPinState.Sha; } }
    }

    public static class UpmManifestReader
    {
        // Same marker as the native reader (ergo_package_manager read_upm_dependencies).
        public const string LudiarsMarker = "LUDIARS/";

        // Throws FormatException when the text is not JSON. Entries keep the manifest order.
        public static List<UpmDependencyEntry> Read(string manifestJson)
        {
            JsonNode root = JsonParser.Parse(manifestJson);
            var result = new List<UpmDependencyEntry>();
            JsonNode deps = root.Get("dependencies");
            if (deps == null || deps.Kind != JsonKind.Object) return result;
            foreach (KeyValuePair<string, JsonNode> m in deps.Members)
            {
                if (m.Value.Kind != JsonKind.String) continue;
                result.Add(Classify(m.Key, m.Value.StringValue));
            }
            return result;
        }

        public static UpmDependencyEntry Classify(string name, string value)
        {
            var e = new UpmDependencyEntry { Name = name, Value = value, Url = value, Repository = "", Rev = "", Note = "" };
            if (value.StartsWith("file:", StringComparison.Ordinal))
            {
                e.Kind = UpmSourceKind.File;
                e.PinState = UpmPinState.NotApplicable;
                e.Note = "file: dependency (not checked for updates)";
                return e;
            }
            if (!IsGitValue(value))
            {
                e.Kind = UpmSourceKind.Registry;
                e.PinState = UpmPinState.NotApplicable;
                e.Note = "registry dependency (not checked for updates)";
                return e;
            }

            e.Kind = UpmSourceKind.Git;
            int hash = value.IndexOf('#');
            e.Url = hash < 0 ? value : value.Substring(0, hash);
            e.Rev = hash < 0 ? "" : value.Substring(hash + 1);
            e.Repository = RepositoryOf(e.Url);
            e.IsLudiars = value.IndexOf(LudiarsMarker, StringComparison.Ordinal) >= 0;
            if (PinRules.IsFullSha(e.Rev)) e.PinState = UpmPinState.Sha;
            else if (e.Rev.Length == 0) e.PinState = UpmPinState.None;
            else e.PinState = UpmPinState.Branch;

            if (!e.IsLudiars) e.Note = "not a LUDIARS repository (not checked for updates)";
            else if (e.PinState == UpmPinState.Branch) e.Note = "#" + e.Rev + " is not a 40-digit SHA; pin it to a commit";
            else if (e.PinState == UpmPinState.None) e.Note = "no #<sha>; pin it to a commit";
            return e;
        }

        // "git+https://host/LUDIARS/Tela.git?path=/unity/x" -> "https://host/LUDIARS/Tela.git" (same as the native side).
        public static string RepositoryOf(string url)
        {
            string s = url;
            if (s.StartsWith("git+", StringComparison.Ordinal)) s = s.Substring(4);
            int query = s.IndexOf('?');
            return query < 0 ? s : s.Substring(0, query);
        }

        private static bool IsGitValue(string value)
        {
            return value.StartsWith("git+", StringComparison.Ordinal)
                || value.StartsWith("git@", StringComparison.Ordinal)
                || value.StartsWith("ssh://", StringComparison.Ordinal)
                || value.StartsWith("https://", StringComparison.Ordinal)
                || value.StartsWith("http://", StringComparison.Ordinal)
                || value.StartsWith("git://", StringComparison.Ordinal);
        }
    }
}
