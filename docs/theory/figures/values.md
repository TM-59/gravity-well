# Measured values

Written by `make theory-figs` (tests/theory_figs.c) from the program's own functions. Do not edit by hand.
Units: lengths in L, times in T, with the star's GM = 1. The last column is the relative error; where the theory column is 0 it is the absolute value.

| id | what is measured | measured | theory | error |
|---|---|---|---|---|
| `accel-r1` | size of the star's pull at (0.6, 0.8), r = 1 (theory GM / r^2) | 0.999999 | 1 | 1.50e-06 |
| `accel-direction` | its direction: ax / ay at (0.6, 0.8) (theory x / y, towards the star) | 0.75 | 0.75 | 0.00e+00 |
| `accel-softening-r0.1` | pull at r = 0.1, where the softening 1e-6 matters most before the crash radius (theory GM / r^2) | 99.985 | 100 | 1.50e-04 |
| `period-r1` | time of one turn at r = 1, integrated with dt = 0.004 (theory 2 pi) | 6.28324 | 6.28319 | 8.33e-06 |
| `period-r3` | time of one turn at r = 3, the moon's ring (theory 2 pi 3^1.5) | 32.6484 | 32.6484 | 5.31e-07 |
| `kepler3-exponent` | slope of log T against log r over 25 radii (theory 3/2) | 1.49999 | 1.5 | 9.43e-06 |
| `circular-speed-r2` | circular_speed at r = 2 (theory sqrt(GM / r)) | 0.707107 | 0.707107 | 0.00e+00 |
| `visviva-max-error` | largest relative gap between the integrated speed and sqrt(GM (2/r - 1/a)) over three turns | 7.90787e-06 | 0 | 7.91e-06 |
| `ellipse-a` | semi-major axis from the largest and smallest radius reached (theory 1 / (2/r - v^2/GM) = 1/0.56) | 1.78573 | 1.78571 | 7.25e-06 |
| `energy-drift-ellipse` | largest relative change of the specific energy over three turns | 6.15037e-06 | 0 | 6.15e-06 |
| `ang-momentum-drift` | largest relative change of x vy - y vx over three turns | 1.18794e-14 | 0 | 1.19e-14 |
| `areal-rate-spread` | largest relative gap between the area swept per T in one step and h / 2 | 1.67422e-13 | 0 | 1.67e-13 |
| `ellipse-area` | area swept in three turns over 3 pi a b (Kepler's second law, summed) | 0.999912 | 1 | 8.81e-05 |
| `apo-from-state` | apoapsis from the state at t = 0 (voyage_value VV_APO) against the largest radius reached | 2.57143 | 2.57145 | 1.01e-05 |
| `peri-from-state` | periapsis from the state at t = 0 (VV_PERI) against the smallest radius reached | 1 | 1 | 0.00e+00 |
| `ecc-from-state` | eccentricity from the state (VV_ECC) against (rmax - rmin) / (rmax + rmin) | 0.44 | 0.440004 | 9.22e-06 |
| `sma-from-state` | semi-major axis from the state (VV_SEMI_MAJOR) (theory 1/0.56) | 1.78571 | 1.78571 | 0.00e+00 |
| `peri-angle` | direction of the eccentricity vector (VV_PERI_ANGLE); the ship starts at periapsis on the +x axis | 0 | 0 | 0.00e+00 |
| `escape-speed-r1` | sqrt(2) x circular_speed at r = 1 (theory sqrt(2 GM / r)) | 1.41421 | 1.41421 | 0.00e+00 |
| `escape-energy` | specific energy at exactly that speed (theory 0) | 2.22045e-16 | 0 | 2.22e-16 |
| `bound-apoapsis` | largest radius reached at 0.9 of the escape speed (theory a (1 + e) = 1 / (2 - 1.62) x 2 - 1) | 4.26321 | 4.26316 | 1.31e-05 |
| `moon-radius-span` | largest minus smallest distance of the moon from the star over 40 T (theory 0) | 8.88178e-16 | 0 | 8.88e-16 |
| `moon-return` | moon's y after one circular_period(3), back on the +x axis (theory 0) | -1.6722e-14 | 0 | 1.67e-14 |
| `moon-omega` | moon_omega (theory sqrt(GM / 27)) | 0.19245 | 0.19245 | 0.00e+00 |
| `energy-change-with-moon` | largest change of the star-only energy of a ship on the ring r = 2.6 over 120 T, moon on (no conservation law; not 0) | 1.58154 | 0 | 1.58e+00 |
| `energy-change-without-moon` | the same ship with the moon's mass set to 0 (theory 0) | 1.59595e-13 | 0 | 1.60e-13 |
| `hohmann-dv1` | first impulse of the demonstration, ring 1 to the transfer ellipse (theory sqrt(4/3) - 1) | 0.154706 | 0.154701 | 3.56e-05 |
| `hohmann-dv2` | second impulse, at apoapsis (theory sqrt(1/2) - sqrt(1/3)) | 0.12976 | 0.129757 | 2.94e-05 |
| `hohmann-dv-total` | both impulses (theory 0.154701 + 0.129757) | 0.284466 | 0.284457 | 3.28e-05 |
| `hohmann-arrival-radius` | radius when the second impulse fires (theory 2) | 2.00006 | 2 | 3.09e-05 |
| `hohmann-coast` | time between the impulses, to within one 0.04 T frame (theory pi 1.5^1.5, half the period of the ellipse) | 5.76 | 5.77147 | 1.99e-03 |
| `hohmann-final-radius` | radius at the end of the demonstration (theory 2) | 2.00013 | 2 | 6.35e-05 |
| `hohmann-burn2-time` | FLIGHT_BURN2_TIME minus FLIGHT_BURN1_TIME (theory half of circular_period(1.5)) | 5.77147 | 5.77147 | 0.00e+00 |
| `thrust-dv` | speed after burning for 10 T in empty space (theory thrust x time = 0.02 x 10) | 0.2 | 0.2 | 6.75e-14 |
| `thrust-direction` | direction of that velocity (theory the heading, 0.3 rad) | 0.3 | 0.3 | 2.66e-14 |
| `manual-dv-meter` | FLIGHT_DV after holding the engine for 30 frames (theory 0.02 x 30 x 0.04) | 0.024 | 0.024 | 2.22e-15 |
| `finite-burn-apoapsis-fast` | apoapsis after spending 0.1547 in a 0.04 T burn (thrust 3.9; an impulse gives 2) | 1.99995 | 2 | 2.74e-05 |
| `finite-burn-apoapsis-tug` | the same in 1.54 T, the Ice Run tug's thrust 0.10 | 1.9379 | 2 | 3.10e-02 |
| `finite-burn-apoapsis-slow` | the same in 6.04 T, almost a full turn of ring 1 (thrust 0.026) | 1.54625 | 2 | 2.27e-01 |
| `raise-periapsis` | periapsis after a prograde burn from ring 1 lifted the apoapsis to 1.8 (an impulse leaves it at 1; a finite burn lifts it a little) | 1.02032 | 1 | 2.03e-02 |
| `raise-cost` | fuel spent on that burn (theory for an impulse sqrt(2 - 1/1.4) - 1) | 0.14 | 0.133893 | 4.56e-02 |
| `match-eccentricity` | eccentricity after holding Match at apoapsis (theory 0) | 0 | 0 | 0.00e+00 |
| `match-radius` | semi-major axis after Match (the radius where Match was held, about the apoapsis) | 1.80051 | 1.81266 | 6.70e-03 |
| `match-error-left` | VV_MATCH_ERROR after Match (theory 0) | 1.41646e-09 | 0 | 1.42e-09 |
| `rel-speed-start` | FLIGHT_REL_SPEED at t = 0: ship (0, 1) against the moon's (0, 3 omega) (theory 1 - sqrt(1/3)) | 0.42265 | 0.42265 | 0.00e+00 |
| `moon-distance-start` | FLIGHT_MOON_DISTANCE at t = 0 (theory 3 - 1) | 2 | 2 | 0.00e+00 |
| `verlet-one-step` | largest gap between orbit_step and the half kick, drift, half kick formulas written out, one step with moon and thrust (theory 0) | 0 | 0 | 0.00e+00 |
| `verlet-reversal` | distance from the start after 2500 steps forward, a velocity flip and 2500 steps more (theory 0) | 1.33574e-16 | 0 | 1.34e-16 |
| `frame-time` | FLIGHT_TIME after one frame (theory 10 steps x 0.004) | 0.04 | 0.04 | 2.22e-16 |
| `verlet-energy-early` | largest relative error of the softened energy with velocity Verlet in the first 160 T (dt = 0.004, ellipse e = 0.44) | 4.51441e-06 | 0 | 4.51e-06 |
| `verlet-energy-late` | the same over the following 1440 T: no growth (ratio to the early value is the test) | 4.51441e-06 | 4.51441e-06 | 1.96e-08 |
| `euler-energy-end` | relative energy error of explicit Euler after the same 1600 T, same dt (it has left the orbit) | 0.687124 | 0 | 6.87e-01 |
| `verlet-order-energy` | slope of log(energy error) against log(dt) between dt = 0.008 and 0.002, energy with the softened potential (theory 2) | 2.00001 | 2 | 7.19e-06 |
| `verlet-order-position` | slope of log(position error after 8 T) against log(dt), same steps, against a run with dt = 0.0000625 (theory 2) | 2.00066 | 2 | 3.31e-04 |
| `verlet-energy-dt0.004` | largest relative error of the softened energy over 8 T at the program's step 0.004 | 4.51441e-06 | 0 | 4.51e-06 |
| `softening-floor` | the same run measured with ship_energy, whose potential is not softened: the 1e-6 softening, not the step, sets this | 6.15037e-06 | 0 | 6.15e-06 |
| `verlet-position-dt0.004` | distance from the fine run after 8 T at dt = 0.004 [L] | 4.29123e-05 | 0 | 4.29e-05 |
| `clear-a-error` | relative gap between the semi-major axis and the target ring in the frame leg 1 was cleared (rule: below 0.04) | 0.0326608 | 0 | 3.27e-02 |
| `clear-ecc` | eccentricity in that frame (rule: below 0.04) | 0.0351736 | 0 | 3.52e-02 |
| `leg1-fuel-used` | fuel the pilot spent on leg 1, ring 1 to 1.5 (theory: the two impulses of a Hohmann transfer) | 0.184 | 0.181645 | 1.30e-02 |
| `leg1-time` | time the pilot took for leg 1 [T] (theory: half the period of the transfer ellipse, pi 1.25^1.5; burns and reaction add to it) | 4.72 | 4.39051 | 7.50e-02 |
| `fuel-per-burn` | fuel spent in two frames of Raise (theory thrust 0.10 x 0.08 T) | 0.008 | 0.008 | 2.86e-14 |
| `speed-per-burn` | speed gained in those frames (the same number, less what gravity took while climbing) | 0.0079823 | 0.008 | 2.21e-03 |
| `tank-start` | tank with no story effect (TANK) | 0.3 | 0.3 | 0.00e+00 |
| `tank-dock` | tank after the dock scene with every effect on (DOCK_TANK) | 0.4 | 0.4 | 0.00e+00 |
| `wobble-alone` | largest wobble over 30 T with nobody calling the burns (bound BASE_WOBBLE 0.35) | 0.349681 | 0.35 | 9.11e-04 |
| `wobble-count` | the same with Ilse's count (bound 0.35 x 0.10) | 0.0349681 | 0.035 | 9.11e-04 |
| `wobble-flare` | the same in leg 4 under the flare, count on (bound 0.035 + 0.08) | 0.114895 | 0.115 | 9.11e-04 |
| `wobble-count-ratio` | ratio of the two amplitudes at every instant (theory COUNT_WOBBLE) | 0.1 | 0.1 | 4.26e-08 |
| `apo-readout` | VV_APO_READ over VV_APO at t = 12.34 (theory 1 + wobble) | 0.788925 | 0.788925 | 2.22e-16 |
| `peri-readout` | VV_PERI_READ over VV_PERI at t = 12.34 (theory 1 - wobble) | 1.21107 | 1.21107 | 0.00e+00 |
| `boil-r2` | voyage_boil at r = 2 in leg 4 (theory FLARE_BOIL) | 0.016 | 0.016 | 0.00e+00 |
| `boil-r1.6` | at r = 1.6, the pod's ring (theory 0.016 x 4 / 1.6^2) | 0.025 | 0.025 | 2.22e-16 |
| `boil-grappled` | grappled to the pod, in its shadow (theory 0) | 0 | 0 | 0.00e+00 |
| `boil-other-leg` | in any other leg (theory 0) | 0 | 0 | 0.00e+00 |
| `boil-per-T` | fuel lost coasting for 1 T on the ring r = 2 (theory 0.016) | 0.016 | 0.016 | 3.64e-08 |
| `drone-wait` | time from the dry tank to the drone's fuel, to within one frame (theory DRONE_WAIT 3) | 3 | 3 | 6.66e-16 |
| `drone-fuel` | fuel at the end of the frame in which the drone docked (theory TOW_FUEL 0.15, less the one 0.004 T step burnt after it) | 0.1496 | 0.1496 | 0.00e+00 |
| `tows-counted` | tows counted by then for that one dry tank (theory 1) | 1 | 1 | 0.00e+00 |
| `carry-tows` | tows right after that frame: the tank ran dry with nobody on the channel (theory 1) | 1 | 1 | 0.00e+00 |
| `carry-radius` | radius right after Lamp Quay's tug has dragged a dry tug back (theory the leg's start ring, 2.2) | 2.2 | 2.2 | 2.22e-16 |
| `carry-fuel` | fuel it leaves (theory CARRY_FUEL 0.20) | 0.2 | 0.2 | 0.00e+00 |
| `carry-ecc` | eccentricity right after the carry (theory 0: it leaves the tug on a circular orbit) | 0 | 0 | 0.00e+00 |
| `scorch-tow` | tows after one frame inside r = 0.35 (theory 1) | 1 | 1 | 0.00e+00 |
| `lost-tow` | tows after one frame beyond r = 4.5 (theory 1) | 1 | 1 | 0.00e+00 |
| `limit-tow` | time coasting on the start ring before Lamp Quay sends a tug (theory LEG_LIMIT 45, to the next frame) | 45.04 | 45 | 8.89e-04 |
| `pod-fuel` | fuel that went in when the grapple held (theory POD_FUEL 0.32, less the flare's boil-off in that frame) | 0.319048 | 0.32 | 2.97e-03 |
| `pod-wait` | time from the grapple to the end of the leg (theory POD_HOLD 11, counted from the first held frame) | 10.96 | 10.96 | 5.15e-14 |
| `scenes-shown` | scenes opened in one whole run (theory 7: dock, route, beacon, flare, ilse, silence, quay) | 7 | 7 | 0.00e+00 |
| `run-time-turn-back` | mission clock at Lamp Quay, turning back for the pod [T] (one run of the bench pilot; no theory) | 47 | 0 | 4.70e+01 |
| `run-time-hold` | the same pilot holding the heading [T] | 41.88 | 0 | 4.19e+01 |
| `scene-first` | scene open right after voyage_init (theory VS_DOCK = 0) | 0 | 0 | 0.00e+00 |
| `heavy-scenes` | scenes that voyage_heavy marks (theory 3: flare, ilse, silence) | 3 | 3 | 0.00e+00 |
| `choice-scenes` | scenes with two options (theory 2: route, ilse) | 2 | 2 | 0.00e+00 |
| `cue-cut` | voyage_cue while raising with the apoapsis still under the ring (theory 0, nothing yet) | 0 | 0 | 0.00e+00 |
| `cue-cut-now` | voyage_cue once the apoapsis has passed the ring 1.5 (theory 1, cut) | 1 | 1 | 0.00e+00 |
| `hold-need-ordinary` | voyage_hold_need on an ordinary leg (theory 0) | 0 | 0 | 0.00e+00 |
| `ending-before` | voyage_ending before Lamp Quay (theory -1) | -1 | -1 | 0.00e+00 |
