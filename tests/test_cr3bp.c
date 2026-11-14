#include "../src/cr3bp.h"
#include "../src/lagrange.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); } } while (0)

/* Independent long-double Newton solve on the signed collinear equation.
 * The production solver uses bounded bisection of vector acceleration. */
static long double reference_root(long double mu, int point)
{
    long double x= point==1 ? 1-mu-powl(mu/3,1.L/3) :
                   point==2 ? 1-mu+powl(mu/3,1.L/3) : -1-5*mu/12;
    for (int i=0;i<40;i++) {
        long double a=x+mu,b=x-1+mu;
        long double f=x-(1-mu)*a/powl(fabsl(a),3)-mu*b/powl(fabsl(b),3);
        long double df=1+2*(1-mu)/powl(fabsl(a),3)+2*mu/powl(fabsl(b),3);
        x-=f/df;
    }
    return x;
}

static void test_equilibria(void)
{
    double root_error=0,force_error=0;
    const double masses[]={0.000001,0.001,1.0/101,0.1,0.4,0.5};
    for (size_t m=0;m<sizeof masses/sizeof *masses;m++) {
        double mu=masses[m];
        for (int i=1;i<=5;i++) {
            Ship p=cr3bp_lagrange(mu,i); double ax,ay;
            cr3bp_acceleration(mu,&p,&ax,&ay);
            force_error=fmax(force_error,hypot(ax,ay));
            CHECK(hypot(ax,ay)<1e-10);
            if (i<=3) {
                root_error=fmax(root_error,fabs(p.x-(double)reference_root(mu,i)));
                CHECK(fabs(p.x-(double)reference_root(mu,i))<1e-6);
            }
            else {
                CHECK(fabs(hypot(p.x+mu,p.y)-1)<1e-14);
                CHECK(fabs(hypot(p.x-1+mu,p.y)-1)<1e-14);
                CHECK(fabs(cr3bp_jacobi(mu,&p)-(3-mu+mu*mu))<1e-14);
            }
        }
    }
    CHECK(isnan(cr3bp_lagrange(0,1).x));
    CHECK(isnan(cr3bp_lagrange(0.6,1).x));
    CHECK(isnan(cr3bp_lagrange(0.1,0).x));
    Ship centre=cr3bp_lagrange(0.5,1);
    CHECK(fabs(centre.x)<1e-14);
    double mu=cr3bp_mu();
    double a=cr3bp_lagrange(mu,1).x,b=cr3bp_lagrange(mu,2).x,c=cr3bp_lagrange(mu,3).x;
    CHECK(a>-mu && a<1-mu && b>1-mu && c<-mu);
    /* Approximate Hill radius is not claimed to be exact for finite mu. */
    double tiny=1e-9,hill=cbrt(tiny/3);
    CHECK(fabs((1-tiny-cr3bp_lagrange(tiny,1).x)/hill-1)<0.001);
    CHECK(fabs((cr3bp_lagrange(tiny,2).x-1+tiny)/hill-1)<0.001);
    printf("L1-L3 vs independent long-double roots: max %.12g; all 30 equilibria force residual %.12g\n",root_error,force_error);
}

static void test_jacobi_and_order(void)
{
    double mu=cr3bp_mu(),worst=0;
    for (int preset=0;preset<3;preset++) {
        Ship s=cr3bp_lagrange(mu,preset==1?5:4);
        s.x+=0.01; if (preset==2) { s.x=0.5; s.y=0.3; s.vx=-0.2; s.vy=0.3; }
        double c=cr3bp_jacobi(mu,&s),local=0;
        for (int k=0;k<24000;k++) {
            cr3bp_step(mu,&s,0.0025);
            local=fmax(local,fabs(cr3bp_jacobi(mu,&s)-c));
            CHECK(isfinite(s.x));
        }
        CHECK(local<1e-6); worst=fmax(worst,local);
    }
    printf("Jacobi max absolute drift across 3 trajectories x 60 T: %.12g\n",worst);
    Ship initial={0.5,0.3,-0.2,0.3},end[3];
    for (int i=0;i<3;i++) {
        end[i]=initial;
        for (int k=0;k<100*(1<<i);k++) cr3bp_step(mu,&end[i],0.01/(1<<i));
    }
    double e1=hypot(end[0].x-end[2].x,end[0].y-end[2].y);
    double e2=hypot(end[1].x-end[2].x,end[1].y-end[2].y);
    printf("RK4 position coarse/fine difference ratio: %.9g\n",e1/e2);
    CHECK(e1/e2>14 && e1/e2<20);
    Ship p={0.43,0.35,0.2,-0.1}; double ax,ay,ax0,ay0,h=1e-6;
    cr3bp_acceleration(mu,&p,&ax,&ay);
    Ship rest=p;rest.vx=rest.vy=0;
    cr3bp_acceleration(mu,&rest,&ax0,&ay0);
    CHECK(fabs(ax-ax0-2*p.vy)<1e-14 && fabs(ay-ay0+2*p.vx)<1e-14);
    CHECK(fabs(ax0-(cr3bp_potential(mu,p.x+h,p.y)-cr3bp_potential(mu,p.x-h,p.y))/(2*h))<1e-8);
    CHECK(fabs(ay0-(cr3bp_potential(mu,p.x,p.y+h)-cr3bp_potential(mu,p.x,p.y-h))/(2*h))<1e-8);
}

static void test_contours(void)
{
    double mu=cr3bp_mu();Ship l1=cr3bp_lagrange(mu,1);
    double c=cr3bp_jacobi(mu,&l1);
    CHECK(2*cr3bp_potential(mu,l1.x,0)-(c+0.01)<0);
    CHECK(2*cr3bp_potential(mu,l1.x,0)-(c-0.01)>0);
    static ZeroSegment segments[16000];
    for (int mode=0;mode<2;mode++) {
        double level=c+(mode?-.01:.01),worst=0;
        size_t n=cr3bp_contours(mu,level,1.7,128,segments,16000);
        CHECK(n>100 && n<16000);
        CHECK(cr3bp_contours(mu,level,1.7,128,NULL,0)==n);
        for (size_t i=0;i<n;i++) {
            ZeroSegment s=segments[i];
            CHECK(isfinite(s.x1+s.y1+s.x2+s.y2));
            worst=fmax(worst,fabs(2*cr3bp_potential(mu,s.x1,s.y1)-level));
            worst=fmax(worst,fabs(2*cr3bp_potential(mu,s.x2,s.y2)-level));
        }
        CHECK(worst<1e-10);
        ZeroSegment small[2]={{0,0,0,0},{123,456,789,999}};
        CHECK(cr3bp_contours(mu,level,1.7,128,small,1)==n);
        CHECK(small[1].x1==123 && small[1].y2==999);
        printf("%s L1 neck: %zu C-generated segments, endpoint residual %.9g\n",mode?"Open":"Closed",n,worst);
    }
    CHECK(cr3bp_contours(mu,c,1,0,NULL,0)==0);
}

static void test_lab(void)
{
    lab_reset(0,0);
    double x=lab_value(LAB_X);
    lab_tick(-1);CHECK(lab_value(LAB_TIME)==0);
    lab_tick(1);CHECK(lab_value(LAB_TIME)>0 && lab_value(LAB_X)!=x);
    lab_reset(0,0);CHECK(lab_value(LAB_X)==x && lab_value(LAB_ERROR)==0);
    for (int i=0;i<6;i++) lab_tick(250);
    CHECK(lab_value(LAB_TIME)>59.99 && lab_value(LAB_ERROR)<1e-6);
    lab_reset(1,0);
    double start=lab_value(LAB_X),l1=lab_point(1,0);
    int passed=0;
    for (int i=0;i<100;i++) { lab_tick(1); if (lab_value(LAB_X)>l1) passed=1; }
    CHECK(start<l1 && passed);
    CHECK(lab_value(LAB_ERROR)<1e-6);
    for (int i=0;i<100;i++) lab_tick(250);
    CHECK(lab_value(LAB_STATUS)>0 && lab_value(LAB_ERROR)<=1e-6);
    lab_reset(0,1);CHECK(lab_allowed(lab_point(1,0),0)<0);
    lab_reset(0,2);CHECK(lab_allowed(lab_point(1,0),0)>0);
    CHECK(isfinite(lab_segment(0,0)) && isnan(lab_segment(-1,0)));
    for (int i=1;i<=5;i++) CHECK(isfinite(lab_point(i,2)));
}

static void test_frames(void)
{
    double mu=cr3bp_mu(),round=0,energy=0;
    Ship s={0.5,0.3,-0.2,0.3};
    for (int k=0;k<=4000;k++) {
        double t=k*0.0025;
        Ship in=cr3bp_to_inertial(&s,t), back=cr3bp_to_rotating(&in,t);
        round=fmax(round,fmax(hypot(back.x-s.x,back.y-s.y),hypot(back.vx-s.vx,back.vy-s.vy)));
        /* In the inertial frame C = -2 (E - n h): energy minus angular momentum. */
        double c=cos(t),sn=sin(t);
        double r1=hypot(in.x+mu*c,in.y+mu*sn), r2=hypot(in.x-(1-mu)*c,in.y-(1-mu)*sn);
        double e=0.5*(in.vx*in.vx+in.vy*in.vy)-(1-mu)/r1-mu/r2, h=in.x*in.vy-in.y*in.vx;
        energy=fmax(energy,fabs(-2*(e-h)-cr3bp_jacobi(mu,&s)));
        cr3bp_step(mu,&s,0.0025);
    }
    CHECK(round<1e-14); CHECK(energy<1e-13);
    Ship moon={1-mu,0,0,0}, m=cr3bp_to_inertial(&moon,1.3);
    CHECK(fabs(hypot(m.x,m.y)-(1-mu))<1e-15 && fabs(hypot(m.vx,m.vy)-(1-mu))<1e-15);
    /* Independent integration: Newtonian gravity of two circling primaries in the
     * inertial frame, no Coriolis or centrifugal term anywhere. */
    Ship rot={0.5,0.3,-0.2,0.3}, in=cr3bp_to_inertial(&rot,0);
    double h=0.00125, worst=0;
    for (int k=0;k<16000;k++) {
        double t=k*h, z[4]={in.x,in.y,in.vx,in.vy}, acc[4]={0,0,0,0}, y[4], d[4];
        static const double node[4]={0,0.5,0.5,1}, weight[4]={1,2,2,1};
        for (int j=0;j<4;j++) d[j]=0;
        for (int stage=0;stage<4;stage++) {
            double tt=t+node[stage]*h, c=cos(tt), sn=sin(tt);
            for (int j=0;j<4;j++) y[j]=z[j]+node[stage]*h*d[j];
            double ax=y[0]+mu*c, ay=y[1]+mu*sn, bx=y[0]-(1-mu)*c, by=y[1]-(1-mu)*sn;
            double ra=pow(hypot(ax,ay),3), rb=pow(hypot(bx,by),3);
            d[0]=y[2]; d[1]=y[3]; d[2]=-(1-mu)*ax/ra-mu*bx/rb; d[3]=-(1-mu)*ay/ra-mu*by/rb;
            for (int j=0;j<4;j++) acc[j]+=weight[stage]*d[j];
        }
        in=(Ship){z[0]+h*acc[0]/6,z[1]+h*acc[1]/6,z[2]+h*acc[2]/6,z[3]+h*acc[3]/6};
        cr3bp_step(mu,&rot,h);
        Ship same=cr3bp_to_inertial(&rot,(k+1)*h);
        worst=fmax(worst,hypot(same.x-in.x,same.y-in.y));
    }
    CHECK(worst<1e-7);
    printf("Frames: round trip %.3g, C + 2(E - h) %.3g, rotating vs independent inertial integration over 20 T %.3g L\n",round,energy,worst);
}

static void test_stability(void)
{
    double g,w[2],xx,xy,yy,h=1e-5,worst=0;
    /* Hessian against central differences of the at-rest acceleration. */
    const double masses[]={0.001,1.0/101,0.1,0.3,0.5};
    for (size_t m=0;m<sizeof masses/sizeof *masses;m++) {
        double mu=masses[m];
        Ship a={0.43+h,0.35,0,0},b={0.43-h,0.35,0,0},c={0.43,0.35+h,0,0},d={0.43,0.35-h,0,0};
        double ax,ay,bx,by,cx,cy,dx,dy;
        cr3bp_acceleration(mu,&a,&ax,&ay);cr3bp_acceleration(mu,&b,&bx,&by);
        cr3bp_acceleration(mu,&c,&cx,&cy);cr3bp_acceleration(mu,&d,&dx,&dy);
        cr3bp_hessian(mu,0.43,0.35,&xx,&xy,&yy);
        CHECK(fabs(xx-(ax-bx)/(2*h))<1e-6 && fabs(yy-(cy-dy)/(2*h))<1e-6);
        CHECK(fabs(xy-(ay-by)/(2*h))<1e-6 && fabs(xy-(cx-dx)/(2*h))<1e-6);
        /* Collinear points: one growing mode and one oscillation, at every mass ratio. */
        for (int i=1;i<=3;i++) {
            CHECK(cr3bp_modes(mu,i,&g,w)==1 && g>0 && w[0]>0);
            double measured=lab_measured_growth(mu,i);
            worst=fmax(worst,fabs(measured/g-1));
            CHECK(fabs(measured/g-1)<1e-4);
        }
        /* L4 and L5: closed forms Oxx=3/4, Oyy=9/4, Oxy=+-(3 sqrt3/4)(1-2mu). */
        Ship l4=cr3bp_lagrange(mu,4);
        cr3bp_hessian(mu,l4.x,l4.y,&xx,&xy,&yy);
        CHECK(fabs(xx-0.75)<1e-13 && fabs(yy-2.25)<1e-13 && fabs(xy-0.75*sqrt(3.0)*(1-2*mu))<1e-13);
        double g5,w5[2];
        int n4=cr3bp_modes(mu,4,&g,w), n5=cr3bp_modes(mu,5,&g5,w5);
        CHECK(n4==n5 && fabs(g-g5)<1e-15);
        if (mu<cr3bp_routh()) {
            CHECK(n4==2 && g==0);
            /* omega^4 - omega^2 + (27/4) mu (1-mu) = 0 */
            for (int k=0;k<2;k++) CHECK(fabs(pow(w[k],4)-w[k]*w[k]+6.75*mu*(1-mu))<1e-13);
        } else CHECK(n4==1 && g>0);
    }
    printf("L1-L3 growth rate, 15 points: integration vs eigenvalue, max relative difference %.3g\n",worst);
    /* Routh: bisect the first mass fraction at which the linear growth is nonzero. */
    double lo=0.03,hi=0.05;
    for (int k=0;k<60;k++) { double mid=(lo+hi)/2; cr3bp_modes(mid,4,&g,w); if (g>0) hi=mid; else lo=mid; }
    CHECK(fabs(hi-cr3bp_routh())<1e-12 && fabs(cr3bp_routh()-0.0385208965)<1e-10);
    CHECK(fabs(27*cr3bp_routh()*(1-cr3bp_routh())-1)<1e-14);
    /* Libration periods of the two pure modes against 2 pi / omega. */
    double mu=cr3bp_mu();
    cr3bp_modes(mu,4,&g,w);
    for (int k=0;k<2;k++) {
        double period=lab_measured_period(mu,k+1), theory=2*acos(-1.0)/w[k];
        printf("L4 %s period: measured %.9f T, linear theory %.9f T, relative difference %.3g\n",k?"long":"short",period,theory,fabs(period/theory-1));
        CHECK(fabs(period/theory-1)<1e-5);
    }
    /* Above Routh's limit a 1e-8 L nudge spirals away at the predicted rate;
     * below it the same nudge stays within a few times its size. */
    double top3,top5,slope5=lab_measured_drift(0.05,&top5),g5;
    lab_measured_drift(0.03,&top3);
    cr3bp_modes(0.05,4,&g5,w);
    printf("L4 nudge 1e-8 L, 70 T: mu 0.03 stays within %.3g L; mu 0.05 reaches %.3g L, rate %.6f /T (theory %.6f)\n",top3,top5,slope5,g5);
    CHECK(top3<1e-6 && top5>1e-4 && fabs(slope5/g5-1)<0.05);
    CHECK(isnan(cr3bp_mode_start(mu,4,0,1e-6).x) && isnan(cr3bp_mode_start(0.05,4,2,1e-6).x));
    CHECK(cr3bp_modes(0,4,&g,w)==-1);
    /* Hill radius is the small-mu limit of the L1/L2 distance, not an identity. */
    double hill=cbrt(mu/3), d1=1-mu-cr3bp_lagrange(mu,1).x, d2=cr3bp_lagrange(mu,2).x-1+mu;
    printf("Hill radius %.6f L at mu 1/101: L1 at %.6f (%.2f%%), L2 at %.6f (%+.2f%%)\n",hill,d1,100*(d1/hill-1),d2,100*(d2/hill-1));
    CHECK(d1<hill && d2>hill && fabs(d1/hill-1)<0.08 && fabs(d2/hill-1)<0.08);
    /* next order: d = hill (1 -+ hill/3) */
    CHECK(fabs(d1-hill*(1-hill/3))<0.002 && fabs(d2-hill*(1+hill/3))<0.002);
}

static void test_lab_mu(void)
{
    lab_set_mu(0.3); lab_reset(0,0);
    CHECK(lab_value(LAB_MU)==0.3 && lab_value(LAB_GROWTH4)>0 && isnan(lab_value(LAB_W_LONG)));
    CHECK(fabs(lab_point(4,0)-0.2)<1e-15 && lab_value(LAB_GROWTH1)>0);
    for (int i=0;i<40;i++) lab_tick(250);
    CHECK(lab_value(LAB_STATUS)>0 && lab_value(LAB_ERROR)<=1e-6);
    CHECK(hypot(lab_value(LAB_X)-lab_point(4,0),lab_value(LAB_Y)-lab_point(4,1))>0.1);
    Ship s={lab_value(LAB_X),lab_value(LAB_Y),lab_value(LAB_VX),lab_value(LAB_VY)};
    Ship in=cr3bp_to_inertial(&s,lab_value(LAB_TIME));
    CHECK(lab_value(LAB_IX)==in.x && lab_value(LAB_IY)==in.y);
    lab_set_mu(9); CHECK(lab_value(LAB_MU)==0.5);
    lab_set_mu(1e-9); CHECK(lab_value(LAB_MU)==0.001);
    lab_set_mu(0); lab_reset(0,0);
    CHECK(lab_value(LAB_MU)==cr3bp_mu() && lab_value(LAB_GROWTH4)==0);
    CHECK(lab_value(LAB_W_SHORT)>lab_value(LAB_W_LONG) && lab_value(LAB_ROUTH)==cr3bp_routh());
    CHECK(fabs(lab_value(LAB_HILL)-cbrt(cr3bp_mu()/3))<1e-16);
    for (int i=0;i<7;i++) lab_tick(250);
    printf("Lab L4 + 0.01 L, 60 T: ends %.4f L from L4 at mu 1/101 (bounded tadpole)\n",hypot(lab_value(LAB_X)-lab_point(4,0),lab_value(LAB_Y)-lab_point(4,1)));
    CHECK(lab_value(LAB_STATUS)==2 && lab_value(LAB_Y)>0.5 && hypot(lab_value(LAB_X)-lab_point(4,0),lab_value(LAB_Y)-lab_point(4,1))<0.5);
}

int main(int argc, char **argv)
{
    if (argc>1 && strcmp(argv[1],"--samples")==0) {
        for (int p=0;p<2;p++) { lab_reset(p,0); for (int i=0;i<50;i++) {
            for (int f=0;f<=LAB_HILL;f++) printf("%.17g%c",lab_value(f),f==LAB_HILL?'\n':',');
            lab_tick(1);
        }}
        return 0;
    }
    test_equilibria();test_jacobi_and_order();test_contours();test_lab();test_frames();test_stability();test_lab_mu();
    printf("CR3BP: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
