#include "render.h"

#include <math.h>

#define UNIT_PER_COL 0.1
#define UNIT_PER_ROW 0.2

static void put(char grid[RENDER_ROWS][RENDER_COLS], double x, double y, char ch)
{
    int c = RENDER_COLS / 2 + (int)lround(x / UNIT_PER_COL);
    int r = RENDER_ROWS / 2 - (int)lround(y / UNIT_PER_ROW);
    if (c >= 0 && c < RENDER_COLS && r >= 0 && r < RENDER_ROWS) grid[r][c] = ch;
}

size_t render_frame(const World *w, const Ship *s, double t, double heading,
                    const Ship *trail, int trail_len, char *buf, size_t cap)
{
    if (cap < (size_t)(RENDER_COLS + 1) * RENDER_ROWS + 1) return 0;
    char grid[RENDER_ROWS][RENDER_COLS];
    for (int r = 0; r < RENDER_ROWS; r++)
        for (int c = 0; c < RENDER_COLS; c++) grid[r][c] = ' ';
    for (int i = 0; i < trail_len; i++) put(grid, trail[i].x, trail[i].y, '.');
    put(grid, 0.0, 0.0, '*');
    double mx, my;
    moon_position(w, t, &mx, &my);
    put(grid, mx, my, 'o');
    double c = cos(heading), sn = sin(heading);
    put(grid, s->x, s->y, fabs(c) > fabs(sn) ? (c > 0 ? '>' : '<') : (sn > 0 ? '^' : 'v'));
    size_t used = 0;
    for (int r = 0; r < RENDER_ROWS; r++) {
        for (int k = 0; k < RENDER_COLS; k++) buf[used++] = grid[r][k];
        buf[used++] = '\n';
    }
    buf[used] = '\0';
    return used;
}
