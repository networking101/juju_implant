#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>

#include "base.h"

// Global variables
extern struct implant_poll *poll_struct;
extern pthread_mutex_t poll_lock;

int poll_add(int sockfd){
	pthread_mutex_lock(&poll_lock);
	poll_struct->pfds[*(poll_struct->fd_count)].fd = sockfd;
    poll_struct->pfds[*(poll_struct->fd_count)].events = POLLIN;
    *(poll_struct->fd_count) = *(poll_struct->fd_count) + 1;
    pthread_mutex_unlock(&poll_lock);
    
	return 0;
}

int poll_delete(int index){	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;

	pthread_mutex_lock(&poll_lock);
	close(pfds[index].fd);
	pfds[index] = pfds[*fd_count-1];
	*(poll_struct->fd_count) = *(poll_struct->fd_count) - 1;
    pthread_mutex_unlock(&poll_lock);
    
	return 0;
}