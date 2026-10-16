#include <math.h>
#include <stdio.h>

#include "../src/orbit.h"
#include "../src/render.h"

static int failures;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

static World star_only(void)
{
    World w = world_default();
    w.moon_gm = 0.0;
    return w;
}

static void test_circular_orbit_period(void)
{
    World w = star_only();
    Ship s = {1.0, 0.0, 0.0, circular_speed(&w, 1.0)};
    double dt = 0.001, period = circular_period(&w, 1.0), t = 0.0, prev_y = 0.0;
    double crossing = -1.0;
    for (int k = 0; k < 8000; k++) {
        prev_y = s.y;
        orbit_step(&w, &s, t, 0.0, 0.0, dt);
        t += dt;
        if (prev_y < 0.0 && s.y >= 0.0 && s.x > 0.0) { crossing = t - dt * s.y / (s.y - prev_y); break; }
    }
    CHECK(crossing > 0.0, "ship never completed an orbit");
    CHECK(fabs(crossing - period) / period < 0.01, "period %.4f vs Kepler %.4f", crossing, period);
}

static void test_energy_and_momentum_drift_are_tiny(void)
{
    World w = star_only();
    Ship s = {1.0, 0.0, 0.0, 0.8};
    double e0 = ship_energy(&w, &s), l0 = ship_ang_momentum(&s), dt = 0.002, t = 0.0, worst = 0.0;
    for (int k = 0; k < 100000; k++) {
        orbit_step(&w, &s, t, 0.0, 0.0, dt);
        t += dt;
        double d = fabs(ship_energy(&w, &s) - e0);
        if (d > worst) worst = d;
    }
    CHECK(worst / fabs(e0) < 1e-3, "energy error %.2e", worst / fabs(e0));
    CHECK(fabs(ship_ang_momentum(&s) - l0) < 1e-9, "angular momentum drifted by %.2e", ship_ang_momentum(&s) - l0);
}

static void test_escape_speed_gives_zero_energy(void)
{
    World w = star_only();
    Ship s = {1.0, 0.0, 0.0, sqrt(2.0) * circular_speed(&w, 1.0)};
    CHECK(fabs(ship_energy(&w, &s)) < 1e-12, "energy at escape speed should be zero");
    double t = 0.0;
    for (int k = 0; k < 20000; k++) { orbit_step(&w, &s, t, 0.0, 0.0, 0.002); t += 0.002; }
    CHECK(hypot(s.x, s.y) > 10.0, "escaping ship should be far away, r = %.2f", hypot(s.x, s.y));
}

static void test_hohmann_transfer_reaches_the_target_radius(void)
{
    World w = star_only();
    double r1 = 1.0, r2 = 2.0, a = 0.5 * (r1 + r2);
    double v1 = circular_speed(&w, r1), vt = sqrt(w.star_gm * (2.0 / r1 - 1.0 / a));
    Ship s = {r1, 0.0, 0.0, vt};
    double t = 0.0, dt = 0.0005, half = 0.5 * circular_period(&w, a);
    for (double u = 0.0; u < half; u += dt) { orbit_step(&w, &s, t, 0.0, 0.0, dt); t += dt; }
    CHECK(vt > v1, "transfer burn should speed the ship up");
    CHECK(fabs(hypot(s.x, s.y) - r2) < 0.01, "apoapsis %.4f vs %.1f", hypot(s.x, s.y), r2);
}

static void test_moon_is_on_a_circle(void)
{
    World w = world_default();
    double x, y;
    moon_position(&w, 1.234, &x, &y);
    CHECK(fabs(hypot(x, y) - w.moon_r) < 1e-12, "moon off its orbit");
    moon_position(&w, 2.0 * 3.14159265358979 / moon_omega(&w), &x, &y);
    CHECK(fabs(x - w.moon_r) < 1e-9 && fabs(y) < 1e-9, "moon should return after one period");
}

static void test_thrust_adds_speed(void)
{
    World w = star_only();
    Ship s = {1.0, 0.0, 0.0, 1.0}, coast = s;
    double t = 0.0;
    for (int k = 0; k < 200; k++) {
        orbit_step(&w, &s, t, 3.14159265358979 / 2.0, 1.0, 0.002);
        orbit_step(&w, &coast, t, 0.0, 0.0, 0.002);
        t += 0.002;
    }
    CHECK(ship_energy(&w, &s) > ship_energy(&w, &coast), "prograde burn should raise energy");
}

static void test_render_fits_the_screen(void)
{
    World w = world_default();
    Ship s = {1.0, 0.0, 0.0, 1.0};
    static char buf[RENDER_COLS * RENDER_ROWS * 2];
    size_t used = render_frame(&w, &s, 0.0, 0.0, &s, 1, buf, sizeof buf);
    int lines = 0;
    for (size_t i = 0; i < used; i++) lines += buf[i] == '\n';
    CHECK(lines == RENDER_ROWS, "expected %d lines, got %d", RENDER_ROWS, lines);
    CHECK(render_frame(&w, &s, 0.0, 0.0, &s, 1, buf, 8) == 0, "tiny buffer must be refused");
}

int main(void)
{
    test_circular_orbit_period();
    test_energy_and_momentum_drift_are_tiny();
    test_escape_speed_gives_zero_energy();
    test_hohmann_transfer_reaches_the_target_radius();
    test_moon_is_on_a_circle();
    test_thrust_adds_speed();
    test_render_fits_the_screen();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
