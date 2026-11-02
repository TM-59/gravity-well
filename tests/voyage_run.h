#ifndef VOYAGE_RUN_H
#define VOYAGE_RUN_H

/* One whole run with the human-like pilot; shared by the story test and bench. */
#include "voyage_policy.h"

#include <string.h>

enum { RUN_GUARD_FRAMES = 12000 };  /* 480 T of flying: counted as "did not finish" */

typedef struct {
    double seconds, red;
    int tows, completed, ending;
    int crisis[V_LEGS], failure[V_LEGS];
    int order[VS_COUNT], order_len;
    int quiet[VS_COUNT];  /* heavy scene shown with no "leg cleared" banner, before or after it */
} RunResult;

static const char *const SCENE_ID[VS_COUNT] = {"dock", "route", "beacon", "flare", "ilse", "silence", "quay"};

static inline void run_voyage(uint64_t key, unsigned effects, int route, int turned, RunResult *out)
{
    Voyage v;
    Pilot p;
    memset(out, 0, sizeof *out);
    out->ending = -1;
    voyage_init(&v, effects);
    pilot_init(&p, key);
    int frames = 0;
    while (v.leg < V_LEGS && frames < RUN_GUARD_FRAMES) {
        if (v.scene != VS_NONE) {
            int scene = v.scene, loud = v.loud, calm = v.banner == VB_NONE;
            int option = scene == VS_ROUTE ? route : scene == VS_ILSE ? turned : 0;
            voyage_answer(&v, option);
            if (voyage_heavy(scene)) {
                voyage_tick(&v, 1, pilot_command(&p, &v));  /* the first frame after the scene closes */
                frames++;
                out->quiet[scene] = calm && v.loud == loud && v.banner != VB_CLEAR;
            }
            continue;
        }
        voyage_tick(&v, 1, pilot_command(&p, &v));
        frames++;
    }
    out->completed = v.leg >= V_LEGS;
    out->seconds = v.time;
    out->red = v.red;
    out->tows = v.tows;
    out->ending = voyage_ending(&v);
    for (int l = 0; l < V_LEGS; l++) { out->crisis[l] = v.crisis_leg[l]; out->failure[l] = v.tows_leg[l]; }
    out->order_len = v.order_len;
    memcpy(out->order, v.order, sizeof v.order);
}

/* Seeds: every row and run gets its own stirred key (no XOR or sums of raw keys). */
#define STORY_BASE 20261102ULL
enum { SALT_SEED = 1, SALT_PLAY = 2, SALT_ABLATION = 3, SALT_TENSION = 4 };
static inline uint64_t story_seed(int i) { return sub_seed(STORY_BASE, (uint64_t)i, SALT_SEED); }
static inline uint64_t play_key(int i) { return sub_seed(story_seed(i), 0, SALT_PLAY); }
static inline uint64_t ablation_key(int i) { return sub_seed(story_seed(i), 0, SALT_ABLATION); }
static inline uint64_t tension_key(int i, int run) { return sub_seed(story_seed(i), (uint64_t)run, SALT_TENSION); }

enum { PLAY_SEEDS = 12, ABLATION_SEEDS = 24, TENSION_SEEDS = 16, TENSION_RUNS = 12 };
/* Default choices for the ablation rows: the long fall, and holding the heading. */
enum { DEFAULT_ROUTE = 1, DEFAULT_TURNED = 1 };
static inline void play_choices(int i, int *route, int *turned) { *route = i % 2; *turned = (i / 2) % 2; }
static inline void tension_choices(int run, int *route, int *turned) { *route = run % 2; *turned = (run / 2) % 2; }

#endif
