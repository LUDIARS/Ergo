// Rewrites exactly one value of Packages/manifest.json: dependencies.<name>. Every other character (key order,
// indentation, line endings, other entries) stays as it was, byte for byte.
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public static class ManifestJsonEditor
    {
        public static bool TryGetDependency(string manifestText, string packageName, out string value, out string error)
        {
            value = null;
            JsonNode node;
            if (!TryFindDependency(manifestText, packageName, out node, out error)) return false;
            value = node.StringValue;
            return true;
        }

        public static bool TrySetDependency(string manifestText, string packageName, string newValue,
                                            out string updated, out string error)
        {
            updated = null;
            if (newValue == null)
            {
                error = "no new value";
                return false;
            }
            JsonNode node;
            if (!TryFindDependency(manifestText, packageName, out node, out error)) return false;
            updated = manifestText.Substring(0, node.Start) + JsonParser.Quote(newValue) + manifestText.Substring(node.End);
            return true;
        }

        private static bool TryFindDependency(string manifestText, string packageName, out JsonNode node, out string error)
        {
            node = null;
            JsonNode root;
            if (!JsonParser.TryParse(manifestText, out root, out error)) return false;
            JsonNode deps = root.Get("dependencies");
            if (deps == null || deps.Kind != JsonKind.Object)
            {
                error = "manifest.json has no \"dependencies\" object";
                return false;
            }
            int found = 0;
            foreach (KeyValuePair<string, JsonNode> m in deps.Members)
            {
                if (m.Key != packageName) continue;
                node = m.Value;
                ++found;
            }
            if (found == 0)
            {
                error = "manifest.json has no dependency '" + packageName + "'";
                return false;
            }
            if (found > 1)
            {
                node = null;
                error = "manifest.json lists '" + packageName + "' more than once";
                return false;
            }
            if (node.Kind != JsonKind.String)
            {
                node = null;
                error = "dependency '" + packageName + "' is not a string";
                return false;
            }
            error = null;
            return true;
        }
    }
}
