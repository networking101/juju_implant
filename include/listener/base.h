#ifndef _BASE_H
#define _BASE_H

#include "implant.h"

#define POLL_TIMEOUT	1000			// 1000 ms
#define FDSIZE			100
#define SELECT_TIMEOUT  1

typedef struct Agent{
	unsigned int alive;
	Fragment_Header last_header;		// this is being used in listener_handler to reassemble fragments
	int next_recv_size;					// this is being used in listener_comms to read the next packet
	int current_message_size;
	char* file_name;
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

