#ifndef VOYAGE_H
#define VOYAGE_H

#include "orbit.h"

/* "The Ice Run": a story layer over the unchanged integrator in orbit.c.
 * The tug carries the inner dock's last water ice out to Lamp Quay in five legs.
 * Each leg is a transfer between two circular orbits around the star; the player
 * holds Raise (prograde), Lower (retrograde) or Match (burn towards the local
 * circular velocity), and the tug points itself. Story scenes open at fixed
 * progress points (the start of the run and the end of a leg) and change game
 * numbers only: the fuel in the tank, the target rings, how much the orbit
 * readout wobbles, and what a tow costs.
 * The moon's gravity is off in this mode (star only, like the Hohmann demonstration);
 * the moon is drawn as a position reference. */

enum { V_LEGS = 5 };

enum {
    VS_NONE = -1,
    VS_DOCK,     /* Ilse gives what fuel the dock can spare and says she will call the burns */
    VS_ROUTE,    /* choice: the near buoy (a short climb) or the far buoy (a long climb, spare fuel sent down) */
    VS_BEACON,   /* the buoy is lit; Pell lights the first lamp; what is left in the buoy's cache */
    VS_FLARE,    /* the flare season comes early: the readout wobbles (heavy: shown with no banner) */
    VS_ILSE,     /* Ilse is not coming; choice: turn back for her fuel pod, or hold the heading */
    VS_SILENCE,  /* her channel goes to static; nobody calls the burns any more */
    VS_QUAY,     /* Lamp Quay; ending */
    VS_COUNT
};

/* Numeric story effects, one bit each, so the bench can remove them one by one. */
enum {
    VE_DOCK = 1u << 0,     /* Ilse adds what the dock can spare, and her count steadies the readout */
    VE_ROUTE = 1u << 1,    /* the near buoy: a short climb; the far buoy: a long climb, and its keeper sends spare fuel */
    VE_CACHE = 1u << 2,    /* the buoy's fuel cache */
    VE_FLARE = 1u << 3,    /* the flare: in leg 4 the apsis readout wobbles more and fuel boils off */
    VE_POD = 1u << 4,      /* turning back: leg 4 goes down to Ilse's fuel pod; grapple it, wait while it pumps */
    VE_SILENCE = 1u << 5,  /* Ilse's count is gone for the last leg, and a tow now drags the tug back */
    VE_ALL = (1u << 6) - 1u
};

enum { VC_COAST, VC_RAISE, VC_LOWER, VC_MATCH };
enum { VB_NONE, VB_CLEAR, VB_TOW, VB_MISS };

typedef struct {
    World world;
    Ship ship;
    unsigned effects;
    int leg;              /* 0..4; V_LEGS when the run is over */
    int scene;            /* VS_* open now, or VS_NONE */
    int route;            /* -1 until chosen; 0 = the near buoy, 1 = the far buoy */
    int turned;           /* -1 until chosen; 0 = turn back for the pod, 1 = hold the heading */
    int count;            /* 1 while Ilse calls the burns */
    int command;          /* VC_* applied in the last frame */
    double time;          /* mission clock, T; every unit is flown, no flat waits */
    double leg_time;
    double held;          /* T flown on the pod's ring while waiting for it */
    double drift;         /* T left until the drone docks (engine dead meanwhile) */
    int refuel;           /* the drifting tug is waiting for fuel (a dry tank), not for the pod to come round */
    int missed;           /* the pod's grapple glanced off once this leg */
    double fuel, red;     /* tank in units of delta-v; seconds (T) flown with the tank in the red */
    double from_r, target_r;
    int tows;
    int tows_leg[V_LEGS];
    int crisis_leg[V_LEGS];   /* 1 if the tank dropped below the crisis line or the tug was towed */
    int banner;           /* VB_* shown on screen */
    double banner_left;
    int loud;             /* number of times a "leg cleared" banner was raised */
    long frames;
    int order[VS_COUNT];
    int order_len;
} Voyage;

enum VoyageValue {
    VV_X, VV_Y, VV_VX, VV_VY, VV_TIME, VV_HEADING, VV_RADIUS, VV_SPEED, VV_FUEL, VV_RED,
    VV_LEG, VV_SCENE, VV_BANNER, VV_TOWS, VV_FROM, VV_TARGET, VV_APO, VV_PERI,
    VV_APO_READ, VV_PERI_READ, VV_MATCH_ERROR, VV_ROUTE, VV_TURNED, VV_COUNT_ON, VV_WOBBLE,
    VV_ENDING, VV_LOUD, VV_HEAVY, VV_COMMAND, VV_SEMI_MAJOR, VV_ECC, VV_PERI_ANGLE, VV_RADIAL,
    VV_MOON_X, VV_MOON_Y, VV_RED_LINE, VV_HELD, VV_HOLD_NEED, VV_BOIL, VV_DRIFT, VV_CUE, VV_FIELDS
};

void voyage_init(Voyage *v, unsigned effects);
/* frames of ten 0.004 T steps, under one command (VC_*). Does nothing while a scene is open or after the end. */
void voyage_tick(Voyage *v, int frames, int command);
/* Close the open scene. Choice scenes take option 0 or 1; others take 0. Returns 0 when accepted. */
int voyage_answer(Voyage *v, int option);
int voyage_options(int scene);
/* 1 for scenes that must be shown without a "leg cleared" banner (the heavy scenes). */
int voyage_heavy(int scene);
/* 0 = turned back for the pod, 1 = held the heading; -1 before Lamp Quay. */
int voyage_ending(const Voyage *v);
/* Relative wobble of the apsis readout right now (0 when the readout is steady). */
double voyage_wobble(const Voyage *v);
/* T the tug must fly on the target ring before the leg ends (the pod's ring); 0 for an ordinary leg. */
double voyage_hold_need(const Voyage *v);
/* Ilse's count while her channel is open: 1 = cut the burn now, 2 = on the ring now (match), 0 = nothing. */
int voyage_cue(const Voyage *v);
/* Fuel boiling off per T right now (the flare in leg 4). */
double voyage_boil(const Voyage *v);
double voyage_value(const Voyage *v, int field);

#endif
