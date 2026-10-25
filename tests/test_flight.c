#include "../src/flight.h"
#include "../src/orbit.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void manual_matches_existing_integrator(void)
{
    World w = world_default();
    Ship s = {1.0, 0.0, 0.0, circular_speed(&w, 1.0)};
    double t = 0.0, heading = 3.14159265358979 / 2.0;
    flight_reset(0);
    for (int frame = 0; frame < 100; frame++) {
        int turn = frame < 15 ? 1 : frame > 80 ? -1 : 0;
        int burn = frame >= 40 && frame < 70;
        heading = remainder(heading + turn * 0.08, 2.0 * 3.14159265358979);
        for (int k = 0; k < 10; k++) {
            orbit_step(&w, &s, t, heading, burn, 0.004);
            t += 0.004;
        }
        flight_tick(1, turn, burn);
        assert(fabs(flight_value(FLIGHT_X) - s.x) < 1e-12);
        assert(fabs(flight_value(FLIGHT_Y) - s.y) < 1e-12);
        assert(fabs(flight_value(FLIGHT_VX) - s.vx) < 1e-12);
        assert(fabs(flight_value(FLIGHT_VY) - s.vy) < 1e-12);
    }
    assert(fabs(flight_value(FLIGHT_DV) - 0.024) < 1e-12);
    flight_reset(0);
    assert(flight_value(FLIGHT_TIME) == 0.0);
    assert(flight_value(FLIGHT_DV) == 0.0);
    flight_tick(-1, 0, 0);
    assert(flight_value(FLIGHT_TIME) == 0.0);
}

static void transfer_bench(void)
{
    flight_reset(1);
    double r_at_second_burn = 0.0, dv1 = 0.0, low = 100.0, high = 0.0;
    int prev = 0, frames = 0;
    while (flight_value(FLIGHT_STATUS) == 0 && frames++ < 1000) {
        flight_tick(1, 1, 1); /* Demo must ignore manual inputs. */
        int phase = (int)flight_value(FLIGHT_PHASE);
        if (prev == 0 && phase == 1) dv1 = flight_value(FLIGHT_DV);
        if (prev == 1 && phase == 2) r_at_second_burn = flight_value(FLIGHT_RADIUS);
        if (phase == 2) {
            low = fmin(low, flight_value(FLIGHT_RADIUS));
            high = fmax(high, flight_value(FLIGHT_RADIUS));
        }
        prev = phase;
    }
    assert(flight_value(FLIGHT_STATUS) == 3);
    assert(fabs(dv1 - (sqrt(4.0 / 3.0) - 1.0)) < 1e-5);
    assert(fabs(r_at_second_burn - 2.0) < 0.002);
    assert(high - low < 0.002);
    printf("Hohmann: dv1=%.6f, burn2 radius=%.6f, final radius=%.6f, "
           "circular radius span=%.6f, frames=%d\n",
           dv1, r_at_second_burn, flight_value(FLIGHT_RADIUS), high - low, frames);
    double t = flight_value(FLIGHT_TIME);
    flight_tick(100, 0, 0);
    assert(flight_value(FLIGHT_TIME) == t);
}

int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "--bench")) manual_matches_existing_integrator();
    transfer_bench();
    puts("flight tests passed");
    return 0;
}
