
#include <stdlib.h>

#include "firfilter.h"

#ifdef FIRFILTER_TIME_DOMAIN

struct FIRFILTER_ {
    double *buffer;
    double *revir;
    int filtlen;
    int bufreadpos;
};

FIRFILTER *firfilter_create(double *ir, int filtlen)
{
    int i;
    FIRFILTER *filter = (FIRFILTER *) malloc(sizeof(FIRFILTER));

    filter->buffer = (double *) calloc(filtlen, sizeof(double));
    filter->revir = (double *) malloc(filtlen * sizeof(double));
    filter->bufreadpos = 0;
    filter->filtlen = filtlen;
    for (i = 0; i < filtlen; i++) {
        filter->revir[filtlen - i - 1] = *ir++;
    }
    return filter;
}

void firfiler_next(FIRFILTER *filter, double *in, double *out, int nframes, double gain)
{
    int i, j ;
    double next;
    for (i = 0; i < nframes; i++) {
        double *revir = filter->revir;
        for (j = 0; j <filter->filtlen - 1; j++) {
            filter->buffer[filter->bufreadpos] += *in * *revir++;
            filter->bufreadpos++;
            filter->bufreadpos = filter->bufreadpos % (filter->filtlen);
        }
        filter->buffer[filter->bufreadpos] += *in * *revir++;
        next = filter->buffer[filter->bufreadpos];
        *out++ = next * gain;
        filter->buffer[filter->bufreadpos] = 0.0;
        in++;
    }
}

void firfilter_free(FIRFILTER *filter)
{
    free(filter->buffer);
    free(filter->revir);
    free(filter);
}

#else
#include <complex.h>
#include <fftw3.h>

struct FIRFILTER_ {
    int filtlen;
    int bufinpos;
    fftw_plan pin;
    fftw_plan pout;
    double *in;
    fftw_complex *comp;
    double *out;
    fftw_complex *ir_complex;
    double *prev_tail;
};

FIRFILTER *firfilter_create(double *ir, int filtlen)
{
    int i;
    FIRFILTER *filter = (FIRFILTER *) malloc(sizeof(FIRFILTER));
    filter->filtlen = filtlen;
    filter->bufinpos = 0;
    filter->in = (double *) fftw_malloc(sizeof(double) * (filtlen * 2));
    filter->comp = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * (filtlen + 1));
    filter->out = (double *) fftw_malloc(sizeof(double) * (filtlen * 2));
    filter->ir_complex = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * (filtlen + 1));
    filter->prev_tail = (double *) fftw_malloc(sizeof(double) * filtlen);
    filter->pin = fftw_plan_dft_r2c_1d(filtlen*2, filter->in, filter->comp, FFTW_MEASURE);
    filter->pout = fftw_plan_dft_c2r_1d(filtlen*2, filter->comp, filter->out, FFTW_MEASURE);

    for (i = 0; i < filtlen; i++) {
        filter->in[i] = ir[i];
        filter->in[i+filtlen] = 0.0;
        filter->prev_tail[i] = 0.0;
    }
    fftw_execute(filter->pin);
    for (i = 0; i < filtlen + 1; i++) {
        filter->ir_complex[i] = filter->comp[i];
    }
    return filter;
}

void firfilter_next(void *filter_, double *in, double *out, int nframes, double gain)
{
    FIRFILTER *filter = (FIRFILTER *)filter_;
    int i, j;
    double *next_in;
    double *next_out;
    double fftsize = filter->filtlen * 2;
    next_in = &(filter->in[filter->bufinpos]);
    next_out = &(filter->out[filter->bufinpos]);
    for (i = 0; i < nframes; i++) {
        *next_in++ = *in++ * gain / fftsize;
        *out++ = *next_out++;
        filter->bufinpos++;
        if (filter->bufinpos == filter->filtlen) { /* process fft */
            fftw_execute(filter->pin);
            for (j = 0; j < filter->filtlen + 1; j++) {
                filter->comp[j] *= filter->ir_complex[j];
            }
            fftw_execute(filter->pout);
            for (j = 0; j < filter->filtlen; j++) {
                filter->out[j] += filter->prev_tail[j];
                filter->prev_tail[j] = filter->out[j + filter->filtlen];
            }
            filter->bufinpos = 0;
            next_in = filter->in;
            next_out = filter->out;
        }
    }
}

void firfilter_free(FIRFILTER *filter)
{
    fftw_free(filter->in);
    fftw_free(filter->out);
    fftw_free(filter->comp);
    fftw_free(filter->ir_complex);
    fftw_free(filter->prev_tail);
    free(filter);
}

#endif
