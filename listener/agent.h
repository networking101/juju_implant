#ifndef AGENT_RECEIVE
#define AGENT_RECEIVE

struct agent_receive{
    int *fd_count;
    struct pollfd *pfds;
};

#endif