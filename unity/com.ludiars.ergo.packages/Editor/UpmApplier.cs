// Applies one update to Packages/manifest.json: plan the pin (PinPlanner), show the change in a confirmation dialog,
// rewrite that one value (ManifestJsonEditor, other bytes unchanged) and ask UPM to resolve. Never commits.
using System.IO;
using System.Text;
using UnityEditor;
using UnityEditor.PackageManager;
using UnityEngine;

namespace Ludiars.Ergo.Packages.Editor
{
    internal static class UpmApplier
    {
        // Returns true when manifest.json was rewritten.
        public static bool Apply(WatchUpdate update)
        {
            string path = ErgoProjectFiles.ManifestPath;
            if (!File.Exists(path))
            {
                EditorUtility.DisplayDialog("Ergo Packages", "Packages/manifest.json not found.", "OK");
                return false;
            }
            byte[] bytes = File.ReadAllBytes(path);
            bool bom = bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF;
            string text = new UTF8Encoding(false).GetString(bytes, bom ? 3 : 0, bom ? bytes.Length - 3 : bytes.Length);

            string current, error;
            if (!ManifestJsonEditor.TryGetDependency(text, update.Dependency, out current, out error))
            {
                EditorUtility.DisplayDialog("Ergo Packages", error, "OK");
                return false;
            }
            PinPlan plan = PinPlanner.PlanFor(update, current);
            if (!plan.Ok)
            {
                EditorUtility.DisplayDialog("Ergo Packages", plan.Error, "OK");
                return false;
            }
            if (!plan.Changes)
            {
                EditorUtility.DisplayDialog("Ergo Packages", update.Dependency + " is already pinned to this commit.", "OK");
                return false;
            }

            string target = string.IsNullOrEmpty(update.ToTag) ? update.Kind : update.ToTag;
            string message = "Packages/manifest.json\n\n" + update.Dependency + " (" + target + ")\n\n- " + plan.OldValue +
                             "\n+ " + plan.NewValue + "\n\nThe file is not committed. Open a pull request for it as usual.";
            if (!EditorUtility.DisplayDialog("Ergo Packages: change pin", message, "Change", "Cancel")) return false;

            string updated;
            if (!ManifestJsonEditor.TrySetDependency(text, update.Dependency, plan.NewValue, out updated, out error))
            {
                EditorUtility.DisplayDialog("Ergo Packages", error, "OK");
                return false;
            }
            File.WriteAllBytes(path, Encode(updated, bom));
            Debug.Log("[Ergo Packages] " + update.Dependency + " pinned to " + plan.Rev.Substring(0, 7) + "; resolving packages.");
            Client.Resolve();
            return true;
        }

        private static byte[] Encode(string text, bool bom)
        {
            byte[] body = new UTF8Encoding(false).GetBytes(text);
            if (!bom) return body;
            byte[] result = new byte[body.Length + 3];
            result[0] = 0xEF;
            result[1] = 0xBB;
            result[2] = 0xBF;
            System.Array.Copy(body, 0, result, 3, body.Length);
            return result;
        }
    }
}
