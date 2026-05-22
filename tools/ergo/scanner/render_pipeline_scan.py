"""Render Pipeline scanner — Pictor のソースを走査して、 render pass DAG /
pipeline spec / shader / attachment flow を 1 つの JSON にまとめる。

設計方針 (ハイブリッド):

- **静的部分**: ソース静的解析 で fast に取れる情報 (シェーダ一覧、
  pipeline 生成箇所、 blend / cull / depth の設定)
- **手で曲げる部分**: render pass の **依存関係 (DAG)** は Pictor の
  ヘッドのコメントで意図が記述されていることが多く、 機械抽出より
  「Pictor 設計者が宣言した結合」 を信じる方が安定する。 そのため
  pass DAG の構造は `render_pipeline_config.yaml` で人手宣言し、
  scanner はそれを読んで JSON に embed する
- **将来 (Phase 2)**: 実行時 GPU timestamp を WS で受け取って各 pass
  に重ねる。 そのフックも JSON に予約フィールドだけ作っておく

出力:
  scanner/render_pipeline.json   ← UI が読む単一スナップショット

CLI:
  python render_pipeline_scan.py [--pictor <path>] [--ergo <path>]
                                  [--out scanner/render_pipeline.json]
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import sys
from pathlib import Path

# script は ergo/tools/ergo/scanner/ にあるので、 兄弟 Pictor は parents[4]/Pictor。
DEFAULT_PICTOR = Path(__file__).resolve().parents[4] / "Pictor"
DEFAULT_ERGO   = Path(__file__).resolve().parents[3]

# ──────────────────────────────────────────────────────────────────────
# Shader walker
# ──────────────────────────────────────────────────────────────────────

SHADER_EXTS = (".vert", ".frag", ".comp", ".tesc", ".tese", ".geom")


def stage_of(p: Path) -> str:
    e = p.suffix.lower()
    return {
        ".vert": "vertex",
        ".frag": "fragment",
        ".comp": "compute",
        ".tesc": "tess_control",
        ".tese": "tess_eval",
        ".geom": "geometry",
    }.get(e, "unknown")


def walk_shaders(root: Path, label: str) -> list[dict]:
    out: list[dict] = []
    if not root.exists():
        return out
    for p in sorted(root.rglob("*")):
        if p.is_file() and p.suffix.lower() in SHADER_EXTS:
            try:
                src = p.read_text(encoding="utf-8", errors="replace")
            except Exception:
                src = ""
            out.append({
                "id":       f"{label}:{p.relative_to(root).as_posix()}",
                "name":     p.name,
                "path":     p.as_posix(),
                "label":    label,
                "rel_path": p.relative_to(root).as_posix(),
                "stage":    stage_of(p),
                "size":     p.stat().st_size,
                "lines":    src.count("\n") + (0 if not src or src.endswith("\n") else 1),
                "source":   src,
                "summary":  shader_summary(src),
            })
    return out


# very lightweight GLSL parser for the "ins / outs / uniforms" pane
_RE_LAYOUT = re.compile(
    r"layout\s*\(([^)]*)\)\s*(in|out|uniform|buffer|push_constant)?\s*([^\s;]+)?\s*([A-Za-z_][\w]*)?",
    re.MULTILINE,
)
_RE_QUALIFIER_LINE = re.compile(
    r"^\s*(in|out|uniform|buffer|push_constant|readonly|writeonly|coherent)\s+(.+?);\s*$",
    re.MULTILINE,
)


def shader_summary(src: str) -> dict:
    if not src:
        return {"version": "", "ins": [], "outs": [], "uniforms": [], "buffers": []}
    version = ""
    m = re.search(r"#version\s+(\S+)", src)
    if m:
        version = m.group(1)
    ins, outs, uniforms, buffers = [], [], [], []
    for m in _RE_LAYOUT.finditer(src):
        layout_args, qual, ty, name = m.group(1), m.group(2), m.group(3), m.group(4)
        if not qual:
            continue
        entry = {
            "layout": (layout_args or "").strip(),
            "type":   (ty or "").strip(),
            "name":   (name or "").strip(),
        }
        if   qual == "in":            ins.append(entry)
        elif qual == "out":           outs.append(entry)
        elif qual == "uniform":       uniforms.append(entry)
        elif qual == "buffer":        buffers.append(entry)
        elif qual == "push_constant": uniforms.append({**entry, "push_constant": True})
    return {
        "version":  version,
        "ins":      ins,
        "outs":     outs,
        "uniforms": uniforms,
        "buffers":  buffers,
    }


# ──────────────────────────────────────────────────────────────────────
# C++ pipeline-creation scanner
# ──────────────────────────────────────────────────────────────────────

# pictor の pipeline 生成は概ね VkGraphicsPipelineCreateInfo を 1 つの
# 関数で組み立て、 末尾で vkCreateGraphicsPipelines を呼ぶ。 関数名 →
# 抽出した spec (blend / cull / depth / topology + 使用シェーダ) を返す。

# 関数境界: シンプルに `XxxRenderer::create_pipeline_` などの定義位置から
# 次の `^[A-Za-z].*\{` ヘッダまでをスコープとみなす。
_RE_FUNC_HEADER = re.compile(
    r"^(?:bool|void|VkResult|int|static[^=\n]+)\s+([A-Za-z_][\w:]*::[A-Za-z_][\w]*)\s*\([^)]*\)\s*\{",
    re.MULTILINE,
)

_RE_BLEND_ENABLE       = re.compile(r"\.blendEnable\s*=\s*(VK_TRUE|VK_FALSE|true|false)")
_RE_COLOR_BLEND_OP     = re.compile(r"\.colorBlendOp\s*=\s*(VK_BLEND_OP_[A-Z_]+)")
_RE_SRC_BLEND          = re.compile(r"\.srcColorBlendFactor\s*=\s*(VK_BLEND_FACTOR_[A-Z_]+)")
_RE_DST_BLEND          = re.compile(r"\.dstColorBlendFactor\s*=\s*(VK_BLEND_FACTOR_[A-Z_]+)")
_RE_CULL               = re.compile(r"\.cullMode\s*=\s*(VK_CULL_MODE_[A-Z_]+)")
_RE_FRONT_FACE         = re.compile(r"\.frontFace\s*=\s*(VK_FRONT_FACE_[A-Z_]+)")
_RE_TOPOLOGY           = re.compile(r"\.topology\s*=\s*(VK_PRIMITIVE_TOPOLOGY_[A-Z_]+)")
_RE_POLYGON_MODE       = re.compile(r"\.polygonMode\s*=\s*(VK_POLYGON_MODE_[A-Z_]+)")
_RE_DEPTH_TEST_ENABLE  = re.compile(r"\.depthTestEnable\s*=\s*(VK_TRUE|VK_FALSE|true|false)")
_RE_DEPTH_WRITE_ENABLE = re.compile(r"\.depthWriteEnable\s*=\s*(VK_TRUE|VK_FALSE|true|false)")
_RE_DEPTH_COMPARE_OP   = re.compile(r"\.depthCompareOp\s*=\s*(VK_COMPARE_OP_[A-Z_]+)")
_RE_MSAA_SAMPLES       = re.compile(r"\.rasterizationSamples\s*=\s*(VK_SAMPLE_COUNT_[A-Z_0-9]+)")
_RE_SHADER_PATH        = re.compile(r"\"([^\"]+\.(?:vert|frag|comp|tesc|tese|geom))(?:\.spv)?\"")
_RE_VK_CREATE_GP       = re.compile(r"vkCreateGraphicsPipelines\s*\(")


def _norm_bool(v: str | None, default: bool = False) -> bool:
    if not v: return default
    return v in ("VK_TRUE", "true")


def scan_cpp_pipelines(roots: list[Path]) -> list[dict]:
    out: list[dict] = []
    for root in roots:
        for p in sorted(root.rglob("*.cpp")):
            try:
                src = p.read_text(encoding="utf-8", errors="replace")
            except Exception:
                continue
            if "vkCreateGraphicsPipelines" not in src:
                continue
            # 関数毎にスライスする (簡易: TopLevel 関数だけ拾えれば十分)
            func_matches = list(_RE_FUNC_HEADER.finditer(src))
            for i, fm in enumerate(func_matches):
                start = fm.end()
                end   = func_matches[i + 1].start() if i + 1 < len(func_matches) else len(src)
                body  = src[start:end]
                if "vkCreateGraphicsPipelines" not in body:
                    continue
                fname = fm.group(1)
                blend_enable     = _norm_bool((_RE_BLEND_ENABLE.search(body) or (None,)).group(1) if _RE_BLEND_ENABLE.search(body) else None)
                color_blend_op   = (_RE_COLOR_BLEND_OP.search(body) or (None, "")).group(1) if _RE_COLOR_BLEND_OP.search(body) else ""
                src_factor       = (_RE_SRC_BLEND.search(body) or (None, "")).group(1) if _RE_SRC_BLEND.search(body) else ""
                dst_factor       = (_RE_DST_BLEND.search(body) or (None, "")).group(1) if _RE_DST_BLEND.search(body) else ""
                cull             = (_RE_CULL.search(body) or (None, "")).group(1) if _RE_CULL.search(body) else ""
                front            = (_RE_FRONT_FACE.search(body) or (None, "")).group(1) if _RE_FRONT_FACE.search(body) else ""
                topology         = (_RE_TOPOLOGY.search(body) or (None, "")).group(1) if _RE_TOPOLOGY.search(body) else ""
                polygon          = (_RE_POLYGON_MODE.search(body) or (None, "")).group(1) if _RE_POLYGON_MODE.search(body) else ""
                d_test           = _norm_bool((_RE_DEPTH_TEST_ENABLE.search(body) or (None, "")).group(1) if _RE_DEPTH_TEST_ENABLE.search(body) else None)
                d_write          = _norm_bool((_RE_DEPTH_WRITE_ENABLE.search(body) or (None, "")).group(1) if _RE_DEPTH_WRITE_ENABLE.search(body) else None)
                d_op             = (_RE_DEPTH_COMPARE_OP.search(body) or (None, "")).group(1) if _RE_DEPTH_COMPARE_OP.search(body) else ""
                samples          = (_RE_MSAA_SAMPLES.search(body) or (None, "")).group(1) if _RE_MSAA_SAMPLES.search(body) else ""
                # 同関数中で参照されてる shader path (文字列リテラル)
                shaders_ref = sorted(set(m.group(1) for m in _RE_SHADER_PATH.finditer(body)))
                out.append({
                    "id":       f"{p.stem}::{fname}",
                    "function": fname,
                    "source":   f"{p.as_posix()}:{src[:start].count(chr(10)) + 1}",
                    "blend": {
                        "enabled":     blend_enable,
                        "op":          color_blend_op,
                        "src_factor":  src_factor,
                        "dst_factor":  dst_factor,
                    },
                    "raster": {
                        "cull":         cull,
                        "front_face":   front,
                        "topology":     topology,
                        "polygon_mode": polygon,
                    },
                    "depth": {
                        "test":     d_test,
                        "write":    d_write,
                        "compare":  d_op,
                    },
                    "msaa_samples": samples,
                    "shaders":      shaders_ref,
                })
    return out


# ──────────────────────────────────────────────────────────────────────
# Render pass DAG (人手宣言ベース)
# ──────────────────────────────────────────────────────────────────────

# Pictor の経路はおおまかに以下:
#
#   ┌──────────────────────────┐
#   │ Scene HDR Pass            │
#   │  ├ Stage (cube + ground)  │
#   │  ├ Skinned (FBX)          │
#   │  ├ Particle (GPU)         │
#   │  └ RiveS3Array (enemy)    │
#   │  attachments: hdr_color, depth
#   └──────────┬───────────────┘
#              │ (HDR color + depth read)
#              ▼
#   ┌──────────────────────────┐
#   │ Decal Compose Pass        │
#   │  attachments: hdr_color (R/W), depth (R)
#   └──────────┬───────────────┘
#              ▼
#   ┌──────────────────────────┐
#   │ Post-Process Chain        │
#   │  ├ Bloom Extract / Blur   │
#   │  ├ LUT Color Grade        │
#   │  └ ToneMap + Vignette     │
#   │  attachments: hdr_color → swapchain
#   └──────────┬───────────────┘
#              ▼
#   ┌──────────────────────────┐
#   │ HUD Load Pass             │
#   │  ├ UIRenderer (uidoc)     │
#   │  └ BitmapText             │
#   │  attachments: swapchain (LOAD)
#   └──────────────────────────┘
#
# 各 pass に下流が読む attachment を produces、 上流が出した attachment を
# consumes にぶら下げる。 これで DAG が決まる。

PASS_DAG = [
    {
        "id":          "scene_hdr",
        "label":       "Scene HDR",
        "kind":        "graphics",
        "description": "3D シーン本体。 stage / skinned / particle / rive_s3 (敵共有 Rive) を 1 つの HDR render pass に描く。",
        "consumes":    [],
        "produces":    ["hdr_color", "scene_depth"],
        "draws":       [
            "stage_pipeline", "skinned_pipeline",
            "particle_pipeline", "rive_s3_pipeline",
        ],
    },
    {
        "id":          "decal_compose",
        "label":       "Decal Compose",
        "kind":        "graphics",
        "description": "投影デカール (影 / 着弾痕) を HDR シーンカラーに合成。 深度を read。 scene_hdr の直後 / post_process の直前。",
        "consumes":    ["hdr_color", "scene_depth"],
        "produces":    ["hdr_color"],
        "draws":       ["decal_pipeline"],
    },
    {
        "id":          "postprocess",
        "label":       "Post-Process Chain",
        "kind":        "graphics_chain",
        "description": "Bloom (Extract → Blur → Composite) → ColorGrade(LUT) → ToneMap + Vignette。 HDR → swapchain への変換チェーン。",
        "consumes":    ["hdr_color"],
        "produces":    ["swapchain"],
        "draws":       [
            "bloom_extract_pipeline", "bloom_blur_pipeline",
            "color_grade_pipeline", "tone_mapping_pipeline",
        ],
    },
    {
        "id":          "hud_load",
        "label":       "HUD Load",
        "kind":        "graphics",
        "description": "post-process 後の swapchain image に LOAD で重ねる HUD。 UIRenderer (uidoc 駆動パネル/バー) + BitmapText (テキスト)。",
        "consumes":    ["swapchain"],
        "produces":    ["swapchain"],
        "draws":       ["ui_widget_pipeline", "bitmap_text_pipeline"],
    },
]

ATTACHMENTS = [
    {
        "id":     "hdr_color",
        "label":  "HDR Scene Color",
        "format": "VK_FORMAT_R16G16B16A16_SFLOAT",
        "usage":  "COLOR_ATTACHMENT + SAMPLED + INPUT_ATTACHMENT",
        "owner":  "PostProcessPipeline",
        "note":   "scene_hdr 出力 → decal_compose 入出力 → postprocess 入力。 17 bit 浮動小数で HDR ハイライトを保持。",
    },
    {
        "id":     "scene_depth",
        "label":  "Scene Depth",
        "format": "VK_FORMAT_D32_SFLOAT (typical)",
        "usage":  "DEPTH_STENCIL + SAMPLED",
        "owner":  "PostProcessPipeline",
        "note":   "scene_hdr で書き込み、 decal_compose で read (projected decal 用)。",
    },
    {
        "id":     "swapchain",
        "label":  "Swapchain Image",
        "format": "VK_FORMAT_B8G8R8A8_UNORM / SRGB (display-dependent)",
        "usage":  "COLOR_ATTACHMENT + PRESENT_SRC",
        "owner":  "VulkanContext",
        "note":   "postprocess 終端で書き込み、 hud_load で LOAD して重ねる。 最後に present。",
    },
]


# ──────────────────────────────────────────────────────────────────────
# CLI
# ──────────────────────────────────────────────────────────────────────

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pictor", default=str(DEFAULT_PICTOR),
                    help=f"Pictor リポジトリ root (default: {DEFAULT_PICTOR})")
    ap.add_argument("--ergo",   default=str(DEFAULT_ERGO),
                    help=f"ergo リポジトリ root (default: {DEFAULT_ERGO})")
    ap.add_argument("--out",    default=str(Path(__file__).parent / "render_pipeline.json"))
    args = ap.parse_args(argv)

    pictor = Path(args.pictor)
    ergo   = Path(args.ergo)

    # ── Shaders ──
    shaders: list[dict] = []
    shaders += walk_shaders(pictor / "shaders",      "pictor/shaders")
    shaders += walk_shaders(pictor / "demo",         "pictor/demo")
    shaders += walk_shaders(ergo   / "shaders",      "ergo/shaders")

    # ── Pipelines ──
    pipelines = scan_cpp_pipelines([pictor / "src"])

    snapshot = {
        "scanned_at":  _dt.datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "pictor_root": pictor.as_posix(),
        "ergo_root":   ergo.as_posix(),
        "passes":      PASS_DAG,
        "attachments": ATTACHMENTS,
        "pipelines":   pipelines,
        "shaders":     shaders,
    }

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(snapshot, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"[scan] passes={len(snapshot['passes'])} "
          f"attachments={len(snapshot['attachments'])} "
          f"pipelines={len(snapshot['pipelines'])} "
          f"shaders={len(snapshot['shaders'])} "
          f"→ {out_path.as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
