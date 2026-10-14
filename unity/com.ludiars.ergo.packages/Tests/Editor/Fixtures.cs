// Locates Tests/Editor/Fixtures (byte-identical copies of Ergo's tests/package_watch/fixtures, checked by
// test_unity_package_structure). Works in Unity's EditMode runner and in `dotnet test` (tests/unity_packages).
using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Text;

namespace Ludiars.Ergo.Packages.Tests
{
    internal static class Fixtures
    {
        public static string Read(string name)
        {
            return File.ReadAllText(Path.Combine(Directory(), name), new UTF8Encoding(false));
        }

        private static string Directory([CallerFilePath] string thisFile = "")
        {
            string beside = Path.Combine(Path.GetDirectoryName(thisFile) ?? "", "Fixtures");
            if (System.IO.Directory.Exists(beside)) return beside;
            string copied = Path.Combine(AppContext.BaseDirectory, "Fixtures");
            if (System.IO.Directory.Exists(copied)) return copied;
            return Path.Combine("Packages", "com.ludiars.ergo.packages", "Tests", "Editor", "Fixtures");
        }
    }
}
