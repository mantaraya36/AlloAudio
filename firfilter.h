#ifndef FIRFILTER_H
#define FIRFILTER_H

typedef struct FIRFILTER_  FIRFILTER;

FIRFILTER *firfilter_create(double *ir, int filtlen);

void firfilter_next(void *filter, double *in, double *out, int nframes, double gain);

void firfilter_free(FIRFILTER *filter);

#endif /* FIRFILTER_H */
