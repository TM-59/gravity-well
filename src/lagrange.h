#ifndef LAGRANGE_H
#define LAGRANGE_H

enum LabValue {
    LAB_X,
    LAB_Y,
    LAB_VX,
    LAB_VY,
    LAB_TIME,
    LAB_C,
    LAB_C0,
    LAB_ERROR,
    LAB_MU,
    LAB_LEVEL,
    LAB_SEGMENTS,
    LAB_STATUS,
    LAB_PRESET,
    /* inertial-frame position of the traveller; linear theory at the lab's mu */
    LAB_IX,
    LAB_IY,
    LAB_GROWTH1,
    LAB_GROWTH4,
    LAB_W_SHORT,
    LAB_W_LONG,
    LAB_ROUTH,
    LAB_HILL
};
/* Secondary mass fraction for the next lab_reset, clamped to [0.001, 0.5];
 * mu <= 0 restores the default 1/101. */
void lab_set_mu(double mu);
/* Display level for the next lab_reset: 0 current trajectory C; 1: C(L1)+0.01 closed;
 * 2: C(L1)-0.01 open. A nonzero level is a comparison, not the moving particle's Jacobi constant.
 */
void lab_set_level(int level);
/* Preset 0: displaced L4 (small libration). Preset 1: passage through L1. */
void lab_reset(int preset);
void lab_tick(int frames);
double lab_value(int field);
/* Lagrange point `index` 1..5: x, y and its Jacobi constant C. */
double lab_point_x(int index);
double lab_point_y(int index);
double lab_point_c(int index);
/* Ends of zero-velocity segment `index`; NaN outside 0..segments-1. */
double lab_segment_x1(int index);
double lab_segment_y1(int index);
double lab_segment_x2(int index);
double lab_segment_y2(int index);
double lab_allowed(double x, double y); /* 2 Omega - display C */
/* Measurements by integration (RK4, dt 0.0025), to set against cr3bp_modes:
 * e-folding rate of the pure growing mode at a collinear point over 3 T; */
double lab_measured_growth(double mu, int index);
/* period of eta's upward zero crossings for a pure L4 mode (1 short, 2 long); */
double lab_measured_period(double mu, int mode);
/* least-squares slope of ln(distance from L4) over 10..70 T after a 1e-8 L
 * displacement; also the largest distance reached (may be NULL). */
double lab_measured_drift(double mu, double *largest);
int lagrange_output(const char *dir); /* CSV tables and the SVG figures of docs/theory page 5 */

#endif
