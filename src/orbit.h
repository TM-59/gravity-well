#ifndef ORBIT_H
#define ORBIT_H

/* Planar restricted problem in units where the star has GM = 1 and sits at the origin.
 * A moon of mass parameter moon_gm follows an exact circular orbit; the ship is a test mass. */
typedef struct {
    double x, y, vx, vy;
} Ship;

typedef struct {
    double star_gm;
    double moon_gm, moon_r;   /* moon orbit radius */
    double thrust;            /* ship acceleration when the engine burns */
} World;

World world_default(void);
double moon_omega(const World *w);
void moon_position(const World *w, double t, double *x, double *y);
/* Acceleration of the ship at (x, y, t) from the star and the moon. */
void gravity(const World *w, double t, double x, double y, double *ax, double *ay);
/* Velocity Verlet step (symplectic); burn in 0..1 along heading [rad]. */
void orbit_step(const World *w, Ship *s, double t, double heading, double burn, double dt);
double ship_energy(const World *w, const Ship *s);          /* star only */
double ship_ang_momentum(const Ship *s);
double circular_speed(const World *w, double r);
double circular_period(const World *w, double r);

#endif
