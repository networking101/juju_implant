#ifndef IMPLANT_POLL_STRUCTURE
#define IMPLANT_POLL_STRUCTURE

typedef struct implant_poll{
    int *fd_count;
    struct pollfd *pfds;
} implant_poll;

#endif

#ifndef AGENT_STRUCTURE
#define AGENT_STRUCTURE

typedef struct Agent{
	uint alive;
	int last_fragment_index;
	int total_message_size;
	int current_message_size;
	char* message;
} Agent;

#endif

int poll_add(int);

int poll_delete(int);