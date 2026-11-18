#ifndef ORBIT_H
#define ORBIT_H

/* Planar restricted problem in units where the star has GM = 1 and sits at the origin.
 * A moon of mass parameter moon_gm follows an exact circular orbit; the ship is a test mass. */
typedef struct {
    double x, y, vx, vy;
} Ship;

typedef struct {
    double star_gm;
    double moon_gm, moon_r; /* moon orbit radius */
    double thrust;          /* ship acceleration when the engine burns */
} World;

/* A point or a vector in the plane. */
typedef struct {
    double x, y;
} Vec2;

/* The engine: thrust direction [rad] and level in 0..1 (0 = coasting). */
typedef struct {
    double heading;
    double level;
} Burn;

World world_default(void);
double moon_omega(const World *w);
Vec2 moon_position(const World *w, double t);
/* Acceleration of the ship at position `pos` and time t from the star and the moon. */
Vec2 gravity(const World *w, double t, Vec2 pos);
/* Velocity Verlet step (symplectic) with the engine at `burn`. */
void orbit_step(const World *w, Ship *s, double t, Burn burn, double dt);
double ship_energy(const World *w, const Ship *s); /* star only */
double ship_ang_momentum(const Ship *s);
double circular_speed(const World *w, double r);
double circular_period(const World *w, double r);

#endif
