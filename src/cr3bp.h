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
/* Second derivatives of Omega at (x,y): the linearised force about a point. */
void cr3bp_hessian(double mu, double x, double y, double *xx, double *xy, double *yy);
/* Linear modes about Lagrange point `index`. With xi''-2 eta' = Oxx xi + Oxy eta,
 * eta''+2 xi' = Oxy xi + Oyy eta, s = lambda^2 solves s^2 + (4-Oxx-Oyy) s +
 * (Oxx Oyy - Oxy^2) = 0. growth = largest Re(lambda) [1/T] (0 when linearly
 * stable); omega[] = angular frequencies of the oscillating modes, largest
 * first. Returns how many omega[] entries were written (0..2), -1 on bad input. */
int cr3bp_modes(double mu, int index, double *growth, double omega[2]);
/* A state displaced from Lagrange point `index` along one linear eigenmode, with
 * position offset of length `scale`. mode 0: the growing real mode (collinear
 * points); mode 1/2: the oscillating mode omega[0]/omega[1], started at an
 * extreme of eta so eta(t) is proportional to cos(omega t). NaN if absent. */
Ship cr3bp_mode_start(double mu, int index, int mode, double scale);
/* Routh's critical mass fraction (1 - sqrt(23/27))/2: L4/L5 are linearly
 * stable only below it. */
double cr3bp_routh(void);
/* Rotating <-> inertial barycentric states at time t (frames coincide at t=0,
 * rotating frame turns counter-clockwise at n = 1). */
Ship cr3bp_to_inertial(const Ship *rotating, double t);
Ship cr3bp_to_rotating(const Ship *inertial, double t);

#endif
