
#include "alloaudio.h"

typedef struct oscdata oscdata_t;

oscdata_t *create_osc(const char *port, connection_data_t *pp);
void delete_osc(oscdata_t *od);

