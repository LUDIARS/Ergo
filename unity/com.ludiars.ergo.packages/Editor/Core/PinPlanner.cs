// Plans a pin change of one UPM git dependency: the new manifest value "<url>#<40-digit SHA>". Branch names, tags
// and abbreviated SHAs are rejected (they move or stop being unique), so the manifest always pins a commit.
using System;

namespace Ludiars.Ergo.Packages
{
    public sealed class PinPlan
    {
        public bool Ok;
        public string Error = "";
        public string OldValue = "";
        public string NewValue = "";
        public string Rev = "";

        public bool Changes { get { return Ok && OldValue != NewValue; } }

        internal static PinPlan Fail(string oldValue, string error)
        {
            return new PinPlan { Ok = false, OldValue = oldValue ?? "", Error = error };
        }
    }

    public static class PinPlanner
    {
        public static PinPlan Plan(string currentValue, string targetRev)
        {
            if (string.IsNullOrEmpty(currentValue)) return PinPlan.Fail(currentValue, "the dependency has no value");
            UpmDependencyEntry entry = UpmManifestReader.Classify("", currentValue);
            if (entry.Kind != UpmSourceKind.Git)
            {
                return PinPlan.Fail(currentValue, "only git dependencies can be pinned (this is a " +
                    (entry.Kind == UpmSourceKind.File ? "file:" : "registry") + " dependency)");
            }

            string rev = (targetRev ?? "").Trim();
            if (rev.Length == 0) return PinPlan.Fail(currentValue, "no target revision");
            string lower = rev.ToLowerInvariant();
            if (!PinRules.IsFullSha(lower))
            {
                if (PinRules.LooksLikeShortSha(rev))
                {
                    return PinPlan.Fail(currentValue, "'" + rev + "' is an abbreviated SHA; use the 40-digit commit SHA");
                }
                return PinPlan.Fail(currentValue, "'" + rev +
                    "' is a branch or tag name; resolve it to a 40-digit commit SHA (tags are annotations only)");
            }

            return new PinPlan
            {
                Ok = true,
                OldValue = currentValue,
                NewValue = entry.Url + "#" + lower,
                Rev = lower,
            };
        }

        // Plans the pin an update asks for (its ToRev is a 40-digit SHA written by ergo_pkg watch).
        public static PinPlan PlanFor(WatchUpdate update, string currentValue)
        {
            if (update == null) throw new ArgumentNullException("update");
            if (update.Owner != "upm")
            {
                return PinPlan.Fail(currentValue, "the pin of '" + update.Dependency +
                    "' is not owned by this Unity project (owner: " + update.Owner + ")");
            }
            return Plan(currentValue, update.ToRev);
        }
    }
}
