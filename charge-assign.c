#include "charge-assign.h"

#include <stdio.h>
#include <math.h>
#include <assert.h>

// #define CA_DEBUG

void assign_charge(system_t *s, parameters_t *p, data_t *d, int ii)
{
    int dim, i0, i1, i2, id;
    FLOAT_TYPE tmp0, tmp1;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    FLOAT_TYPE cur_ca_frac_val;
    int cf_cnt;
    // Mesh coordinates of the closest mesh point
    int base[3];
    int i,j,k;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)d->mesh/(double)s->length;

    // Make sure parameter-set and data-set are compatible
    assert( p->mesh == d->mesh );

    int MESHMASKE = d->mesh - 1;
    double pos_shift;

    for (id=0;id<s->nparticles;id++) {
        cf_cnt = id*p->cao3;

        pos_shift = (double)((p->cao-1)/2);
        /* particle position in mesh coordinates */
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift + 0.5*ii;
            nmp = (int) floor(pos + 0.5);
            base[dim]  = (nmp > 0) ? nmp&MESHMASKE : (nmp + d->mesh)&MESHMASKE;
            arg[dim] = (int) floor((pos - nmp + 0.5)*MI2);
            d->ca_ind[ii][3*id + dim] = base[dim];
        }

        for (i0=0; i0<p->cao; i0++) {
            i = (base[0] + i0)&MESHMASKE;
            tmp0 = s->q[id] * LadInt[i0][arg[0]];
            for (i1=0; i1<p->cao; i1++) {
                j = (base[1] + i1)&MESHMASKE;
                tmp1 = tmp0 * LadInt[i1][arg[1]];
                for (i2=0; i2<p->cao; i2++) {
                    k = (base[2] + i2)&MESHMASKE;
                    cur_ca_frac_val = tmp1 * LadInt[i2][arg[2]];
                    d->cf[ii][cf_cnt++] = cur_ca_frac_val ;
                    d->Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val;
                }
            }
        }
    }
}

// assign the forces obtained from k-space
void assign_forces(FLOAT_TYPE force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii) {
    int i,i0,i1,i2, dim;
    int cf_cnt=0;
    int *base;
    int j,k,l;
    int MESHMASKE = d->mesh - 1;
    FLOAT_TYPE A,B;

    for (dim=0;dim<3;dim++) {
        cf_cnt=0;

        for (i=0; i<s->nparticles; i++) {
            base = d->ca_ind[ii] + 3*i;
            for (i0=0; i0<p->cao; i0++) {
                j = (base[0] + i0)&MESHMASKE;
                for (i1=0; i1<p->cao; i1++) {
                    k = (base[1] + i1)&MESHMASKE;
                    for (i2=0; i2<p->cao; i2++) {
                        l = (base[2] + i2)&MESHMASKE;
                        A = d->cf[ii][cf_cnt];
                        B = d->Fmesh->fields[dim][c_ind(j,k,l)+ii];

                        f->f_k->fields[dim][i] -= force_prefac*A*B;
                        cf_cnt++;
                    }
                }
            }
            if (ii==1)
                f->f_k->fields[dim][i] *= 0.5;
        }
    }
}

void assign_charge_and_derivatives(system_t *s, parameters_t *p, data_t *d, int ii, int derivatives)
{
    int dim, i0, i1, i2;
    int id;
    FLOAT_TYPE tmp0, tmp1, tmp2;
    FLOAT_TYPE tmp0_x, tmp1_y, tmp2_z;
    /* position of a particle in local mesh units */
    FLOAT_TYPE pos;
    /* 1d-index of nearest mesh point */
    int nmp;
    /* index for caf interpolation grid */
    int arg[3];
    /* index, index jumps for rs_mesh array */
    FLOAT_TYPE cur_ca_frac_val;
    int cf_cnt;

    int base[3];
    int i,j,k;
    int Mesh = p->mesh;
    FLOAT_TYPE MI2 = 2.0*(FLOAT_TYPE)MaxInterpol;

    FLOAT_TYPE Hi = (double)Mesh/s->length;
    FLOAT_TYPE Leni = 1.0/s->length;


    int MESHMASKE = Mesh - 1;
    double pos_shift;

    pos_shift = (double)((p->cao-1)/2);
    /* particle position in mesh coordinates */
    for (id=0;id<s->nparticles;id++) {
        cf_cnt = id*p->cao3;
        for (dim=0;dim<3;dim++) {
            pos    = s->p->fields[dim][id]*Hi - pos_shift;
            nmp = (int) floor(pos + 0.5);
            base[dim]  = (nmp > 0) ? nmp%MESHMASKE : (nmp + Mesh)%MESHMASKE;
            arg[dim] = (int) floor((pos - nmp + 0.5)*MI2);
            d->ca_ind[ii][3*id + dim] = base[dim];
        }

        for (i0=0; i0<p->cao; i0++) {
            i = (base[0] + i0)&MESHMASKE;
            tmp0 = LadInt[i0][arg[0]];
            tmp0_x = LadInt_[i0][arg[0]];
            for (i1=0; i1<p->cao; i1++) {
                j = (base[1] + i1)&MESHMASKE;
                tmp1 = LadInt[i1][arg[1]];
                tmp1_y = LadInt_[i1][arg[1]];
                for (i2=0; i2<p->cao; i2++) {
                    k = (base[2] + i2)&MESHMASKE;
                    tmp2 = LadInt[i2][arg[2]];
                    tmp2_z = LadInt_[i2][arg[2]];
                    cur_ca_frac_val = s->q[id] * tmp0 * tmp1 * tmp2;
                    d->cf[ii][cf_cnt] = cur_ca_frac_val ;
                    if (derivatives) {
                        d->dQdx[ii][cf_cnt] = Leni * tmp0_x * tmp1 * tmp2 * s->q[id];
                        d->dQdy[ii][cf_cnt] = Leni * tmp0 * tmp1_y * tmp2 * s->q[id];
                        d->dQdz[ii][cf_cnt] = Leni * tmp0 * tmp1 * tmp2_z * s->q[id];
                    }
                    d->Qmesh[c_ind(i,j,k)+ii] += cur_ca_frac_val;
                    cf_cnt++;
                }
            }
        }
    }
}

// assign the forces obtained from k-space
void assign_forces_ad(double force_prefac, system_t *s, parameters_t *p, data_t *d, forces_t *f, int ii)
{
    int i,i0,i1,i2, dim;
    int cf_cnt=0;
    int *base;
    int j,k,l;
    int MESHMASKE = p->mesh - 1;
    FLOAT_TYPE B;

    cf_cnt=0;
    for (i=0; i<s->nparticles; i++) {
        base = d->ca_ind[ii] + 3*i;
        cf_cnt = i*p->cao3;
        for (i0=0; i0<p->cao; i0++) {
            j = (base[0] + i0)&MESHMASKE;
            for (i1=0; i1<p->cao; i1++) {
                k = (base[1] + i1)&MESHMASKE;
                for (i2=0; i2<p->cao; i2++) {
                    l = (base[2] + i2)&MESHMASKE;
                    B = d->Qmesh[c_ind(j,k,l)+ii];
                    f->f_k->x[i] -= force_prefac*B*d->dQdx[ii][cf_cnt];
                    f->f_k->y[i] -= force_prefac*B*d->dQdy[ii][cf_cnt];
                    f->f_k->z[i] -= force_prefac*B*d->dQdz[ii][cf_cnt];
                    cf_cnt++;
                }
            }
        }
        if (ii==1) {
            for (dim=0;dim<3;dim++)
                f->f_k->fields[dim][i] *= 0.5;
        }
    }
}
