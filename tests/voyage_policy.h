#ifndef VOYAGE_POLICY_H
#define VOYAGE_POLICY_H

/* Shared by the story test and the story bench: seeds and a human-like pilot. */
#include "../src/voyage.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static inline uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* Seed and index are stirred separately before they are combined (no XOR or sums of raw keys). */
static inline uint64_t sub_seed(uint64_t seed, uint64_t index, uint64_t salt)
{
    return mix64(mix64(seed + salt * 0x632be59bd9b4e019ULL) + (index + 1u) * 0x9e3779b97f4a7c15ULL);
}

typedef struct { uint64_t s; } Rng;
static inline double rng_unit(Rng *r)
{
    r->s += 0x9e3779b97f4a7c15ULL;
    return (double)(mix64(r->s) >> 11) / 9007199254740992.0;
}

/* A pilot who reads the gauges, not the numbers in the code: burns until the apsis readout reaches the target ring
 * (a little early or late), coasts until the orbit nears the target ring, then holds Match until the match error
 * looks small. If the orbit turns before reaching the ring, it circularizes where it is and tries again. */
enum { P_TRANSFER, P_COAST, P_MATCH };

typedef struct {
    Rng rng;
    uint64_t key;
    int leg, tows, phase, out, wait, late, idle, coast;
    double bias, lead, tol, rise;
    int hesitate, overrun;
} Pilot;

static inline void pilot_plan(Pilot *p)
{
    p->bias = -0.03 + 0.11 * rng_unit(&p->rng);   /* releases the transfer burn this far past (or short of) the target ring */
    p->lead = 0.05 * rng_unit(&p->rng);           /* starts matching this early before the ring */
    p->tol = 0.004 + 0.016 * rng_unit(&p->rng);   /* stops matching when the error reads below this */
    p->hesitate = (int)(6.0 * rng_unit(&p->rng)); /* frames before reacting at the ring */
    p->overrun = (int)(3.0 * rng_unit(&p->rng));  /* frames the thumb stays on a burn after the gauge says stop */
    p->phase = P_TRANSFER;
    p->wait = p->late = 0;
    p->rise = 0.0;
}

static inline void pilot_init(Pilot *p, uint64_t key)
{
    memset(p, 0, sizeof *p);
    p->key = key;
    p->leg = -1;
    p->rng.s = key;
}

static inline int pilot_command(Pilot *p, const Voyage *v)
{
    /* Each leg gets its own stirred stream, so a choice changes the legs it touches and not, by chance, every later one. */
    if (v->leg != p->leg || v->tows != p->tows) {
        /* A new leg, or a tow back to the start of this one: start the plan again. */
        if (v->leg != p->leg) p->rng.s = sub_seed(p->key, (uint64_t)v->leg, 5);
        p->leg = v->leg;
        p->tows = v->tows;
        pilot_plan(p);
    }
    double r = voyage_value(v, VV_RADIUS), target = v->target_r;
    double radial = voyage_value(v, VV_RADIAL);
    /* On the pod's ring: engine off and wait for the grapple while the orbit stays good. */
    if (voyage_hold_need(v) > 0.0 && fabs(voyage_value(v, VV_SEMI_MAJOR) - target) < 0.04 * target &&
        voyage_value(v, VV_ECC) < 0.04 && (p->phase != P_MATCH || voyage_value(v, VV_MATCH_ERROR) < p->tol)) {
        p->phase = P_TRANSFER;
        p->late = 0;
        return VC_COAST;
    }
    if (p->phase == P_TRANSFER) {
        if (p->late == 0) p->out = target > r;
        /* With Ilse calling the burns, cut on her word; alone, on the gauge. */
        int done = voyage_cue(v) == 1 ||
                   (!voyage_value(v, VV_COUNT_ON) ? (p->out ? voyage_value(v, VV_APO_READ) >= target * (1.0 + p->bias)
                                        : voyage_value(v, VV_PERI_READ) <= target * (1.0 - p->bias)) : 0);
        if (done && ++p->late > p->overrun) { p->phase = P_COAST; p->wait = p->idle = p->coast = 0; p->rise = 0.0; return VC_COAST; }
        if (!done) p->late = 0;
        return p->out ? VC_RAISE : VC_LOWER;
    }
    if (p->phase == P_COAST) {
        if (fabs(radial) > p->rise) p->rise = fabs(radial);
        int at_ring = voyage_cue(v) == 2 || (p->out ? r >= target * (1.0 - p->lead) : r <= target * (1.0 + p->lead));
        /* Passed the apsis without reaching the ring (the radial speed has turned): circularize here and try again. */
        int turned = p->rise > 0.01 && (p->out ? radial < -0.002 : radial > 0.002);
        /* Nothing is happening (the burn was released at once on a wobbling gauge), or the ring is still far after
         * three T of coasting: burn again. */
        ++p->coast;
        if (!at_ring && ((p->rise < 0.003 && ++p->idle > 25) || p->coast > 75)) {
            p->phase = P_TRANSFER;
            p->late = p->idle = 0;
            return VC_COAST;
        }
        if (at_ring || turned) {
            if (++p->wait > p->hesitate) { p->phase = P_MATCH; return VC_MATCH; }
        }
        return VC_COAST;
    }
    if (voyage_value(v, VV_MATCH_ERROR) < p->tol) {
        /* Circular, but not on the ring: plan another transfer from here with a fresh hand. */
        p->bias = -0.03 + 0.11 * rng_unit(&p->rng);
        p->lead = 0.05 * rng_unit(&p->rng);
        p->phase = P_TRANSFER;
        p->late = 0;
        return VC_COAST;
    }
    return VC_MATCH;
}

#endif
