#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "orbit.h"
#include "render.h"
#include "svg.h"

#define TRAIL 400
#define PI 3.14159265358979

/* Hohmann transfer 1 -> 2 with two impulsive burns, logged as CSV and SVG charts. */
static int headless(const char *dir)
{
    World w = world_default();
    w.moon_gm = 0.0;
    double r1 = 1.0, r2 = 2.0, a = 0.5 * (r1 + r2);
    Ship s = {r1, 0.0, 0.0, circular_speed(&w, r1)};
    double dv1 = sqrt(w.star_gm * (2.0 / r1 - 1.0 / a)) - s.vy;
    s.vy += dv1;
    double half = 0.5 * circular_period(&w, a), total = half + circular_period(&w, r2);
    double dt = 0.002, t = 0.0, e0 = ship_energy(&w, &s);
    enum { STRIDE = 25 };
    int n = (int)(total / dt) / STRIDE + 2, i = 0, burned = 0;
    float *tt = malloc(n * sizeof *tt), *xs = malloc(n * sizeof *xs), *ys = malloc(n * sizeof *ys);
    float *rr = malloc(n * sizeof *rr), *er = malloc(n * sizeof *er);
    if (!tt || !xs || !ys || !rr || !er) return 1;
    char path[512];
    snprintf(path, sizeof path, "%s/run.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "t,x,y,r,energy\n");
    for (int k = 0; t < total && i < n; k++) {
        if (!burned && t >= half) {
            double r = hypot(s.x, s.y), vc = circular_speed(&w, r), sp = hypot(s.vx, s.vy);
            s.vx *= vc / sp;
            s.vy *= vc / sp;
            burned = 1;
            e0 = ship_energy(&w, &s);
        }
        if (k % STRIDE == 0) {
            tt[i] = (float)t; xs[i] = (float)s.x; ys[i] = (float)s.y; rr[i] = (float)hypot(s.x, s.y);
            er[i] = (float)(ship_energy(&w, &s) - e0);
            fprintf(csv, "%.3f,%.5f,%.5f,%.5f,%.6f\n", t, s.x, s.y, rr[i], ship_energy(&w, &s));
            i++;
        }
        orbit_step(&w, &s, t, 0.0, 0.0, dt);
        t += dt;
    }
    fclose(csv);
    snprintf(path, sizeof path, "%s/track.svg", dir);
    Series tr[] = {{"y", ys, "#1f77b4"}};
    if (svg_plot(path, "Hohmann transfer path (y vs x)", "x", "y", xs, i, tr, 1)) return 1;
    snprintf(path, sizeof path, "%s/radius.svg", dir);
    Series ra[] = {{"r", rr, "#2ca02c"}};
    if (svg_plot(path, "Orbital radius during the transfer", "t", "r", tt, i, ra, 1)) return 1;
    snprintf(path, sizeof path, "%s/energy.svg", dir);
    Series en[] = {{"E - E0 (after 2nd burn)", er, "#d62728"}};
    if (svg_plot(path, "Energy error of velocity Verlet", "t", "dE", tt, i, en, 1)) return 1;
    printf("dv1 %.4f, final r %.4f\n", dv1, hypot(s.x, s.y));
    free(tt); free(xs); free(ys); free(rr); free(er);
    return 0;
}

static int snapshot(void)
{
    World w = world_default();
    Ship s = {1.0, 0.0, 0.0, circular_speed(&w, 1.0)};
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    if (!render_frame(&w, &s, 0.0, PI / 2.0, &s, 1, buf, sizeof buf)) return 1;
    fputs(buf, stdout);
    return 0;
}

static struct termios saved;
static void restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &saved); fputs("\033[?25h", stdout); }

static int play(void)
{
    if (!isatty(STDIN_FILENO)) { fputs("interactive mode needs a terminal; try --snapshot\n", stderr); return 1; }
    tcgetattr(STDIN_FILENO, &saved);
    struct termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restore);
    fputs("\033[2J\033[?25l", stdout);

    World w = world_default();
    Ship s = {1.0, 0.0, 0.0, circular_speed(&w, 1.0)};
    static Ship trail[TRAIL];
    int tlen = 0, head = 0, paused = 0, frame = 0;
    double t = 0.0, heading = PI / 2.0, burn = 0.0;
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    struct timespec tick = {0, 40 * 1000000L};
    for (;;) {
        char c;
        burn = 0.0;
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q') return 0;
            if (c == 'a') heading += 0.2;
            if (c == 'd') heading -= 0.2;
            if (c == 'w') burn = 1.0;
            if (c == 'p') paused = !paused;
        }
        if (!paused) {
            for (int k = 0; k < 10; k++) {
                orbit_step(&w, &s, t, heading, burn, 0.004);
                t += 0.004;
            }
            if (frame++ % 3 == 0) {
                trail[head] = s;
                head = (head + 1) % TRAIL;
                if (tlen < TRAIL) tlen++;
            }
        }
        double mx, my;
        moon_position(&w, t, &mx, &my);
        double r = hypot(s.x, s.y), dm = hypot(s.x - mx, s.y - my);
        render_frame(&w, &s, t, heading, trail, tlen, buf, sizeof buf);
        printf("\033[H%s", buf);
        printf("r %.2f  speed %.2f  energy %+.3f  moon dist %.2f  %s      \n", r, hypot(s.vx, s.vy),
               ship_energy(&w, &s), dm, burn > 0.0 ? "BURN" : "coast");
        puts("a/d turn  w burn (hold)  p pause  q quit    goal: rendezvous with the moon 'o'");
        fflush(stdout);
        if (r < 0.1) { puts("\nCrashed into the star."); return 2; }
        double mvx = -moon_omega(&w) * my, mvy = moon_omega(&w) * mx;
        if (dm < 0.15 && hypot(s.vx - mvx, s.vy - mvy) < 0.25) { puts("\nRendezvous achieved!"); return 0; }
        nanosleep(&tick, NULL);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--headless") == 0) return headless(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0) return snapshot();
    return play();
}
