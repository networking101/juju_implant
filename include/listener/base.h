#ifndef _BASE_H
#define _BASE_H

#define POLL_TIMEOUT	10			// 10 ms
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
    int *fd_count;
    struct pollfd *pfds;
	Agent *agents;
} Connected_Agents;


int agent_add(int);

int agent_delete(int);

#endif /* _BASE_H */

