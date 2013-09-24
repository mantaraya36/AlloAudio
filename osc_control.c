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

/* OSC functions */
void osc_error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int global_gain_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- %f\n", path, argv[0]->f);
    fflush(stdout);

    set_global_gain(pp, argv[0]->f);
    return 0;
}

int gain_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- f:%f\n", path, argv[0]->f);
    fflush(stdout);

    set_gain(pp, argv[0]->i, argv[0]->f);
    return 0;
}

int mute_handler(const char *path, const char *types, lo_arg ** argv,
                int argc, void *data, void *user_data)
{
    connection_data_t *pp = (connection_data_t *) user_data;
    /* example showing pulling the argument values out of the argv array */
    printf("%s <- f:%f\n", path, argv[0]->f);
    fflush(stdout);

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


    /* add method that will match the path /foo/bar, with two numbers, coerced
     * to float and int */
    lo_server_thread_add_method(od->st, "/Alloaudio/global_gain", "f", global_gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/gain", "if", gain_handler, pp);
    lo_server_thread_add_method(od->st, "/Alloaudio/mute", "f", mute_handler, pp);


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
