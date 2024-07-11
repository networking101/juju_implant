#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent_handler.h"

void *agent_handler(void *vargp){
    struct agent_connection *agent = vargp;
    printf("new connection %d, %lu, %d\n", agent->sock_id, agent->ip_addr, agent->port);
    close(agent->sock_id);
    free(agent);
    return 0;
}