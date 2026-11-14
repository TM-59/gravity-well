# Gravity Well

C11 + libm の軌道力学シミュレーター。既存の端末表示・GIF・CSV/SVG と、
同じ C の計算を Emscripten で実行するブラウザー観測画面を持つ。

- 物理の正典は `src/orbit.c`。星 GM=1、月は半径3の円軌道、テスト質量の船。
- 手動航行は星+月、Hohmann デモと The Ice Run は星だけ(月は位置の参考)。モードの違いを画面と README に明記する。
- The Ice Run(物語の航行)は `src/voyage.c`。物語の場面は区間の始まり・終わりで開き、燃料・目標輪・表示の揺れ・救援の扱いだけを変える。
  台詞は `web/app.js`。ビート表は `docs/narrative/beats.json`、bench の出力(play-order・ablation・tension)は `make story` が書く(手で書かない)。
  `make test` は bench の行をバイト一致で照合する。ローカルは Docker を使わず、互換コンパイラで生成して全行を照合する。既存 CI の処理系は Debian 13。
- M2 の回転座標系は `src/cr3bp.c` の独立した重心モデル。距離=主星間隔1・全GM=1・角速度1、既定の質量比 mu=1/101(観測画面は `lab_set_mu` で 0.001〜0.5)、軟化なし。
  コリオリ項は速度依存なので RK4(dt=0.0025)を使う。旧 `src/orbit.c` の固定星モデルを置換しない。
  `src/lagrange.c` の端末 `--lagrange <dir>` と `web/lagrange.html`(WASM)が同じC計算を使い、`make lagrange-plots` がCSV/SVGを再生成する。
  線形安定性(`cr3bp_modes`・Routh の限界)と慣性系への変換(`cr3bp_to_inertial`)も C。画面の慣性系表示は描画の回転だけを JS で行い、船の位置は C の値。
  近接/保存誤差1e-6超のステップは受理せず停止。比較用ゼロ速度曲線のCと実軌道のCを画面で区別する。
- 単位は無次元の L/T。原画を測定図と混同しない。向きと速度ベクトルは別々に表示する。
- JavaScript は描画・入力のみ。積分・物理量・実演のインパルスは C で計算する。
- 保存機能・サーバー API は無い。第三者画像・音声・フォントは持ち込まない。
- 検証: `make test` / `make bench` / `make story-check` / `make plots` / `make web` / `make smoke`。
  Web は Emscripten、撮影は Playwright を使用。Python は 3.14。`make -j` は最大4。
- 理論の手引きは `docs/theory/`。`make theory-figs` が図と実測値の表を `tests/theory_figs.c` と `./gravity-well --lagrange`(第5頁)から再生成し、`make theory-check` が
  対応表(文 → 頁 → 図 → 関数 → テスト)の欠け・リンク切れ・消えた関数名・未掲載の公開関数で落ちる。法則・規則を足したら同じ PR で頁と図と表を足す。
  README の「34 statements」は表の行数と合わせる。
- CI は PR の軽量単体テストだけ(`docker build .` の中の `make test` と `make plots`)。ブラウザー撮影・Web ビルドはローカルで行う。
- ゲーム本体に乱数は無い。bench の自動操縦の種は `tests/voyage_policy.h` の subSeed(splitmix64)で作る。
- 実画面は `docs/images/`。原画は `assets/art/`、採否・生成指示は `docs/art-direction.md`。
- 公開リポジトリ。コミット・PR に生成 AI の署名を入れない。

## M4 の独自原画の例外

Codex built-in image_gen が生成したオリジナルの宇宙船原画2枚を許可する。
既存作品の名称・意匠、第三者画像の持込みは許可しない。
ヘッダーの装飾画と地図上の船のスプライトへ実描画し、科学図の軌道・目盛りは計算から描く。
初期ダウンロードは原画を含め10,000,000 B以下。
