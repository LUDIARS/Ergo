// Reads <project>/.ergo/watch-report.json into WatchReport. Only the format is shared with the native side;
// this reader never decides whether an update exists.
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Ludiars.Ergo.Packages
{
    public static class WatchReportReader
    {
        // Throws FormatException when the text is not JSON or the schema is not supported.
        public static WatchReport Read(string reportJson)
        {
            JsonNode root = JsonParser.Parse(reportJson);
            if (root.Kind != JsonKind.Object) throw new FormatException("watch report: not an object");
            int? schema = root.GetInt("schema");
            if (schema != WatchReport.SupportedSchema)
            {
                throw new FormatException("watch report: schema " +
                    (schema.HasValue ? schema.Value.ToString(CultureInfo.InvariantCulture) : "(missing)") +
                    " is not supported (expected " + WatchReport.SupportedSchema.ToString(CultureInfo.InvariantCulture) + ")");
            }
            var report = new WatchReport
            {
                Schema = schema.Value,
                Host = root.GetString("host"),
                GeneratedAt = root.GetString("generated_at"),
            };
            foreach (JsonNode d in root.GetArray("dependencies")) report.Dependencies.Add(ReadDependency(d));
            foreach (JsonNode e in root.GetArray("events")) report.Events.Add(ReadUpdate(e));
            foreach (JsonNode p in root.GetArray("pending")) report.Pending.Add(ReadUpdate(p));
            return report;
        }

        // "2026-10-15T00:00:00Z" -> UTC DateTime; null when malformed.
        public static DateTime? ParseUtc(string text)
        {
            DateTime t;
            if (DateTime.TryParseExact(text, "yyyy-MM-dd'T'HH:mm:ss'Z'", CultureInfo.InvariantCulture,
                    DateTimeStyles.AdjustToUniversal | DateTimeStyles.AssumeUniversal, out t))
            {
                return t;
            }
            return null;
        }

        private static WatchDependencyStatus ReadDependency(JsonNode d)
        {
            var s = new WatchDependencyStatus
            {
                Name = d.GetString("name"),
                Code = d.GetString("code"),
                Owner = d.GetString("owner"),
                Private = d.GetBool("private"),
                Observed = d.GetBool("observed"),
                Reason = d.GetString("reason"),
                AsOf = d.GetString("as_of"),
                SiblingBehind = d.GetInt("sibling_behind"),
            };
            JsonNode pin = d.Get("pin");
            if (pin != null)
            {
                s.PinRev = pin.GetString("rev");
                s.PinTag = pin.GetString("tag");
            }
            JsonNode track = d.Get("track");
            if (track != null)
            {
                s.TrackBranch = track.GetString("branch");
                s.TrackHead = track.GetString("head");
                s.TrackBehind = track.GetInt("behind");
                s.OnTrack = track.GetOptionalBool("on_track");
            }
            foreach (JsonNode u in d.GetArray("updates")) s.Updates.Add(ReadUpdate(u));
            return s;
        }

        private static WatchUpdate ReadUpdate(JsonNode u)
        {
            return new WatchUpdate
            {
                Id = u.GetString("id"),
                Host = u.GetString("host"),
                Dependency = u.GetString("dependency"),
                Code = u.GetString("code"),
                Owner = u.GetString("owner"),
                Private = u.GetBool("private"),
                Kind = u.GetString("kind"),
                Subject = u.GetString("subject"),
                FromRev = u.GetString("from_rev"),
                FromTag = u.GetString("from_tag"),
                ToRev = u.GetString("to_rev"),
                ToTag = u.GetString("to_tag"),
                Behind = u.GetInt("behind"),
                Warning = u.GetBool("warning"),
                StrictOk = u.GetBool("strict_ok"),
                Next = u.GetString("next"),
                AsOf = u.GetString("as_of"),
                At = u.GetString("at"),
                SinceNotified = u.GetInt("since_notified"),
                FirstSeen = u.GetString("first_seen"),
                LastNotified = u.GetString("last_notified"),
            };
        }
    }
}
