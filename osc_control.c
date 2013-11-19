#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <lo/lo.h>
#include <assert.h>

#include "osc_control.h"

struct oscdata {
    lo_server_thread st;
    connection_data_t *pp;
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
    connection_data_t *pp = (connection_data_t *) user_data;
    printf("%s <- %f\n", path, argv[0]->f); fflush(stdout);

    set_global_gain(pp, argv[0]->f);
    return 0;
}

int gain_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    printf("%s <- f:%f\n", path, argv[0]->f); fflush(stdout);

    set_gain(pp, argv[0]->i, argv[0]->f);
    return 0;
}

int mute_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    printf("%s <- f:%i\n", path, argv[0]->i); fflush(stdout);

    set_mute_all(pp, argv[0]->i);
    return 0;
}

int clipper_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    printf("%s <- f:%i\n", path, argv[0]->i); fflush(stdout);

    set_clipper_on(pp, argv[0]->i);
    return 0;
}

int room_compensation_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    printf("%s <- f:%i\n", path, argv[0]->i); fflush(stdout);

    set_room_compensation_on(pp, argv[0]->i);
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


oscdata_t *create_osc(const char *port, connection_data_t *pp)
{
    oscdata_t *od = (oscdata_t *) malloc(sizeof(oscdata_t));
    od->st = lo_server_thread_new(port, osc_error);

    if(!od->st) {
        printf("Error setting up OSC. Is Alloaudio already running?\n");
        free(od);
        return 0;
    }

    lo_server_thread_add_method(od->st, "/Alloaudio/global_gain", "f", global_gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/gain", "if", gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/mute_all", "i", mute_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/clipper_on", "i", mute_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/room_compensation_on", "i", room_compensation_handler, pp);

    /* add method that will match any path and args */
    lo_server_thread_add_method(od->st, NULL, NULL, generic_handler, NULL);

    lo_server_thread_start(od->st);
    return od;
}

void delete_osc(oscdata_t *od)
{
    assert(od->st != 0);
    lo_server_thread_free(od->st);
    free(od);
}
