// Read state of the report's updates, keyed by update id ("<host>/<dependency>/<kind>/<subject>").
//   * unread  = open updates the user has not marked as read (the window's tab shows the count)
//   * notify  = open updates never notified before; each id is notified once
// Storage is the caller's (the Editor keeps it in EditorPrefs); this class only does the bookkeeping.
using System.Collections.Generic;

namespace Ludiars.Ergo.Packages
{
    public sealed class EventInbox
    {
        private readonly HashSet<string> _read;
        private readonly HashSet<string> _notified;

        public EventInbox(IEnumerable<string> readIds, IEnumerable<string> notifiedIds)
        {
            _read = new HashSet<string>(readIds ?? new string[0]);
            _notified = new HashSet<string>(notifiedIds ?? new string[0]);
        }

        public IEnumerable<string> ReadIds { get { return _read; } }
        public IEnumerable<string> NotifiedIds { get { return _notified; } }

        public bool IsRead(WatchUpdate update)
        {
            return _read.Contains(update.Id);
        }

        public int UnreadCount(IEnumerable<WatchUpdate> open)
        {
            int n = 0;
            foreach (WatchUpdate u in open)
            {
                if (!string.IsNullOrEmpty(u.Id) && !_read.Contains(u.Id)) ++n;
            }
            return n;
        }

        // Returns the updates not notified yet and remembers them, so a second call with the same input returns none.
        public List<WatchUpdate> TakeNewForNotification(IEnumerable<WatchUpdate> open)
        {
            var result = new List<WatchUpdate>();
            foreach (WatchUpdate u in open)
            {
                if (string.IsNullOrEmpty(u.Id) || _read.Contains(u.Id)) continue;
                if (_notified.Add(u.Id)) result.Add(u);
            }
            return result;
        }

        public void MarkRead(string id)
        {
            if (!string.IsNullOrEmpty(id)) _read.Add(id);
        }

        public void MarkAllRead(IEnumerable<WatchUpdate> open)
        {
            foreach (WatchUpdate u in open) MarkRead(u.Id);
        }

        // Forgets ids that are no longer open (resolved updates), so the stored sets do not grow forever.
        public void Prune(IEnumerable<WatchUpdate> open)
        {
            var live = new HashSet<string>();
            foreach (WatchUpdate u in open) live.Add(u.Id);
            _read.IntersectWith(live);
            _notified.IntersectWith(live);
        }
    }
}
