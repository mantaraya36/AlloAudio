#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include "autoconnector.h"

typedef struct list_member_s {
    char *in_port;
    char *out_port;
    struct list_member_s *next;
} list_member_t;

struct autoconnector_s {
    jack_data_t *jd;
    int cur_port_index;
    pthread_t conn_thread;
    pthread_mutex_t conn_list_mutex;
    list_member_t *conn_list;
    int closing;
};

void *connector_thread(void *arg)
{
    autoconnector_t *ac = (autoconnector_t *) arg;
    while(!ac->closing) {
        pthread_mutex_lock(&ac->conn_list_mutex);
        while (ac->conn_list) {
            list_member_t *conn = ac->conn_list;
            ac->conn_list = ac->conn_list->next;
            jack_connect(ac->jd->client, conn->in_port, conn->out_port);
            printf("Connect %s to %s\n", conn->in_port, conn->out_port); fflush(stdout);
            free(conn->in_port);
            free(conn->out_port);
            free(conn);
        }
        pthread_mutex_unlock(&ac->conn_list_mutex);
        sleep(1);
    }
    return NULL;
}

void client_registered(const char* name, int reg, void *arg)
{
    autoconnector_t *ac = (autoconnector_t *) arg;
    if (reg) {
        printf("Client registered: %s\n", name); fflush(stdout);
        ac->cur_port_index = 0;
    }
}

void port_registered(jack_port_id_t port_id, int reg, void *arg)
{
    autoconnector_t *ac = (autoconnector_t *) arg;
    jack_data_t *jd = ac->jd;
    jack_port_t *port = jack_port_by_id( jd->client, port_id);

    int flags = jack_port_flags(port);

    if (reg && (flags & JackPortIsOutput && !jack_port_is_mine(jd->client, port))) {
        const char *out_name = jack_port_name(jd->input_ports[ac->cur_port_index%jd->num_chnls]);
        const char *in_name = jack_port_name(port);
        int len_out = strlen(out_name);
        int len_in = strlen(in_name);

        list_member_t *conn = (list_member_t *) malloc(sizeof(list_member_t));
        pthread_mutex_lock(&ac->conn_list_mutex);
        list_member_t *last = ac->conn_list;
        while (last && last->next) {
            last = last->next;
        }

        printf("Port registered: %s\n", out_name); fflush(stdout);
        conn->in_port = (char *) calloc(len_in + 1, sizeof(char));
        strncpy(conn->in_port, in_name, len_in);
        conn->out_port = (char *) calloc(len_out + 1, sizeof(char));
        strncpy(conn->out_port, out_name, len_out);
        conn->next = NULL;
        if (ac->conn_list) {
            last->next = conn;
        } else {
            ac->conn_list = conn;
        }
        ac->cur_port_index++;
        pthread_mutex_unlock(&ac->conn_list_mutex);
    }
}


void connect_output_ports(autoconnector_t *ac)
{
    int i;
    for (i = 0; i < ac->jd->num_chnls; i++) {
        char system_port[64];
        const char *portname = jack_port_name (ac->jd->output_ports[i]);
        sprintf(system_port, "system:playback_%i", i + 1);
        if (jack_connect (ac->jd->client, portname, system_port)) {
            fprintf (stderr, "cannot connect %s to %s\n", portname, system_port); fflush(stdout);
        }
    }
}


autoconnector_t *create_autoconnect(jack_data_t *jd)
{
    autoconnector_t *ac = (autoconnector_t *) malloc(sizeof(autoconnector_t));
    ac->jd = jd;
    ac->closing = 0;

    jack_set_client_registration_callback(jd->client, client_registered, ac);
    jack_set_port_registration_callback(jd->client, port_registered, ac);
    ac->cur_port_index = 0;
    pthread_mutex_init(&ac->conn_list_mutex, NULL);
    ac->conn_list = NULL;
    if (pthread_create(&ac->conn_thread, NULL, connector_thread, ac) != 0) {
        printf("Error creating connector thread ports.\n");
    }

    return ac;
}

void destroy_autoconnect(autoconnector_t *ac)
{
    ac->closing = 1;
    pthread_join(ac->conn_thread, NULL);
    free(ac);
}

