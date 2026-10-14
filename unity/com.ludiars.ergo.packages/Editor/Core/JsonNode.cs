// A parsed JSON value that remembers where it sits in the source text.
// Core has noEngineReferences (plain C#, testable with dotnet), so it cannot use JsonUtility; JsonUtility also
// cannot tell null from 0, which the watch report relies on ("behind": null = unknown distance).
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public enum JsonKind
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    }

    public sealed class JsonNode
    {
        public JsonKind Kind;
        public bool BoolValue;
        public double NumberValue;
        public string StringValue;
        public List<JsonNode> Items;                              // Array
        public List<KeyValuePair<string, JsonNode>> Members;      // Object, in source order

        // Span of this value in the parsed text: [Start, End). ManifestJsonEditor replaces exactly this range.
        public int Start;
        public int End;

        public bool IsNull { get { return Kind == JsonKind.Null; } }

        public JsonNode Get(string key)
        {
            if (Kind != JsonKind.Object || Members == null) return null;
            for (int i = 0; i < Members.Count; ++i)
            {
                if (Members[i].Key == key) return Members[i].Value;
            }
            return null;
        }

        public string GetString(string key, string fallback = "")
        {
            JsonNode v = Get(key);
            return v != null && v.Kind == JsonKind.String ? v.StringValue : fallback;
        }

        public bool GetBool(string key)
        {
            JsonNode v = Get(key);
            return v != null && v.Kind == JsonKind.Bool && v.BoolValue;
        }

        // Missing, null or not a number -> null.
        public int? GetInt(string key)
        {
            JsonNode v = Get(key);
            if (v == null || v.Kind != JsonKind.Number) return null;
            return (int)v.NumberValue;
        }

        // Missing or null -> null (unknown), otherwise the boolean.
        public bool? GetOptionalBool(string key)
        {
            JsonNode v = Get(key);
            if (v == null || v.Kind != JsonKind.Bool) return null;
            return v.BoolValue;
        }

        // Missing or not an array -> empty.
        public IList<JsonNode> GetArray(string key)
        {
            JsonNode v = Get(key);
            if (v == null || v.Kind != JsonKind.Array || v.Items == null) return new List<JsonNode>();
            return v.Items;
        }
    }
}
