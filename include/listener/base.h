#ifndef _BASE_H
#define _BASE_H

#define POLL_TIMEOUT	1000			// 1000 ms
#define FDSIZE			100
#define SELECT_TIMEOUT  1

typedef struct Agent{
	uint alive;
	int last_fragment_index;
	int total_message_size;
	int current_message_size;
	char* message;
} Agent;

typedef struct Connected_Agents{
    int num_open_fds;
	int nfds;
    struct pollfd *pfds;
	Agent* agents;
	pthread_mutex_t lock;
} Connected_Agents;


int agent_add(Connected_Agents*, int);

int agent_delete(Connected_Agents*, int);

#endif /* _BASE_H */

