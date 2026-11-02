/* "The Ice Run" story layer: scene order, the numbers each story beat changes, quiet heavy scenes, choices. */
#include "voyage_run.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const int DESIGN[VS_COUNT] = {VS_DOCK, VS_ROUTE, VS_BEACON, VS_FLARE, VS_ILSE, VS_SILENCE, VS_QUAY};

static int close_to(double a, double b) { return fabs(a - b) <= 1e-9 * fmax(1.0, fabs(b)); }

/* Fly with the pilot, answering scenes with the given choices, until `target` opens. Returns frames flown. */
static int drive(Voyage *v, Pilot *p, int target, int route, int turned)
{
    int frames = 0;
    while (v->scene != target) {
        assert(v->leg < V_LEGS && frames < RUN_GUARD_FRAMES);
        if (v->scene != VS_NONE) {
            assert(voyage_answer(v, v->scene == VS_ROUTE ? route : v->scene == VS_ILSE ? turned : 0) == 0);
            continue;
        }
        double t = v->time;
        voyage_tick(v, 1, pilot_command(p, v));
        assert(fabs(v->time - t - 0.04) < 1e-9);  /* every frame is 0.04 T of flight: no flat waits */
        frames++;
    }
    return frames;
}

static void start(Voyage *v, Pilot *p, unsigned effects)
{
    voyage_init(v, effects);
    pilot_init(p, play_key(0));
    assert(v->scene == VS_DOCK && voyage_options(VS_DOCK) == 1);
}

static char *slurp(const char *path)
{
    static char text[2][1 << 17];
    static int slot;
    FILE *f = fopen(path, "rb");
    assert(f);
    char *buf = text[slot++ & 1];
    size_t len = fread(buf, 1, sizeof text[0] - 1, f);
    fclose(f);
    buf[len] = 0;
    return buf;
}

/* Reads one choice's two paths from ablation.json; checks each path wins one outcome and neither dominates. */
static void check_choice(const char *text, const char *head)
{
    double v[2][ABLATION_SEEDS][2];
    const char *at = strstr(text, head);
    assert(at);
    for (int path = 0; path < 2; path++) {
        char key[16];
        snprintf(key, sizeof key, "\"%d\": [", path);
        at = strstr(at, key);
        assert(at);
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            at = strstr(at, "\"seconds\": ");
            assert(at && sscanf(at, "\"seconds\": %lf, \"red\": %lf", &v[path][i][0], &v[path][i][1]) == 2);
            at++;
        }
    }
    int wins[2] = {0, 0};
    for (int m = 0; m < 2; m++) {
        int lower0 = 0, lower1 = 0;
        double diff = 0.0, base = 0.0;
        for (int i = 0; i < ABLATION_SEEDS; i++) {
            lower0 += v[0][i][m] < v[1][i][m];
            lower1 += v[1][i][m] < v[0][i][m];
            diff += v[0][i][m] - v[1][i][m];
            base += v[1][i][m];
        }
        int k = lower0 > lower1 ? lower0 : lower1;
        double tail = 0.0;
        for (int x = k; x <= ABLATION_SEEDS; x++) {
            double comb = 1.0;
            for (int y = 0; y < x; y++) comb = comb * (ABLATION_SEEDS - y) / (y + 1);
            tail += comb;
        }
        double p = 2.0 * tail / pow(2.0, ABLATION_SEEDS);
        double rel = fabs(diff / ABLATION_SEEDS) / fmax(fabs(base / ABLATION_SEEDS), 1.0);
        if (p <= 0.05 && rel >= 0.05) wins[lower0 > lower1 ? 0 : 1]++;
    }
    int dominated[2] = {0, 0};
    for (int i = 0; i < ABLATION_SEEDS; i++)
        for (int hi = 0; hi < 2; hi++) {
            const double *x = v[hi][i], *y = v[1 - hi][i];
            dominated[hi] += x[0] <= y[0] && x[1] <= y[1] && (x[0] < y[0] || x[1] < y[1]);
        }
    assert(wins[0] == 1 && wins[1] == 1);
    assert(dominated[0] < 0.85 * ABLATION_SEEDS && dominated[1] < 0.85 * ABLATION_SEEDS);
    printf("[choice] %s each path wins one outcome; better-or-equal on both in %d/%d and %d/%d seeds\n",
           head, dominated[0], ABLATION_SEEDS, dominated[1], ABLATION_SEEDS);
}

int main(void)
{
    /* [order] the bench's play-order.json (re-checked against the code by story_bench --check): every seed meets the
     * scenes in the designed order (setups before payoffs, the bottom before the end) and both endings are reached. */
    {
        const char *text = slurp("docs/narrative/play-order.json");
        char want[256] = "\"order\": [";
        for (int k = 0; k < VS_COUNT; k++) {
            strcat(want, k ? ", \"" : "\"");
            strcat(want, SCENE_ID[DESIGN[k]]);
            strcat(want, "\"");
        }
        strcat(want, "], \"won\": true");
        int seeds = 0, orders = 0;
        for (const char *c = text; (c = strstr(c, "\"seed\": ")); c++) seeds++;
        for (const char *c = text; (c = strstr(c, want)); c++) orders++;
        assert(seeds >= 8 && orders == seeds);
        assert(strstr(text, "\"ending\": \"end-turned\", \"quiet\": [\"ilse\", \"silence\"]"));
        assert(strstr(text, "\"ending\": \"end-held\", \"quiet\": [\"ilse\", \"silence\"]"));
        printf("[order] %d seeds: dock .. quay in the designed order; both endings reached\n", seeds);
        RunResult r;
        run_voyage(play_key(3), VE_ALL, 1, 0, &r);
        assert(r.completed && r.order_len == VS_COUNT && r.ending == 0 && r.quiet[VS_ILSE] && r.quiet[VS_SILENCE]);
        for (int k = 0; k < VS_COUNT; k++) assert(r.order[k] == DESIGN[k]);
    }

    /* [mech] dock: Ilse adds what the dock can spare (0.30 -> 0.40), and her count steadies the readout (x0.10). */
    {
        Voyage a, b; Pilot p;
        start(&a, &p, VE_ALL); start(&b, &p, VE_ALL & ~VE_DOCK);
        assert(close_to(a.fuel, 0.30) && close_to(b.fuel, 0.30));
        assert(voyage_answer(&a, 0) == 0 && voyage_answer(&b, 0) == 0);
        assert(close_to(a.fuel, 0.40) && close_to(b.fuel, 0.30) && a.count == 1 && b.count == 1);
        voyage_tick(&a, 7, VC_COAST); voyage_tick(&b, 7, VC_COAST);
        assert(fabs(voyage_wobble(&b)) > 1e-3 && close_to(voyage_wobble(&a) / voyage_wobble(&b), 0.10));
        assert(a.target_r == 1.5 && voyage_value(&a, VV_FROM) == 1.0);
        puts("[mech] dock: tank 0.30 -> 0.40; Ilse's count: readout wobble x0.10");
    }

    /* [mech] route: the near buoy is a short climb (ring 1.9); the far buoy a long one (2.4), and its keeper fires 0.30 of
     * fuel down at once. With no route effect the buoy sits at 2.2 and nothing is sent. */
    {
        Voyage a, b, c; Pilot p, q, s;
        start(&a, &p, VE_ALL); start(&b, &q, VE_ALL); start(&c, &s, VE_ALL & ~VE_ROUTE);
        drive(&a, &p, VS_ROUTE, 0, 1); drive(&b, &q, VS_ROUTE, 1, 1); drive(&c, &s, VS_ROUTE, 1, 1);
        assert(voyage_options(VS_ROUTE) == 2 && !voyage_heavy(VS_ROUTE));
        double fa = a.fuel, fb = b.fuel, fc = c.fuel;
        assert(voyage_answer(&a, 0) == 0 && voyage_answer(&b, 1) == 0 && voyage_answer(&c, 1) == 0);
        assert(a.route == 0 && b.route == 1);
        assert(close_to(a.target_r, 1.9) && close_to(b.target_r, 2.4) && close_to(c.target_r, 2.2));
        assert(close_to(a.fuel, fa) && close_to(b.fuel - fb, 0.30) && close_to(c.fuel, fc));
        puts("[mech] route: near ring 1.9, no fuel; far ring 2.4 and +0.30 fuel sent down");
    }

    /* [mech] beacon: what is left in the buoy's cache, +0.22, goes into the tank; then the flare scene opens. */
    {
        Voyage a, b; Pilot p, q;
        start(&a, &p, VE_ALL); start(&b, &q, VE_ALL & ~VE_CACHE);
        drive(&a, &p, VS_BEACON, 1, 1); drive(&b, &q, VS_BEACON, 1, 1);
        assert(a.banner == VB_CLEAR);
        double fa = a.fuel, fb = b.fuel;
        assert(voyage_answer(&a, 0) == 0 && voyage_answer(&b, 0) == 0);
        assert(close_to(a.fuel - fa, 0.22) && close_to(b.fuel, fb) && a.scene == VS_FLARE && b.scene == VS_FLARE);
        puts("[mech] beacon: +0.22 fuel from the cache");
    }

    /* [mech] flare: in leg 4 only, fuel boils off at 0.016 x 4/r^2 per T, burning or not, and the readout wobbles more. */
    {
        Voyage a, b; Pilot p, q;
        start(&a, &p, VE_ALL); start(&b, &q, VE_ALL & ~VE_FLARE);
        drive(&a, &p, VS_FLARE, 1, 1); drive(&b, &q, VS_FLARE, 1, 1);
        assert(voyage_answer(&a, 0) == 0 && voyage_answer(&b, 0) == 0);
        assert(a.leg == 2 && voyage_boil(&a) == 0.0);  /* leg 3, below the front, is calm */
        drive(&a, &p, VS_ILSE, 1, 1); drive(&b, &q, VS_ILSE, 1, 1);
        assert(voyage_answer(&a, 1) == 0 && voyage_answer(&b, 1) == 0);
        assert(a.leg == 3 && b.leg == 3);
        double r2 = a.ship.x * a.ship.x + a.ship.y * a.ship.y, f = a.fuel, t = a.time;
        assert(close_to(voyage_boil(&a), 0.016 * 4.0 / r2) && voyage_boil(&b) == 0.0);
        voyage_tick(&a, 1, VC_COAST);
        assert(f - a.fuel > 0.0 && fabs((f - a.fuel) - 0.016 * 4.0 / r2 * (a.time - t)) < 1e-4);
        b.time = a.time;
        assert(fabs(voyage_wobble(&a)) > fabs(voyage_wobble(&b)));
        puts("[mech] flare: leg 4 boil-off 0.016 x 4/r^2 per T and a wilder readout");
    }

    /* [mech] ilse, both paths. Turning back: leg 4 goes down to the pod's ring at 1.6; with the engine off and a round
     * orbit the pod grapples, adds 0.32 of fuel and holds the tug for 11 T of flying (the flare cannot boil it there). Come in
     * with eccentricity over 0.012 and the grapple glances off once: a failure, and the tug drifts 3 T. Holding the heading:
     * leg 4 goes to 2.2 and nothing more is sent. [quiet] ilse: no "leg cleared" banner when the call comes, nor after. */
    for (int turned = 0; turned < 2; turned++) {
        Voyage a; Pilot p;
        start(&a, &p, VE_ALL);
        drive(&a, &p, VS_ILSE, 1, turned);
        assert(voyage_options(VS_ILSE) == 2 && voyage_heavy(VS_ILSE) && a.banner == VB_NONE);
        int loud = a.loud;
        assert(voyage_answer(&a, turned) == 0);
        assert(a.turned == turned && close_to(a.target_r, turned ? 2.2 : 1.6));
        assert(close_to(voyage_hold_need(&a), turned ? 0.0 : 11.0));
        voyage_tick(&a, 1, pilot_command(&p, &a));
        assert(a.loud == loud && a.banner != VB_CLEAR);
        if (turned == 0) {
            /* Put the tug on the pod's ring with a slightly eccentric orbit: the first grapple glances off. */
            Voyage g = a;
            double r = 1.6, s = sqrt(1.0 / r) * 1.012;
            g.ship = (Ship){r, 0.0, 0.0, s};
            g.fuel = 0.1;
            int tows = g.tows;
            voyage_tick(&g, 1, VC_COAST);
            assert(g.tows == tows + 1 && g.tows_leg[3] == 1 && g.missed && close_to(g.drift, 3.0) && g.banner == VB_MISS);
            assert(g.held == 0.0 && g.fuel < 0.1);
            /* The second pass always holds: once the drone has brought the pod round (no fuel from it), the pod grapples,
             * pumps 0.32 across and shields the tug from the flare. */
            double f = g.fuel;
            while (g.held == 0.0) { f = g.fuel; voyage_tick(&g, 1, VC_COAST); }
            assert(g.drift == 0.0 && g.fuel - f > 0.32 - 0.002 && g.fuel - f < 0.32 && voyage_boil(&g) == 0.0);
            double t = g.time;
            while (g.scene == VS_NONE) voyage_tick(&g, 1, VC_COAST);
            assert(g.scene == VS_SILENCE && g.time - t >= 11.0 - 0.04 - 1e-9);
            /* A round orbit with the engine on: the pod waits for the engine to go quiet. */
            Voyage h = a;
            h.ship = (Ship){r, 0.0, 0.0, sqrt(1.0 / r)};
            voyage_tick(&h, 1, VC_MATCH);
            assert(h.held == 0.0 && h.tows == a.tows);
            voyage_tick(&h, 1, VC_COAST);
            assert(h.held > 0.0 && h.tows == a.tows && !h.missed);
            puts("[quiet] ilse: shown with no cleared banner");
        }
    }
    puts("[mech] ilse: turn back = the pod's ring 1.6, +0.32 fuel on a quiet grapple, an 11 T wait, a glancing grapple is a "
         "failure; hold = ring 2.2, nothing more");

    /* [mech] silence: once Ilse's channel is gone, a dry tank brings Lamp Quay's tug, which drags the tug back to the start
     * of the leg and leaves 0.20; before, her dock's drone refuels it in place with 0.15 after 3 T adrift.
     * [quiet] silence: no banner when the static comes, nor after. */
    {
        Voyage a, b; Pilot p, q;
        start(&a, &p, VE_ALL); start(&b, &q, VE_ALL & ~VE_SILENCE);
        drive(&a, &p, VS_SILENCE, 1, 1); drive(&b, &q, VS_SILENCE, 1, 1);
        assert(voyage_heavy(VS_SILENCE) && a.banner == VB_NONE && b.banner == VB_NONE);
        int loud = a.loud;
        assert(voyage_answer(&a, 0) == 0 && voyage_answer(&b, 0) == 0);
        assert(a.count == 0 && b.count == 1 && a.leg == 4);
        voyage_tick(&a, 1, VC_COAST);
        assert(a.loud == loud && a.banner != VB_CLEAR);
        puts("[quiet] silence: shown with no cleared banner");
        a.fuel = b.fuel = 0.0005;
        double from = a.from_r;
        voyage_tick(&a, 1, VC_RAISE); voyage_tick(&b, 1, VC_RAISE);
        assert(a.tows_leg[4] == 1 && close_to(hypot(a.ship.x, a.ship.y), from) && close_to(a.fuel, 0.20) && a.drift == 0.0);
        assert(b.tows_leg[4] == 1 && close_to(b.drift, 3.0) && b.fuel == 0.0);
        while (b.drift > 0.0) voyage_tick(&b, 1, VC_RAISE);
        assert(b.fuel > 0.14 && b.fuel <= 0.15);
        puts("[mech] silence: a dry tank now means Lamp Quay's tug drags you back to the start of the leg");
    }

    /* [choice] both paths of each choice trade one measured outcome for the other in the bench's ablation.json. */
    {
        const char *text = slurp("docs/narrative/ablation.json");
        check_choice(text, "\"route\": {\"paths\"");
        check_choice(text, "\"ilse\": {\"paths\"");
    }

    /* An ordinary cleared leg is loud, so the quiet checks above are not vacuous; out-of-range answers are refused. */
    {
        Voyage a; Pilot p;
        start(&a, &p, VE_ALL);
        drive(&a, &p, VS_ROUTE, 0, 0);
        assert(a.banner == VB_CLEAR && a.loud == 1);
        assert(voyage_answer(&a, 2) == 1 && voyage_answer(&a, -1) == 1);
        voyage_tick(&a, -1, VC_RAISE);
        assert(a.scene == VS_ROUTE);
    }
    puts("voyage tests passed");
    return 0;
}
