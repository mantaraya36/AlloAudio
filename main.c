#include <stdio.h>
#include <jack/intclient.h>
#include <jack/jack.h>
#include <lo/lo.h>

#include "alloaudio.h"
#include "osc_control.h"


int main(void)
{
    connection_data_t *pp;
    oscdata_t *od;
    int i;

    pp = jack_initialize(8);
    od = create_osc("7070", pp);

    scanf("Enter :%i", &i);
    printf("Alloaudio Closing.\n");

    delete_osc(od);
    jack_close(pp);
    return 0;
}

