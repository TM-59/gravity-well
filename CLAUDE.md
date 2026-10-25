# Gravity Well

C11 + libm の軌道力学シミュレーター。既存の端末表示・GIF・CSV/SVG と、
同じ C の計算を Emscripten で実行するブラウザー観測画面を持つ。

- 物理の正典は `src/orbit.c`。星 GM=1、月は半径3の円軌道、テスト質量の船。
- 手動航行は星+月、Hohmann デモは星だけ。モードの違いを画面と README に明記する。
- 単位は無次元の L/T。原画を測定図と混同しない。向きと速度ベクトルは別々に表示する。
- JavaScript は描画・入力のみ。積分・物理量・実演のインパルスは C で計算する。
- 保存機能・乱数・サーバー API は無い。第三者画像・音声・フォントは持ち込まない。
- 検証: `make test` / `make bench` / `make plots` / `make web` / `make smoke`。
  Web は Emscripten、撮影は Playwright を使用。Python は 3.14。`make -j` は最大4。
- CI は PR の軽量単体テストだけ。ブラウザー撮影・Web ビルドはローカルで行う。
- 実画面は `docs/images/`。原画は `assets/art/`、採否・生成指示は `docs/art-direction.md`。
- 公開リポジトリ。コミット・PR に生成 AI の署名を入れない。

## M4 の独自原画の例外

Codex built-in image_gen が生成したオリジナルの宇宙船原画2枚を許可する。
既存作品の名称・意匠、第三者画像の持込みは許可しない。
ヘッダーの装飾画と地図上の船のスプライトへ実描画し、科学図の軌道・目盛りは計算から描く。
初期ダウンロードは原画を含め10,000,000 B以下。
