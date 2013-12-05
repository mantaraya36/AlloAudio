#ifndef AUTOCONNECTOR_H
#define AUTOCONNECTOR_H

#include <jack/jack.h>

#include "alloaudio.h"

typedef struct autoconnector_s autoconnector_t;

autoconnector_t *create_autoconnect(jack_data_t *jd);
void join_autoconnect(autoconnector_t *ac);
void destroy_autoconnect(autoconnector_t *ac);

void connect_output_ports(autoconnector_t *ac);

#endif // AUTOCONNECTOR_H
