#ifndef ALLOAUDIO_H
#define ALLOAUDIO_H

#include <jack/jack.h>

typedef enum {
    BASSMODE_NONE = 0,
    BASSMODE_MIX = 1,
    BASSMODE_LOWPASS = 2,
    BASSMODE_HIGHPASS = 3,
    BASSMODE_FULL = 4,
    BASSMODE_COUNT
} bass_mgmt_mode_t;

typedef struct {
    jack_client_t *client;
    jack_port_t **input_ports;
    jack_port_t **output_ports;
    int num_chnls;
} jack_data_t;

typedef struct connection_data_s alloaudio_data_t;

alloaudio_data_t *create_alloaudio(int num_chnls);

void free_alloaudio(alloaudio_data_t *pp);

void set_filters(alloaudio_data_t *pp, double **irs, int filter_len);


/* variable setters */
void set_global_gain(alloaudio_data_t *pp, float gain);
void set_gain(alloaudio_data_t *pp, int channel_index, float gain);
void set_mute_all(alloaudio_data_t *pp, int mute_all);
void set_clipper_on(alloaudio_data_t *pp, int clipper_on);
void set_room_compensation_on(alloaudio_data_t *pp, int room_compensation_on);

/* set frequency to 0 to skip bass management cross-over filters, but still have signals added to subwoofers.
   set to -1 skip bass_management completely */
void set_bass_management_freq(alloaudio_data_t *pp, double frequency);
void set_bass_management_mode(alloaudio_data_t *pp, bass_mgmt_mode_t mode);
void set_sw_indeces(alloaudio_data_t *pp, int i1, int i2, int i3, int i4);
void set_meter(alloaudio_data_t *pp, int meter_on);

/* value getters */

int get_meter_values(alloaudio_data_t *pp, float *values);
int get_num_chnls(alloaudio_data_t *pp);
int is_closing(alloaudio_data_t *pp);

#endif //ALLOAUDIO_H
