#ifndef CR3BP_H
#define CR3BP_H

#include <stddef.h>
#include "orbit.h"

/* Barycentric circular restricted three-body problem. Separation = total GM =
 * angular frequency = 1; mu is the secondary's mass fraction, 0 < mu <= 1/2.
 * Ship velocities are rotating-frame derivatives. Primaries: (-mu,0),(1-mu,0).
 * Unlike the legacy fixed-star model, both primaries orbit the barycentre.
 * No softening: callers must keep trajectories away from either point mass. */
typedef struct { double x1, y1, x2, y2; } ZeroSegment;
double cr3bp_mu(void);
double cr3bp_potential(double mu, double x, double y); /* Omega, L^2/T^2 */
void cr3bp_acceleration(double mu, const Ship *s, double *ax, double *ay);
double cr3bp_jacobi(double mu, const Ship *s); /* C = 2 Omega - |v_rot|^2 */
/* RK4 for the velocity-dependent Coriolis acceleration, not velocity Verlet. */
void cr3bp_step(double mu, Ship *s, double dt);
/* index 1..5; bisection for collinear roots, exact equilateral L4/L5. */
Ship cr3bp_lagrange(double mu, int index);
/* Marching triangles on [-extent,extent]^2; each edge root is refined by
 * bisection of the actual potential. Returns required count, writes at most cap.
 * Cells with a singular vertex are omitted. */
size_t cr3bp_contours(double mu, double c, double extent, int grid,
                     ZeroSegment *out, size_t cap);

#endif
