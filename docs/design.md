# 設計

今のコードから起こした構成。JavaScript は描画と入力だけで、積分・物理量・物語の数値は C が持つ。

## ファイルと責務

| ファイル | 責務 |
|---|---|
| `src/orbit.c` / `orbit.h` | 物理の正典。星 GM=1、半径3の円軌道の月、テスト質量の船の積分 |
| `src/flight.c` / `flight.h` | ブラウザーの手動航行(mode 0、星+月)と Hohmann 実演(mode 1、星のみ) |
| `src/voyage.c` / `voyage.h` | The Ice Run。5区間の遷移、場面の発火、燃料・目標輪・表示の揺れ・救援の扱い |
| `src/voyage_web.c` | WASM の窓口 `vw_reset` / `vw_tick` / `vw_answer` / `vw_value`(物語の効果は `VE_ALL` で全部有効) |
| `src/main.c`・`render.c`・`svg.c`・`gif.c` | 端末表示、CSV/SVG、GIF 出力 |
| `web/index.html`・`style.css`・`app.js` | 観測画面の配置、台詞(`SCENES`)、描画と入力 |
| `tests/` | 単体テスト、物語の bench(`story_bench.c`)、人に近い自動操縦(`voyage_policy.h`) |
| `scripts/smoke.cjs` | 実 WASM をキー・クリック・タッチで動かす検査と実画面の撮影 |

## 画面の状態

- 観測画面の上の案内(`#welcome`)は読み込み直後だけ出る。いずれかの航行を選ぶか Resume で消える。
- モード: `reset(0)` 手動航行、`reset(1)` Hohmann 実演、`reset(2)` The Ice Run(`body[data-mode]` が `flight` / `voyage`)。
- The Ice Run の場面は C の `VS_DOCK`〜`VS_QUAY`(0〜6)。場面が開くと飛行は止まり、`#scene` に台詞と選択肢を出す。
  重い場面(flare・ilse・silence)は暗い面で、区間完了の帯を出さない。選択肢は場面の下端に固定する。
- 帯(`#banner`)は C の `VB_CLEAR`・`VB_TOW`・`VB_MISS`。場面が出ている間は隠す。

## データ

保存形式は無い(localStorage も使わない)。画面の値は C の `enum VoyageValue` / `enum FlightValue` の順に読む。

## 入力

| 操作 | キー | タッチ・マウス |
|---|---|---|
| 手動航行 回転・噴射 | A・D・W(矢印キーも可) | A・W THRUST・D のボタンを押し続ける |
| Ice Run Raise / Lower / Match | W・S・M か Space | RAISE・LOWER・MATCH のボタンを押し続ける |
| 一時停止 | P | Pause / Resume |
| 場面の選択 | ― | 選択肢のボタン |

指を離す・タッチが取り消される・窓がフォーカスを失うと、押している操作はすべて解除される。

## 配置

- 960 px 以上: 装飾画、観測画面と右のデータ欄、下に操作。
- 縦持ち(幅 720 px 以下): 見出し、観測画面(画面の高さの 60%)、操作、データ、装飾画の順。
- 横持ち(高さ 520 px 以下): 観測画面を左に画面の高さいっぱい、操作とデータを右の 260 px に。
  航行を選ぶと観測画面の上端まで自動でスクロールする。
- 文字は 14 CSS px 以上、押せる物は 44×44 CSS px 以上(canvas の目盛り・ラベルも 14 px)。

## test hook

- `window.orbitView.snapshot`: smoke 用の読み取り専用の状態。
- `window.__tmGate`(遊べる門): `logical` は canvas の CSS 寸法、`scene()`(`title`・`scene:<id>`・`play`・`paused`・`done`)、
  `goal()`(The Ice Run の最初の会話を閉じて出港した)、`objective()`、`hint()`(次に押す場所: 案内のボタン・場面の最初の選択肢・Resume)。
  `texts`・`targets` は空(DOM と canvas の文字は門が直接測る)、`player()` は null(上下左右へ動かす操作が無い)。

## 既知の限界

- 船・月・星のラベルは位置が近いと重なる。
- 実スマートフォン(Safari)・実 GPU・人間の初見試遊は未検証。
