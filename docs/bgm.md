# BGM(M63 #34、親 TM-59/project-context#598)

方針(project-context 20-stacks.md「ゲームの BGM」): 基調はバラード。戦闘の役と物語は別の曲で、場面の切り替えで替える。歌は文字を読まない場面だけ。

## 鳴らす経路

ブラウザー版(C の物理を Emscripten で動かす頁)だけに Web Audio の再生を足した。物理は C のまま、JS は画面から曲を選ぶだけ。
端末版・CSV/SVG/GIF は無音のまま。ラグランジュ点の頁 `lagrange.html` は物語の無い計測の頁なので鳴らさない。
曲は最初の操作の後に `bgm/list.json` と `bgm/<曲>.ogg` を読む(初期ロードに入れない)。読めない・復号できない曲は無音のまま遊びを止めない。

## 場面 → 曲

| 画面 | 曲 | 強さ |
|---|---|---|
| 入口・手動航行・Hohmann の実演 | orbit(基調のバラード) | 1 |
| The Ice Run の区間を飛ぶ間 | voyage(戦闘の役、pulse) | 1(区間の一時停止は 0.4) |
| 航行の場面(dock・route・beacon) | story | 0.7 |
| 重い場面(flare・ilse・silence) | story | 0.4 |
| Lamp Quay の場面(結末、quay)と着いた後 | quay | 1 |

戦う相手・主戦は無い。燃料と時間で輪に乗せる区間を戦闘の役にした。手動航行と実演は押すまで止まっているのが普通なので一時停止で下げない。場面は選択を押すまで止まる(時間で流れない)。判定は `web/bgm.js` の `cue()`(Ice Run か・場面・重さ・一時停止・着いたかだけを読む)。

消音の鍵は **N**(M は Ice Run の match の噴射に使われている)。

- 切り替えは 2.5 秒の交差フェード。一時停止は 0.4 倍。
- **全部器楽**: どの場面も文字か計器を読むので、歌入りは作らない。
- 消音: 見出しの ♪(44px)と `M`。保存はしない(この repo は保存機能を持たない)。消音は出口(master)でも切るので、交差フェードで消えていく前の曲も同時に黙る(TM-59/project-context#668 の穴を最初から作らない)。

## 曲(採用・門の値)

| 場面(曲) | 鳴る画面 | 門 | 採用 | 通過 | ループ | LUFS / 山 | 継ぎ目の音色 | 落ち込み / 静かな窓 | 大きさ |
|---|---|---|---|---|---|---|---|---|---|
| orbit | 基調(入口・手動航行・Hohmann の実演) | soft | orbit-c1-s921 | 10/12 | 30.0 秒 | -16.0 / -1.6 dBFS | 0.32 | 0.5 秒 / 0.033 | 698,820 B |
| voyage | 戦闘の役(The Ice Run の 5 区間を燃料と時間で飛ぶ間) | pulse 55.0 BPM(計画 110) | voyage-c2-s924 | 8/24 | 34.839 秒 | -16.0 / -1.7 dBFS | 1.19 | 0.0 秒 / 0.0 | 889,074 B |
| story | 物語(The Ice Run の場面。重い場面 flare・ilse・silence と一時停止は 0.4 倍) | soft | story-c1-s925 | 8/12 | 20.69 秒 | -16.0 / -1.6 dBFS | 0.4 | 0.5 秒 / 0.049 | 475,914 B |
| quay | 結末(Lamp Quay に着いた後の場面) | soft | quay-c0-s925 | 7/12 | 19.355 秒 | -16.0 / -2.9 dBFS | 0.53 | 0.0 秒 / 0.0 | 458,360 B |

計 2,522,168 B(4 曲)。通過 33/60。全候補の値は `docs/bgm/gate.md`、生の値は `docs/bgm/report.json`。

## 作り方

1. 計画: `docs/bgm/plan.json`(場面 × caption 2 × seed 6、seed 921〜926。他の作品と同じ caption・seed を使わない)。caption は音の特徴だけで、曲名・作者・作品名を書かない。
2. 生成: ACE-Step 1.5 turbo(MIT、steps 8)を Colab の L4 で(2 回の確保: 48 本で計算枠 587.5 → 587.39(端末からの 1 回 286.7 秒)、voyage の足し 12 本で 587.39 → 587.39(0.01 未満、205.0 秒))。
   `cd ~/dev/local-llm-ops && .venv/bin/python -m tools.colab bgm --repo <この repo> --out <dir> --backend colab --gpu L4`
   Colab が使えない時の落ちる先は `scripts/bgm_gen.py`(手元の CPU、local-llm-ops の music venv)。
3. 門と書き出し: `~/.local/share/local-llm-ops/genvenv/music/bin/python scripts/bgm_bake.py --takes <dir>`(ffmpeg が要る)。
   門は spot-gap #33・td-games #38 と同じ値で緩めていない: soft(バラード・物語・結末)は local-llm-ops `tools/genmodels/mood.py` の soft の門、pulse(戦闘の役)は PULSE_GATE + テンポ ±5%、全部に -16 LUFS ±1.5・山 -1 dBFS・継ぎ目の音色の跳び 2 倍以下・12 dB の落ち込み 2 秒以下、pulse は静かな窓の割合 0.10 以下、soft のループは 16 秒以上。
   この repo の scripts/ は Ruff ALL・mypy strict の対象なので、spot-gap の `scripts/bgm_bake.py` を同じ門のまま書き直した(soundfile・matplotlib を使わず ffmpeg で読み書き・作図)。門の判定が元の script と一致することは current-swimmer #34 で 48 本を照合して確かめた(同じ script)。

**voyage の作り直し**: 最初の c0・c1 の 12 本は門の通過が 2 本だけで、採れた候補(voyage-c0-s925)は 1 周 35 秒の前半約 16 秒が薄かった(打楽器と低い帯が無い。0.5 秒窓の音量の 25 百分位 -24.3 dB と中央値 -17.9 dB の差は 12 dB に届かず、落ち込み・静かな窓の門は通る)。戦闘の役は聞いてすぐ戦闘と分かる必要があるので門は緩めずに、頭から終わりまで拍が続くよう指示した c2・c3 を同じ seed で 12 本足した(Colab の 2 回目の確保)。24 本の通過 8、採用 voyage-c2-s924 は全体に拍が続く(図)。
4. 出所と利用条件: 曲は ACE-Step 1.5(MIT、出力に第三者の権利の主張なし)で手元の計画から生成した独自の音源。第三者の音源・歌詞・旋律は使っていない。外部の生成 AI・API key は使っていない。

## 波形とスペクトル(自分で見た所見)

画像は 2 周つないだ物で、真ん中が継ぎ目(上が波形、下が対数の周波数のスペクトル)。

- `orbit`: 細かい鍵盤の粒が全体に続き、和音の帯が切れない。継ぎ目は音色の跳び 0.32 で段差は見えない。
- `voyage`: c2 の候補。頭から終わりまで等間隔の拍と低い帯が続き、薄い区間が無い(落ち込み 0 秒・静かな窓 0)。継ぎ目の音色の跳び 1.19(門 2 の内)。
- `story`: 句ごとに鍵盤の音がまとまって鳴り、句の間は静か(最長 0.5 秒)。ループの終わりに高い帯の残響が伸びて継ぎ目へつながる。
- `quay`: 旋律の帯がはっきり見え、句の間に短い静けさ。山 -2.9 dBFS。継ぎ目の段差は見えない(0.53)。

人の試聴は門にしない(客観の門を通した物だけを採る)。

## 試験

- `node scripts/bgm-smoke.cjs`(ローカル専用、port `GW_BGM_PORT` 既定 22082、記録 `docs/bgm/smoke.json`):
- nothing fetched before the first press
- orbit plays(出口 -18 dBFS)
- story plays(出口 -18.9 dBFS)
- mute during a crossfade silences the fading track too(出口 -120 dBFS)
- voyage plays(出口 -22.7 dBFS)
- pause ducks(出口 -22.7 → -40.4 dBFS)
- ♪ mutes and restores(出口 -120 → -40.3 dBFS)
- cues: heavy scenes story 0.4x, Lamp Quay and after it quay, flight and demo orbit
- dist bytes
- dist 2,933,329 B(10 MB の門の内。BGM 4 曲 2,522,168 B を含む)。
- 既存の `make test`(軌道・保存則・GIF・物語・CR3BP、`story_bench --check` 8 行一致)、`node scripts/smoke.cjs`(36 項目、Ice Run を実キーで 2 回)と `node scripts/lagrange-smoke.cjs` は変わらず通る(Chromium の共有ライブラリは `LD_LIBRARY_PATH` で渡す)。
- 遊べる門(`bin/playability-gate.py`、main と branch を同じ時間帯 2026-11-27 09:13): main 7/7・branch 7/7(②タップで進行 p 2 / l 1.6 秒 → 2.2 / 1.7 秒)。
