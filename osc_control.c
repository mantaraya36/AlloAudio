#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <lo/lo.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>

#include "osc_control.h"

struct oscdata {
    lo_server_thread st;
    alloaudio_data_t *pp;
    char outport[8];
    pthread_t metr_thread;
    int closing;
};

/* OSC handler functions */
void osc_error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int global_gain_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- %f\n", path, argv[0]->f); fflush(stdout);

    set_global_gain(pp, argv[0]->f);
    return 0;
}

int gain_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- f:%f\n", path, argv[0]->f); fflush(stdout);

    set_gain(pp, argv[0]->i, argv[0]->f);
    return 0;
}

int mute_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- i:%i\n", path, argv[0]->i); fflush(stdout);

    set_mute_all(pp, argv[0]->i);
    return 0;
}

int clipper_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- i:%i\n", path, argv[0]->i); fflush(stdout);

    set_clipper_on(pp, argv[0]->i);
    return 0;
}

int room_compensation_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- i:%i\n", path, argv[0]->i); fflush(stdout);

    set_room_compensation_on(pp, argv[0]->i);
    return 0;
}

int bass_management_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- f:%f\n", path, argv[0]->f); fflush(stdout);

    set_bass_management_freq(pp, argv[0]->f);
    return 0;
}

int bass_management_mode_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("%s <- i:%i\n", path, argv[0]->i); fflush(stdout);

    if (argv[0]->i < 0 || argv[0]->i >= BASSMODE_COUNT) {
        printf("Invalid bass management mode %i.\n", argv[0]->i); fflush(stdout);
        return 0;
    }
    set_bass_management_mode(pp, argv[0]->i);
    return 0;
}

int sw_indeces_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("sw_indeces_handler %s <- %i %i, %i, %i\n", path, argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i); fflush(stdout);

    set_sw_indeces(pp, argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i);
    return 0;
}

int meter_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    alloaudio_data_t *pp = (alloaudio_data_t *) user_data;
    printf("meter_handler %s <- i:%i\n", path, argv[0]->i); fflush(stdout);

    set_meter(pp, argv[0]->i);
    return 0;
}


/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
int generic_handler(const char *path, const char *types, lo_arg ** argv,
                    int argc, void *data, void *user_data)
{
    int i;

    printf("Not processed path: <%s>\n", path);
    for (i = 0; i < argc; i++) {
        printf("arg %d '%c' ", i, types[i]);
        lo_arg_pp((lo_type)types[i], argv[i]);
        printf("\n");
    }
    printf("\n");
    fflush(stdout);

    return 1;
}

void *meter_thread(void *arg)
{
    oscdata_t *od = (oscdata_t *) arg;
    alloaudio_data_t *pp = od->pp;
    int i;
    int num_chnls = get_num_chnls(pp);
    float meter_levels[num_chnls];
    lo_address t = lo_address_new("localhost", od->outport);
    while(!is_closing(pp)) {
        int bytes_read = get_meter_values(pp, meter_levels);
        if (bytes_read) {
            if (bytes_read != num_chnls *sizeof(float)) {
                printf("Warning. Meter values xrun.\n"); fflush(stdout);
            }
        }
        for (i = 0; i < bytes_read; i++) {
            char addr[64];
//            sprintf(addr,"/Alloaudio/meter%i", i);
            lo_send(t, "/Alloaudio/meter", "if", i, meter_levels[i]);
//            lo_send(t, "/Alloaudio/meter", "if", i, 20.0 * log10(meter_levels[i]));
//            lo_send(t, addr, "f", 20.0 * log10(meter_levels[i]));
//            lo_send(t, addr, "f", meter_levels[i]);
        }
        usleep(10000); /* This rate should be significantly faster than the accumulation of meter values in the audio function */
    }
}


oscdata_t *create_osc(const char *inport, const char *outport, alloaudio_data_t *pp)
{
    oscdata_t *od = (oscdata_t *) malloc(sizeof(oscdata_t));
    od->st = lo_server_thread_new(inport, osc_error);
    od->pp = pp;
    od->closing = 0;

    if(!od->st) {
        printf("Error setting up OSC. Is Alloaudio already running?\n");
        free(od);
        return 0;
    }

    if (pthread_create(&od->metr_thread, NULL, meter_thread, od) != 0) {
        printf("Error creating meter values thread.\n");
    }

    strncpy(od->outport, outport, 8);

    printf("OSC: Listening on port %s. Sending on port %s.\n", inport, od->outport);

    lo_server_thread_add_method(od->st, "/Alloaudio/global_gain", "f", global_gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/gain", "if", gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/mute_all", "i", mute_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/clipper_on", "i", mute_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/room_compensation_on", "i", room_compensation_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/bass_management_freq", "f", bass_management_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/bass_management_mode", "i", bass_management_mode_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/sw_indeces", "iiii", sw_indeces_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/meter_on", "i", meter_handler, pp);

    /* add method that will match any path and args */
    lo_server_thread_add_method(od->st, NULL, NULL, generic_handler, NULL);

    lo_server_thread_start(od->st);
    return od;
}

void delete_osc(oscdata_t *od)
{
    assert(od->st != 0);
    od->closing = 1;
    pthread_join(od->metr_thread, NULL);
    lo_server_thread_free(od->st);
    free(od);
}
