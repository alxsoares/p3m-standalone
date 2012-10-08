
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fftw3.h>
#include <string.h>

#include "types.h"
#include "charge-assign.h"
#include "p3m-ad.h"
#include "common.h"
#include "p3m-ad-self-forces.h"

#include "realpart.h"

#ifdef __detailed_timings
#include <mpi.h>
#endif


const method_t method_p3m_ad = { METHOD_P3M_ad, "P3M with analytic differentiation, not intelaced.", 
				 METHOD_FLAG_P3M | METHOD_FLAG_ad | METHOD_FLAG_self_force_correction, 
				 &Init_ad, &Influence_function_berechnen_ad, &P3M_ad, &Error_ad, &p3m_k_space_error_ad };

static void forward_fft( data_t *d );
static void backward_fft( data_t *d );

inline void forward_fft ( data_t *d ) {
  FFTW_EXECUTE ( d->forward_plan[0] );
}

inline void backward_fft ( data_t *d ) {
  FFTW_EXECUTE ( d->backward_plan[0] );
}

data_t *Init_ad ( system_t *s, parameters_t *p ) {
    int mesh = p->mesh;

    data_t *d = Init_data ( &method_p3m_ad, s, p );

    d->forward_plans = 1;
    d->backward_plans = 1;

    d->forward_plan[0] = FFTW_PLAN_DFT_3D ( mesh, mesh, mesh, ( FFTW_COMPLEX * ) d->Qmesh, ( FFTW_COMPLEX * ) d->Qmesh, FFTW_FORWARD, FFTW_PATIENT );

    d->backward_plan[0] = FFTW_PLAN_DFT_3D ( mesh, mesh, mesh, ( FFTW_COMPLEX * ) ( d->Qmesh ), ( FFTW_COMPLEX * ) ( d->Qmesh ), FFTW_BACKWARD, FFTW_PATIENT );

    return d;
}

void Aliasing_sums_ad(int NX, int NY, int NZ, system_t *s, parameters_t *p, data_t *d,
		      FLOAT_TYPE *Zaehler, FLOAT_TYPE *Nenner1, FLOAT_TYPE *Nenner2)
{
  FLOAT_TYPE S1,S2,S3;
  FLOAT_TYPE fak1,fak2,zwi;
  int    MX,MY,MZ;
  FLOAT_TYPE NMX,NMY,NMZ;
  FLOAT_TYPE NM2;
  FLOAT_TYPE expo, TE;
  int Mesh = p->mesh;
  FLOAT_TYPE Len = s->length;
  FLOAT_TYPE Leni = 1.0/Len;

  fak1 = 1.0/(FLOAT_TYPE)Mesh;
  fak2 = SQR(PI/p->alpha);

  *Zaehler = *Nenner1 = *Nenner2 = 0.0;

  for (MX = -P3M_BRILLOUIN; MX <= P3M_BRILLOUIN; MX++) {
    NMX = d->nshift[NX] + Mesh*MX;
    S1   = my_power(sinc(fak1*NMX), 2*p->cao); 
    for (MY = -P3M_BRILLOUIN; MY <= P3M_BRILLOUIN; MY++) {
      NMY = d->nshift[NY] + Mesh*MY;
      S2   = S1*my_power(sinc(fak1*NMY), 2*p->cao);
      for (MZ = -P3M_BRILLOUIN; MZ <= P3M_BRILLOUIN; MZ++) {
	NMZ = d->nshift[NZ] + Mesh*MZ;
	S3   = S2*my_power(sinc(fak1*NMZ), 2*p->cao);

	NM2 = SQR(NMX*Leni) + SQR(NMY*Leni) + SQR(NMZ*Leni);
	*Nenner1 += S3;
	*Nenner2 += S3 * NM2;

	expo = fak2*NM2;
	TE = EXP(-expo);
	zwi  = S3 * TE;
        *Zaehler += zwi;
      }
    }
  }
}

void Influence_function_berechnen_ad( system_t *s, parameters_t *p, data_t *d )
{

  int    NX,NY,NZ;
  FLOAT_TYPE Zaehler=0.0,Nenner1=0.0, Nenner2=0.0;

  int ind = 0;
  int Mesh = p->mesh;

  /* bei Zahlen >= Mesh/2 wird noch Mesh abgezogen! */
  for (NX=0; NX<Mesh; NX++)
    {
      for (NY=0; NY<Mesh; NY++)
	{
	  for (NZ=0; NZ<Mesh; NZ++)
	    {
              ind = r_ind(NX,NY,NZ);

	      if ((NX==0) && (NY==0) && (NZ==0))
		d->G_hat[ind]=0.0;
              else if ((NX%(Mesh/2) == 0) && (NY%(Mesh/2) == 0) && (NZ%(Mesh/2) == 0))
                d->G_hat[ind]=0.0;
	      else
		{
		  Aliasing_sums_ad(NX,NY,NZ,s,p,d,&Zaehler,&Nenner1, &Nenner2);
		  d->G_hat[ind] = Zaehler / ( PI * Nenner1 * Nenner2 );
		}
	      if(isnan(d->G_hat[ind])) {
		puts("G_hat is nan!");
		printf("N (%d, %d, %d)\n",NX, NY, NZ);
		exit(0);
	      }
	    }
	}
    }
  Init_self_forces( s, p, d);
}


void P3M_ad( system_t *s, parameters_t *p, data_t *d, forces_t *f )
{
  
  /* Loop counters */
  int i, j, k, c_index; 
  /* Helper variables */
  FLOAT_TYPE T1;
  FLOAT_TYPE Leni = 1.0/s->length;
  int Mesh = p->mesh;
  
  memset(d->Qmesh, 0, 2*Mesh*Mesh*Mesh * sizeof(FLOAT_TYPE));

  #ifdef __detailed_timings
  timer = MPI_Wtime();
  #endif
  
  /* chargeassignment */
  assign_charge_and_derivatives( s, p, d, 0, 1 );

  #ifdef __detailed_timings
  timer = MPI_Wtime() - timer;
  t_charge_assignment[2] = timer;
  timer = MPI_Wtime();
  #endif
  
  /* Forward Fast Fourier Transform */
  forward_fft(d);

  #ifdef __detailed_timings
    timer = MPI_Wtime() - timer;
    t_fft[2] = timer;
   timer = MPI_Wtime();
  #endif
 

  for (i=0; i<Mesh; i++)
    for (j=0; j<Mesh; j++)
      for (k=0; k<Mesh; k++)
	{
          c_index = c_ind(i,j,k);

	  T1 = d->G_hat[r_ind(i,j,k)];
	  d->Qmesh[c_index] *= T1;
	  d->Qmesh[c_index+1] *= T1;
	}

  #ifdef __detailed_timings
    timer = MPI_Wtime() - timer;
    t_convolution[2] = timer;
   timer = MPI_Wtime();
  #endif


  /* Backward FFT */
  backward_fft(d);

  #ifdef __detailed_timings
    timer = MPI_Wtime() - timer;
    t_fft[2] += timer;
   timer = MPI_Wtime();
  #endif


  /* Force assignment */
  assign_forces_ad( Mesh * Leni * Leni * Leni , s, p, d, f, 0 );

  #ifdef __detailed_timings
    timer = MPI_Wtime() - timer;
    t_force_assignment[2] = timer;
  #endif

    Substract_self_forces(s,p,d,f);

  return;
}

void p3m_tune_aliasing_sums_ad(int nx, int ny, int nz, 
			       system_t *s, parameters_t *p,
			       FLOAT_TYPE *alias1, FLOAT_TYPE *alias2, FLOAT_TYPE *alias3,FLOAT_TYPE *alias4)
{

  int    mx,my,mz;
  FLOAT_TYPE nmx,nmy,nmz;
  FLOAT_TYPE fnmx,fnmy,fnmz;

  FLOAT_TYPE ex,ex2,nm2,U2,factor1;

  FLOAT_TYPE mesh_i = 1.0 / p->mesh;

  factor1 = SQR ( PI / ( p->alpha*s->length ) );

  *alias1 = *alias2 = *alias3 = *alias4 = 0.0;

  for (mx=-P3M_BRILLOUIN_TUNING; mx<=P3M_BRILLOUIN_TUNING; mx++) {
    fnmx = mesh_i * (nmx = nx + mx*p->mesh);
    for (my=-P3M_BRILLOUIN_TUNING; my<=P3M_BRILLOUIN_TUNING; my++) {
      fnmy = mesh_i * (nmy = ny + my*p->mesh);
      for (mz=-P3M_BRILLOUIN_TUNING; mz<=P3M_BRILLOUIN_TUNING; mz++) {
	fnmz = mesh_i * (nmz = nz + mz*p->mesh);
	
	nm2 = SQR ( nmx ) + SQR ( nmy ) + SQR ( nmz );
        ex = EXP(-factor1*nm2);
	ex2 = SQR( ex );
	
	U2 = my_power(sinc(fnmx)*sinc(fnmy)*sinc(fnmz), 2*p->cao);
	
	*alias1 += ex2 / nm2;
	*alias2 += U2 * ex;
	*alias3 += U2 * nm2;
	*alias4 += U2;
      }
    }
  }
}

void p3m_tune_aliasing_sums_ad_correlated_dipoles(int nx, int ny, int nz, 
			       system_t *s, parameters_t *p,
			       FLOAT_TYPE *alias1, FLOAT_TYPE *alias2, FLOAT_TYPE *alias3,FLOAT_TYPE *alias4)
{

  int    mx,my,mz;
  FLOAT_TYPE nmx,nmy,nmz;
  FLOAT_TYPE fnmx,fnmy,fnmz;

  FLOAT_TYPE ex,ex2,nm2,U2,factor1;

  FLOAT_TYPE mesh_i = 1.0 / p->mesh;

  factor1 = SQR ( PI / ( p->alpha*s->length ) );

  *alias1 = *alias2 = *alias3 = *alias4 = 0.0;

  for (mx=-P3M_BRILLOUIN_TUNING; mx<=P3M_BRILLOUIN_TUNING; mx++) {
    fnmx = mesh_i * (nmx = nx + mx*p->mesh);
    for (my=-P3M_BRILLOUIN_TUNING; my<=P3M_BRILLOUIN_TUNING; my++) {
      fnmy = mesh_i * (nmy = ny + my*p->mesh);
      for (mz=-P3M_BRILLOUIN_TUNING; mz<=P3M_BRILLOUIN_TUNING; mz++) {
	fnmz = mesh_i * (nmz = nz + mz*p->mesh);
	
	nm2 = SQR ( nmx ) + SQR ( nmy ) + SQR ( nmz );
        ex = EXP(-factor1*nm2);
	ex2 = SQR( ex );
	
	U2 = my_power(sinc(fnmx)*sinc(fnmy)*sinc(fnmz), 2*p->cao);
	
	*alias1 += ex2 / nm2;
	*alias2 += U2 * ex;
	*alias3 += U2 * nm2;
	*alias4 += U2;
      }
    }
  }
}


FLOAT_TYPE p3m_k_space_error_ad( system_t *s, parameters_t *p )
{
  int  nx, ny, nz;
  FLOAT_TYPE mesh = p->mesh;
  FLOAT_TYPE box_size = s->length;
  FLOAT_TYPE he_q = 0.0;
  FLOAT_TYPE alias1, alias2, alias3, alias4;

  for (nx=-mesh/2; nx<mesh/2; nx++) {
    for (ny=-mesh/2; ny<mesh/2; ny++) {
      for (nz=-mesh/2; nz<mesh/2; nz++) {
	if((nx!=0) || (ny!=0) || (nz!=0)) {
	  p3m_tune_aliasing_sums_ad(nx,ny,nz, s, p, &alias1,&alias2,&alias3,&alias4);	//alias4 = cs

	  if( (alias3 == 0.0) || (alias4 == 0.0) )
	    continue;
	  he_q += alias1  -  (SQR(alias2) / (alias3*alias4));
	}
      }
    }
  }
  he_q = FLOAT_ABS(he_q);
  return 2.0*s->q2*SQRT ( he_q/ (FLOAT_TYPE)s->nparticles) / SQR(box_size);
}

FLOAT_TYPE Error_ad( system_t *s, parameters_t *p ) {
  FLOAT_TYPE real = Realspace_error( s, p);
  FLOAT_TYPE recp = p3m_k_space_error_ad( s, p );
  //  printf("p3m ad error for mesh %d rcut %lf cao %d alpha %lf : real %e recp %e\n", p->mesh, p->rcut, p->cao, p->alpha, real, recp);
  //  printf("system size %d box %lf\n", s->nparticles, s->length);
  return SQRT( SQR ( real ) + SQR ( recp ) );
}
