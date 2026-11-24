# Ergo Studio: プロジェクトの様式判定と Gen5 データの生成 (インテーク)

依頼 (2026-11-23, neco): 「ErgoStudio はプロジェクトのソリューションそのものは持たず、そのプロジェクトの様式
(Unity か C++/LUDIARS (Pictor) かそれ以外か) を最初にチェックし、Gen5 の AI 開発ワークフローに必要なデータを全て生成する」
「終わったら起動して見せて」。

関連: [ランチャーとリモート開発サーバ](2026-11-23-studio-launcher-and-devserver.md)、Gen5 の全体像は Castra
`spec/plan/2026-11-21-gen5-autonomous-loop-design-directive.md` (AI 試遊 → Discutere 議論 → 改善提案 → 実装・Revisor 自動マージ → 再試遊)。

## 原則

- Studio はプロジェクトのビルド一式 (sln / CMake / Unity プロジェクト) を持たない・書き換えない。ビルド・起動の手順は
  プロジェクト側の定義 (プロフィールの `play`、Excubitor の catalog) を呼ぶだけ。
- Studio が作るのは「Gen5 のループがそのプロジェクトを扱うためのデータ」。生成物はプロジェクトのリポジトリに、
  feat ブランチ + Revisor local PR で入れる (main に直接書かない)。

## 1. 様式の判定 (ランチャーに追加した直後に必ず行う)

| 様式 | 判定の根拠 (root 直下から) |
|---|---|
| `unity` | `ProjectSettings/ProjectVersion.txt` と `Assets/` がある (版は ProjectVersion の `m_EditorVersion`) |
| `cpp-ludiars` | `CMakeLists.txt` があり、Pictor / Ergo / Lapilli を参照する (`find_package`・`add_subdirectory`・`FetchContent`・`ERGO_PACKAGES` の文字列、または `lib/lapilli` サブモジュール) |
| `cpp-other` | `CMakeLists.txt` はあるが上の参照が無い |
| `other` | どれにも当たらない (Web / Node など。種類名は `package.json` などから参考表示だけ) |

判定は純粋関数 (`ProjectStyle detect(const FileProbe&)`) にしてテストする。結果はランチャーの行と `recent_projects.json` に
`style` として持ち、プロフィールに明示 (`"style": "unity"`) があればそれを優先する。

## 2. Gen5 に要るデータと生成元

インテークは「足りないものの一覧 (点検)」→「生成」の 2 段。点検は読み取りだけ。生成は項目ごとに確認してから。

| データ | 置き場 (対象リポ) | 生成のしかた | 様式による違い |
|---|---|---|---|
| Studio プロフィール (`*.studio.json`: services / assets / play / generation) | `tools/studio_pack/` | 決定的なひな形 (様式ごとの既定値) | unity: play は Unity の Player ビルド / エディタ起動、cpp-ludiars: CMake ターゲット |
| Excubitor の catalog / bootstrap | `excubitor.catalog.yaml` `excubitor.bootstrap.json` | ひな形 | |
| 審査の設定 (Praesidium `praesidium.json`、Augur `augur.contracts.json`) | リポ直下 | ひな形 + 既存ソースの列挙 (C/C++ の unit) | unity は C# (native 検査の対象外)、cpp は unit 列挙 |
| Revisor 登録 | Revisor | 既存の `ludiars-repo-bootstrap` 手順を呼ぶ (Studio は依頼だけ) | |
| 遠隔操作の口 (画面・音声・キー・状態: Custos/Odeum の `/stream` `/screenshot` `/key` `/state`) | ゲーム側 | unity: パッケージの追加手順、cpp-ludiars: Odeum `odeum_remote_play` の組み込み (Ergo play プラグインと同じ) | 様式で大きく違う。生成は AI 委託 |
| 試遊 AI のデータ (Ingenium: `bundle/manifest.json`・`play-rules/*.json`・`manual.md`) | Ingenium `data/games/<game>/` | AI 委託 (Cc → Astra)。説明書は Praeforma から | |
| 攻略本・プレイデータセット (Commentarii) | Commentarii | AI 委託 | |
| アセット生成の仕様 (モデル / エフェクト) | 対象リポ `spec/assets/` | ひな形 (空の仕様) + プロフィールの `generation` (生成は Figmentum、管理は Clio) | |
| Cc / Conflux / CDGD への登録 (project code・チーム) | Concordia | Cc の API (無いものは Cc に足す) | |

## 3. 画面

ランチャーの行に様式を出す。行の「インテーク」で、上の表を「ある / 足りない / 作れる」で並べ、
「足りない分を作る」で生成を始める (決定的なものは Studio が PR を作り、AI が要るものは Cc に依頼)。

## 4. 段階への組み込み

- 段 1 (ランチャー) に: 様式の判定と表示 (`ProjectStyle`、テスト付き)。
- 新しい段 (段 2 の前後どちらでも可): 点検 (読み取りのみ) → 決定的なひな形の生成 → AI 委託の生成。
- 生成のひな形と点検の項目は、Gen5 側の仕様が変わったら追従する (正本は各サービスの spec)。

## 未確定

- Unity 様式の遠隔操作の口をどのパッケージで出すか (Custos の Unity 版の有無を要確認)。
- Cc に「プロジェクト登録」の API があるか (無ければ Cc に足す)。アセット生成の口は Figmentum、一覧は Clio の側で決める。
