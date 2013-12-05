#ifndef AUTOCONNECTOR_H
#define AUTOCONNECTOR_H

#include <jack/jack.h>

#include "alloaudio.h"

typedef struct autoconnector_s autoconnector_t;

autoconnector_t *create_autoconnect(jack_data_t *jd);
void join_autoconnect(autoconnector_t *ac);
void destroy_autoconnect(autoconnector_t *ac);

/* jack callbacks */
//void client_registered(const char* name, int reg, void *arg);
//void port_registered(jack_port_id_t port_id, int reg, void *arg);

#endif // AUTOCONNECTOR_H
