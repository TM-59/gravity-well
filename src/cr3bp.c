#include "cr3bp.h"
#include <math.h>

double cr3bp_mu(void) { return 1.0 / 101.0; }

double cr3bp_potential(double mu, double x, double y)
{
    return 0.5 * (x*x + y*y) + (1.0-mu)/hypot(x+mu,y) + mu/hypot(x-1.0+mu,y);
}

void cr3bp_acceleration(double mu, const Ship *s, double *ax, double *ay)
{
    double dx=s->x+mu, ex=s->x-1.0+mu;
    double r=hypot(dx,s->y), q=hypot(ex,s->y);
    double a=(1.0-mu)/(r*r*r), b=mu/(q*q*q);
    *ax=s->x-a*dx-b*ex+2.0*s->vy;
    *ay=s->y-a*s->y-b*s->y-2.0*s->vx;
}

double cr3bp_jacobi(double mu, const Ship *s)
{
    return 2.0*cr3bp_potential(mu,s->x,s->y)-s->vx*s->vx-s->vy*s->vy;
}

static Ship derivative(double mu, Ship s)
{
    Ship d={s.vx,s.vy,0,0};
    cr3bp_acceleration(mu,&s,&d.vx,&d.vy);
    return d;
}

static Ship add(Ship s, Ship d, double h)
{
    return (Ship){s.x+h*d.x,s.y+h*d.y,s.vx+h*d.vx,s.vy+h*d.vy};
}

void cr3bp_step(double mu, Ship *s, double dt)
{
    Ship a=derivative(mu,*s), b=derivative(mu,add(*s,a,dt/2));
    Ship c=derivative(mu,add(*s,b,dt/2)), d=derivative(mu,add(*s,c,dt));
    *s=add(*s,(Ship){a.x+2*b.x+2*c.x+d.x,a.y+2*b.y+2*c.y+d.y,
                     a.vx+2*b.vx+2*c.vx+d.vx,a.vy+2*b.vy+2*c.vy+d.vy},dt/6);
}

Ship cr3bp_lagrange(double mu, int index)
{
    if (!(mu>0 && mu<=0.5) || index<1 || index>5) return (Ship){NAN,NAN,0,0};
    if (index>=4) return (Ship){0.5-mu,(index==4?1:-1)*sqrt(3.0)/2,0,0};
    double lo=-2, hi=-mu-1e-10;
    if (index==1) { lo=-mu+1e-10; hi=1-mu-1e-10; }
    if (index==2) { lo=1-mu+1e-10; hi=2; }
    for (int k=0;k<100;k++) {
        double mid=(lo+hi)/2, ax,ay;
        Ship s={mid,0,0,0};
        cr3bp_acceleration(mu,&s,&ax,&ay);
        if (ax>0) hi=mid; else lo=mid;
    }
    return (Ship){(lo+hi)/2,0,0,0};
}

typedef struct { double x,y,f; } Vertex;

static Vertex edge(double mu, double c, Vertex a, Vertex b)
{
    for (int k=0;k<42;k++) {
        Vertex m={(a.x+b.x)/2,(a.y+b.y)/2,0};
        m.f=2*cr3bp_potential(mu,m.x,m.y)-c;
        if ((a.f>=0)==(m.f>=0)) a=m; else b=m;
    }
    return (Vertex){(a.x+b.x)/2,(a.y+b.y)/2,0};
}

size_t cr3bp_contours(double mu, double c, double extent, int grid,
                     ZeroSegment *out, size_t cap)
{
    if (!(mu>0 && mu<=0.5 && extent>0 && isfinite(c)) || grid<2 || grid>512) return 0;
    size_t count=0;
    double h=2*extent/grid;
    for (int j=0;j<grid;j++) for (int i=0;i<grid;i++) {
        Vertex v[4]={{-extent+i*h,-extent+j*h,0},{-extent+(i+1)*h,-extent+j*h,0},
                     {-extent+(i+1)*h,-extent+(j+1)*h,0},{-extent+i*h,-extent+(j+1)*h,0}};
        int valid=1;
        for (int k=0;k<4;k++) { v[k].f=2*cr3bp_potential(mu,v[k].x,v[k].y)-c; valid &= isfinite(v[k].f); }
        if (!valid) continue;
        for (int t=0;t<2;t++) {
            Vertex tri[3]={v[0],v[t+1],v[t+2]}, hits[3];
            int n=0;
            for (int k=0;k<3;k++) if ((tri[k].f>=0)!=(tri[(k+1)%3].f>=0))
                hits[n++]=edge(mu,c,tri[k],tri[(k+1)%3]);
            if (n==2) {
                if (out && count<cap) out[count]=(ZeroSegment){hits[0].x,hits[0].y,hits[1].x,hits[1].y};
                count++;
            }
        }
    }
    return count;
}

void cr3bp_hessian(double mu, double x, double y, double *xx, double *xy, double *yy)
{
    double dx=x+mu, ex=x-1.0+mu, r=hypot(dx,y), q=hypot(ex,y);
    double a=(1.0-mu)/(r*r*r), b=mu/(q*q*q), a5=3.0*a/(r*r), b5=3.0*b/(q*q);
    *xx=1.0-a-b+a5*dx*dx+b5*ex*ex;
    *yy=1.0-a-b+(a5+b5)*y*y;
    *xy=a5*dx*y+b5*ex*y;
}

int cr3bp_modes(double mu, int index, double *growth, double omega[2])
{
    Ship p=cr3bp_lagrange(mu,index);
    if (isnan(p.x)) return -1;
    double xx,xy,yy;
    cr3bp_hessian(mu,p.x,p.y,&xx,&xy,&yy);
    double b=4.0-xx-yy, c=xx*yy-xy*xy, disc=b*b-4.0*c;
    int n=0;
    *growth=0;
    if (disc<0) { /* s is a complex pair: all four lambda leave the axes */
        double re=-b/2, mod=sqrt(c);
        *growth=sqrt((mod+re)/2);
        omega[n++]=sqrt((mod-re)/2);
        return n;
    }
    double root=sqrt(disc), s[2]={(-b-root)/2,(-b+root)/2};
    for (int k=0;k<2;k++) {
        if (s[k]<0) omega[n++]=sqrt(-s[k]);
        else *growth=fmax(*growth,sqrt(s[k]));
    }
    return n;
}

double cr3bp_routh(void) { return (1.0-sqrt(23.0/27.0))/2.0; }

Ship cr3bp_to_inertial(const Ship *r, double t)
{
    double c=cos(t), s=sin(t), ux=r->vx-r->y, uy=r->vy+r->x;
    return (Ship){r->x*c-r->y*s, r->x*s+r->y*c, ux*c-uy*s, ux*s+uy*c};
}

Ship cr3bp_to_rotating(const Ship *i, double t)
{
    double c=cos(t), s=sin(t);
    double x=i->x*c+i->y*s, y=-i->x*s+i->y*c;
    return (Ship){x, y, i->vx*c+i->vy*s+y, -i->vx*s+i->vy*c-x};
}

Ship cr3bp_mode_start(double mu, int index, int mode, double scale)
{
    Ship p=cr3bp_lagrange(mu,index), bad={NAN,NAN,0,0};
    double g,w[2],xx,xy,yy,xi,eta,vxi,veta;
    int n=cr3bp_modes(mu,index,&g,w);
    if (n<0) return bad;
    cr3bp_hessian(mu,p.x,p.y,&xx,&xy,&yy);
    if (mode==0) {
        if (!(g>0) || (4-xx-yy)*(4-xx-yy)<4*(xx*yy-xy*xy)) return bad;
        xi=2*g+xy; eta=g*g-xx; vxi=g*xi; veta=g*eta;
    } else {
        if (mode<1 || mode>n || (g>0 && n==1)) return bad;
        double o=w[mode-1];
        xi=xy; eta=-(o*o+xx); vxi=-2*o*o; veta=0;
    }
    double k=scale/hypot(xi,eta);
    return (Ship){p.x+k*xi,p.y+k*eta,k*vxi,k*veta};
}
