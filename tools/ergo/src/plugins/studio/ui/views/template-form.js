// @implements SPEC-ergo-studio-instruct
// 委託テンプレートの選択と、input_schema から作る入力欄。
// target_repo はサーバ側がプロジェクトの repo で埋めるので入力欄に出さない。

import { h } from "../lib.js";

const AUTO_FIELDS = new Set(["target_repo"]);

/// options.hideTask: タスクから依頼するとき (task はタスク本文で埋まる)。
export function templateForm(templates, { hideTask = false } = {}) {
  const select = h("select", {});
  for (const t of templates) {
    const optionLabel = t.model ? `${t.title} (${t.target_provider} / ${t.model})` : `${t.title} (${t.target_provider})`;
    select.append(h("option", { value: t.call_name }, optionLabel));
  }
  const fieldsBox = h("div", {});
  const description = h("p", { class: "muted" });
  const extra = h("textarea", { placeholder: "追加の指示 (任意)" });
  const inputs = new Map();

  function renderFields() {
    const t = templates.find((x) => x.call_name === select.value);
    description.textContent = t?.description ?? "";
    fieldsBox.replaceChildren();
    inputs.clear();
    for (const f of t?.input_schema ?? []) {
      if (AUTO_FIELDS.has(f.name) || (hideTask && f.name === "task")) continue;
      const input = f.name === "task" || f.name.endsWith("_extra")
        ? h("textarea", { placeholder: f.description ?? "" })
        : h("input", { type: "text", placeholder: f.description ?? "" });
      inputs.set(f.name, input);
      fieldsBox.append(h("div", { class: "field" },
        h("label", {}, `${f.name}${f.required ? " *" : ""}`), input));
    }
  }
  select.addEventListener("change", renderFields);
  renderFields();

  const element = h("div", {},
    h("div", { class: "field" }, h("label", {}, "テンプレート"), select),
    description, fieldsBox,
    h("div", { class: "field" }, h("label", {}, "追加の指示"), extra));

  return {
    element,
    value() {
      const args = {};
      for (const [name, input] of inputs) if (input.value.trim() !== "") args[name] = input.value;
      return { call_name: select.value, args, extra_prompt: extra.value.trim() || undefined };
    },
    reset() {
      for (const input of inputs.values()) input.value = "";
      extra.value = "";
    },
  };
}
