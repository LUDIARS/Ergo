// Persists the EventInbox read / notified ids in EditorPrefs, per project (EditorPrefs is machine wide).
using System;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;

namespace Ludiars.Ergo.Packages.Editor
{
    internal static class InboxStore
    {
        private const string Prefix = "Ludiars.Ergo.Packages.";

        public static EventInbox Load()
        {
            return new EventInbox(Split(EditorPrefs.GetString(Key("read"), "")),
                                  Split(EditorPrefs.GetString(Key("notified"), "")));
        }

        public static void Save(EventInbox inbox)
        {
            EditorPrefs.SetString(Key("read"), string.Join("\n", inbox.ReadIds));
            EditorPrefs.SetString(Key("notified"), string.Join("\n", inbox.NotifiedIds));
        }

        private static string[] Split(string s)
        {
            return s.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
        }

        // The project path is hashed so the key holds no path.
        private static string Key(string name)
        {
            using (SHA1 sha = SHA1.Create())
            {
                byte[] digest = sha.ComputeHash(Encoding.UTF8.GetBytes(ErgoProjectFiles.ProjectRoot));
                var sb = new StringBuilder();
                for (int i = 0; i < 8; ++i) sb.Append(digest[i].ToString("x2"));
                return Prefix + sb + "." + name;
            }
        }
    }
}
