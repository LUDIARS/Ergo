// The pin rule shared with the native side (spec/feature/package-manager.md §6.3): a pin is a 40-digit lowercase
// commit SHA; tags are only annotations. Nothing here decides whether an update exists (that is C++ only).
namespace Ludiars.Ergo.Packages
{
    public static class PinRules
    {
        public const int FullShaLength = 40;

        public static bool IsFullSha(string rev)
        {
            if (rev == null || rev.Length != FullShaLength) return false;
            foreach (char c in rev)
            {
                if (!IsLowerHex(c)) return false;
            }
            return true;
        }

        // 4..39 hex digits: looks like an abbreviated SHA (rejected, it can stop being unique).
        public static bool LooksLikeShortSha(string rev)
        {
            if (rev == null || rev.Length < 4 || rev.Length >= FullShaLength) return false;
            foreach (char c in rev)
            {
                if (!IsLowerHex(char.ToLowerInvariant(c))) return false;
            }
            return true;
        }

        private static bool IsLowerHex(char c)
        {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        }
    }
}
