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
    if (fread(&num_chnls, sizeof(int), 1, fp) != 1) {
        printf("Error reading num_chanls.");
        return 0;
    }
    if (fread(filt_len, sizeof(int), 1, fp) != 1) {
        printf("Error reading filt_len.");
        return 0;
    }
    irs = (double **) calloc(num_chnls, sizeof(double *));
    for (chan = 0; chan < num_chnls; chan++) {
        irs[chan] = (double *) calloc(*filt_len, sizeof(double));
        if(fread(irs[chan], sizeof(double), *filt_len, fp) != *filt_len) {
            printf("Error wrong ir size %i for ir #%i\n.", *filt_len, chan);
            return 0;
        }
    }
    return num_chnls;
}

int main(int argc, char *argv[])
{
    connection_data_t *pp;
    oscdata_t *od;
    int i, filt_len, num_irs;
    double **irs;

    pp = jack_initialize(8);
    od = create_osc("7070", pp);

    if (argc == 2) {
        num_irs = read_irs(argv[1], irs, &filt_len);
    } else {
        num_irs = read_irs("irs.dat", irs, &filt_len);
    }

    if (num_irs) {
        set_filters(pp, irs, filt_len);
    }

    scanf("Enter :%i", &i);
    printf("Alloaudio Closing.\n");

    delete_osc(od);
    jack_close(pp);
    return 0;
}

