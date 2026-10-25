# M4 アート方針と原画の採否

画風: **インク色の宇宙・アイボリーの船体・銅の機構を、精密なハード SF 原画と静かな計器表示で統一する。**

## 変更前の観察

旧画面は文字を拡大した星・月・船・航跡だけで、画面内に目盛り・単位・色の凡例が無かった。
旧 C レンダラーが出力した GIF の45番フレームを `images/before.png` に保存した。
元の端末・GIF・CSV/SVG は残し、同じ C 積分の上にブラウザー表示を追加した。
人物・敵は登場しないため、宇宙船の視認性と軌道観測を優先した。

## 制作・採否

2026-10-25、Codex built-in image_gen で2枚生成、2枚採用。既存作品や第三者の画像を参照せず制作した。

| 原画 | 出力・実際の用途 | 目視結果・判断 |
|---|---|---|
| `assets/art/observatory.jpg` | 1600×534 / 94,114 B。ヘッダーの img で直接表示 | 船体・タンク・放熱板を判読でき、文字化け・人物・ロゴなし。地平線は星の写真ではなく架空天体のイラストに見えるため、ARTIST'S IMPRESSION と明示して装飾画として採用 |
| `assets/art/tug-sprite.png` | 384×576 RGBA / 187,118 B。Canvas の drawImage で船の位置・船首方向へ描画 | 上向きの船首と下向きのエンジン、タンクと放熱板の非対称性を目視確認。透明アルファを保持して採用。縮小時の実画面でも船首の向きが読める |

圧縮・縮小だけを行い、原画のデザインは描き換えていない。
色だけに依存せず、月は輪、船は上面図、航跡は実線、参考円は破線、速度は矢印で区別する。
船の大きさと星の記号は可視性のため拡大している。原画は重力・当たり判定・数値に使わない。

## 生成指示（最終版、英語）

### ヘッダー

Use case: stylized-concept. Asset type: decorative panoramic header illustration for an orbital mechanics simulator called Gravity Well. Original hard-science spacecraft art, painterly precision, restrained cinematic concept painting with clear shapes, deep ink blue space, warm ivory hulls, oxidised copper accents and subtle cold cyan engine light. A small asymmetric uncrewed research tug with twin cylindrical fuel tanks, compact docking nose, thin antenna and a long radiator passes above the curved shadowed limb of an amber sun; a tiny gray moon far in the upper distance. Wide 3:1 composition, spaceship in the right half, generous dark negative space left for typeset interface text added separately. The light creates beautiful fine rim edges and readable hull panels; quiet sense of orbital scale. No people, no explosion, no orbital tracks, no diagram, no text, no lettering, no insignias, no logo, no watermark, no recognizable existing spacecraft/IP. This is illustrative decoration; it must not present a scientific plot.

### 船の上面スプライト

Use case: stylized-concept. Asset type: top-down spacecraft sprite for the orbital map of a scientific simulator. One single original uncrewed orbital research tug, straight orthographic overhead view, with its nose pointing exactly toward the TOP of the image, engine bells to the BOTTOM. Long compact central ivory docking nose, a pair of cylindrical cream fuel tanks flanking the spine, small asymmetrical dark radiator fins and copper mechanical hardware, tiny cyan lights. Match a precise painterly hard-science illustration, realistic engineering without borrowing any existing spacecraft design. Entire silhouette fully contained, ample transparent margin on all sides, crisp readable silhouette at small sizes, balanced near the center. No floor, no stars, no planets, no perspective, no shadow outside silhouette, no thrust flame, no people, no labels, no letters, no watermark. Transparent background.

## 科学表示の根拠

[NASA: Chapter 4 — Trajectories](https://science.nasa.gov/learn/basics-of-space-flight/chapter4-1/)
を2026-10-25に確認し、楕円を経由する遷移と目的軌道での速度合わせを表示設計の根拠とした。
文章・画像は転載していない。本作の無次元値と月の円軌道は独自の簡略モデル。
手動モードは月の引力あり、Hohmann 実演は星のみで、モード差を画面内に明示する。

## 検証と限界

実画面は `images/observatory.jpg`・`manual-flight.jpg`・`transfer.jpg`・`circularized.jpg`・`mobile.jpg`。
測定値と検証内容は [validation.md](validation.md)・[validation.json](validation.json)。
アート点は材料の代理指標で、絵の良し悪しや遊びの面白さを測らない。人間の初見試遊・実機は未検証。
