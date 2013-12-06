#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <pthread.h>
#include <jack/ringbuffer.h>

#include "alloaudio.h"
#include "firfilter.h"
#include "butter.h"
#include "autoconnector.h"

struct connection_data_s {
    jack_data_t *jd;
    autoconnector_t *ac;

    /* parameters */
    double *gains;
    int mute_all; // 0=no 1=yes
    double master_gain;
    int clipper_on;
    int filters_active;
    bass_mgmt_mode_t bass_management_mode; /* -1 no management, 0 SW routing without filters, >0 cross-over freq. in Hz. */
    int sw_index[4]; /* support for 4 SW max */
    int meter_on;

    pthread_mutex_t param_mutex;

    /* output data */
    float *meters;
    jack_ringbuffer_t *meter_buffer;
    int meter_counter; /* count samples for level updates */
    int meter_update_samples; /* number of samples between level updates */

    /* DRC (output) filters */
    FIRFILTER **filters;

    /* bass management filters */
    BUTTER **lopass1, **lopass2, **hipass1, **hipass2;

    int closing; /* to let other threads know they must end */
};

int chan_is_subwoofer(alloaudio_data_t *pp, int index)
{
    int i;
    for (i = 0; i < 4; i++) {
        if (pp->sw_index[i] == index && pp->bass_management_mode != BASSMODE_NONE) return 1;
    }
    return 0;
}

void allocate_ports(alloaudio_data_t *pp, int num_chnls)
{
    int i;
    jack_data_t *jd = pp->jd;
    jack_nframes_t sr = jack_get_sample_rate(jd->client);
    if (jd->num_chnls > 0) {
        for (i = 0; i < jd->num_chnls; i++) {
            jack_port_unregister(jd->client, jd->input_ports[i]);
            jack_port_unregister(jd->client, jd->output_ports[i]);
        }
        free(jd->input_ports);
        free(jd->output_ports);
        free(pp->gains);
    }

    jd->input_ports = (jack_port_t **) calloc(num_chnls, sizeof(jack_port_t *));
    jd->output_ports = (jack_port_t **) calloc(num_chnls, sizeof(jack_port_t *));
    pp->gains = (double *) calloc(num_chnls, sizeof(double));
    pp->filters = (FIRFILTER **) calloc(num_chnls, sizeof(FIRFILTER *));
    pp->lopass1 = (BUTTER **) calloc(num_chnls, sizeof(BUTTER *));
    pp->lopass2 = (BUTTER **) calloc(num_chnls, sizeof(BUTTER *));
    pp->hipass1 = (BUTTER **) calloc(num_chnls, sizeof(BUTTER *));
    pp->hipass2 = (BUTTER **) calloc(num_chnls, sizeof(BUTTER *));
    pp->meters = (float *) calloc(num_chnls, sizeof(float));
    pp->sw_index[0] = num_chnls - 1;
    pp->sw_index[1] =  pp->sw_index[2] = pp->sw_index[3] = -1;
    pp->meter_update_samples = 1024;

    for (i = 0; i < num_chnls; i++) {
        char name[32];
        sprintf(name, "input_%i", i);
        jd->input_ports[i] = jack_port_register (jd->client, name,
                                             JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsInput, 0);
        sprintf(name, "output_%i", i);
        jd->output_ports[i] = jack_port_register (jd->client, name,
                                              JACK_DEFAULT_AUDIO_TYPE,
                                              JackPortIsOutput, 0);
        pp->gains[i] = 0.2;
        pp->lopass1[i] = butter_create(sr, BUTTER_LP);
        pp->lopass2[i] = butter_create(sr, BUTTER_LP);
        pp->hipass1[i] = butter_create(sr, BUTTER_HP);
        pp->hipass2[i] = butter_create(sr, BUTTER_HP);

    }
    jd->num_chnls = num_chnls;
}

void initialize_data(alloaudio_data_t *pp, jack_client_t *client)
{
    jack_data_t *jd = pp->jd;
    jd->client = client;
    jd->num_chnls = 0;
    pp->master_gain = 1.0;
    pp->mute_all = 0;
    pp->clipper_on = 1;
    pp->filters_active = 0;

    set_bass_management_mode(pp, BASSMODE_FULL);
    set_bass_management_freq(pp, 150);

    pp->meter_buffer = jack_ringbuffer_create(4096);
    pp->meter_counter = 0;
    pp->meter_on = 0;
    pp->closing = 0;

    pthread_mutex_init(&pp->param_mutex, NULL);
}


/* audio callback */
int inprocess (jack_nframes_t nframes, void *arg)
{
    int i, chan = 0;
    alloaudio_data_t *pp = (alloaudio_data_t *) arg;
    jack_data_t * jd = pp->jd;
    double bass_buf[nframes];
    double filt_out[nframes];
    double filt_low[nframes];
    double in_buf[nframes];
    float master_gain;

    if (!pthread_mutex_trylock(&pp->param_mutex)) {
        return 0; /* don't process buffers if changing parameters */
    }
    master_gain = pp->master_gain * (pp->mute_all == 0 ? 1 : 0);
    memset(bass_buf, 0, nframes * sizeof(double));
    for (chan = 0; chan < jd->num_chnls; chan++) {
        float gain = master_gain * pp->gains[chan];
        jack_default_audio_sample_t *out =
                jack_port_get_buffer (jd->output_ports[chan], nframes);
        jack_default_audio_sample_t *in =
                jack_port_get_buffer (jd->input_ports[chan], nframes);
        double filt_temp[nframes];
        double *buf = bass_buf;

        for (i = 0; i < nframes; i++) {
            in_buf[i] = *in++;
        }
        switch (pp->bass_management_mode) {
        case BASSMODE_NONE:
            break;
        case BASSMODE_MIX:
            for (i = 0; i < nframes; i++) {
                filt_low[i] = in_buf[i];
            }
            break;
        case BASSMODE_LOWPASS:
            butter_next(pp->lopass1[chan], in_buf, filt_temp, nframes);
            butter_next(pp->lopass2[chan], filt_temp, filt_low, nframes);
            break;
        case BASSMODE_HIGHPASS:
            for (i = 0; i < nframes; i++) {
                filt_low[i] = in_buf[i];
            }
            butter_next(pp->hipass1[chan], in_buf, filt_temp, nframes);
            butter_next(pp->hipass2[chan], filt_temp, filt_out, nframes);
            for (i = 0; i < nframes; i++) {
                in_buf[i] = filt_out[i];
            }
            break;
        case BASSMODE_FULL:
            butter_next(pp->lopass1[chan], in_buf, filt_temp, nframes);
            butter_next(pp->lopass2[chan], filt_temp, filt_low, nframes);
            butter_next(pp->hipass1[chan], in_buf, filt_temp, nframes);
            butter_next(pp->hipass2[chan], filt_temp, filt_out, nframes);
            for (i = 0; i < nframes; i++) {
                in_buf[i] = filt_out[i]; /* a bit inefficient to copy here, but makes code simpler below */
            }
            break;
	default:
	    break;
        }
        for (i = 0; i < nframes; i++) { /* accumulate SW signal */
            *buf++ += filt_low[i];
        }
        if (pp->filters_active
                && pp->bass_management_mode == BASSMODE_NONE && !chan_is_subwoofer(pp, chan)) { /* apply DRC filters */
            firfilter_next(pp->filters[chan],in_buf, filt_out, nframes, gain);
            for (i = 0; i < nframes; i++) {
                *out = filt_out[i];
                if (pp->clipper_on && *out > gain) {
                    *out = gain;
                }
                out++;
            }
        } else { /* No DRC filters, just apply gain */
            for (i = 0; i < nframes; i++) {
                *out = in_buf[i] * gain;
                if (pp->clipper_on && *out > gain) {
                    *out = gain;
                }
                out++;
            }
        }
    }
    if (pp->bass_management_mode != BASSMODE_NONE) {
        int sw;
        for(sw = 0; sw < 4; sw++) {
            if (pp->sw_index[sw] < 0) continue;
            jack_default_audio_sample_t *out =
                    jack_port_get_buffer (jd->output_ports[pp->sw_index[sw]], nframes);
            memset(out, 0, nframes * sizeof(jack_default_audio_sample_t));
            for (i = 0; i < nframes; i++) {
                *out++ = bass_buf[i];
            }
        }
    }
    if (pp->meter_on) {
        for (chan = 0; chan < jd->num_chnls; chan++) {
            jack_default_audio_sample_t *out =
                    jack_port_get_buffer (jd->output_ports[chan], nframes);
            for (i = 0; i < nframes; i++) {
                if (pp->meters[chan] < *out) {
                    pp->meters[chan] = *out;
                }
                out++;
            }
        }
        pp->meter_counter += nframes;
        if (pp->meter_counter > pp->meter_update_samples) {
            jack_ringbuffer_write(pp->meter_buffer, (char *) pp->meters, sizeof(float) * jd->num_chnls);
            memset(pp->meters, 0, sizeof(float) * jd->num_chnls);
            pp->meter_counter = 0;
        }
    }
    pthread_mutex_unlock(&pp->param_mutex);
    return 0;	/* continue */
}


int sr_changed(jack_nframes_t nframes, void *arg) {
    /* sr change affects filters */
    printf("Sample Rate changed in jack to %i.\n", nframes);
    return -1;
}


alloaudio_data_t *create_alloaudio(int num_chnls)
{
    int i;
    jack_client_t *client = jack_client_open("Alloaudio", JackNoStartServer, NULL);
    alloaudio_data_t *pp;

    if (!client) {
        printf("Error creating jack client.\n");
        return NULL;
    }
    if (pp == NULL) {
        printf("Error allocating internal data.\n");
        return NULL;	/* heap exhausted */
    }
    pp = (alloaudio_data_t *) malloc (sizeof (alloaudio_data_t));
    pp->jd = (jack_data_t *) malloc(sizeof(jack_data_t));
    initialize_data(pp, client);

    allocate_ports(pp, num_chnls);

    pp->ac = create_autoconnect(pp->jd);

    jack_set_process_callback (client, inprocess, pp);
    jack_set_sample_rate_callback(client, sr_changed, NULL);
    if (jack_activate (client)) {
        free_alloaudio(pp);
        return NULL;
    };

    connect_output_ports(pp->ac);

    return pp;
}

void set_filters(alloaudio_data_t *pp, double **irs, int filter_len)
{
    int i;
    jack_data_t *jd = pp->jd;
    pp->filters_active = 0;
    if (!irs) { /* if NULL, leave filtering off */
        return;
    }
    for (i = 0; i < jd->num_chnls; i++) {
        FIRFILTER *new_filter = firfilter_create(irs[i], filter_len);
        FIRFILTER *old_filter = pp->filters[i];
        pp->filters[i] = new_filter;
        if (old_filter) {
            firfilter_free(old_filter);
        }
    }

    pp->filters_active = 1;
}

void free_alloaudio (alloaudio_data_t *pp)
{
    int i;
    jack_data_t *jd = pp->jd;

    /* FIXME close ports and deallocate filters before clearing memory */
    for (i = 0; i < jd->num_chnls; i++) {
        butter_free(pp->lopass1[i]);
        butter_free(pp->lopass2[i]);
        butter_free(pp->hipass1[i]);
        butter_free(pp->hipass2[i]);
    }
    free(pp->lopass1);
    free(pp->lopass2);
    free(pp->hipass1);
    free(pp->hipass2);
    free(jd->input_ports);
    free(jd->output_ports);
    free(pp->gains);
    free(pp->filters);
    free(pp->meters);
    jack_ringbuffer_free(pp->meter_buffer);

    destroy_autoconnect(pp->ac);
    free(jd);
    free(pp);
}

/* parameter setters */

void set_global_gain(alloaudio_data_t *pp, float gain)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->master_gain = gain;
    pthread_mutex_unlock(&pp->param_mutex);
}

void set_gain(alloaudio_data_t *pp, int channel_index, float gain)
{
    pthread_mutex_lock(&pp->param_mutex);
    if (channel_index >= 0 && channel_index < pp->jd->num_chnls) {
        pp->gains[channel_index] = gain;
    } else {
        printf("Alloaudio error: set_gain() for invalid channel %i", channel_index);
    }
    pthread_mutex_unlock(&pp->param_mutex);
}

void set_mute_all(alloaudio_data_t *pp, int mute_all)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->mute_all = mute_all;
    pthread_mutex_unlock(&pp->param_mutex);
}

void set_clipper_on(alloaudio_data_t *pp, int clipper_on)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->clipper_on = clipper_on;
    pthread_mutex_unlock(&pp->param_mutex);
}

void set_room_compensation_on(alloaudio_data_t *pp, int room_compensation_on)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->filters_active = room_compensation_on;
    pthread_mutex_unlock(&pp->param_mutex);
}

void set_bass_management_freq(alloaudio_data_t *pp, double frequency)
{
    int i;
    if (frequency > 0) {
        pthread_mutex_lock(&pp->param_mutex);
        for (i = 0; i < pp->jd->num_chnls; i++) {
            butter_set_fc(pp->lopass1[i], frequency);
            butter_set_fc(pp->lopass2[i], frequency);
            butter_set_fc(pp->hipass1[i], frequency);
            butter_set_fc(pp->hipass2[i], frequency);
        }
        pthread_mutex_unlock(&pp->param_mutex);
    }
}


void set_bass_management_mode(alloaudio_data_t *pp, bass_mgmt_mode_t mode)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->bass_management_mode = mode;
    pthread_mutex_unlock(&pp->param_mutex);
}


void set_sw_indeces(alloaudio_data_t *pp, int i1, int i2, int i3, int i4)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->sw_index[0] = i1;
    pp->sw_index[1] = i1;
    pp->sw_index[2] = i1;
    pp->sw_index[3] = i1;
    pthread_mutex_unlock(&pp->param_mutex);

}


void set_meter(alloaudio_data_t *pp, int meter_on)
{
    pthread_mutex_lock(&pp->param_mutex);
    pp->meter_on = meter_on;
    pthread_mutex_unlock(&pp->param_mutex);
}


int get_meter_values(alloaudio_data_t *pp, float *values)
{
    return jack_ringbuffer_read(pp->meter_buffer, (char *) values, pp->jd->num_chnls * sizeof(float));
}


int get_num_chnls(alloaudio_data_t *pp)
{
    return pp->jd->num_chnls;
}


int is_closing(alloaudio_data_t *pp)
{
    return pp->closing;
}
