#ifndef FLIGHT_H
#define FLIGHT_H

/* Presentation controller. All forces still come from orbit.c. Units: GM=1. */
enum FlightValue {
    FLIGHT_X, FLIGHT_Y, FLIGHT_VX, FLIGHT_VY, FLIGHT_TIME, FLIGHT_HEADING,
    FLIGHT_RADIUS, FLIGHT_SPEED, FLIGHT_ENERGY, FLIGHT_MOON_X, FLIGHT_MOON_Y,
    FLIGHT_MOON_DISTANCE, FLIGHT_REL_SPEED, FLIGHT_STATUS, FLIGHT_PHASE,
    FLIGHT_DV, FLIGHT_MODE, FLIGHT_BURN1_TIME, FLIGHT_BURN2_TIME, FLIGHT_END_TIME
};
/* mode 0: the original moon rendezvous; mode 1: star-only Hohmann 1 -> 2. */
void flight_reset(int mode);
/* A frame is ten existing 0.004-unit Verlet steps. turn: -1,0,+1; burn: 0,1. */
void flight_tick(int frames, int turn, int burn);
double flight_value(int field);

#endif
