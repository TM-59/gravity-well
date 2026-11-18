#include "orbit.h"

#include <math.h>

World world_default(void)
{
    World w = {.star_gm = 1.0, .moon_gm = 0.01, .moon_r = 3.0, .thrust = 0.02};
    return w;
}

double moon_omega(const World *w) { return sqrt(w->star_gm / (w->moon_r * w->moon_r * w->moon_r)); }

Vec2 moon_position(const World *w, double t)
{
    double a = moon_omega(w) * t;
    Vec2 at = {w->moon_r * cos(a), w->moon_r * sin(a)};
    return at;
}

Vec2 gravity(const World *w, double t, Vec2 pos)
{
    double x = pos.x, y = pos.y;
    double r2 = x * x + y * y + 1e-6, r3 = r2 * sqrt(r2);
    Vec2 acc = {-w->star_gm * x / r3, -w->star_gm * y / r3};
    Vec2 moon = moon_position(w, t);
    double dx = x - moon.x, dy = y - moon.y, d2 = dx * dx + dy * dy + 1e-4, d3 = d2 * sqrt(d2);
    acc.x -= w->moon_gm * dx / d3;
    acc.y -= w->moon_gm * dy / d3;
    return acc;
}

void orbit_step(const World *w, Ship *s, double t, Burn burn, double dt)
{
    double bx = burn.level * w->thrust * cos(burn.heading);
    double by = burn.level * w->thrust * sin(burn.heading);
    Vec2 acc = gravity(w, t, (Vec2){s->x, s->y});
    double ax = acc.x + bx, ay = acc.y + by;
    double hx = s->vx + 0.5 * dt * ax, hy = s->vy + 0.5 * dt * ay;
    s->x += dt * hx;
    s->y += dt * hy;
    acc = gravity(w, t + dt, (Vec2){s->x, s->y});
    s->vx = hx + 0.5 * dt * (acc.x + bx);
    s->vy = hy + 0.5 * dt * (acc.y + by);
}

double ship_energy(const World *w, const Ship *s)
{
    return 0.5 * (s->vx * s->vx + s->vy * s->vy) - w->star_gm / sqrt(s->x * s->x + s->y * s->y);
}

double ship_ang_momentum(const Ship *s) { return s->x * s->vy - s->y * s->vx; }

double circular_speed(const World *w, double r) { return sqrt(w->star_gm / r); }

double circular_period(const World *w, double r)
{
    return 2.0 * 3.14159265358979 * sqrt(r * r * r / w->star_gm);
}
