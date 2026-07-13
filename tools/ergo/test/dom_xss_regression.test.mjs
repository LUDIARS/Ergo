import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { fileURLToPath } from "node:url";

const sourceUrl = new URL("../src/plugins/", import.meta.url);

async function readPluginSource(plugin) {
    return readFile(fileURLToPath(new URL(`${plugin}/ui/app.js`, sourceUrl)), "utf8");
}

function assertInnerHtmlOnlyClears(source, name) {
    const assignments = [...source.matchAll(/\.innerHTML\s*=\s*([^;]+);/g)]
        .map((match) => match[1].trim());
    assert.ok(assignments.length > 0, `${name} should clear rendered containers`);
    for (const assignment of assignments) {
        assert.equal(assignment, "\"\"", `${name} must not parse dynamically assembled HTML`);
    }
}

const adversarialValues = [
    "\"><img src=x onerror=alert(1)>",
    "</textarea><svg onload=alert(1)>",
    "x\" autofocus onfocus=alert(1) x=\"",
];

test("layout document values never enter HTML parsing", async () => {
    const source = await readPluginSource("ui_layout");

    assertInnerHtmlOnlyClears(source, "ui_layout");
    assert.match(source, /const info = createBlock\(node\.id\);/);
    assert.match(source, /heading\.textContent = title;/);
    assert.match(source, /const idInput = textInput\(node\.id\);/);
    assert.match(source, /bindsInput\.value = JSON\.stringify\(node\.binds/);

    for (const value of adversarialValues) {
        assert.ok(value.includes("<") || value.includes("\""));
        assert.doesNotMatch(source, new RegExp(`innerHTML\\s*=\\s*[^;]*${value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}`));
    }
});

test("Rive metadata names are assigned as text and values", async () => {
    const source = await readPluginSource("rive");

    assertInnerHtmlOnlyClears(source, "rive");
    assert.match(source, /o\.value = n; o\.textContent = n;/);
    assert.match(source, /name\.textContent = n;/);
    assert.match(source, /li\.append\(name, details\);/);

    for (const value of adversarialValues) {
        assert.ok(value.includes("<") || value.includes("\""));
        assert.doesNotMatch(source, new RegExp(`innerHTML\\s*=\\s*[^;]*${value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}`));
    }
});
