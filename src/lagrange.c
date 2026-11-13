#include "cr3bp.h"
#include "lagrange.h"
#include "svg.h"
#include <math.h>
#include <stdio.h>

#define CAP 16000
#define DT 0.0025
static Ship ship;
static double time_now,c0,worst,level_c;
static int preset_now,status;
static size_t segments;
static ZeroSegment contour[CAP];

void lab_reset(int preset, int level)
{
    preset_now=preset==1;
    ship=cr3bp_lagrange(cr3bp_mu(),preset_now?1:4);
    if (preset_now) { ship.x-=0.04; ship.vx=0.25; }
    else ship.x+=0.01;
    time_now=worst=0; status=0;
    c0=cr3bp_jacobi(cr3bp_mu(),&ship); level_c=c0;
    Ship l1=cr3bp_lagrange(cr3bp_mu(),1);
    if (level==1 || level==2) level_c=cr3bp_jacobi(cr3bp_mu(),&l1)+(level==1?0.01:-0.01);
    segments=cr3bp_contours(cr3bp_mu(),level_c,1.7,128,contour,CAP);
}

void lab_tick(int frames)
{
    if (frames<0) return;
    if (frames>250) frames=250;
    for (int i=0;i<frames && !status;i++) for (int k=0;k<16 && !status;k++) {
        Ship next=ship;
        cr3bp_step(cr3bp_mu(),&next,DT);
        double err=fabs(cr3bp_jacobi(cr3bp_mu(),&next)-c0);
        /* Point masses have singular gravity. Reject a step outside this lab's
         * accuracy budget; do not pretend that a close encounter conserves C. */
        if (!isfinite(err) || err>1e-6 || hypot(next.x+cr3bp_mu(),next.y)<0.02 ||
            hypot(next.x-1+cr3bp_mu(),next.y)<0.02) { status=1; break; }
        ship=next; time_now+=DT; worst=fmax(worst,err);
        if (time_now>=60) status=2;
    }
}

double lab_value(int field)
{
    switch (field) {
    case LAB_X:return ship.x; case LAB_Y:return ship.y;
    case LAB_VX:return ship.vx; case LAB_VY:return ship.vy;
    case LAB_TIME:return time_now; case LAB_C:return cr3bp_jacobi(cr3bp_mu(),&ship);
    case LAB_C0:return c0; case LAB_ERROR:return worst; case LAB_MU:return cr3bp_mu();
    case LAB_LEVEL:return level_c; case LAB_SEGMENTS:return (double)segments;
    case LAB_STATUS:return status; case LAB_PRESET:return preset_now;
    default:return NAN;
    }
}

double lab_point(int index, int field)
{
    Ship p=cr3bp_lagrange(cr3bp_mu(),index);
    if (field==0) return p.x;
    if (field==1) return p.y;
    return cr3bp_jacobi(cr3bp_mu(),&p);
}

double lab_segment(int index, int field)
{
    if (index<0 || (size_t)index>=segments || index>=CAP) return NAN;
    ZeroSegment s=contour[index];
    switch (field) { case 0:return s.x1; case 1:return s.y1; case 2:return s.x2; case 3:return s.y2; default:return NAN; }
}

double lab_allowed(double x, double y) { return 2*cr3bp_potential(cr3bp_mu(),x,y)-level_c; }

int lagrange_output(const char *dir)
{
    char path[1024];
    snprintf(path,sizeof path,"%s/lagrange.csv",dir);
    FILE *f=fopen(path,"w"); if (!f) return 1;
    fputs("point,x,y,C,acceleration_residual\n",f);
    for (int i=1;i<=5;i++) {
        Ship p=cr3bp_lagrange(cr3bp_mu(),i); double ax,ay;
        cr3bp_acceleration(cr3bp_mu(),&p,&ax,&ay);
        fprintf(f,"L%d,%.15g,%.15g,%.15g,%.6g\n",i,p.x,p.y,cr3bp_jacobi(cr3bp_mu(),&p),hypot(ax,ay));
    }
    if (fclose(f)) return 1;
    lab_reset(0,0);
    enum { N=1501 };
    static float tt[N],drift[N];
    snprintf(path,sizeof path,"%s/jacobi.csv",dir); f=fopen(path,"w"); if (!f) return 1;
    fputs("t,x,y,vx,vy,C,C_minus_C0\n",f);
    for (int i=0;i<N;i++) {
        tt[i]=(float)time_now; drift[i]=(float)(cr3bp_jacobi(cr3bp_mu(),&ship)-c0);
        fprintf(f,"%.8f,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g\n",time_now,ship.x,ship.y,ship.vx,ship.vy,cr3bp_jacobi(cr3bp_mu(),&ship),cr3bp_jacobi(cr3bp_mu(),&ship)-c0);
        if (i<N-1) lab_tick(1);
    }
    if (fclose(f)) return 1;
    snprintf(path,sizeof path,"%s/jacobi.svg",dir);
    Series series={"C - C0",drift,"#136d88"};
    if (svg_plot(path,"Jacobi conservation: displaced L4, RK4 dt=0.0025","t [T]","C - C0 [L^2/T^2]",tt,N,&series,1)) return 1;
    printf("CR3BP mu %.12g: L4 60 T, max |C-C0| %.6g\n",cr3bp_mu(),worst);
    /* Both topology comparisons are evaluated by the same C contour generator
     * used in the browser; the default particle trajectory is a separate inset. */
    snprintf(path,sizeof path,"%s/zero-velocity.svg",dir); f=fopen(path,"w"); if (!f) return 1;
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"550\" viewBox=\"0 0 1000 550\"><rect width=\"1000\" height=\"550\" fill=\"#f4f8fb\"/><g font-family=\"sans-serif\" font-size=\"15\" fill=\"#142f43\"><text x=\"25\" y=\"28\">CR3BP: barycentric rotating frame; primary separation = total GM = n = 1</text>",f);
    for (int pane=0;pane<2;pane++) {
        lab_reset(0,pane+1);
        double ox=250+500*pane, oy=285, sc=125;
        fprintf(f,"<text x=\"%d\" y=\"58\">%s: C = %.9f</text>",30+500*pane,pane?"Open L1 neck":"Closed L1 neck",level_c);
        for (int j=0;j<100;j++) for (int i=0;i<100;i++) {
            double x=-1.7+(i+0.5)*3.4/100,y=-1.7+(j+0.5)*3.4/100;
            if (lab_allowed(x,y)<0) fprintf(f,"<rect x=\"%.2f\" y=\"%.2f\" width=\"4.3\" height=\"4.3\" fill=\"#bdcbd9\"/>",ox+x*sc-2.13,oy-y*sc-2.13);
        }
        fputs("<g stroke=\"#167b8d\" stroke-width=\"1.4\">",f);
        for (size_t i=0;i<segments && i<CAP;i++) {
            ZeroSegment s=contour[i];
            fprintf(f,"<path d=\"M%.3f %.3fL%.3f %.3f\"/>",ox+s.x1*sc,oy-s.y1*sc,ox+s.x2*sc,oy-s.y2*sc);
        }
        fputs("</g>",f);
        for (int i=1;i<=5;i++) {
            Ship p=cr3bp_lagrange(cr3bp_mu(),i);
            fprintf(f,"<circle cx=\"%.3f\" cy=\"%.3f\" r=\"3\"/><text x=\"%.3f\" y=\"%.3f\">L%d</text>",ox+p.x*sc,oy-p.y*sc,ox+p.x*sc+5,oy-p.y*sc-7,i);
        }
        fprintf(f,"<circle cx=\"%.3f\" cy=\"285\" r=\"6\" fill=\"#b77d17\"/><circle cx=\"%.3f\" cy=\"285\" r=\"4\" fill=\"#594790\"/>",ox-cr3bp_mu()*sc,ox+(1-cr3bp_mu())*sc);
        fprintf(f,"<text x=\"%d\" y=\"508\">x, y in L; grey: forbidden (2 Omega &lt; C)</text>",25+500*pane);
    }
    fputs("<text x=\"25\" y=\"535\">Each curve has v_rot = 0; open geometry alone does not guarantee a transit.</text></g></svg>\n",f);
    return fclose(f)!=0;
}
