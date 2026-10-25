#include "flight.h"
#include "orbit.h"
#include <math.h>

#define PI 3.14159265358979
static World world;
static Ship ship;
static double time_now, heading, delta_v, burn1, burn2, end_time;
static int demo, phase, status;

void flight_reset(int mode)
{
    world = world_default();
    demo = mode == 1;
    if (demo) world.moon_gm = 0.0;
    ship = (Ship){1.0, 0.0, 0.0, circular_speed(&world, 1.0)};
    time_now = delta_v = 0.0;
    heading = PI / 2.0;
    phase = status = 0;
    burn1 = 0.25 * circular_period(&world, 1.0);
    burn2 = burn1 + 0.5 * circular_period(&world, 1.5);
    end_time = burn2 + 0.6 * circular_period(&world, 2.0);
}

static void impulse(double target_speed)
{
    double speed = hypot(ship.vx, ship.vy);
    delta_v += fabs(target_speed - speed);
    ship.vx *= target_speed / speed;
    ship.vy *= target_speed / speed;
    phase++;
}

void flight_tick(int frames, int turn, int burn)
{
    if (frames < 0) return;
    if (frames > 250) frames = 250;
    turn = (turn > 0) - (turn < 0);
    burn = burn > 0;
    for (int frame = 0; frame < frames && !status; frame++) {
        if (!demo) heading = remainder(heading + turn * 0.08, 2.0 * PI);
        for (int k = 0; k < 10 && !status; k++) {
            if (demo) {
                if (!phase && time_now >= burn1)
                    impulse(sqrt(world.star_gm * (2.0 - 1.0 / 1.5)));
                if (phase == 1 && time_now >= burn2)
                    impulse(circular_speed(&world, hypot(ship.x, ship.y)));
                heading = atan2(ship.vy, ship.vx);
            }
            orbit_step(&world, &ship, time_now, heading, demo ? 0.0 : burn, 0.004);
            time_now += 0.004;
            if (!demo && burn) delta_v += world.thrust * 0.004;
            if (hypot(ship.x, ship.y) < 0.1) status = 1;
            if (!demo && flight_value(FLIGHT_MOON_DISTANCE) < 0.15 &&
                flight_value(FLIGHT_REL_SPEED) < 0.25) status = 2;
            if (demo && time_now >= end_time) status = 3;
        }
    }
}

double flight_value(int field)
{
    double mx, my;
    moon_position(&world, time_now, &mx, &my);
    switch (field) {
    case FLIGHT_X: return ship.x;
    case FLIGHT_Y: return ship.y;
    case FLIGHT_VX: return ship.vx;
    case FLIGHT_VY: return ship.vy;
    case FLIGHT_TIME: return time_now;
    case FLIGHT_HEADING: return heading;
    case FLIGHT_RADIUS: return hypot(ship.x, ship.y);
    case FLIGHT_SPEED: return hypot(ship.vx, ship.vy);
    case FLIGHT_ENERGY: return ship_energy(&world, &ship);
    case FLIGHT_MOON_X: return mx;
    case FLIGHT_MOON_Y: return my;
    case FLIGHT_MOON_DISTANCE: return hypot(ship.x - mx, ship.y - my);
    case FLIGHT_REL_SPEED: return hypot(ship.vx + moon_omega(&world) * my,
                                       ship.vy - moon_omega(&world) * mx);
    case FLIGHT_STATUS: return status;
    case FLIGHT_PHASE: return phase;
    case FLIGHT_DV: return delta_v;
    case FLIGHT_MODE: return demo;
    case FLIGHT_BURN1_TIME: return burn1;
    case FLIGHT_BURN2_TIME: return burn2;
    case FLIGHT_END_TIME: return end_time;
    default: return 0.0;
    }
}
