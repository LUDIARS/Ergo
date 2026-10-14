using NUnit.Framework;

namespace Ludiars.Ergo.Packages.Tests
{
    public class ManifestJsonEditorTests
    {
        private const string Name = "com.ludiars.ergo.packages";
        private const string Old = "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        private const string New = "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#9c2d4e7ccccccccccccccccccccccccccccccccc";

        [Test]
        public void ChangesOnlyThatOneValue()
        {
            string before = Fixtures.Read("upm-manifest.json");
            string after;
            string error;
            Assert.IsTrue(ManifestJsonEditor.TrySetDependency(before, Name, New, out after, out error), error);

            int at = before.IndexOf(Old, System.StringComparison.Ordinal);
            Assert.Greater(at, 0);
            // Everything before and after the value is the same text; only the value differs.
            Assert.AreEqual(before.Substring(0, at), after.Substring(0, at));
            Assert.AreEqual(before.Substring(at + Old.Length), after.Substring(at + New.Length));
            Assert.AreEqual(New, after.Substring(at, New.Length));
            Assert.AreEqual(before.Length - Old.Length + New.Length, after.Length);
        }

        [Test]
        public void KeepsCrLfAndOddSpacing()
        {
            string before = "{\r\n  \"dependencies\" :{\r\n\t\"a\"  :   \"x#1\",\r\n  \"" + Name + "\":\"" + Old + "\"   }\r\n}\r\n";
            string after;
            string error;
            Assert.IsTrue(ManifestJsonEditor.TrySetDependency(before, Name, New, out after, out error), error);
            Assert.AreEqual(before.Replace(Old, New), after);
        }

        [Test]
        public void ReadsTheCurrentValue()
        {
            string value;
            string error;
            Assert.IsTrue(ManifestJsonEditor.TryGetDependency(Fixtures.Read("upm-manifest.json"), Name, out value, out error));
            Assert.AreEqual(Old, value);
        }

        [Test]
        public void MissingDependencyIsAnError()
        {
            string after;
            string error;
            Assert.IsFalse(ManifestJsonEditor.TrySetDependency(Fixtures.Read("upm-manifest.json"), "com.nope", New, out after, out error));
            Assert.IsNull(after);
            StringAssert.Contains("com.nope", error);
        }

        [Test]
        public void ANameOutsideDependenciesIsNotTouched()
        {
            string before = "{\"other\":{\"" + Name + "\":\"keep\"},\"dependencies\":{}}";
            string after;
            string error;
            Assert.IsFalse(ManifestJsonEditor.TrySetDependency(before, Name, New, out after, out error));
        }

        [Test]
        public void NonStringValueIsAnError()
        {
            string after;
            string error;
            Assert.IsFalse(ManifestJsonEditor.TrySetDependency("{\"dependencies\":{\"" + Name + "\":3}}", Name, New, out after, out error));
        }

        [Test]
        public void BrokenJsonIsAnError()
        {
            string after;
            string error;
            Assert.IsFalse(ManifestJsonEditor.TrySetDependency("{\"dependencies\":", Name, New, out after, out error));
            Assert.IsNotNull(error);
        }

        [Test]
        public void ValueIsEscaped()
        {
            string after;
            string error;
            Assert.IsTrue(ManifestJsonEditor.TrySetDependency("{\"dependencies\":{\"a\":\"b\"}}", "a", "q\"\\", out after, out error));
            Assert.AreEqual("{\"dependencies\":{\"a\":\"q\\\"\\\\\"}}", after);
            string value;
            Assert.IsTrue(ManifestJsonEditor.TryGetDependency(after, "a", out value, out error));
            Assert.AreEqual("q\"\\", value);
        }
    }
}
