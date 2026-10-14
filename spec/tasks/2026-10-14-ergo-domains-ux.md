# 2026-10-14 Ergo の Praeforma 登録と Anatomia ドメインの切り直し

- 起点: Actio `actio:32bd1e63-95b1-42d1-a91b-e8f8d84181cc`
- 設計: [spec/architecture/domains-and-ux.md](../architecture/domains-and-ux.md) (AI 案・人の承認前)
- ブランチ: `docs/ergo-domains-ux`

| # | 作業 | 成果物 | 状態 |
| --- | --- | --- | --- |
| 1 | モジュール・ツール・CMake・spec の棚卸しと、古い処理の候補の洗い出し | 設計文書 §5 | 済 |
| 2 | UX・コア / ビジネスドメイン・Anatomia ドメインの設計 | 設計文書 §1〜§4・§6 | 済 |
| 3 | `spec/domains/*.domain.json` の 16 本への切り直しと `.anatomia/layers.json` の不足分の追加 (`anatomia domains program --repo .` で unclassified 0) | `spec/domains/`・`.anatomia/layers.json`・`spec/domains/README.md`・`spec/module/*.md` の所属ドメイン | 済 |
| 4 | Praeforma への登録 (UX・ターゲット・ジャーニー・制約・シナリオとユースケース・コア / ビジネスと所属) | Pf プロジェクト (リポ外) | 済 |
| 5 | ビジネスドメインの `anatomiaDomain` の対応付け | Pf プロジェクト (リポ外) | 本 PR が main に入り、Anatomia に新しいドメインが出てから |

コードの移動・削除はしない。古い処理をどうするかは人が決める (設計文書 §5・§6)。
