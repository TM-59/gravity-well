# M2 verification — 2026-11-14

[Issue #2](https://github.com/TM-59/gravity-well/issues/2): rotating-frame L1–L5, Jacobi conservation within 1e−6, zero-velocity curves, shared C/WASM display, unchanged legacy outputs.

| Command | Observed result |
|---|---|
| `make test` | Existing orbit/GIF/flight/story harnesses pass. New CR3BP harness: seven test groups, 74,405 internal checks, zero failures; checks are not individual testcases. |
| `make bench` | Hohmann Δv1 = 0.154706; final radius = 2.000127, unchanged. |
| `make story-check` | All 396 runs reproduce the three narrative JSON files byte for byte. |
| `make plots` | Existing CSV/SVG/GIF byte for byte unchanged. |
| `make theory-figs` | Existing figures and values unchanged; also generates eight new SVG and three CSV files for page 5. |
| `make theory-check` | 34 statements, each illustrated; 27 generated figures, 93 function/test/value references. |
| `make lagrange-plots` | Default mass fraction 1/101; 1,501 trajectory samples, five equilibrium rows; repeated generation has identical bytes. |
| `make web` | Both WASM modules and browser pages build; total distribution remains below the 10,000,000 B gate. |
| `make smoke` | Existing 36 checks and new 2,208 checks pass; zero browser errors, zero external requests. |

The C test uses **six mass fractions** (1e−6, 0.001, 1/101, 0.1, 0.4, 0.5). Three collinear points are compared to an independent long-double Newton solver: maximum position difference 2.22e−16 L. All 30 equilibrium acceleration residuals are below 1.57e−15 L/T². L4/L5 are checked against equilateral geometry and their exact C formula.

For three trajectories, each integrated 60 T at dt=0.0025 T, the maximum Jacobi error across every step is 9.35699731031e−8 L²/T². A separate three-resolution experiment gives the RK4 position-difference ratio 17.3974 (asymptotic ratio 17). No energy projection or state correction is used.

The L1-closed/open levels have 1,100/1,096 contour segments. Bisection-refined endpoint residuals are at most 7.55e−15. This checks the endpoint values; it does not prove that a finite grid resolves every feature of the exact level set.

The local browser harness compares **100×21 native/WASM state values**, with maximum scaled difference 1.44e−15. It then exercises real mouse navigation at 1440×1000 and 1024×768 and real touch taps at 390×844 and 844×390: entry/return links, L4 start, L1 passage, pause, reset, comparison levels, focus loss, the rotating/inertial view switch, and the mass-fraction slider with real keys (End = 0.5, unstable L4 leaves by more than 0.3 L; Home = 0.001, stable). No page scrolls horizontally at any of the four sizes. Every visible text size, including canvas labels, is at least 14 CSS px; every link/button/select/slider is at least 44×44 CSS px. Point tables fit without clipping. Missing WASM produces a recovery message and disabled controls.

Stability and frames (added 2026-11-14, all from `./test_cr3bp`):

| Check | Theory | Measured |
|---|---|---|
| Rotating ⇄ inertial round trip, 4,001 states | identity | 8.88e−16 |
| C + 2(E − h) in the inertial frame | 0 | 5.33e−15 |
| Independent inertial RK4 (no fictitious forces), 20 T | same trajectory | 1.29e−8 L |
| L1–L3 growth rate, 15 points | eigenvalue | max relative difference 1.85e−5 |
| L4 short / long libration period, mass fraction 1/101 | 6.519728535 / 23.539634264 T | 6.519731926 / 23.539610506 T |
| Routh's critical mass fraction | 0.0385208965 | bisection of the growth rate within 1e−12 |
| L4 nudged 1e−8 L, 70 T, mass fraction 0.03 / 0.05 | bounded / rate 0.181986 per T | max 1.92e−7 L / reaches 0.0177 L, rate 0.182532 |
| Hill radius at 1/101 | 0.148886 L (approximation) | L1 0.141475 L (−4.98 %), L2 0.156221 L (+4.93 %) |

Actual screenshots:

- [Desktop](images/lagrange-1440x1000.png)
- [Phone portrait](images/lagrange-390x844.png)
- [Phone landscape](images/lagrange-844x390.png)
- [Desktop 1024 wide](images/lagrange-1024x768.png)
- [Inertial view](images/lagrange-inertial-1440x1000.png)

Local environment: Linux x86_64, GCC 14, Python 3.14.4, Node 24 and Playwright 1.63.0/Chromium headless. Local builds use no containers. Existing CI is retained. The original physics/story sources and 48 pre-existing generated/documentation artifacts outside the updated handbook index, requirements and design are unchanged. Reproducing old browser screenshots writes them anew; this verification retains their original tracked bytes and stores the new smoke measurements separately.

Unverified: physical phones/Safari, other browser engines, human impressions, long-time trajectories beyond the 60 T observation window, and accuracy during close approaches rejected by the lab. No deployment, submission, new assets, runtime dependencies or persistent storage.
