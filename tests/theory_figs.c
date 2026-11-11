/* Regenerates every plotted figure of docs/theory and the table of measured values
 * (docs/theory/figures/values.md). Run with `make theory-figs`. Each figure is produced by the
 * same integrator (src/orbit.c), flight controller (src/flight.c) and story layer (src/voyage.c)
 * the program uses; nothing here changes them. */
#include "../src/flight.h"
#include "../src/orbit.h"
#include "../src/svg.h"
#include "../src/voyage.h"
#include "voyage_policy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define MAXN 8192

static char dir[512];
static FILE *values;
static float xs[MAXN], ya[MAXN], yb[MAXN], yc[MAXN];

static const char *path(const char *name)
{
    static char buf[640];
    snprintf(buf, sizeof buf, "%s/%s", dir, name);
    return buf;
}

static void plot(const char *name, const char *title, const char *xl, const char *yl, int count, const Series *s, int ns)
{
    if (svg_plot(path(name), title, xl, yl, xs, count, s, ns) != 0) {
        fprintf(stderr, "cannot write %s\n", name);
        exit(1);
    }
}

/* One row of the measured-values table: id | what | measured | theory | error (relative; absolute when theory is 0). */
static void value(const char *id, const char *what, double measured, double theory)
{
    double err = theory != 0.0 ? fabs(measured / theory - 1.0) : fabs(measured - theory);
    fprintf(values, "| `%s` | %s | %.6g | %.6g | %.2e |\n", id, what, measured, theory, err);
}

static uint64_t play_key_like(void) { return sub_seed(20261111ULL, 0, 9); }

static double largest(const float *a, int from, int n)
{
    double m = 0.0;
    for (int i = from; i < n; i++) m = fmax(m, a[i]);
    return m;
}

static World star_only(void)
{
    World w = world_default();
    w.moon_gm = 0.0;
    return w;
}

/* A ship at (r, 0) moving tangentially with the given speed. */
static Ship tangential(double r, double speed) { return (Ship){r, 0.0, 0.0, speed}; }

/* S01: the size of the acceleration from the star against 1 / r^2. */
static void fig_gravity(void)
{
    World w = star_only();
    int n = 186;
    for (int i = 0; i < n; i++) {
        double r = 0.3 + 0.02 * i, ax, ay;
        gravity(&w, 0.0, r, 0.0, &ax, &ay);
        xs[i] = (float)r;
        ya[i] = (float)hypot(ax, ay);
        yb[i] = (float)(1.0 / (r * r));
    }
    double ax, ay;
    gravity(&w, 0.0, 0.6, 0.8, &ax, &ay);
    value("accel-r1", "size of the star's pull at (0.6, 0.8), r = 1 (theory GM / r^2)", hypot(ax, ay), 1.0);
    value("accel-direction", "its direction: ax / ay at (0.6, 0.8) (theory x / y, towards the star)", ax / ay, 0.75);
    gravity(&w, 0.0, 0.1, 0.0, &ax, &ay);
    value("accel-softening-r0.1", "pull at r = 0.1, where the softening 1e-6 matters most before the crash radius (theory GM / r^2)", hypot(ax, ay), 100.0);
    Series s[] = {{"|a| from gravity()", ya, "#1f77b4"}, {"GM / r^2", yb, "#d62728"}};
    plot("gravity.svg", "Pull of the star against distance", "r [L]", "acceleration [L/T^2]", n, s, 2);
}

/* Time for one full turn of a circular orbit at radius r, measured by integration. */
static double measured_period(const World *w, double r, double dt)
{
    Ship s = tangential(r, circular_speed(w, r));
    double t = 0.0, prev_y = s.y;
    for (int k = 0; k < 4000000; k++) {
        orbit_step(w, &s, t, 0.0, 0.0, dt);
        t += dt;
        if (prev_y < 0.0 && s.y >= 0.0 && s.x > 0.0) return t - dt * s.y / (s.y - prev_y);
        prev_y = s.y;
    }
    return -1.0;
}

/* S02: circular period against radius (Kepler's third law). */
static void fig_period(void)
{
    World w = star_only();
    int n = 25;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (int i = 0; i < n; i++) {
        double r = 0.6 + 0.1 * i, T = measured_period(&w, r, 0.004);
        xs[i] = (float)r;
        ya[i] = (float)T;
        yb[i] = (float)circular_period(&w, r);
        sx += log(r); sy += log(T); sxx += log(r) * log(r); sxy += log(r) * log(T);
    }
    value("period-r1", "time of one turn at r = 1, integrated with dt = 0.004 (theory 2 pi)", measured_period(&w, 1.0, 0.004), 2.0 * PI);
    value("period-r3", "time of one turn at r = 3, the moon's ring (theory 2 pi 3^1.5)", measured_period(&w, 3.0, 0.004), 2.0 * PI * pow(3.0, 1.5));
    value("kepler3-exponent", "slope of log T against log r over 25 radii (theory 3/2)", (n * sxy - sx * sy) / (n * sxx - sx * sx), 1.5);
    value("circular-speed-r2", "circular_speed at r = 2 (theory sqrt(GM / r))", circular_speed(&w, 2.0), sqrt(0.5));
    Series s[] = {{"integrated", ya, "#1f77b4"}, {"2 pi r^1.5", yb, "#d62728"}};
    plot("period.svg", "Period of a circular orbit", "r [L]", "period [T]", n, s, 2);
}

/* S03, S04, S05: one ellipse (r = 1, tangential speed 1.2) followed for three turns. */
static void fig_ellipse(void)
{
    World w = star_only();
    Ship s = tangential(1.0, 1.2);
    double a = 1.0 / (2.0 - 1.44), T = 2.0 * PI * pow(a, 1.5), dt = 0.004;
    double e0 = ship_energy(&w, &s), h0 = ship_ang_momentum(&s);
    Voyage v;
    voyage_init(&v, 0u);
    v.ship = s;
    double apo = voyage_value(&v, VV_APO), peri = voyage_value(&v, VV_PERI), ecc = voyage_value(&v, VV_ECC);
    double sma = voyage_value(&v, VV_SEMI_MAJOR), peri_angle = voyage_value(&v, VV_PERI_ANGLE);
    int steps = (int)(3.0 * T / dt), stride = steps / 1500 + 1, n = 0;
    double t = 0.0, rmax = 0.0, rmin = 1e9, vis_err = 0.0, areal_err = 0.0, h_err = 0.0, e_err = 0.0, area = 0.0;
    for (int k = 0; k < steps; k++) {
        double r = hypot(s.x, s.y), sp = hypot(s.vx, s.vy), vv = sqrt(w.star_gm * (2.0 / r - 1.0 / a));
        Ship before = s;
        if (k % stride == 0 && n < MAXN) {
            xs[n] = (float)t;
            ya[n] = (float)sp;
            yb[n] = (float)vv;
            yc[n] = (float)r;
            n++;
        }
        rmax = fmax(rmax, r);
        rmin = fmin(rmin, r);
        vis_err = fmax(vis_err, fabs(sp / vv - 1.0));
        h_err = fmax(h_err, fabs(ship_ang_momentum(&s) / h0 - 1.0));
        e_err = fmax(e_err, fabs(ship_energy(&w, &s) / e0 - 1.0));
        orbit_step(&w, &s, t, 0.0, 0.0, dt);
        t += dt;
        /* Area of the thin triangle swept in this step, over dt. */
        double rate = 0.5 * fabs(before.x * s.y - before.y * s.x) / dt;
        areal_err = fmax(areal_err, fabs(rate / (0.5 * h0) - 1.0));
        area += rate * dt;
    }
    value("visviva-max-error", "largest relative gap between the integrated speed and sqrt(GM (2/r - 1/a)) over three turns", vis_err, 0.0);
    value("ellipse-a", "semi-major axis from the largest and smallest radius reached (theory 1 / (2/r - v^2/GM) = 1/0.56)", 0.5 * (rmax + rmin), a);
    value("energy-drift-ellipse", "largest relative change of the specific energy over three turns", e_err, 0.0);
    value("ang-momentum-drift", "largest relative change of x vy - y vx over three turns", h_err, 0.0);
    value("areal-rate-spread", "largest relative gap between the area swept per T in one step and h / 2", areal_err, 0.0);
    value("ellipse-area", "area swept in three turns over 3 pi a b (Kepler's second law, summed)", area / (3.0 * PI * a * a * sqrt(1.0 - ecc * ecc)), 1.0);
    value("apo-from-state", "apoapsis from the state at t = 0 (voyage_value VV_APO) against the largest radius reached", apo, rmax);
    value("peri-from-state", "periapsis from the state at t = 0 (VV_PERI) against the smallest radius reached", peri, rmin);
    value("ecc-from-state", "eccentricity from the state (VV_ECC) against (rmax - rmin) / (rmax + rmin)", ecc, (rmax - rmin) / (rmax + rmin));
    value("sma-from-state", "semi-major axis from the state (VV_SEMI_MAJOR) (theory 1/0.56)", sma, a);
    value("peri-angle", "direction of the eccentricity vector (VV_PERI_ANGLE); the ship starts at periapsis on the +x axis", peri_angle, 0.0);
    Series sp[] = {{"integrated speed", ya, "#1f77b4"}, {"vis-viva", yb, "#d62728"}};
    plot("visviva.svg", "Speed along an ellipse: integration against vis-viva", "t [T]", "speed [L/T]", n, sp, 2);
    for (int i = 0; i < n; i++) { ya[i] = (float)apo; yb[i] = (float)peri; }
    Series el[] = {{"r(t)", yc, "#1f77b4"}, {"apoapsis (state)", ya, "#d62728"}, {"periapsis (state)", yb, "#2ca02c"}};
    plot("elements.svg", "Radius over three turns, and the apsides predicted at t = 0", "t [T]", "r [L]", n, el, 3);

    /* S04: the area swept from the star, turn by turn, grows in a straight line. */
    s = tangential(1.0, 1.2);
    t = 0.0;
    n = 0;
    area = 0.0;
    for (int k = 0; k < steps; k++) {
        Ship before = s;
        if (k % stride == 0 && n < MAXN) {
            xs[n] = (float)t;
            ya[n] = (float)area;
            yb[n] = (float)(0.5 * h0 * t);
            yc[n] = (float)hypot(s.x, s.y);
            n++;
        }
        orbit_step(&w, &s, t, 0.0, 0.0, dt);
        t += dt;
        area += 0.5 * fabs(before.x * s.y - before.y * s.x);
    }
    Series ar[] = {{"area swept", ya, "#1f77b4"}, {"h t / 2", yb, "#d62728"}, {"r(t)", yc, "#888888"}};
    plot("areal.svg", "Equal areas in equal times, while the radius swings", "t [T]", "area [L^2], r [L]", n, ar, 3);
}

/* S06: just below, at and just above the escape speed. */
static void fig_escape(void)
{
    World w = star_only();
    double ve = sqrt(2.0) * circular_speed(&w, 1.0), dt = 0.004;
    Ship s[3] = {tangential(1.0, 0.9 * ve), tangential(1.0, ve), tangential(1.0, 1.1 * ve)};
    float *out[3] = {ya, yb, yc};
    int n = 0;
    for (int k = 0; k < 10000; k++) {
        if (k % 10 == 0) {
            xs[n] = (float)(k * dt);
            for (int j = 0; j < 3; j++) out[j][n] = (float)hypot(s[j].x, s[j].y);
            n++;
        }
        for (int j = 0; j < 3; j++) orbit_step(&w, &s[j], k * dt, 0.0, 0.0, dt);
    }
    Ship e = tangential(1.0, ve);
    value("escape-speed-r1", "sqrt(2) x circular_speed at r = 1 (theory sqrt(2 GM / r))", ve, sqrt(2.0));
    value("escape-energy", "specific energy at exactly that speed (theory 0)", ship_energy(&w, &e), 0.0);
    value("bound-apoapsis", "largest radius reached at 0.9 of the escape speed (theory a (1 + e) = 1 / (2 - 1.62) x 2 - 1)", largest(ya, 0, n), 2.0 / (2.0 - 1.62) - 1.0);
    Series p[] = {{"0.9 v_esc (bound)", ya, "#1f77b4"}, {"v_esc", yb, "#2ca02c"}, {"1.1 v_esc", yc, "#d62728"}};
    plot("escape.svg", "Radius against time around the escape speed", "t [T]", "r [L]", n, p, 3);
}

/* S07: the moon's circle, and what its pull does to the star-only energy of a ship. */
static void fig_moon(void)
{
    World w = world_default(), off = star_only();
    double lo = 1e9, hi = 0.0, mx, my, dt = 0.004;
    for (int i = 0; i <= 2000; i++) {
        moon_position(&w, 0.02 * i, &mx, &my);
        lo = fmin(lo, hypot(mx, my));
        hi = fmax(hi, hypot(mx, my));
    }
    moon_position(&w, circular_period(&w, 3.0), &mx, &my);
    value("moon-radius-span", "largest minus smallest distance of the moon from the star over 40 T (theory 0)", hi - lo, 0.0);
    value("moon-return", "moon's y after one circular_period(3), back on the +x axis (theory 0)", my, 0.0);
    value("moon-omega", "moon_omega (theory sqrt(GM / 27))", moon_omega(&w), sqrt(1.0 / 27.0));
    Ship a = tangential(2.6, circular_speed(&w, 2.6)), b = a;
    double e0 = ship_energy(&w, &a), worst = 0.0, worst_off = 0.0;
    int n = 0;
    for (int k = 0; k < 30000; k++) {
        if (k % 20 == 0) {
            xs[n] = (float)(k * dt);
            ya[n] = (float)(ship_energy(&w, &a) - e0);
            yb[n] = (float)(ship_energy(&off, &b) - e0);
            n++;
        }
        orbit_step(&w, &a, k * dt, 0.0, 0.0, dt);
        orbit_step(&off, &b, k * dt, 0.0, 0.0, dt);
        worst = fmax(worst, fabs(ship_energy(&w, &a) - e0));
        worst_off = fmax(worst_off, fabs(ship_energy(&off, &b) - e0));
    }
    value("energy-change-with-moon", "largest change of the star-only energy of a ship on the ring r = 2.6 over 120 T, moon on (no conservation law; not 0)", worst, 0.0);
    value("energy-change-without-moon", "the same ship with the moon's mass set to 0 (theory 0)", worst_off, 0.0);
    Series s[] = {{"moon on", ya, "#d62728"}, {"moon off", yb, "#1f77b4"}};
    plot("energy-moon.svg", "Star-only energy of a ship near the moon's ring", "t [T]", "E - E0 [L^2/T^2]", n, s, 2);
}

/* S08: the Hohmann demonstration of flight.c (mode 1), frame by frame. */
static void fig_hohmann(void)
{
    World w = star_only();
    flight_reset(1);
    double dv1 = 0.0, dv_total, r_burn2 = 0.0, t1 = 0.0, t2 = 0.0;
    int n = 0, prev = 0;
    while (flight_value(FLIGHT_STATUS) == 0.0 && n < MAXN) {
        xs[n] = (float)flight_value(FLIGHT_TIME);
        ya[n] = (float)flight_value(FLIGHT_RADIUS);
        yb[n] = (float)flight_value(FLIGHT_SPEED);
        n++;
        flight_tick(1, 0, 0);
        int phase = (int)flight_value(FLIGHT_PHASE);
        if (prev == 0 && phase == 1) { dv1 = flight_value(FLIGHT_DV); t1 = flight_value(FLIGHT_TIME); }
        if (prev == 1 && phase == 2) { r_burn2 = flight_value(FLIGHT_RADIUS); t2 = flight_value(FLIGHT_TIME); }
        prev = phase;
    }
    dv_total = flight_value(FLIGHT_DV);
    double th1 = sqrt(4.0 / 3.0) - 1.0, th2 = sqrt(0.5) - sqrt(1.0 / 3.0);
    value("hohmann-dv1", "first impulse of the demonstration, ring 1 to the transfer ellipse (theory sqrt(4/3) - 1)", dv1, th1);
    value("hohmann-dv2", "second impulse, at apoapsis (theory sqrt(1/2) - sqrt(1/3))", dv_total - dv1, th2);
    value("hohmann-dv-total", "both impulses (theory 0.154701 + 0.129757)", dv_total, th1 + th2);
    value("hohmann-arrival-radius", "radius when the second impulse fires (theory 2)", r_burn2, 2.0);
    value("hohmann-coast", "time between the impulses, to within one 0.04 T frame (theory pi 1.5^1.5, half the period of the ellipse)", t2 - t1, PI * pow(1.5, 1.5));
    value("hohmann-final-radius", "radius at the end of the demonstration (theory 2)", flight_value(FLIGHT_RADIUS), 2.0);
    value("hohmann-burn2-time", "FLIGHT_BURN2_TIME minus FLIGHT_BURN1_TIME (theory half of circular_period(1.5))", flight_value(FLIGHT_BURN2_TIME) - flight_value(FLIGHT_BURN1_TIME), 0.5 * circular_period(&w, 1.5));
    Series s[] = {{"radius", ya, "#1f77b4"}, {"speed", yb, "#d62728"}};
    plot("hohmann.svg", "Hohmann transfer 1 -> 2: radius and speed", "t [T]", "r [L], speed [L/T]", n, s, 2);

    /* The cost of a two-impulse transfer from ring 1 to ring R, from the same formulas. */
    n = 0;
    for (double R = 1.0; R <= 4.0001; R += 0.02, n++) {
        double at = 0.5 * (1.0 + R);
        double d1 = fabs(sqrt(2.0 - 1.0 / at) - 1.0), d2 = fabs(sqrt(1.0 / R) - sqrt(2.0 / R - 1.0 / at));
        xs[n] = (float)R;
        ya[n] = (float)d1;
        yb[n] = (float)d2;
        yc[n] = (float)(d1 + d2);
    }
    Series c[] = {{"first impulse", ya, "#1f77b4"}, {"second impulse", yb, "#2ca02c"}, {"total", yc, "#d62728"}};
    plot("hohmann-cost.svg", "Delta-v of a Hohmann transfer from ring 1 to ring R", "R [L]", "delta-v [L/T]", n, c, 3);
}

/* S09: thrust along the heading adds speed at the rate `thrust`; a short burn approaches an impulse. */
static void fig_thrust(void)
{
    World w = star_only();
    w.star_gm = 0.0;   /* no star: only the engine acts */
    Ship s = {5.0, 0.0, 0.0, 0.0};
    int n = 0;
    double dt = 0.004;
    for (int k = 0; k <= 2500; k++) {
        if (k % 10 == 0) {
            xs[n] = (float)(k * dt);
            ya[n] = (float)hypot(s.vx, s.vy);
            yb[n] = (float)(w.thrust * k * dt);
            n++;
        }
        if (k < 2500) orbit_step(&w, &s, k * dt, 0.3, 1.0, dt);
    }
    value("thrust-dv", "speed after burning for 10 T in empty space (theory thrust x time = 0.02 x 10)", hypot(s.vx, s.vy), 0.2);
    value("thrust-direction", "direction of that velocity (theory the heading, 0.3 rad)", atan2(s.vy, s.vx), 0.3);
    flight_reset(0);
    flight_tick(30, 0, 1);
    value("manual-dv-meter", "FLIGHT_DV after holding the engine for 30 frames (theory 0.02 x 30 x 0.04)", flight_value(FLIGHT_DV), 0.024);
    Series v[] = {{"speed", ya, "#1f77b4"}, {"thrust x t", yb, "#d62728"}};
    plot("thrust.svg", "Engine alone: speed grows at the rate of the thrust", "t [T]", "speed [L/T]", n, v, 2);

    /* The same delta-v 0.1547 delivered as an impulse and as finite burns of growing length: where does the orbit end up? */
    World g = star_only();
    n = 0;
    double dv = sqrt(4.0 / 3.0) - 1.0;
    for (int i = 0; i <= 40; i++) {
        double burn_time = 0.04 + 0.15 * i, thrust = dv / burn_time;
        g.thrust = thrust;
        Ship q = tangential(1.0, 1.0);
        double t = 0.0, left = dv;
        while (left > 1e-12) {
            double b = fmin(1.0, left / (thrust * dt));
            orbit_step(&g, &q, t, atan2(q.vy, q.vx), b, dt);
            left -= b * thrust * dt;
            t += dt;
        }
        Voyage v2;
        voyage_init(&v2, 0u);
        v2.ship = q;
        xs[n] = (float)burn_time;
        ya[n] = (float)voyage_value(&v2, VV_APO);
        yb[n] = 2.0f;
        n++;
        if (i == 0) value("finite-burn-apoapsis-fast", "apoapsis after spending 0.1547 in a 0.04 T burn (thrust 3.9; an impulse gives 2)", voyage_value(&v2, VV_APO), 2.0);
        if (i == 10) value("finite-burn-apoapsis-tug", "the same in 1.54 T, the Ice Run tug's thrust 0.10", voyage_value(&v2, VV_APO), 2.0);
        if (i == 40) value("finite-burn-apoapsis-slow", "the same in 6.04 T, almost a full turn of ring 1 (thrust 0.026)", voyage_value(&v2, VV_APO), 2.0);
    }
    Series f[] = {{"apoapsis reached", ya, "#1f77b4"}, {"impulse: 2", yb, "#d62728"}};
    plot("finite-burn.svg", "The same delta-v spread over a longer burn reaches less far", "length of the burn [T]", "apoapsis [L]", n, f, 2);
}

/* Open a voyage past the dock scene, with the chosen effects. */
static void voyage_start(Voyage *v, unsigned effects)
{
    voyage_init(v, effects);
    voyage_answer(v, 0);
}

/* S10: Raise, coast to apoapsis, Match. */
static void fig_apsides(void)
{
    Voyage v;
    voyage_start(&v, 0u);
    v.fuel = 5.0;   /* plenty: this figure is about the orbit, not the tank */
    v.target_r = 9.0;   /* out of reach, so the leg does not end while we watch */
    int n = 0, phase = 0;
    double peri_after_raise = 0.0, apo_after_raise = 0.0, fuel0 = v.fuel, raise_cost = 0.0;
    for (int f = 0; f < 420 && n < MAXN; f++) {
        xs[n] = (float)v.time;
        ya[n] = (float)voyage_value(&v, VV_APO);
        yb[n] = (float)voyage_value(&v, VV_PERI);
        yc[n] = (float)voyage_value(&v, VV_RADIUS);
        n++;
        int cmd = VC_COAST;
        if (phase == 0) {
            cmd = VC_RAISE;
            if (voyage_value(&v, VV_APO) >= 1.8) {
                phase = 1;
                cmd = VC_COAST;
                peri_after_raise = voyage_value(&v, VV_PERI);
                apo_after_raise = voyage_value(&v, VV_APO);
                raise_cost = fuel0 - v.fuel;
            }
        } else if (phase == 1) {
            if (voyage_value(&v, VV_RADIUS) >= 0.999 * apo_after_raise || voyage_value(&v, VV_RADIAL) < 0.0) phase = 2;
        }
        if (phase == 2) cmd = voyage_value(&v, VV_MATCH_ERROR) > 1e-9 ? VC_MATCH : VC_COAST;
        voyage_tick(&v, 1, cmd);
    }
    value("raise-periapsis", "periapsis after a prograde burn from ring 1 lifted the apoapsis to 1.8 (an impulse leaves it at 1; a finite burn lifts it a little)", peri_after_raise, 1.0);
    value("raise-cost", "fuel spent on that burn (theory for an impulse sqrt(2 - 1/1.4) - 1)", raise_cost, sqrt(2.0 - 1.0 / 1.4) - 1.0);
    value("match-eccentricity", "eccentricity after holding Match at apoapsis (theory 0)", voyage_value(&v, VV_ECC), 0.0);
    value("match-radius", "semi-major axis after Match (the radius where Match was held, about the apoapsis)", voyage_value(&v, VV_SEMI_MAJOR), apo_after_raise);
    value("match-error-left", "VV_MATCH_ERROR after Match (theory 0)", voyage_value(&v, VV_MATCH_ERROR), 0.0);
    Series s[] = {{"apoapsis", ya, "#d62728"}, {"periapsis", yb, "#2ca02c"}, {"radius", yc, "#1f77b4"}};
    plot("apsides.svg", "Raise, coast to the top, Match", "t [T]", "r [L]", n, s, 3);
}

/* S11: the rendezvous numbers of the manual mode at t = 0. */
static void val_rendezvous(void)
{
    flight_reset(0);
    value("rel-speed-start", "FLIGHT_REL_SPEED at t = 0: ship (0, 1) against the moon's (0, 3 omega) (theory 1 - sqrt(1/3))", flight_value(FLIGHT_REL_SPEED), 1.0 - sqrt(1.0 / 3.0));
    value("moon-distance-start", "FLIGHT_MOON_DISTANCE at t = 0 (theory 3 - 1)", flight_value(FLIGHT_MOON_DISTANCE), 2.0);
}

/* S12: one step of orbit_step against the kick-drift-kick formulas written out. */
static void val_verlet(void)
{
    World w = world_default();
    Ship s = {1.1, 0.2, -0.1, 0.9}, q = s;
    double t = 3.0, dt = 0.004, heading = 0.7, burn = 0.5, ax, ay;
    double bx = burn * w.thrust * cos(heading), by = burn * w.thrust * sin(heading);
    gravity(&w, t, q.x, q.y, &ax, &ay);
    double hx = q.vx + 0.5 * dt * (ax + bx), hy = q.vy + 0.5 * dt * (ay + by);
    q.x += dt * hx;
    q.y += dt * hy;
    gravity(&w, t + dt, q.x, q.y, &ax, &ay);
    q.vx = hx + 0.5 * dt * (ax + bx);
    q.vy = hy + 0.5 * dt * (ay + by);
    orbit_step(&w, &s, t, heading, burn, dt);
    value("verlet-one-step", "largest gap between orbit_step and the half kick, drift, half kick formulas written out, one step with moon and thrust (theory 0)",
          fmax(fmax(fabs(s.x - q.x), fabs(s.y - q.y)), fmax(fabs(s.vx - q.vx), fabs(s.vy - q.vy))), 0.0);
    /* Time reversal: step forward, flip the velocity, step again, flip back. */
    World g = star_only();
    Ship a = tangential(1.0, 1.2), b = a;
    for (int k = 0; k < 2500; k++) orbit_step(&g, &b, 0.0, 0.0, 0.0, dt);
    b.vx = -b.vx; b.vy = -b.vy;
    for (int k = 0; k < 2500; k++) orbit_step(&g, &b, 0.0, 0.0, 0.0, dt);
    value("verlet-reversal", "distance from the start after 2500 steps forward, a velocity flip and 2500 steps more (theory 0)", hypot(b.x - a.x, b.y - a.y), 0.0);
    flight_reset(0);
    flight_tick(1, 0, 0);
    value("frame-time", "FLIGHT_TIME after one frame (theory 10 steps x 0.004)", flight_value(FLIGHT_TIME), 0.04);
}

/* The energy that the softened force of gravity() conserves exactly: the potential is -GM / sqrt(r^2 + 1e-6). */
static double soft_energy(const World *w, const Ship *s)
{
    return 0.5 * (s->vx * s->vx + s->vy * s->vy) - w->star_gm / sqrt(s->x * s->x + s->y * s->y + 1e-6);
}

static void euler_step(const World *w, Ship *s, double t, double dt)
{
    double ax, ay;
    gravity(w, t, s->x, s->y, &ax, &ay);
    s->x += dt * s->vx;
    s->y += dt * s->vy;
    s->vx += dt * ax;
    s->vy += dt * ay;
}

/* S13: energy error of velocity Verlet stays bounded; explicit Euler (written here only for contrast) drifts. */
static void fig_energy_drift(void)
{
    World w = star_only();
    Ship v = tangential(1.0, 1.2), e = v;
    double e0 = soft_energy(&w, &v), dt = 0.004, worst = 0.0, worst_late = 0.0, euler_last = 0.0;
    int steps = 400000, n = 0;
    for (int k = 0; k < steps; k++) {
        double dv = fabs(soft_energy(&w, &v) / e0 - 1.0), de = fabs(soft_energy(&w, &e) / e0 - 1.0);
        if (k % 400 == 0) {
            xs[n] = (float)(k * dt);
            ya[n] = (float)log10(dv + 1e-16);
            yb[n] = (float)log10(de + 1e-16);
            n++;
        }
        if (k < steps / 10) worst = fmax(worst, dv); else worst_late = fmax(worst_late, dv);
        euler_last = de;
        orbit_step(&w, &v, k * dt, 0.0, 0.0, dt);
        euler_step(&w, &e, k * dt, dt);
    }
    value("verlet-energy-early", "largest relative error of the softened energy with velocity Verlet in the first 160 T (dt = 0.004, ellipse e = 0.44)", worst, 0.0);
    value("verlet-energy-late", "the same over the following 1440 T: no growth (ratio to the early value is the test)", worst_late, worst);
    value("euler-energy-end", "relative energy error of explicit Euler after the same 1600 T, same dt (it has left the orbit)", euler_last, 0.0);
    Series s[] = {{"velocity Verlet", ya, "#1f77b4"}, {"explicit Euler", yb, "#d62728"}};
    plot("energy-drift.svg", "Relative energy error over 1600 T (log scale)", "t [T]", "log10 |E / E0 - 1|", n, s, 2);
}

/* S14: the error falls with the square of the step. Energy is the one the softened force conserves; the position is
 * compared with a run at a step sixteen times finer than the finest plotted. */
static void fig_order(void)
{
    World w = star_only();
    double dts[7] = {0.064, 0.032, 0.016, 0.008, 0.004, 0.002, 0.001}, errs[7], pos[7], plain = 0.0;
    double span = 8.0;
    Ship ref = tangential(1.0, 1.2);
    for (int k = 0; k < 128000; k++) orbit_step(&w, &ref, 0.0, 0.0, 0.0, span / 128000.0);
    for (int i = 0; i < 7; i++) {
        Ship s = tangential(1.0, 1.2);
        int steps = (int)floor(span / dts[i] + 0.5);
        double e0 = soft_energy(&w, &s), p0 = ship_energy(&w, &s), worst = 0.0, worst_plain = 0.0, dt = span / steps;
        for (int k = 0; k < steps; k++) {
            orbit_step(&w, &s, 0.0, 0.0, 0.0, dt);
            worst = fmax(worst, fabs(soft_energy(&w, &s) / e0 - 1.0));
            worst_plain = fmax(worst_plain, fabs(ship_energy(&w, &s) / p0 - 1.0));
        }
        errs[i] = worst;
        pos[i] = hypot(s.x - ref.x, s.y - ref.y);
        if (i == 4) plain = worst_plain;
        xs[i] = (float)log10(dt);
        ya[i] = (float)log10(worst);
        yb[i] = (float)log10(pos[i]);
        yc[i] = (float)(log10(errs[0]) + 2.0 * (log10(dt) - xs[0]));
    }
    value("verlet-order-energy", "slope of log(energy error) against log(dt) between dt = 0.008 and 0.002, energy with the softened potential (theory 2)", log(errs[3] / errs[5]) / log(dts[3] / dts[5]), 2.0);
    value("verlet-order-position", "slope of log(position error after 8 T) against log(dt), same steps, against a run with dt = 0.0000625 (theory 2)", log(pos[3] / pos[5]) / log(dts[3] / dts[5]), 2.0);
    value("verlet-energy-dt0.004", "largest relative error of the softened energy over 8 T at the program's step 0.004", errs[4], 0.0);
    value("softening-floor", "the same run measured with ship_energy, whose potential is not softened: the 1e-6 softening, not the step, sets this", plain, 0.0);
    value("verlet-position-dt0.004", "distance from the fine run after 8 T at dt = 0.004 [L]", pos[4], 0.0);
    Series s[] = {{"energy error", ya, "#1f77b4"}, {"position error", yb, "#2ca02c"}, {"slope 2", yc, "#d62728"}};
    plot("order.svg", "Second order: error against step size (log-log)", "log10 dt", "log10 error", 7, s, 3);
}

/* One piloted run of The Ice Run, recorded frame by frame. */
typedef struct { double t_clear[V_LEGS], fuel_start[V_LEGS], fuel_end[V_LEGS], a_err[V_LEGS], ecc[V_LEGS]; int scenes, tows; double pod_gain, pod_wait, total; } Flown;

static void fly(uint64_t key, unsigned effects, int route, int turned, Flown *out, int record)
{
    Voyage v;
    Pilot p;
    voyage_init(&v, effects);
    pilot_init(&p, key);
    int n = 0, frames = 0;
    double held_since = -1.0;
    out->scenes = 0;
    out->pod_gain = out->pod_wait = 0.0;
    while (v.leg < V_LEGS && frames < 12000) {
        if (v.scene != VS_NONE) {
            int scene = v.scene;
            out->scenes++;
            voyage_answer(&v, scene == VS_ROUTE ? route : scene == VS_ILSE ? turned : 0);
            if (v.scene == VS_NONE && v.leg < V_LEGS) out->fuel_start[v.leg] = v.fuel;
            continue;
        }
        int leg = v.leg;
        double fuel_before = v.fuel, a = voyage_value(&v, VV_SEMI_MAJOR), e = voyage_value(&v, VV_ECC);
        if (record && n < MAXN && frames % 2 == 0) {
            xs[n] = (float)v.time;
            ya[n] = (float)v.fuel;
            yb[n] = (float)voyage_value(&v, VV_RED_LINE);
            yc[n] = (float)(voyage_value(&v, VV_RADIUS) / 10.0);
            n++;
        }
        (void)a; (void)e;
        voyage_tick(&v, 1, pilot_command(&p, &v));
        frames++;
        if (v.held > 0.0 && held_since < 0.0) { held_since = v.time; out->pod_gain = v.fuel - fuel_before; }
        if (v.leg != leg) {
            out->t_clear[leg] = v.time;
            out->fuel_end[leg] = v.fuel;
            if (held_since >= 0.0 && out->pod_wait == 0.0) out->pod_wait = v.time - held_since;
        }
    }
    if (v.scene != VS_NONE) out->scenes++;   /* the quay */
    out->tows = v.tows;
    out->total = v.time;
    if (record) {
        Series s[] = {{"fuel", ya, "#1f77b4"}, {"red line 0.25", yb, "#d62728"}, {"radius / 10", yc, "#888888"}};
        plot("fuel.svg", "The tank over one piloted run (far buoy, hold the heading)", "t [T]", "fuel [delta-v], r/10 [L]", n, s, 3);
    }
}

/* S16: the first leg, flown by the bench's pilot: semi-major axis and eccentricity against the clearing band. */
static void fig_clearing(void)
{
    Voyage v;
    Pilot p;
    voyage_start(&v, VE_ALL);
    pilot_init(&p, 7u);
    int n = 0;
    double a = 0.0, e = 0.0, fuel0 = v.fuel;
    while (v.leg == 0 && n < MAXN) {
        a = voyage_value(&v, VV_SEMI_MAJOR);
        e = voyage_value(&v, VV_ECC);
        xs[n] = (float)v.time;
        ya[n] = (float)fabs(a / v.target_r - 1.0);
        yb[n] = (float)e;
        yc[n] = 0.04f;
        n++;
        voyage_tick(&v, 1, pilot_command(&p, &v));
    }
    a = voyage_value(&v, VV_SEMI_MAJOR);
    e = voyage_value(&v, VV_ECC);
    double ideal = (sqrt(2.0 - 1.0 / 1.25) - 1.0) + (sqrt(1.0 / 1.5) - sqrt(2.0 / 1.5 - 1.0 / 1.25));
    value("clear-a-error", "relative gap between the semi-major axis and the target ring in the frame leg 1 was cleared (rule: below 0.04)", fabs(a / 1.5 - 1.0), 0.0);
    value("clear-ecc", "eccentricity in that frame (rule: below 0.04)", e, 0.0);
    value("leg1-fuel-used", "fuel the pilot spent on leg 1, ring 1 to 1.5 (theory: the two impulses of a Hohmann transfer)", fuel0 - v.fuel, ideal);
    value("leg1-time", "time the pilot took for leg 1 [T] (theory: half the period of the transfer ellipse, pi 1.25^1.5; burns and reaction add to it)", v.time, PI * pow(1.25, 1.5));
    Series s[] = {{"|a / target - 1|", ya, "#1f77b4"}, {"eccentricity", yb, "#2ca02c"}, {"band 0.04", yc, "#d62728"}};
    plot("clearing.svg", "Leg 1 with the bench pilot against the clearing band", "t [T]", "relative error, eccentricity", n, s, 3);
}

/* S17: fuel is delta-v. */
static void fig_fuel(void)
{
    Voyage v;
    voyage_start(&v, 0u);
    double fuel0 = v.fuel, speed0 = voyage_value(&v, VV_SPEED);
    voyage_tick(&v, 2, VC_RAISE);
    value("fuel-per-burn", "fuel spent in two frames of Raise (theory thrust 0.10 x 0.08 T)", fuel0 - v.fuel, 0.008);
    value("speed-per-burn", "speed gained in those frames (the same number, less what gravity took while climbing)", voyage_value(&v, VV_SPEED) - speed0, 0.008);
    value("tank-start", "tank with no story effect (TANK)", fuel0, 0.30);
    voyage_start(&v, VE_ALL);
    value("tank-dock", "tank after the dock scene with every effect on (DOCK_TANK)", v.fuel, 0.40);
    Flown f;
    fly(play_key_like(), VE_ALL, 1, 1, &f, 1);
}

/* S18: the wobble of the apsis readout. */
static void fig_wobble(void)
{
    Voyage a, b, c;
    voyage_start(&a, 0u);          /* nobody calls the burns */
    voyage_start(&b, VE_ALL);      /* Ilse's count */
    voyage_start(&c, VE_ALL);      /* the flare, leg 4, count still on */
    c.leg = 3;
    int n = 0;
    double ma = 0.0, mb = 0.0, mc = 0.0;
    for (int i = 0; i <= 1500; i++) {
        double t = 0.02 * i;
        a.time = b.time = c.time = t;
        xs[n] = (float)t;
        ya[n] = (float)voyage_wobble(&a);
        yb[n] = (float)voyage_wobble(&b);
        yc[n] = (float)voyage_wobble(&c);
        ma = fmax(ma, fabs(ya[n])); mb = fmax(mb, fabs(yb[n])); mc = fmax(mc, fabs(yc[n]));
        n++;
    }
    value("wobble-alone", "largest wobble over 30 T with nobody calling the burns (bound BASE_WOBBLE 0.35)", ma, 0.35);
    value("wobble-count", "the same with Ilse's count (bound 0.35 x 0.10)", mb, 0.035);
    value("wobble-flare", "the same in leg 4 under the flare, count on (bound 0.035 + 0.08)", mc, 0.115);
    value("wobble-count-ratio", "ratio of the two amplitudes at every instant (theory COUNT_WOBBLE)", mb / ma, 0.10);
    a.time = 12.34;
    a.ship = tangential(1.0, 1.2);
    value("apo-readout", "VV_APO_READ over VV_APO at t = 12.34 (theory 1 + wobble)", voyage_value(&a, VV_APO_READ) / voyage_value(&a, VV_APO), 1.0 + voyage_wobble(&a));
    value("peri-readout", "VV_PERI_READ over VV_PERI at t = 12.34 (theory 1 - wobble)", voyage_value(&a, VV_PERI_READ) / voyage_value(&a, VV_PERI), 1.0 - voyage_wobble(&a));
    Series s[] = {{"alone", ya, "#d62728"}, {"Ilse counting", yb, "#1f77b4"}, {"flare, leg 4", yc, "#2ca02c"}};
    plot("wobble.svg", "Relative wobble of the apsis readout", "mission clock [T]", "wobble (fraction of the reading)", n, s, 3);
}

/* S19: the flare's boil-off against distance from the star. */
static void fig_boil(void)
{
    Voyage v;
    voyage_start(&v, VE_ALL);
    v.leg = 3;
    int n = 0;
    for (double r = 1.0; r <= 3.0001; r += 0.02, n++) {
        v.ship = tangential(r, 0.5);
        xs[n] = (float)r;
        ya[n] = (float)voyage_boil(&v);
        yb[n] = (float)(0.016 * 4.0 / (r * r));
    }
    v.ship = tangential(2.0, 0.5);
    value("boil-r2", "voyage_boil at r = 2 in leg 4 (theory FLARE_BOIL)", voyage_boil(&v), 0.016);
    v.ship = tangential(1.6, 0.5);
    value("boil-r1.6", "at r = 1.6, the pod's ring (theory 0.016 x 4 / 1.6^2)", voyage_boil(&v), 0.025);
    v.held = 1.0;
    value("boil-grappled", "grappled to the pod, in its shadow (theory 0)", voyage_boil(&v), 0.0);
    v.held = 0.0;
    v.leg = 2;
    value("boil-other-leg", "in any other leg (theory 0)", voyage_boil(&v), 0.0);
    v.leg = 3;
    v.ship = tangential(2.0, circular_speed(&v.world, 2.0));
    v.target_r = 9.0;
    double fuel0 = v.fuel;
    voyage_tick(&v, 25, VC_COAST);
    value("boil-per-T", "fuel lost coasting for 1 T on the ring r = 2 (theory 0.016)", fuel0 - v.fuel, 0.016);
    Series s[] = {{"voyage_boil", ya, "#1f77b4"}, {"0.016 x 4 / r^2", yb, "#d62728"}};
    plot("boil.svg", "Flare boil-off in leg 4 against distance from the star", "r [L]", "fuel lost per T", n, s, 2);
}

/* S20: a dry tank while Ilse is on the channel, and after the silence. */
static void fig_tow(void)
{
    Voyage v;
    voyage_start(&v, VE_ALL);
    v.fuel = 0.02;
    v.target_r = 9.0;
    int n = 0;
    double dry_at = -1.0, back_at = -1.0, fuel_back = 0.0;
    int tows_back = 0;
    for (int f = 0; f < 150; f++) {
        xs[n] = (float)v.time;
        ya[n] = (float)v.fuel;
        yb[n] = (float)(v.drift / 20.0);
        n++;
        voyage_tick(&v, 1, VC_RAISE);
        if (dry_at < 0.0 && v.tows == 1) dry_at = v.time;
        if (dry_at >= 0.0 && back_at < 0.0 && v.fuel > 0.0) { back_at = v.time; fuel_back = v.fuel; tows_back = v.tows; }
    }
    value("drone-wait", "time from the dry tank to the drone's fuel, to within one frame (theory DRONE_WAIT 3)", back_at - dry_at, 3.0);
    value("drone-fuel", "fuel at the end of the frame in which the drone docked (theory TOW_FUEL 0.15, less the one 0.004 T step burnt after it)", fuel_back, 0.15 - 0.10 * 0.004);
    value("tows-counted", "tows counted by then for that one dry tank (theory 1)", tows_back, 1.0);
    Series s[] = {{"fuel", ya, "#1f77b4"}, {"drift left / 20", yb, "#888888"}};
    plot("tow.svg", "A dry tank while Ilse is on the channel: three T adrift, then 0.15", "t [T]", "fuel [delta-v], drift [T] / 20", n, s, 2);

    voyage_start(&v, VE_ALL);
    v.count = 0;            /* after the silence */
    v.leg = 4;
    v.from_r = 2.2;
    v.target_r = 2.6;
    v.ship = tangential(2.3, circular_speed(&v.world, 2.3) * 1.05);
    v.fuel = 0.004;   /* exactly one frame of burning */
    voyage_tick(&v, 1, VC_RAISE);
    value("carry-tows", "tows right after that frame: the tank ran dry with nobody on the channel (theory 1)", v.tows, 1.0);
    value("carry-radius", "radius right after Lamp Quay's tug has dragged a dry tug back (theory the leg's start ring, 2.2)", voyage_value(&v, VV_RADIUS), 2.2);
    value("carry-fuel", "fuel it leaves (theory CARRY_FUEL 0.20)", v.fuel, 0.20);
    value("carry-ecc", "eccentricity right after the carry (theory 0: it leaves the tug on a circular orbit)", voyage_value(&v, VV_ECC), 0.0);
    voyage_start(&v, VE_ALL);
    v.ship = tangential(0.34, 1.0);
    voyage_tick(&v, 1, VC_COAST);
    value("scorch-tow", "tows after one frame inside r = 0.35 (theory 1)", v.tows, 1.0);
    voyage_start(&v, VE_ALL);
    v.ship = tangential(4.6, 0.1);
    voyage_tick(&v, 1, VC_COAST);
    value("lost-tow", "tows after one frame beyond r = 4.5 (theory 1)", v.tows, 1.0);
    voyage_start(&v, VE_ALL);
    int frames = 0;
    while (v.tows == 0 && frames < 2000) { voyage_tick(&v, 1, VC_COAST); frames++; }
    value("limit-tow", "time coasting on the start ring before Lamp Quay sends a tug (theory LEG_LIMIT 45, to the next frame)", frames * 0.04, 45.0);
}

/* S21, S22: the pod and the order of the scenes, from whole piloted runs. */
static void val_runs(void)
{
    Flown back, hold;
    fly(play_key_like(), VE_ALL, 1, 0, &back, 0);
    fly(play_key_like(), VE_ALL, 1, 1, &hold, 0);
    value("pod-fuel", "fuel that went in when the grapple held (theory POD_FUEL 0.32, less the flare's boil-off in that frame)", back.pod_gain, 0.32);
    value("pod-wait", "time from the grapple to the end of the leg (theory POD_HOLD 11, counted from the first held frame)", back.pod_wait, 11.0 - 0.04);
    value("scenes-shown", "scenes opened in one whole run (theory 7: dock, route, beacon, flare, ilse, silence, quay)", back.scenes, 7.0);
    value("run-time-turn-back", "mission clock at Lamp Quay, turning back for the pod [T] (one run of the bench pilot; no theory)", back.total, 0.0);
    value("run-time-hold", "the same pilot holding the heading [T]", hold.total, 0.0);
    Voyage v;
    voyage_init(&v, VE_ALL);
    value("scene-first", "scene open right after voyage_init (theory VS_DOCK = 0)", v.scene, 0.0);
    value("heavy-scenes", "scenes that voyage_heavy marks (theory 3: flare, ilse, silence)", voyage_heavy(VS_FLARE) + voyage_heavy(VS_ILSE) + voyage_heavy(VS_SILENCE) + voyage_heavy(VS_DOCK) + voyage_heavy(VS_ROUTE) + voyage_heavy(VS_BEACON) + voyage_heavy(VS_QUAY), 3.0);
    value("choice-scenes", "scenes with two options (theory 2: route, ilse)", (voyage_options(VS_ROUTE) == 2) + (voyage_options(VS_ILSE) == 2) + (voyage_options(VS_DOCK) == 2) + (voyage_options(VS_QUAY) == 2), 2.0);
    voyage_answer(&v, 0);
    voyage_tick(&v, 1, VC_RAISE);
    value("cue-cut", "voyage_cue while raising with the apoapsis still under the ring (theory 0, nothing yet)", voyage_cue(&v), 0.0);
    v.ship = tangential(1.0, sqrt(2.0 - 1.0 / 1.26));
    v.command = VC_RAISE;
    value("cue-cut-now", "voyage_cue once the apoapsis has passed the ring 1.5 (theory 1, cut)", voyage_cue(&v), 1.0);
    value("hold-need-ordinary", "voyage_hold_need on an ordinary leg (theory 0)", voyage_hold_need(&v), 0.0);
    value("ending-before", "voyage_ending before Lamp Quay (theory -1)", voyage_ending(&v), -1.0);
}

int main(int argc, char **argv)
{
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "docs/theory/figures");
    values = fopen(path("values.md"), "w");
    if (!values) { fprintf(stderr, "cannot write %s/values.md\n", dir); return 1; }
    fprintf(values, "# Measured values\n\nWritten by `make theory-figs` (tests/theory_figs.c) from the program's own functions. Do not edit by hand.\n"
                    "Units: lengths in L, times in T, with the star's GM = 1. The last column is the relative error; where the theory column is 0 it is the absolute value.\n\n"
                    "| id | what is measured | measured | theory | error |\n|---|---|---|---|---|\n");
    fig_gravity();
    fig_period();
    fig_ellipse();
    fig_escape();
    fig_moon();
    fig_hohmann();
    fig_thrust();
    fig_apsides();
    val_rendezvous();
    val_verlet();
    fig_energy_drift();
    fig_order();
    fig_clearing();
    fig_fuel();
    fig_wobble();
    fig_boil();
    fig_tow();
    val_runs();
    fclose(values);
    return 0;
}
