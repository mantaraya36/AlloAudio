#include <stdio.h>
#include <stdlib.h>

#include "alloaudio.h"
#include "osc_control.h"


int read_irs(const char *filename, double **irs, int *filt_len)
{
    int num_chnls, chan, i;
    FILE *fp = fopen(filename, "rb");
    irs = NULL;
    if (!fp) {
        printf("Error opening file: '%s'.\n", filename);
        return 0;
    }
    if (fread(&num_chnls, sizeof(int), 1, fp) != sizeof(int)) {
        printf("Error reading num_chanls.");
        return 0;
    }
    if (fread(filt_len, sizeof(int), 1, fp) != sizeof(int)) {
        printf("Error reading filt_len.");
        return 0;
    }
    irs = (double **) calloc(num_chnls, sizeof(double *));
    for (chan = 0; chan < num_chnls; chan++) {
        irs[chan] = (double *) calloc(*filt_len, sizeof(double));
        if(fread(irs[chan], sizeof(double), *filt_len, fp) != *filt_len * sizeof(double)) {
            printf("Error wrong ir size %i for ir #%i\n.", *filt_len, chan);
            return 0;
        }
    }
    return num_chnls;
}

void print_usage()
{
    printf("Usage:\n");
    printf("alloaudiocontrol [numchnls ir.dat]\n");
}

int main(int argc, char *argv[])
{
    alloaudio_data_t *pp;
    oscdata_t *od;
    int i, filt_len, num_irs;
    double **irs;
    int numchnls = 8;

    if (argc > 1) {
        numchnls = atoi(argv[1]);
        if (numchnls < 1 || numchnls > 128) {
            printf("Error: wrong number of channels: %i\n", numchnls);
            print_usage();
            return -1;
        }
    }
    pp = create_alloaudio(numchnls);
    if (!pp) return -1;

    od = create_osc("8082", "8083", pp);
    if (!od) return -1;

    if (argc > 2) {
        num_irs = read_irs(argv[2], irs, &filt_len);
    } else {
        num_irs = read_irs("irs.dat", irs, &filt_len);
    }

    if (num_irs == numchnls) {
        set_filters(pp, irs, filt_len);
    } else {
        printf("Could not set filters.\n");
    }

    printf("Alloaudio started.\n");fflush(stdout);
    i = 0;
    while (!i) {
        scanf("Enter :%i", &i);
    }
    printf("Alloaudio Closing.\n");

    delete_osc(od);
    free_alloaudio(pp);
    return 0;
}

