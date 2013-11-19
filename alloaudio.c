#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <unistd.h>

#include "alloaudio.h"
#include "firfilter.h"
#include "pthread.h"

typedef struct list_member{
    char *in_port;
    char* out_port;
    struct list_member *next;
} list_member_t;

struct connection_data {
    jack_client_t *client;
    jack_port_t **input_ports;
    jack_port_t **output_ports;
    int num_chnls;

    /* parameters */
    double *gains;
    int mute_all; // 0=no 1=yes
    double master_gain;
    int clipper_on;
    int filters_active;

    /* output filters */
    FIRFILTER **filters;

    /* auto-connection */
    int cur_port_index;
    pthread_t conn_thread;
    pthread_mutex_t conn_list_mutex;
    list_member_t *conn_list;
};

void allocate_ports(connection_data_t *pp, int num_chnls)
{
    int i;
    if (pp->num_chnls > 0) {
        for (i = 0; i < pp->num_chnls; i++) {
            jack_port_unregister(pp->client, pp->input_ports[i]);
            jack_port_unregister(pp->client, pp->output_ports[i]);
        }
        free(pp->input_ports);
        free(pp->output_ports);
        free(pp->gains);
    }
    pp->num_chnls = 0;
    pp->input_ports = (jack_port_t **) calloc(num_chnls, sizeof(jack_port_t *));
    pp->output_ports = (jack_port_t **) calloc(num_chnls, sizeof(jack_port_t *));
    pp->gains = (double *) calloc(num_chnls, sizeof(double));
    pp->master_gain = 1.0;
    pp->mute_all = 0;
    pp->clipper_on = 1;
    pp->filters_active = 0;
    pp->filters = (FIRFILTER **) calloc(num_chnls, sizeof(FIRFILTER *));
    pp->cur_port_index = 0;
    pthread_mutex_init(&pp->conn_list_mutex, NULL);
    pp->conn_list = NULL;

    for (i = 0; i < num_chnls; i++) {
        char name[32];
        sprintf(name, "input_%i", i);
        pp->input_ports[i] = jack_port_register (pp->client, name,
                                             JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsInput, 0);
        sprintf(name, "output_%i", i);
        pp->output_ports[i] = jack_port_register (pp->client, name,
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput, 0);
        pp->gains[i] = 0.2;
    }
    pp->num_chnls = num_chnls;
}

void connect_ports(connection_data_t *pp)
{
    //    /* try to connect to the first physical input & output ports */

    //    if (jack_connect (client, "system:playback_1",
    //                      jack_port_name (pp->input_ports))) {
    //        fprintf (stderr, "cannot connect input port\n");
    //        return 1;	/* terminate client */
    //    }

    //    if (jack_connect (client, jack_port_name (pp->output_ports),
    //                      "system:playback_1")) {
    //        fprintf (stderr, "cannot connect output port\n");
    //        return 1;	/* terminate client */
    //    }
}

/* audio callback */
int inprocess (jack_nframes_t nframes, void *arg)
{
    int i, chan = 0;
    connection_data_t *pp = (connection_data_t *) arg;
    float master_gain = pp->master_gain * (pp->mute_all == 0 ? 1 : 0);
    for (chan = 0; chan < pp->num_chnls; chan++) {
        float gain = master_gain * pp->gains[chan];
        jack_default_audio_sample_t *out =
                jack_port_get_buffer (pp->output_ports[chan], nframes);
        jack_default_audio_sample_t *in =
                jack_port_get_buffer (pp->input_ports[chan], nframes);
        if (pp->filters_active) {
            double filt_in[nframes];
            double filt_out[nframes];

            for (i = 0; i < nframes; i++) {
                filt_in[i] = *in++;
            }
            firfilter_next(pp->filters[chan],filt_in, filt_out, nframes, gain);
            for (i = 0; i < nframes; i++) {
                *out = filt_out[i];
                if (pp->clipper_on && *out > gain) {
                    *out = gain;
                }
                out++;
            }
        } else {
            for (i = 0; i < nframes; i++) {
                *out = *in++ * gain;
                if (pp->clipper_on && *out > gain) {
                    *out = gain;
                }
                out++;
            }
        }
    }
    return 0;	/* continue */
}

void client_registered(const char* name, int reg, void *arg)
{
    connection_data_t *pp = (connection_data_t *) arg;
    printf("Client registered: %s\n", name);
    pp->cur_port_index = 0;
}

void port_registered(jack_port_id_t port_id, int reg, void *arg)
{
    connection_data_t *pp = (connection_data_t *) arg;
    jack_port_t *port = jack_port_by_id(pp->client, port_id);

    int flags = jack_port_flags(port);

    if (flags & JackPortIsOutput && !jack_port_is_mine(pp->client, port)) {
        const char *out_name = jack_port_name(pp->input_ports[pp->cur_port_index&pp->num_chnls]);
        const char *in_name = jack_port_name(port);
        int len_out = strlen(out_name);
        int len_in = strlen(in_name);
        list_member_t *conn = (list_member_t *) malloc(sizeof(list_member_t));
        list_member_t *last = pp->conn_list;
        while (last->next) {
            last = last->next;
        }
        conn->in_port = (char *) calloc(len_in + 1, sizeof(char));
        strncpy(conn->in_port, in_name, len_in);
        conn->out_port = (char *) calloc(len_out + 1, sizeof(char));
        strncpy(conn->out_port, out_name, len_out);
        pthread_mutex_lock(&pp->conn_list_mutex);
        last->next = conn;
        conn->next = NULL;
        pthread_mutex_unlock(&pp->conn_list_mutex);
        pp->cur_port_index++;
    }
}

void *connector_thread(void *arg)
{
    connection_data_t *pp = (connection_data_t *) arg;
    while(1) {
        pthread_mutex_lock(&pp->conn_list_mutex);
        while (pp->conn_list) {
            list_member_t *conn = pp->conn_list;
            pp->conn_list = pp->conn_list->next;
//            jack_connect(pp->client, conn->in_port, conn->out_port);
            printf("Connect %s to %s\n", conn->in_port, conn->out_port);
            free(conn->in_port);
            free(conn->out_port);
            free(conn);
        }
        pthread_mutex_unlock(&pp->conn_list_mutex);
        sleep(1);
    }
}

connection_data_t *jack_initialize(int num_chnls)
{
    jack_client_t *client = jack_client_open("Alloaudio", JackNoStartServer, NULL);
    connection_data_t *pp = malloc (sizeof (connection_data_t));

    if (!client) {
        printf("Error creating jack client.\n");
        return 0;
    }
    if (pp == NULL) {
        printf("Error allocating internal data.\n");
        return 0;	/* heap exhausted */
    }
    /* init connection data structure */
    pp->client = client;

    jack_set_process_callback (client, inprocess, pp);

    allocate_ports(pp, num_chnls);
    jack_set_client_registration_callback(client, client_registered, pp);
    jack_set_port_registration_callback(client, port_registered, pp);
    jack_activate (client);

    if (!pthread_create(&pp->conn_thread, NULL, connector_thread, pp)) {
        printf("Error creating connector thread ports.\n");
    }
    connect_ports(pp);
    return pp;
}

void set_filters(connection_data_t *pp, double **irs, int filter_len)
{
    int i;
    pp->filters_active = 0;
    if (!irs) { /* if NULL, leave filtering off */
        return;
    }
    /* FIXME: app can crash if filter changes while filtering in the audio callback, for now, make apps wait a bit when changing */
    for (i = 0; i < pp->num_chnls; i++) {
        FIRFILTER *new_filter = firfilter_create(irs[i], filter_len);
        FIRFILTER *old_filter = pp->filters[i];
        pp->filters[i] = new_filter;
        if (old_filter) {
            firfilter_free(old_filter);
        }
    }

    pp->filters_active = 1;
}

void jack_close (connection_data_t *pp)
{
    /* FIXME close ports and deallocate filters before clearing memory */
    free(pp->input_ports);
    free(pp->output_ports);
    free(pp->gains);
    free(pp->filters);

    free(pp);
}

/* parameter setters */

void set_global_gain(connection_data_t *pp, float gain)
{
    pp->master_gain = gain;
}

void set_gain(connection_data_t *pp, int channel_index, float gain)
{
    if (channel_index >= 0 && channel_index < pp->num_chnls) {
        pp->gains[channel_index] = gain;
    } else {
        printf("Alloaudio error: set_gain() for invalid channel %i", channel_index);
    }
}

void set_mute_all(connection_data_t *pp, int mute_all)
{
    pp->mute_all = mute_all;
}

void set_clipper_on(connection_data_t *pp, int clipper_on)
{
    pp->clipper_on = clipper_on;
}

void set_room_compensation_on(connection_data_t *pp, int room_compensation_on)
{
    pp->filters_active = room_compensation_on;
}

