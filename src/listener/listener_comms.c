#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include <sys/socket.h>

#include "queue.h"
#include "implant.h"
#include "base.h"
#include "listener_comms.h"

// Global variables
extern struct implant_poll* poll_struct;
extern pthread_mutex_t poll_lock;
// Receive Queue
extern struct Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
// Send Queue
extern struct Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

bool check_alive(){
	return true;
}

void *agent_receive(void *vargp){
	char buf[BUFFERSIZE];
	char* buffer;
	
	int *fd_count = poll_struct->fd_count;
	struct pollfd *pfds = poll_struct->pfds;
	
    for (;;){
		int poll_count = poll(pfds, *fd_count, 1000);
		
		for (int i = 0; i < *fd_count; i++){
			
			// check if ready to read
			if (pfds[i].revents & POLLIN){
//				printf("DEBUG POLLIN  %d\n", pfds[i].fd);
				int nbytes = recv(pfds[i].fd, buf, BUFFERSIZE, 0);
				
				// close file descriptor and remove from poll array
				if (nbytes == 0){
					printf("connection closed\n");
					poll_delete(i);
				}
				else if (nbytes == -1){
					printf("recv error\n");
					exit(-1);
				}
				else{
					int message_size;
					int bytes_received = 0;
					// TODO make this a separate function that is static
					// check if alive message
					if (!check_alive()){
						printf("alive error\n");
						exit(-1);
					}
					
					bytes_received += nbytes - 4;
		
					// get total size of message
					message_size += (buf[0] && 0xFF) << 24;
					message_size += (buf[1] && 0xFF) << 16;
					message_size += (buf[2] && 0xFF) << 8;
					message_size += buf[3] && 0xFF;
					
					buffer = malloc(message_size);
					memcpy(buffer, buf+4, nbytes);
					
					// get rest of message
					while (bytes_received < message_size){
						nbytes = recv(pfds[i].fd, buf, BUFFERSIZE, 0);
						// close file descriptor and remove from poll array
						// TODO if partial data received when connection closed, discard partial message
						if (nbytes == 0){
							printf("connection closed\n");
							poll_delete(i);
						}
						else if (nbytes == -1){
							printf("recv error\n");
							exit(-1);
						}
						else{
							// TODO make this a separate function that is static
							// check if alive message
							if (!check_alive()){
								printf("alive error\n");
								exit(-1);
							}
							
							memcpy(buffer + bytes_received, buf, nbytes);
							bytes_received += nbytes;
						}
					}
					
					// Set up Message structure
					struct Message* message = malloc(sizeof(message));
					message->id = pfds[i].fd;
					message->size = message_size;
					message->buffer = buffer;
					
					pthread_mutex_lock(&receive_queue_lock);
					enqueue(receive_queue, message);
					pthread_mutex_unlock(&receive_queue_lock);
				}
			}
		}
	}
	
    return 0;
}

void *agent_send(void *vargp){
	struct Message* message;
	
//	int *fd_count = poll_struct->fd_count;
//	struct pollfd *pfds = poll_struct->pfds;
	
	for(;;){
		pthread_mutex_lock(&send_queue_lock);
		if (message = dequeue(send_queue)){
			char* buf = malloc(message->size + 4);
			int bytes_left = message->size + 4;
			
			// set first 4 bytes of message to message size
			buf[0] = (message->size >> 24) & 0xFF;
			buf[1] = (message->size >> 16) & 0xFF;
			buf[2] = (message->size >> 8) & 0xFF;
			buf[3] = message->size & 0xFF;
			
			// copy payload into buf+4
			memcpy(buf+4, message->buffer, message->size);
			
			// send message in 4096 byte chunks
			while (bytes_left){
				uint chunk_size = (bytes_left > 4096) ? 4096: bytes_left;
				bytes_left -= chunk_size;
				 
				int nbytes = send(message->id, buf, chunk_size, 0);
				if (nbytes == -1){
					printf("Send error\n");
					exit(-1);
				}
				
				buf += chunk_size;
			}
			
			free(message->buffer);
			free(buf);
			free(message);
		}
		pthread_mutex_unlock(&send_queue_lock);
	}
    return 0;
}