using NUnit.Framework;

namespace Ludiars.Ergo.Packages.Tests
{
    public class PinPlannerTests
    {
        private const string Current = "https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        private const string Target = "9c2d4e7ccccccccccccccccccccccccccccccccc";

        [Test]
        public void FullShaBuildsThePinnedUrlAndKeepsThePath()
        {
            PinPlan plan = PinPlanner.Plan(Current, Target);
            Assert.IsTrue(plan.Ok, plan.Error);
            Assert.IsTrue(plan.Changes);
            Assert.AreEqual(Current, plan.OldValue);
            Assert.AreEqual("https://github.com/LUDIARS/Ergo.git?path=/unity/com.ludiars.ergo.packages#" + Target, plan.NewValue);
            Assert.AreEqual(Target, plan.Rev);
        }

        [Test]
        public void UppercaseShaIsNormalized()
        {
            PinPlan plan = PinPlanner.Plan(Current, Target.ToUpperInvariant());
            Assert.IsTrue(plan.Ok, plan.Error);
            StringAssert.EndsWith("#" + Target, plan.NewValue);
        }

        [Test]
        public void BranchNameIsRejected()
        {
            PinPlan plan = PinPlanner.Plan(Current, "main");
            Assert.IsFalse(plan.Ok);
            StringAssert.Contains("branch or tag", plan.Error);
            Assert.AreEqual("", plan.NewValue);
        }

        [Test]
        public void TagNameIsRejected()
        {
            Assert.IsFalse(PinPlanner.Plan(Current, "v0.10.0").Ok);
        }

        [Test]
        public void ShortShaIsRejected()
        {
            PinPlan plan = PinPlanner.Plan(Current, "9c2d4e7");
            Assert.IsFalse(plan.Ok);
            StringAssert.Contains("abbreviated", plan.Error);
        }

        [Test]
        public void EmptyTargetIsRejected()
        {
            Assert.IsFalse(PinPlanner.Plan(Current, "").Ok);
            Assert.IsFalse(PinPlanner.Plan(Current, null).Ok);
        }

        [Test]
        public void NonGitDependencyIsRejected()
        {
            Assert.IsFalse(PinPlanner.Plan("1.0.0", Target).Ok);
            Assert.IsFalse(PinPlanner.Plan("file:../Local/x", Target).Ok);
        }

        [Test]
        public void BranchPinCanBeReplacedByASha()
        {
            PinPlan plan = PinPlanner.Plan("https://github.com/LUDIARS/Tela.git?path=/unity/com.ludiars.tela#main", Target);
            Assert.IsTrue(plan.Ok, plan.Error);
            Assert.AreEqual("https://github.com/LUDIARS/Tela.git?path=/unity/com.ludiars.tela#" + Target, plan.NewValue);
        }

        [Test]
        public void SamePinIsNoChange()
        {
            PinPlan plan = PinPlanner.Plan(Current, "b74f127aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
            Assert.IsTrue(plan.Ok);
            Assert.IsFalse(plan.Changes);
        }

        [Test]
        public void UpdateOwnedByAnotherRepositoryIsRejected()
        {
            var update = new WatchUpdate { Dependency = "pictor", Owner = "ergo", ToRev = Target };
            Assert.IsFalse(PinPlanner.PlanFor(update, Current).Ok);
            update.Owner = "upm";
            Assert.IsTrue(PinPlanner.PlanFor(update, Current).Ok);
        }
    }
}
