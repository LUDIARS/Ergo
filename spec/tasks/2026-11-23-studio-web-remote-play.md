---
task: Ergo Studio の限定 Web 公開 (アセット一覧・ライティング調整) と Odeum runner による遠隔プレイ連携
project: Eg
kind: feature
created: 2026-11-23
memory_links: []
---

# Ergo Studio の限定 Web 公開

## 依頼 (neco, 2026-11-23)

- ErgoStudio を限定 Web 公開して、アセットの一覧、ライティング調整が出来るようにする。
- ゲームのビルドと実行と、実行しながらの調整を出来るようにする。
- Amicii をサンプルとして、セットアップ・Web 公開まで。
- 遠隔プレイは Odeum / Custos の WebRTC リモートプレイ実装を使う。

## 完了条件

- [ ] 未マージだった `feat/studio-game-ops` 系列 (ネイティブ全タブ・lookdev・services / tuning / git) を main に合流
- [ ] `web` プラグイン (ブラウザのページ) と `assets` プラグイン (読み取り専用の一覧)
- [ ] 公開名 (`ERGO_STUDIO_PUBLIC_HOSTS`) は Cloudflare Access のヘッダー付き要求だけ通す。listen は loopback のまま
- [ ] git の変更系は公開名から受けない
- [ ] 仕様 `spec/feature/tool/web.md`、カタログ検査 (`test_module_catalog`) と studio のテストが通る
- [ ] Amicii 側: プロフィールの `assets`、runner の `apps.json`、Studio 用の起動設定 (Amicii の別 PR)
- [ ] 本社で Studio と odeum-runner を Excubitor から起動し、Access 付きで公開して表示を確かめる
