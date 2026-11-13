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

int main(int argc, char **argv)
{
    if (argc>1 && strcmp(argv[1],"--samples")==0) {
        for (int p=0;p<2;p++) { lab_reset(p,0); for (int i=0;i<50;i++) {
            for (int f=0;f<=LAB_PRESET;f++) printf("%.17g%c",lab_value(f),f==LAB_PRESET?'\n':',');
            lab_tick(1);
        }}
        return 0;
    }
    test_equilibria();test_jacobi_and_order();test_contours();test_lab();
    printf("CR3BP: %d checks, %d failures\n",checks,failures);
    return failures!=0;
}
