# Gravity Well

C11 + libm の軌道力学シミュレーター。既存の端末表示・GIF・CSV/SVG と、
同じ C の計算を Emscripten で実行するブラウザー観測画面を持つ。

- 物理の正典は `src/orbit.c`。星 GM=1、月は半径3の円軌道、テスト質量の船。
- 手動航行は星+月、Hohmann デモと The Ice Run は星だけ(月は位置の参考)。モードの違いを画面と README に明記する。
- The Ice Run(物語の航行)は `src/voyage.c`。物語の場面は区間の始まり・終わりで開き、燃料・目標輪・表示の揺れ・救援の扱いだけを変える。
  台詞は `web/app.js`。ビート表は `docs/narrative/beats.json`、bench の出力(play-order・ablation・tension)は `make story` が書く(手で書かない)。
  `make test` は bench の行をバイト一致で照合するので、生成・確認は Dockerfile の Debian 13 で行う(CI も `docker build .`)。
- 単位は無次元の L/T。原画を測定図と混同しない。向きと速度ベクトルは別々に表示する。
- JavaScript は描画・入力のみ。積分・物理量・実演のインパルスは C で計算する。
- 保存機能・サーバー API は無い。第三者画像・音声・フォントは持ち込まない。
- 検証: `make test` / `make bench` / `make story-check` / `make plots` / `make web` / `make smoke`。
  Web は Emscripten、撮影は Playwright を使用。Python は 3.14。`make -j` は最大4。
- CI は PR の軽量単体テストだけ(`docker build .` の中の `make test` と `make plots`)。ブラウザー撮影・Web ビルドはローカルで行う。
- ゲーム本体に乱数は無い。bench の自動操縦の種は `tests/voyage_policy.h` の subSeed(splitmix64)で作る。
- 実画面は `docs/images/`。原画は `assets/art/`、採否・生成指示は `docs/art-direction.md`。
- 公開リポジトリ。コミット・PR に生成 AI の署名を入れない。

## M4 の独自原画の例外

Codex built-in image_gen が生成したオリジナルの宇宙船原画2枚を許可する。
既存作品の名称・意匠、第三者画像の持込みは許可しない。
ヘッダーの装飾画と地図上の船のスプライトへ実描画し、科学図の軌道・目盛りは計算から描く。
初期ダウンロードは原画を含め10,000,000 B以下。
