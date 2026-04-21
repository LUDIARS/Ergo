# placer — block-based level designer

Ergo の統一開発者ツール (`tools/ergo`) にホストされるプラグインの
1 つ。URL は `http://localhost:5170/placer/`。

AdventureCube 等の個別ゲームにリンクされず、**Ergo アプリの 1 モード**
として独立ビルド・独立デプロイされる。データファイルだけが利用側と
やりとりする唯一の接点。

## 目的

ブロック単位でレベル (ステージ) をデザインする。各ブロックは
**3×10 または 5×10 のグリッド**で、セル毎に出現オブジェクト
(敵・スキル・マーカー等) を配置する。複数のブロックを並べて
1 ステージを構成する。

## データモデル

ブロックとステージは単一 JSON `placer-data.json` に保存される。
場所は `ERGO_PLACER_FILE` 環境変数、未指定なら ergo サーバ cwd 直下。

```jsonc
{
  "version": 1,
  "blocks": [
    {
      "id":       "block_01",
      "name":     "導入",
      "rows":     3,           // 3 または 5
      "cols":     10,          // 固定
      "grid":     [            // rows × cols の 2 次元配列
        [ { "objects": [] }, { "objects": [] }, ... ],
        ...
      ],
      "appears":  { "kind": "start" },
      "contents": "enemies",
      "special":  null         // TBD (任意 JSON)
    },
    {
      "id":   "block_02",
      "rows": 5,
      ...
      "appears": { "kind": "time", "value": 30 }
    }
  ],
  "stages": [
    { "id": "stage_01", "name": "World 1-1", "blocks": ["block_01", "block_02", "block_01"] }
  ]
}
```

### 各セル (`Cell.objects[]`)

```jsonc
{
  "objects": [
    { "type": "enemy", "label": "melee-guard" },
    { "type": "skill", "label": "decoy N=2",  "params": { "effect_id": "decoy_on_hit", "number": 2 } }
  ]
}
```

- `objects` が 2 件以上 = **組み合わせ出現** (UI は `combo ×N` バッジ付きで
  ラベル列挙)。
- `type` / `label` はフリー文字列。ゲームごとに `enemy|skill|item|marker`
  などの規約を決めれば、UI は `.cell .obj.type-enemy` 等の色クラスで
  視覚分類する。
- `params` はゲーム側が解釈する任意オブジェクト。

### ブロックの `appears` (いつ出現)

| kind     | value                      | 意味                                |
|----------|----------------------------|-------------------------------------|
| `start`  | –                          | ステージ開始と同時                 |
| `time`   | 秒数 (number)              | ステージ開始から N 秒後            |
| `after`  | 前提ブロック id (string)   | 指定ブロックをクリア後             |
| `score`  | スコア閾値 (number)        | プレイヤースコアが N 到達時        |
| `manual` | –                          | ホストが明示的にトリガ             |

### ブロックの `contents` (何が出るか)

フリー文字列。「この block では何が出るか」を人間可読に 1 行で
書く (例: `"enemies"`, `"skill rewards"`, `"mixed"`)。

### ブロックの `special` (TBD)

任意 JSON。UI は textarea で生 JSON を編集。将来、既定スキーマが
決まったら `normaliseBlock` 側で検証を追加する。

## REST API

すべて `/placer/api/*` 下:

| method | path                       | 役割                                  |
|--------|----------------------------|---------------------------------------|
| GET    | `/meta`                    | 許容行数・列数・storePath・version   |
| GET    | `/health`                  | 集計                                  |
| GET    | `/store`                   | 全体エクスポート                      |
| POST   | `/store/import`            | 全置換                                |
| GET    | `/blocks`                  | 全ブロック                            |
| PUT    | `/blocks/:id`              | upsert (id は URL 優先)               |
| DELETE | `/blocks/:id`              | 削除 (参照ステージから自動プルーン)    |
| POST   | `/blocks/:id/resize`       | `rows` を 3↔5 変更 (既存セル保持)     |
| POST   | `/new/block`               | 骨組 Block (未保存)                   |
| GET    | `/stages`                  | 全ステージ                            |
| PUT    | `/stages/:id`              | upsert (存在しない block id は無視)   |
| DELETE | `/stages/:id`              | 削除                                  |
| POST   | `/new/stage`               | 骨組 Stage                            |

WebSocket `/placer/ws` は予約。現状は接続を受け付け、保存時に
`{ "op": "reload" }` をブロードキャストするのみ (UI 間同期用の
将来フック)。

## UI 概要

- ヘッダに **Blocks / Stages** の 2 タブと `storePath` 表示
- Blocks モード
  - 左: ブロック一覧 + 追加ボタン
  - 右: 選択中ブロックのメタデータ (id / 名 / サイズ / contents /
    いつ出現 / 特殊 JSON) + 3×10 or 5×10 のセルグリッド
  - セルクリックで **追加ダイアログ**。`type` と `label` を入力して
    追加 / 削除
  - セル可視化はテキスト (最大 3 件 + `+N` 省略、2 件以上は `combo ×N`
    バッジ)
- Stages モード
  - 左: ステージ一覧 + 追加ボタン
  - 右: ブロック id の並び順を select で選択、↑ / ↓ / 削除
- 全編集は **即座に PUT**。保存ボタンは無い (`storePath` の内容が常に
  真実)。

## FR (機能要件)

- **FR-pl-1**: `BLOCK_ROWS_ALLOWED = [3, 5]`、`BLOCK_COLS = 10`。
- **FR-pl-2**: ブロックのリサイズは既存セルの内容を可能な限り保存。
- **FR-pl-3**: ブロックを削除したとき、参照しているステージ配列から
  自動的に除去される。
- **FR-pl-4**: ステージの `blocks` 配列に未知の id が含まれたら、
  PUT 受け付け時に無視 (サイレント drop、警告は server log)。
- **FR-pl-5**: 各セルは 0 件以上の `PlacedObject` を保持。2 件以上で
  「組み合わせ」扱いし、UI に `combo ×N` を表示する。
- **FR-pl-6**: `appears.kind` は列挙値のいずれか。それ以外は `start`
  にフォールバック。
- **FR-pl-7**: `special` は任意 JSON。parse エラー時は UI が警告。
- **FR-pl-8**: `storePath` は `ERGO_PLACER_FILE` 環境変数で上書き可能。
  存在しないファイルは初回 `load()` で `emptyStore()` を返す (back-compat)。

## AC (受け入れ基準)

- **AC-pl-1**: 3×10 ブロックを作り、各セルに 1 オブジェクト配置、
  再読込しても全セル内容が保持される。
- **AC-pl-2**: 5 行にリサイズ後、既存 3 行分のセル内容が維持される。
- **AC-pl-3**: 同一セルに 2 件オブジェクトを追加すると UI に
  `combo ×2` が表示され、JSON も 2 件保持。
- **AC-pl-4**: ブロック A を削除すると、A を含むステージの `blocks`
  配列から A の id が消える。
- **AC-pl-5**: `appears.kind=time` に変更し `value=30` を入れると
  JSON に `{"kind":"time","value":30}` として保存される。

## Director Decisions

- **D-pl-1**: ブロック行は 3 / 5 のみ (本仕様)。任意行 / 任意列化は
  将来拡張。10 列固定は横スクロールアクション想定。
- **D-pl-2**: オブジェクト `type` は文字列 free-form。厳密カタログ化は
  ゲーム側プラグインが担う (skill-effect-editor 等)。
- **D-pl-3**: `special` は TBD。スキーマが固まるまで raw JSON 編集。
- **D-pl-4**: 保存ボタンは設けない。編集 = PUT、アトミック rename。
- **D-pl-5**: AdventureCube の `stage-*.json` への逆輸出は別仕様。
  現状 placer は **独立したファイル**を持ち、ゲームとは JSON 越しで
  接続する (デカップル方針)。

## レイアウト

```
tools/ergo/src/plugins/placer/
├── index.ts       ← plugin factory, Hono routes, WS stub
├── schema.ts      ← Block / Stage / Cell / PlacedObject types + normalise*
├── store.ts       ← JSON file persistence (atomic write)
└── ui/
    ├── index.html
    ├── style.css
    └── app.js     ← block grid + stage composer + cell dialog
```

## 将来

- 複数ステージのツリー / チャプター
- ブロックのテンプレート / コピー&ペースト / マルチ選択
- `special` のスキーマ確定 + 型付き UI
- ゲームごとの adapter プラグイン (placer-data.json → stage-NN.json
  変換バイナリなど)
- WS 経由のマルチユーザー同時編集 (`op: reload` はそのための土台)
