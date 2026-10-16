#include "orbit.h"

#include <math.h>

World world_default(void)
{
    World w = {.star_gm = 1.0, .moon_gm = 0.01, .moon_r = 3.0, .thrust = 0.02};
    return w;
}

double moon_omega(const World *w)
{
    return sqrt(w->star_gm / (w->moon_r * w->moon_r * w->moon_r));
}

void moon_position(const World *w, double t, double *x, double *y)
{
    double a = moon_omega(w) * t;
    *x = w->moon_r * cos(a);
    *y = w->moon_r * sin(a);
}

void gravity(const World *w, double t, double x, double y, double *ax, double *ay)
{
    double r2 = x * x + y * y + 1e-6, r3 = r2 * sqrt(r2);
    *ax = -w->star_gm * x / r3;
    *ay = -w->star_gm * y / r3;
    double mx, my;
    moon_position(w, t, &mx, &my);
    double dx = x - mx, dy = y - my, d2 = dx * dx + dy * dy + 1e-4, d3 = d2 * sqrt(d2);
    *ax -= w->moon_gm * dx / d3;
    *ay -= w->moon_gm * dy / d3;
}

void orbit_step(const World *w, Ship *s, double t, double heading, double burn, double dt)
{
    double ax, ay, bx = burn * w->thrust * cos(heading), by = burn * w->thrust * sin(heading);
    gravity(w, t, s->x, s->y, &ax, &ay);
    ax += bx;
    ay += by;
    double hx = s->vx + 0.5 * dt * ax, hy = s->vy + 0.5 * dt * ay;
    s->x += dt * hx;
    s->y += dt * hy;
    gravity(w, t + dt, s->x, s->y, &ax, &ay);
    s->vx = hx + 0.5 * dt * (ax + bx);
    s->vy = hy + 0.5 * dt * (ay + by);
}

double ship_energy(const World *w, const Ship *s)
{
    return 0.5 * (s->vx * s->vx + s->vy * s->vy) - w->star_gm / sqrt(s->x * s->x + s->y * s->y);
}

double ship_ang_momentum(const Ship *s)
{
    return s->x * s->vy - s->y * s->vx;
}

double circular_speed(const World *w, double r)
{
    return sqrt(w->star_gm / r);
}

double circular_period(const World *w, double r)
{
    return 2.0 * 3.14159265358979 * sqrt(r * r * r / w->star_gm);
}
