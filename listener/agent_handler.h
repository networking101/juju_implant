#include <sys/types.h>

struct agent_connection{
    int sock_id;
    u_long ip_addr;
    u_short port;
};

void *agent_handler(void*);