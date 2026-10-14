// Reads Packages/packages-lock.json: the commit UPM actually resolved for each git dependency ("hash").
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public sealed class PackagesLockEntry
    {
        public string Name;
        public string Version;
        public string Source;  // "git", "registry", "builtin", "embedded", "local" ...
        public string Hash;    // resolved commit for git sources, "" otherwise
    }

    public static class PackagesLockReader
    {
        // Throws FormatException when the text is not JSON.
        public static Dictionary<string, PackagesLockEntry> Read(string lockJson)
        {
            JsonNode root = JsonParser.Parse(lockJson);
            var result = new Dictionary<string, PackagesLockEntry>();
            JsonNode deps = root.Get("dependencies");
            if (deps == null || deps.Kind != JsonKind.Object) return result;
            foreach (KeyValuePair<string, JsonNode> m in deps.Members)
            {
                if (m.Value.Kind != JsonKind.Object) continue;
                result[m.Key] = new PackagesLockEntry
                {
                    Name = m.Key,
                    Version = m.Value.GetString("version"),
                    Source = m.Value.GetString("source"),
                    Hash = m.Value.GetString("hash"),
                };
            }
            return result;
        }
    }
}
