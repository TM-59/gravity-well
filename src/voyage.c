#include "voyage.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979
#define V_DT 0.004
#define V_SUBSTEPS 10
#define V_THRUST 0.10          /* the tug's engine for this run (the world's thrust parameter, not the integrator) */
#define TANK 0.30              /* fuel at the start, in units of delta-v */
#define RED_LINE 0.25          /* the gauge turns red below this (a crisis in the bench) */
#define TOW_FUEL 0.15          /* a drone from Ilse's dock refuels a dry tank where it drifts, this much */
#define DRONE_WAIT 3.0         /* T the tug drifts, engine dead, until the drone docks */
#define CARRY_FUEL 0.20        /* Lamp Quay's tug drags the tug back to the start of the leg and leaves this much */
#define CLEAR_A 0.04           /* a leg is cleared when the orbit's semi-major axis is within 4 % of the target */
#define CLEAR_E 0.04           /* and its eccentricity is below 0.04 */
#define LEG_LIMIT 45.0         /* T without clearing: Lamp Quay sends a tug (a tow) */
#define SCORCH_R 0.35          /* closer than this to the star: towed */
#define LOST_R 4.5             /* further than this: towed */
#define BASE_WOBBLE 0.35       /* relative wobble of the apsis readout with nobody calling the burns */

#define DOCK_TANK 0.40         /* VE_DOCK: Ilse fills the tank past the line */
#define COUNT_WOBBLE 0.10      /* VE_DOCK: her count steadies the readout (wobble x0.10) */
#define NEAR_R 1.90            /* VE_ROUTE: the near buoy's ring */
#define FAR_R 2.40             /* VE_ROUTE: the far buoy's ring */
#define FAR_BONUS 0.30         /* VE_ROUTE: the far buoy's keeper fires its spare fuel down to you */
#define CACHE_FUEL 0.22        /* VE_CACHE: what is left in the buoy's cache; it keeps the flare from boiling the tank dry */
#define FLARE_WOBBLE 0.08      /* VE_FLARE: extra relative wobble in leg 4 */
#define FLARE_BOIL 0.016       /* VE_FLARE: fuel that boils off per T in leg 4 at r = 2, burning or not (x 4/r^2) */
#define POD_R 1.60             /* VE_POD: leg 4 goes down to Ilse's pod */
#define POD_FUEL 0.32          /* VE_POD: the pod refills the tank by this much */
#define POD_SOFT 0.012         /* VE_POD: the grapple holds only below this eccentricity */
#define POD_HOLD 11.0          /* VE_POD: T of flying on the pod's ring while it drifts up to the grapple */

/* Leg rings: 1.0 -> 1.5 -> the buoy (near 1.9 or far 2.4; 2.2 with no route effect) -> 1.8 -> (2.2, or Ilse's pod at 1.6) -> 2.6. */
static const double TARGET[V_LEGS] = {1.5, 2.2, 1.8, 2.2, 2.6};

/* The flare, Ilse's call and the static get the screen to themselves: no "leg cleared" banner before or after. */
static const int HEAVY[VS_COUNT] = {0, 0, 0, 1, 1, 1, 0};

int voyage_options(int scene) { return scene == VS_ROUTE || scene == VS_ILSE ? 2 : 1; }
int voyage_heavy(int scene) { return scene >= 0 && scene < VS_COUNT && HEAVY[scene]; }

int voyage_ending(const Voyage *v)
{
    return v->scene == VS_QUAY || v->leg >= V_LEGS ? v->turned : -1;
}

static int on(const Voyage *v, unsigned effect) { return (v->effects & effect) != 0; }

static void elements(const Voyage *v, double *a, double *e, double *apo, double *peri)
{
    const Ship *s = &v->ship;
    double r = hypot(s->x, s->y), v2 = s->vx * s->vx + s->vy * s->vy;
    double energy = 0.5 * v2 - v->world.star_gm / r, h = s->x * s->vy - s->y * s->vx;
    double ecc2 = 1.0 + 2.0 * energy * h * h / (v->world.star_gm * v->world.star_gm);
    *e = sqrt(fmax(0.0, ecc2));
    if (energy >= 0.0) { *a = 1e9; *apo = 1e9; *peri = h * h / (v->world.star_gm * (1.0 + *e)); return; }
    *a = -v->world.star_gm / (2.0 * energy);
    *apo = *a * (1.0 + *e);
    *peri = *a * (1.0 - *e);
}

double voyage_wobble(const Voyage *v)
{
    if (v->leg >= V_LEGS) return 0.0;
    double amp = BASE_WOBBLE;
    if (v->count && on(v, VE_DOCK)) amp *= COUNT_WOBBLE;
    if (v->leg == 3 && on(v, VE_FLARE)) amp += FLARE_WOBBLE;
    /* The same smooth wobble for everyone who reads the gauge: it depends only on the mission clock. */
    double t = v->time;
    return amp * (0.6 * sin(2.3 * t + 0.4) + 0.4 * sin(5.9 * t + 1.7));
}

double voyage_boil(const Voyage *v)
{
    /* The flare's heat falls off as 1/r^2 (FLARE_BOIL at r = 2); grappled to Ilse's pod, the tug sits in its shadow. */
    if (v->leg != 3 || !on(v, VE_FLARE) || v->held > 0.0) return 0.0;
    double r2 = v->ship.x * v->ship.x + v->ship.y * v->ship.y;
    return FLARE_BOIL * 4.0 / r2;
}

/* Direction and size of the burn that would make the orbit circular at the current radius. */
static void match_vector(const Voyage *v, double *dx, double *dy)
{
    const Ship *s = &v->ship;
    double r = hypot(s->x, s->y), vc = circular_speed(&v->world, r);
    double sign = s->x * s->vy - s->y * s->vx >= 0.0 ? 1.0 : -1.0;
    *dx = sign * vc * (-s->y / r) - s->vx;
    *dy = sign * vc * (s->x / r) - s->vy;
}

int voyage_cue(const Voyage *v)
{
    /* Ilse's count, from the true orbit (not the wobbling gauge): "cut" when the burn has put the far side of the orbit
     * on the ring, "now" when the tug reaches the ring. Nothing once her channel is gone. */
    if (!v->count || !on(v, VE_DOCK) || v->scene != VS_NONE || v->leg >= V_LEGS) return 0;
    double a, e, apo, peri, r = hypot(v->ship.x, v->ship.y), t = v->target_r;
    elements(v, &a, &e, &apo, &peri);
    if (v->command == VC_RAISE && apo >= t) return 1;
    if (v->command == VC_LOWER && peri <= t) return 1;
    if (v->command == VC_COAST && fabs(r - t) < 0.02 * t && e >= CLEAR_E) return 2;
    return 0;
}

double voyage_hold_need(const Voyage *v)
{
    return v->leg == 3 && v->turned == 0 && on(v, VE_POD) ? POD_HOLD : 0.0;
}

static void open_scene(Voyage *v, int scene)
{
    v->scene = scene;
    if (v->order_len < VS_COUNT) v->order[v->order_len++] = scene;
    if (voyage_heavy(scene)) { v->banner = VB_NONE; v->banner_left = 0.0; }
}

static void raise_banner(Voyage *v, int banner)
{
    v->banner = banner;
    v->banner_left = 1.5;
    if (banner == VB_CLEAR) v->loud++;
}

static void start_leg(Voyage *v)
{
    v->from_r = hypot(v->ship.x, v->ship.y);
    v->target_r = TARGET[v->leg];
    if (v->leg == 3 && v->turned == 0 && on(v, VE_POD)) v->target_r = POD_R;
    if (v->leg == 1 && v->route >= 0 && on(v, VE_ROUTE)) v->target_r = v->route == 0 ? NEAR_R : FAR_R;
    v->leg_time = 0.0;
    v->held = 0.0;
    v->missed = 0;
}

void voyage_init(Voyage *v, unsigned effects)
{
    memset(v, 0, sizeof *v);
    v->world = world_default();
    v->world.moon_gm = 0.0;
    v->world.thrust = V_THRUST;
    v->effects = effects;
    v->route = v->turned = -1;
    v->ship = (Ship){1.0, 0.0, 0.0, circular_speed(&v->world, 1.0)};
    v->fuel = TANK;
    v->scene = VS_NONE;
    v->from_r = v->target_r = 1.0;
    open_scene(v, VS_DOCK);
}

static void clear_leg(Voyage *v)
{
    static const int next[V_LEGS] = {VS_ROUTE, VS_BEACON, VS_ILSE, VS_SILENCE, VS_QUAY};
    int scene = next[v->leg];
    v->leg++;
    /* The heavy scenes and the ending get the screen to themselves: no "leg cleared" banner. */
    if (!voyage_heavy(scene) && scene != VS_QUAY) raise_banner(v, VB_CLEAR);
    open_scene(v, scene);
}

/* A tow: Lamp Quay flies fuel out to the tug. A dry tank is refilled where the tug is; a tug that fell too close to the
 * star, flew off, or wandered for too long is also carried back to the start of the leg. Both are failures. */
static void tow(Voyage *v, int carry)
{
    if (carry) {
        double angle = atan2(v->ship.y, v->ship.x), r = v->from_r, s = circular_speed(&v->world, r);
        v->ship = (Ship){r * cos(angle), r * sin(angle), -s * sin(angle), s * cos(angle)};
        v->leg_time = 0.0;
    }
    if (carry) {
        if (v->fuel < CARRY_FUEL) v->fuel = CARRY_FUEL;
    } else {
        v->drift = DRONE_WAIT;  /* the drone takes this long to reach the tug; the fuel goes in when it docks */
        v->refuel = 1;
    }
    v->tows++;
    v->tows_leg[v->leg]++;
    v->crisis_leg[v->leg] = 1;
    raise_banner(v, VB_TOW);
}

void voyage_tick(Voyage *v, int frames, int command)
{
    if (frames < 0 || v->scene != VS_NONE || v->leg >= V_LEGS) return;
    if (frames > 250) frames = 250;
    if (command < VC_COAST || command > VC_MATCH) command = VC_COAST;
    for (int f = 0; f < frames && v->scene == VS_NONE && v->leg < V_LEGS; f++) {
        int leg = v->leg;
        v->command = command;
        for (int k = 0; k < V_SUBSTEPS; k++) {
            double heading = atan2(v->ship.vy, v->ship.vx), burn = 0.0;
            if (v->drift > 0.0 && (v->drift -= V_DT) <= 0.0) {
                v->drift = 0.0;
                if (v->refuel && v->fuel < TOW_FUEL) v->fuel = TOW_FUEL;
                v->refuel = 0;
            }
            if (v->fuel > 0.0 && v->drift <= 0.0 && command != VC_COAST) {
                if (command == VC_RAISE) burn = 1.0;
                if (command == VC_LOWER) { burn = 1.0; heading += PI; }
                if (command == VC_MATCH) {
                    double dx, dy;
                    match_vector(v, &dx, &dy);
                    double need = hypot(dx, dy);
                    heading = atan2(dy, dx);
                    burn = fmin(1.0, need / (v->world.thrust * V_DT));
                }
                burn = fmin(burn, v->fuel / (v->world.thrust * V_DT));
            }
            orbit_step(&v->world, &v->ship, v->time, heading, burn, V_DT);
            v->fuel = fmax(0.0, v->fuel - burn * v->world.thrust * V_DT - voyage_boil(v) * V_DT);
            v->time += V_DT;
            v->leg_time += V_DT;
            if (v->fuel < RED_LINE) v->red += V_DT;
            if (v->fuel < RED_LINE) v->crisis_leg[leg] = 1;
        }
        v->frames++;
        if (v->banner_left > 0.0 && (v->banner_left -= V_DT * V_SUBSTEPS) <= 0.0) v->banner = VB_NONE;
        double r = hypot(v->ship.x, v->ship.y), a, e, apo, peri;
        elements(v, &a, &e, &apo, &peri);
        int on_ring = fabs(a - v->target_r) < CLEAR_A * v->target_r && e < CLEAR_E;
        double need = voyage_hold_need(v), frame = V_DT * V_SUBSTEPS;
        if (v->held > 0.0) {
            /* Grappled to the pod: the leg ends when its fuel is across. The clock keeps running while you wait. */
            if ((v->held += frame) >= need - 1e-9) { clear_leg(v); continue; }
        } else if (on_ring && need <= 0.0) {
            clear_leg(v);
            continue;
        } else if (on_ring && command == VC_COAST && v->drift <= 0.0) {
            /* The pod reaches only for a tug on its ring with the engine off. Come in with too much eccentricity and the
             * grapple glances off: a failure, and a drone brings the pod round again while the tug drifts (the second
             * pass always holds). */
            if (e > POD_SOFT && !v->missed) {
                v->missed = 1;
                v->tows++;
                v->tows_leg[v->leg]++;
                v->crisis_leg[v->leg] = 1;
                v->drift = DRONE_WAIT;
                raise_banner(v, VB_MISS);
            } else {
                v->fuel += POD_FUEL;
                v->held = frame;
            }
        }
        if (r < SCORCH_R || r > LOST_R || v->leg_time > LEG_LIMIT) tow(v, 1);
        /* While Ilse is on the channel her dock's drone refuels a dry tug where it drifts. After the silence only
         * Lamp Quay's tug can come, and it drags the tug back to the start of the leg (VE_SILENCE). */
        else if (v->fuel <= 0.0 && v->drift <= 0.0) tow(v, !v->count);
    }
}

int voyage_answer(Voyage *v, int option)
{
    int scene = v->scene;
    if (scene == VS_NONE || option < 0 || option >= voyage_options(scene)) return 1;
    v->scene = VS_NONE;
    switch (scene) {
    case VS_DOCK:
        v->count = 1;
        if (on(v, VE_DOCK)) v->fuel = DOCK_TANK;
        start_leg(v);
        break;
    case VS_ROUTE:
        v->route = option;
        /* The far buoy's keeper fires its spare fuel down to a tug that commits to the long climb. */
        if (on(v, VE_ROUTE) && option == 1) v->fuel += FAR_BONUS;
        start_leg(v);
        break;
    case VS_BEACON:
        /* What is left in the buoy's cache goes into the tank. */
        if (on(v, VE_CACHE)) v->fuel += CACHE_FUEL;
        open_scene(v, VS_FLARE);
        break;
    case VS_FLARE:
        start_leg(v);
        break;
    case VS_ILSE:
        v->turned = option;
        start_leg(v);
        break;
    case VS_SILENCE:
        if (on(v, VE_SILENCE)) v->count = 0;
        start_leg(v);
        break;
    case VS_QUAY:
        break;
    default:
        return 1;
    }
    return 0;
}

double voyage_value(const Voyage *v, int field)
{
    double a, e, apo, peri, mx, my;
    elements(v, &a, &e, &apo, &peri);
    moon_position(&v->world, v->time, &mx, &my);
    const Ship *s = &v->ship;
    double r = hypot(s->x, s->y), w = voyage_wobble(v);
    switch (field) {
    case VV_X: return s->x;
    case VV_Y: return s->y;
    case VV_VX: return s->vx;
    case VV_VY: return s->vy;
    case VV_TIME: return v->time;
    case VV_HEADING: {
        double h = atan2(s->vy, s->vx);
        if (v->command == VC_LOWER) h += PI;
        if (v->command == VC_MATCH) { double dx, dy; match_vector(v, &dx, &dy); h = atan2(dy, dx); }
        return remainder(h, 2.0 * PI);
    }
    case VV_RADIUS: return r;
    case VV_SPEED: return hypot(s->vx, s->vy);
    case VV_FUEL: return v->fuel;
    case VV_RED: return v->red;
    case VV_LEG: return v->leg;
    case VV_SCENE: return v->scene;
    case VV_BANNER: return v->banner;
    case VV_TOWS: return v->tows;
    case VV_FROM: return v->from_r;
    case VV_TARGET: return v->target_r;
    case VV_APO: return apo;
    case VV_PERI: return peri;
    case VV_APO_READ: return apo * (1.0 + w);
    case VV_PERI_READ: return peri * (1.0 - w);
    case VV_MATCH_ERROR: { double dx, dy; match_vector(v, &dx, &dy); return hypot(dx, dy); }
    case VV_ROUTE: return v->route;
    case VV_TURNED: return v->turned;
    case VV_COUNT_ON: return v->count && on(v, VE_DOCK);
    case VV_WOBBLE: return w;
    case VV_ENDING: return voyage_ending(v);
    case VV_LOUD: return v->loud;
    case VV_HEAVY: return voyage_heavy(v->scene);
    case VV_COMMAND: return v->command;
    case VV_SEMI_MAJOR: return a;
    case VV_ECC: return e;
    case VV_PERI_ANGLE: {
        /* Direction of the eccentricity vector (towards periapsis), for drawing the predicted ellipse. */
        double mu = v->world.star_gm, vr = s->x * s->vx + s->y * s->vy, v2 = s->vx * s->vx + s->vy * s->vy;
        double ex = ((v2 - mu / r) * s->x - vr * s->vx) / mu, ey = ((v2 - mu / r) * s->y - vr * s->vy) / mu;
        return atan2(ey, ex);
    }
    case VV_RADIAL: return (s->x * s->vx + s->y * s->vy) / r;
    case VV_MOON_X: return mx;
    case VV_MOON_Y: return my;
    case VV_RED_LINE: return RED_LINE;
    case VV_HELD: return v->held;
    case VV_HOLD_NEED: return voyage_hold_need(v);
    case VV_BOIL: return voyage_boil(v);
    case VV_DRIFT: return v->drift;
    case VV_CUE: return voyage_cue(v);
    default: return 0.0;
    }
}
