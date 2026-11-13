#ifndef LAGRANGE_H
#define LAGRANGE_H

enum LabValue { LAB_X,LAB_Y,LAB_VX,LAB_VY,LAB_TIME,LAB_C,LAB_C0,LAB_ERROR,
                LAB_MU,LAB_LEVEL,LAB_SEGMENTS,LAB_STATUS,LAB_PRESET };
/* Preset 0: displaced L4 (small libration). Preset 1: passage through L1.
 * level 0: current trajectory C; 1: C(L1)+0.01 closed; 2: C(L1)-0.01 open.
 * A nonzero level is a comparison, not the moving particle's Jacobi constant. */
void lab_reset(int preset, int level);
void lab_tick(int frames);
double lab_value(int field);
double lab_point(int index, int field); /* index 1..5, x/y/C */
double lab_segment(int index, int field); /* x1/y1/x2/y2 */
double lab_allowed(double x, double y); /* 2 Omega - display C */
int lagrange_output(const char *dir); /* CSV, zero-velocity and Jacobi SVG */

#endif
