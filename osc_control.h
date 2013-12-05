#ifndef OSC_CONTROL_H
#define OSC_CONTROL_H

#include "alloaudio.h"

typedef struct oscdata oscdata_t;

oscdata_t *create_osc(const char *inport, const char *outport, alloaudio_data_t *pp);
void delete_osc(oscdata_t *od);

#endif //OSC_CONTROL_H
