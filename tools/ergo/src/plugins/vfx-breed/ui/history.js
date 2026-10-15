// Generation history, kept in the browser only. Each entry is one shown
// generation; breeding from an older entry drops the entries after it
// (the new branch replaces them).

export function createHistory() {
  /** @type {{ generation: number, configs: object[], seed: number|null, note: string }[]} */
  const entries = [];
  let index = -1;

  return {
    get current() { return entries[index] ?? null; },
    get index() { return index; },
    get entries() { return entries; },
    reset(entry) { entries.length = 0; entries.push(entry); index = 0; },
    push(entry) { entries.length = index + 1; entries.push(entry); index = entries.length - 1; },
    goTo(i) { if (i >= 0 && i < entries.length) index = i; },
  };
}
