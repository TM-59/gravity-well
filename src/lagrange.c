#include "cr3bp.h"
#include "lagrange.h"
#include "svg.h"
#include <math.h>
#include <stdio.h>

#define CAP 16000
#define DT 0.0025
#define PI 3.14159265358979323846
static Ship ship;
static double time_now,c0,worst,level_c;
static int preset_now,status;
static double mu_now=1.0/101.0;
static size_t segments;
static ZeroSegment contour[CAP];

void lab_set_mu(double mu)
{
    mu_now=!(mu>0) ? cr3bp_mu() : mu<0.001 ? 0.001 : mu>0.5 ? 0.5 : mu;
}

void lab_reset(int preset, int level)
{
    preset_now=preset==1;
    ship=cr3bp_lagrange(mu_now,preset_now?1:4);
    if (preset_now) { ship.x-=0.04; ship.vx=0.25; }
    else ship.x+=0.01;
    time_now=worst=0; status=0;
    c0=cr3bp_jacobi(mu_now,&ship); level_c=c0;
    Ship l1=cr3bp_lagrange(mu_now,1);
    if (level==1 || level==2) level_c=cr3bp_jacobi(mu_now,&l1)+(level==1?0.01:-0.01);
    segments=cr3bp_contours(mu_now,level_c,1.7,128,contour,CAP);
}

void lab_tick(int frames)
{
    if (frames<0) return;
    if (frames>250) frames=250;
    for (int i=0;i<frames && !status;i++) for (int k=0;k<16 && !status;k++) {
        Ship next=ship;
        cr3bp_step(mu_now,&next,DT);
        double err=fabs(cr3bp_jacobi(mu_now,&next)-c0);
        /* Point masses have singular gravity. Reject a step outside this lab's
         * accuracy budget; do not pretend that a close encounter conserves C. */
        if (!isfinite(err) || err>1e-6 || hypot(next.x+mu_now,next.y)<0.02 ||
            hypot(next.x-1+mu_now,next.y)<0.02) { status=1; break; }
        ship=next; time_now+=DT; worst=fmax(worst,err);
        if (time_now>=60) status=2;
    }
}

double lab_value(int field)
{
    switch (field) {
    case LAB_X:return ship.x; case LAB_Y:return ship.y;
    case LAB_VX:return ship.vx; case LAB_VY:return ship.vy;
    case LAB_TIME:return time_now; case LAB_C:return cr3bp_jacobi(mu_now,&ship);
    case LAB_C0:return c0; case LAB_ERROR:return worst; case LAB_MU:return mu_now;
    case LAB_LEVEL:return level_c; case LAB_SEGMENTS:return (double)segments;
    case LAB_STATUS:return status; case LAB_PRESET:return preset_now;
    case LAB_IX:return cr3bp_to_inertial(&ship,time_now).x;
    case LAB_IY:return cr3bp_to_inertial(&ship,time_now).y;
    case LAB_ROUTH:return cr3bp_routh();
    case LAB_HILL:return cbrt(mu_now/3);
    case LAB_GROWTH1: case LAB_GROWTH4: case LAB_W_SHORT: case LAB_W_LONG: {
        double g=0,w[2]={NAN,NAN};
        int n=cr3bp_modes(mu_now,field==LAB_GROWTH1?1:4,&g,w);
        if (field==LAB_W_SHORT) return n>0?w[0]:NAN;
        if (field==LAB_W_LONG) return n>1?w[1]:NAN;
        return g;
    }
    default:return NAN;
    }
}

double lab_point(int index, int field)
{
    Ship p=cr3bp_lagrange(mu_now,index);
    if (field==0) return p.x;
    if (field==1) return p.y;
    return cr3bp_jacobi(mu_now,&p);
}

double lab_segment(int index, int field)
{
    if (index<0 || (size_t)index>=segments || index>=CAP) return NAN;
    ZeroSegment s=contour[index];
    switch (field) { case 0:return s.x1; case 1:return s.y1; case 2:return s.x2; case 3:return s.y2; default:return NAN; }
}

double lab_allowed(double x, double y) { return 2*cr3bp_potential(mu_now,x,y)-level_c; }

double lab_measured_growth(double mu, int index)
{
    Ship p=cr3bp_lagrange(mu,index), s=cr3bp_mode_start(mu,index,0,1e-8);
    for (int k=0;k<1200;k++) cr3bp_step(mu,&s,DT);
    return log(hypot(s.x-p.x,s.y-p.y)/1e-8)/3.0;
}

double lab_measured_period(double mu, int mode)
{
    Ship p=cr3bp_lagrange(mu,4), s=cr3bp_mode_start(mu,4,mode,1e-6);
    double first=NAN, prev=s.y-p.y;
    for (int k=1;k<=40000;k++) {
        cr3bp_step(mu,&s,DT);
        double now=s.y-p.y;
        if (prev<0 && now>=0) {
            double t=(k-1+prev/(prev-now))*DT;
            if (isnan(first)) first=t; else return t-first;
        }
        prev=now;
    }
    return NAN;
}

double lab_measured_drift(double mu, double *largest)
{
    Ship p=cr3bp_lagrange(mu,4), s=p;
    double st=0,sy=0,stt=0,sty=0,top=0; int n=0;
    s.x+=1e-8;
    for (int k=1;k<=28000;k++) {
        cr3bp_step(mu,&s,DT);
        double d=hypot(s.x-p.x,s.y-p.y), t=k*DT;
        top=fmax(top,d);
        if (k>=4000 && k%40==0) { double y=log(d); st+=t; sy+=y; stt+=t*t; sty+=t*y; n++; }
    }
    if (largest) *largest=top;
    return (n*sty-st*sy)/(n*stt-st*st);
}

/* One square panel of an SVG: axes box, caption, and a polyline in data units. */
static void panel_open(FILE *f, double ox, double oy, double size, const char *caption)
{
    fprintf(f,"<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" fill=\"#ffffff\" stroke=\"#8a99a8\"/>"
              "<text x=\"%.1f\" y=\"%.1f\">%s</text>",ox,oy,size,size,ox,oy-10,caption);
}

static int stability_output(const char *dir)
{
    char path[1024];
    double mu=cr3bp_mu(), g, w[2];
    FILE *f;
    /* 1. table: linear theory against integration */
    snprintf(path,sizeof path,"%s/stability.csv",dir); f=fopen(path,"w"); if (!f) return 1;
    fputs("quantity,theory,measured,relative_difference\n",f);
    for (int i=1;i<=3;i++) {
        cr3bp_modes(mu,i,&g,w);
        double m=lab_measured_growth(mu,i);
        fprintf(f,"L%d growth rate [1/T] (mu=1/101),%.9f,%.9f,%.2e\n",i,g,m,fabs(m/g-1));
    }
    cr3bp_modes(mu,4,&g,w);
    double period[2];
    for (int k=0;k<2;k++) {
        period[k]=lab_measured_period(mu,k+1);
        fprintf(f,"L4 %s period [T] (mu=1/101),%.9f,%.9f,%.2e\n",k?"long":"short",2*PI/w[k],period[k],fabs(period[k]*w[k]/(2*PI)-1));
    }
    double top3,top5,slope5=lab_measured_drift(0.05,&top5),slope3=lab_measured_drift(0.03,&top3),g5,w5[2];
    cr3bp_modes(0.05,4,&g5,w5);
    fprintf(f,"L4 growth rate [1/T] (mu=0.05 above Routh),%.9f,%.9f,%.2e\n",g5,slope5,fabs(slope5/g5-1));
    fprintf(f,"L4 largest distance in 70 T from 1e-8 L (mu=0.03 below Routh),bounded,%.3e,\n",top3);
    fprintf(f,"L4 largest distance in 70 T from 1e-8 L (mu=0.05 above Routh),unbounded,%.3e,\n",top5);
    fprintf(f,"Routh critical mass fraction,%.12f,,\n",cr3bp_routh());
    double hill=cbrt(mu/3), d1=1-mu-cr3bp_lagrange(mu,1).x, d2=cr3bp_lagrange(mu,2).x-1+mu;
    fprintf(f,"L1 distance from secondary [L] vs Hill radius (mu=1/101),%.9f,%.9f,%.2e\n",hill,d1,fabs(d1/hill-1));
    fprintf(f,"L2 distance from secondary [L] vs Hill radius (mu=1/101),%.9f,%.9f,%.2e\n",hill,d2,fabs(d2/hill-1));
    if (fclose(f)) return 1;
    (void)slope3;

    /* 2. L4 mode frequencies and growth against mass fraction */
    enum { M=240, N=1001 };
    static float xs[N],a[N],b[N],c[N];
    for (int i=0;i<M;i++) {
        double m=0.0005+i*0.00025, gg, ww[2];
        int n=cr3bp_modes(m,4,&gg,ww);
        xs[i]=(float)m; a[i]=(float)ww[0]; b[i]=(float)(n>1?ww[1]:ww[0]); c[i]=(float)gg;
    }
    Series modes[3]={{"short-period omega",a,"#136d88"},{"long-period omega",b,"#b77d17"},{"growth rate Re(lambda)",c,"#b03a48"}};
    snprintf(path,sizeof path,"%s/l4-modes.svg",dir);
    if (svg_plot(path,"L4/L5 linear modes: frequencies merge at Routh's mu = 0.03852","mass fraction mu","rate [1/T]",xs,M,modes,3)) return 1;

    /* 3. the two pure L4 modes in time */
    for (int k=0;k<2;k++) {
        Ship p=cr3bp_lagrange(mu,4), s=cr3bp_mode_start(mu,4,k+1,1e-6);
        float *dst=k?b:a;
        for (int i=0;i<N;i++) {
            xs[i]=(float)(i*20*DT); dst[i]=(float)((s.y-p.y)*1e6);
            for (int j=0;j<20;j++) cr3bp_step(mu,&s,DT);
        }
    }
    char title[160];
    snprintf(title,sizeof title,"L4 modes, mu = 1/101: %.4f T, %.4f T (theory %.4f, %.4f)",period[0],period[1],2*PI/w[0],2*PI/w[1]);
    Series lib[2]={{"short-period mode",a,"#136d88"},{"long-period mode",b,"#b77d17"}};
    snprintf(path,sizeof path,"%s/l4-libration.svg",dir);
    if (svg_plot(path,title,"t [T]","eta = y - y(L4) [1e-6 L]",xs,N,lib,2)) return 1;

    /* 4. below and above Routh's limit */
    for (int k=0;k<2;k++) {
        double m=k?0.05:0.03;
        Ship p=cr3bp_lagrange(m,4), s=p; s.x+=1e-8;
        float *dst=k?b:a;
        for (int i=0;i<N;i++) {
            xs[i]=(float)(i*28*DT); dst[i]=(float)log10(hypot(s.x-p.x,s.y-p.y));
            c[i]=(float)(-8+g5*i*28*DT/log(10.0));
            for (int j=0;j<28;j++) cr3bp_step(m,&s,DT);
        }
    }
    snprintf(title,sizeof title,"L4 nudged 1e-8 L: slope %.4f /T at mu = 0.05 (theory %.4f)",slope5,g5);
    Series crit[3]={{"mu = 0.03 (stable)",a,"#136d88"},{"mu = 0.05 (unstable)",b,"#b03a48"},{"theory slope, mu = 0.05",c,"#8a99a8"}};
    snprintf(path,sizeof path,"%s/l4-routh.svg",dir);
    if (svg_plot(path,title,"t [T]","log10 distance from L4 [L]",xs,N,crit,3)) return 1;

    /* 5. the collinear saddles */
    enum { K=301 };
    static float l[3][K];
    for (int i=1;i<=3;i++) {
        Ship p=cr3bp_lagrange(mu,i), s=cr3bp_mode_start(mu,i,0,1e-8);
        for (int j=0;j<K;j++) {
            xs[j]=(float)(j*4*DT); l[i-1][j]=(float)log10(hypot(s.x-p.x,s.y-p.y));
            for (int q=0;q<4;q++) cr3bp_step(mu,&s,DT);
        }
    }
    Series saddle[3]={{"L1",l[0],"#136d88"},{"L2",l[1],"#b77d17"},{"L3",l[2],"#594790"}};
    snprintf(path,sizeof path,"%s/collinear-growth.svg",dir);
    if (svg_plot(path,"L1-L3 are saddles: growing mode, mu = 1/101","t [T]","log10 distance from the point [L]",xs,K,saddle,3)) return 1;

    /* 5b. Hill radius against the actual L1/L2 distances */
    for (int i=0;i<M;i++) {
        double m=0.0005*(i+1);
        xs[i]=(float)m; a[i]=(float)cbrt(m/3);
        b[i]=(float)(1-m-cr3bp_lagrange(m,1).x); c[i]=(float)(cr3bp_lagrange(m,2).x-1+m);
    }
    Series hills[3]={{"Hill radius",a,"#8a99a8"},{"secondary to L1",b,"#136d88"},{"secondary to L2",c,"#b77d17"}};
    snprintf(path,sizeof path,"%s/hill.svg",dir);
    if (svg_plot(path,"L1 and L2 straddle the Hill radius (mu/3)^(1/3)","mass fraction mu","distance from the secondary [L]",xs,M,hills,3)) return 1;

    /* 6. one trajectory, two frames */
    lab_set_mu(0); lab_reset(0,0);
    enum { P=1501 };
    static double rx[P],ry[P],ix[P],iy[P];
    double lo[2]={1e9,1e9},hi[2]={-1e9,-1e9};
    for (int i=0;i<P;i++) {
        rx[i]=ship.x; ry[i]=ship.y; ix[i]=lab_value(LAB_IX); iy[i]=lab_value(LAB_IY);
        lo[0]=fmin(lo[0],rx[i]); hi[0]=fmax(hi[0],rx[i]); lo[1]=fmin(lo[1],ry[i]); hi[1]=fmax(hi[1],ry[i]);
        if (i<P-1) lab_tick(1);
    }
    double span=1.15*fmax(hi[0]-lo[0],hi[1]-lo[1]), cx=(lo[0]+hi[0])/2, cy=(lo[1]+hi[1])/2;
    Ship l4=cr3bp_lagrange(mu,4);
    snprintf(path,sizeof path,"%s/frames.svg",dir); f=fopen(path,"w"); if (!f) return 1;
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"520\" viewBox=\"0 0 1000 520\"><rect width=\"1000\" height=\"520\" fill=\"#f4f8fb\"/><g font-family=\"sans-serif\" font-size=\"15\" fill=\"#142f43\">",f);
    fputs("<text x=\"25\" y=\"28\">One C trajectory (L4 + 0.01 L in x, 60 T, mu = 1/101) drawn in two frames</text>",f);
    char cap[160];
    snprintf(cap,sizeof cap,"Rotating frame, window %.3f L wide around L4 (+)",span);
    panel_open(f,60,70,400,cap);
    fputs("<path fill=\"none\" stroke=\"#b77d17\" stroke-width=\"1.4\" d=\"",f);
    for (int i=0;i<P;i++) fprintf(f,"%c%.2f %.2f",i?'L':'M',260+(rx[i]-cx)/span*400,270-(ry[i]-cy)/span*400);
    fprintf(f,"\"/><path stroke=\"#142f43\" d=\"M%.2f %.2fh12m-6 -6v12\"/>",254+(l4.x-cx)/span*400,270-(l4.y-cy)/span*400);
    panel_open(f,540,70,400,"Inertial frame, window 2.6 L wide");
    fprintf(f,"<circle cx=\"740\" cy=\"270\" r=\"%.2f\" fill=\"none\" stroke=\"#594790\" stroke-dasharray=\"4 4\"/>",(1-mu)*400/2.6);
    fprintf(f,"<circle cx=\"740\" cy=\"270\" r=\"%.2f\" fill=\"none\" stroke=\"#b77d17\"/>",fmax(1.5,mu*400/2.6));
    fputs("<path fill=\"none\" stroke=\"#b77d17\" stroke-width=\"1.2\" d=\"",f);
    for (int i=0;i<P;i++) fprintf(f,"%c%.2f %.2f",i?'L':'M',740+ix[i]/2.6*400,270-iy[i]/2.6*400);
    fputs("\"/>",f);
    fprintf(f,"<text x=\"60\" y=\"500\">Rotating x, y in L. The loop is %.4f L by %.4f L.</text>",hi[0]-lo[0],hi[1]-lo[1]);
    fprintf(f,"<text x=\"540\" y=\"500\">Dashed: secondary's orbit. Same path: %.2f turns.</text></g></svg>\n",time_now/(2*PI));
    if (fclose(f)) return 1;
    printf("CR3BP stability: L1 growth %.6f/T, L4 periods %.4f T and %.4f T, Routh mu %.9f\n",lab_measured_growth(mu,1),period[0],period[1],cr3bp_routh());
    return 0;
}

int lagrange_output(const char *dir)
{
    char path[1024];
    lab_set_mu(0);
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
    if (fclose(f)) return 1;
    return stability_output(dir);
}
