#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>

#include "orbit.h"

#define RENDER_COLS 78
#define RENDER_ROWS 30

/* Top view, 1 row = 0.2 units, 1 column = 0.1 units, star '*', moon 'o', ship '^>v<' by heading,
 * with a '.' trail of recent ship positions. Returns bytes written or 0 if cap is too small. */
size_t render_frame(const World *w, const Ship *s, double t, double heading,
                    const Ship *trail, int trail_len, char *buf, size_t cap);

#endif
