#ifndef AGENT_RECEIVE
#define AGENT_RECEIVE

struct agent_receive{
    int *fd_count;
    struct pollfd *pfds;
};

#endif

#ifndef AGENT_QUEUE
#define AGENT_QUEUE

struct agent_queue{
	int agent_fd;
	char* message;
};

#endif