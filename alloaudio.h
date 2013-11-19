#ifndef ALLOAUDIO_H
#define ALLOAUDIO_H

#include <jack/jack.h>

typedef struct connection_data connection_data_t;

connection_data_t *jack_initialize (int num_chnls);

void jack_close (connection_data_t *pp);

void set_filters(connection_data_t *pp, double **irs, int filter_len);


/* variable setters */
void set_global_gain(connection_data_t *pp, float gain);
void set_gain(connection_data_t *pp, int channel_index, float gain);
void set_mute_all(connection_data_t *pp, int mute_all);
void set_clipper_on(connection_data_t *pp, int clipper_on);
void set_room_compensation_on(connection_data_t *pp, int room_compensation_on);

#endif //ALLOAUDIO_H
