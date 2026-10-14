using System.Collections.Generic;
using NUnit.Framework;

namespace Ludiars.Ergo.Packages.Tests
{
    public class UpmManifestReaderTests
    {
        private static UpmDependencyEntry Find(List<UpmDependencyEntry> entries, string name)
        {
            foreach (UpmDependencyEntry e in entries)
            {
                if (e.Name == name) return e;
            }
            Assert.Fail("missing " + name);
            return null;
        }

        [Test]
        public void ReadsEveryDependencyInManifestOrder()
        {
            List<UpmDependencyEntry> entries = UpmManifestReader.Read(Fixtures.Read("upm-manifest.json"));
            Assert.AreEqual(5, entries.Count);
            Assert.AreEqual("com.ludiars.ergo.packages", entries[0].Name);
            Assert.AreEqual("com.unity.modules.jsonserialize", entries[4].Name);
        }

        [Test]
        public void GitDependencyWithFullShaIsPinned()
        {
            UpmDependencyEntry e = Find(UpmManifestReader.Read(Fixtures.Read("upm-manifest.json")), "com.ludiars.ergo.packages");
            Assert.AreEqual(UpmSourceKind.Git, e.Kind);
            Assert.IsTrue(e.IsLudiars);
            Assert.AreEqual(UpmPinState.Sha, e.PinState);
            Assert.AreEqual("b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", e.Rev);
            Assert.AreEqual("https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages", e.Url);
            Assert.AreEqual("https://github.com/LUDIARS/Ergo.git", e.Repository);
            Assert.IsFalse(e.HasPinWarning);
            Assert.AreEqual("", e.Note);
        }

        [Test]
        public void GitPlusPrefixAndPathQueryAreStrippedFromRepository()
        {
            UpmDependencyEntry e = Find(UpmManifestReader.Read(Fixtures.Read("upm-manifest.json")), "com.ludiars.pictor");
            Assert.AreEqual(UpmSourceKind.Git, e.Kind);
            Assert.AreEqual("https://github.com/LUDIARS/Pictor.git", e.Repository);
            Assert.AreEqual(UpmPinState.Sha, e.PinState);
        }

        [Test]
        public void BranchPinIsWarned()
        {
            UpmDependencyEntry e = Find(UpmManifestReader.Read(Fixtures.Read("upm-manifest.json")), "com.ludiars.tela");
            Assert.AreEqual(UpmPinState.Branch, e.PinState);
            Assert.AreEqual("main", e.Rev);
            Assert.IsTrue(e.HasPinWarning);
            StringAssert.Contains("#main", e.Note);
        }

        [Test]
        public void RegistryAndFileDependenciesAreListedButNotJudged()
        {
            List<UpmDependencyEntry> entries = UpmManifestReader.Read(Fixtures.Read("upm-manifest.json"));
            UpmDependencyEntry file = Find(entries, "com.example.local");
            Assert.AreEqual(UpmSourceKind.File, file.Kind);
            Assert.AreEqual(UpmPinState.NotApplicable, file.PinState);
            StringAssert.Contains("not checked", file.Note);
            UpmDependencyEntry registry = Find(entries, "com.unity.modules.jsonserialize");
            Assert.AreEqual(UpmSourceKind.Registry, registry.Kind);
            StringAssert.Contains("not checked", registry.Note);
        }

        [Test]
        public void GitUrlWithoutHashHasNoPin()
        {
            UpmDependencyEntry e = UpmManifestReader.Classify("x", "https://github.com/LUDIARS/Tela.git");
            Assert.AreEqual(UpmPinState.None, e.PinState);
            Assert.IsTrue(e.HasPinWarning);
        }

        [Test]
        public void ShortShaIsNotAPin()
        {
            UpmDependencyEntry e = UpmManifestReader.Classify("x", "https://github.com/LUDIARS/Tela.git#b74f127");
            Assert.AreEqual(UpmPinState.Branch, e.PinState);
        }

        [Test]
        public void NonLudiarsGitIsNotJudged()
        {
            UpmDependencyEntry e = UpmManifestReader.Classify("x", "https://example.invalid/Other.git#main");
            Assert.AreEqual(UpmSourceKind.Git, e.Kind);
            Assert.IsFalse(e.IsLudiars);
            StringAssert.Contains("not a LUDIARS", e.Note);
        }
    }
}
