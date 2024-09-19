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

STATIC int listener_parse_first_fragment(Agent* agent, Fragment* fragment, int32_t size){

	// check if this is a first fragment
	if (fragment->index != 0){
		debug_print("Expected first fragment but got: %d\n", ntohl(fragment->type));
		return RET_ORDER;
	}

	debug_print("Handling first fragment: type: %d\n", ntohl(fragment->type));

	// get payload size
	agent.total_message_size = ntohl(fragment->first_payload.total_size);
	
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

STATIC int listener_handle_message_fragment(Connected_Agents* CA){
	Queue_Message* q_message;
	Fragment* fragment;
	Agent* agent;

	if (!(q_message = dequeue(listener_receive_queue, &listener_receive_queue_lock))) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", q_message->id, q_message->size);

	fragment = q_message->fragment;
	agent = CA->agents[q_message->id];

	pthread_mutex_lock(&CA->lock);

	// check if socket file descriptor was removed from agents global variable
	if (!agent->alive){
		pthread_mutex_unlock(&CA->lock);
		debug_print("%s\n", "agent was disconnected");
		return RET_OK;
	}

	// if type is alive packet, update keep alive parameter
	if (ntohl(fragment->type) == TYPE_ALIVE){
		CA->agents[q_message->id].alive = (uint)time(NULL);
		pthread_mutex_unlock(&CA->lock);
		debug_print("Agent %d is alive: %lu seconds\n", q_message->id, (unsigned long)ntohl(fragment->alive_time));
		return RET_OK;
	}

	// handle message type other than keep alive
	if (CA->agents[q_message->id].last_fragment_index == -1){
		int retval = listener_parse_first_fragment(CA->agents[q_message->id], fragment, q_message->size);
		if (retval != RET_OK){
			// debug_print("%s\n", "first message out of order");
			//TODO
		}
	}
	else{
		int retval = listener_parse_next_fragment(q_message, CA->agents[q_message->id]);
		if (retval != RET_OK){
			a_message->last_fragment_index = -1;
			// debug_print("%s\n", "next message out of order");
			//TODO
		}
	}

	pthread_mutex_unlock(&CA->lock);

	return RET_OK;
}

STATIC int listener_handle_message(Connected_Agents* CA){

	if (listener_handle_message_fragment(CA) != RET_OK){
		return RET_ERROR;
	}
	
	if (listener_handle_complete_message(a_message) != RET_OK){
		return RET_ERROR;
	}

	return RET_OK;
}

int listener_prepare_message(int sockfd, int type, char* message, int message_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0;
	int this_size;
	int next_size;
	int index = 0;

	// prepare first fragment. Only purpose is to prepare for next fragment size
	fragment = calloc(1, sizeof(Fragment));
	fragment->type = htonl(type);
	fragment->index = htonl(index++);
	fragment->total_size = htonl(message_size);
	fragment->checksum = 0							// TODO
	
	next_size = message_size < PAYLOAD_SIZE ? message_size : PAYLOAD_SIZE;
	fragment->next_size = next_size;
	
	q_message = malloc(sizeof(Queue_Message));
	q_message->id = sockfd;
	q_message->size = INITIAL_SIZE;
	q_message->fragment = fragment;

	enqueue(listener_send_queue, &listener_send_queue_lock, q_message);
	
	while (next_size > 0){
		this_size = next_size;

		// send remaining fragments
		fragment = calloc(1, sizeof(Fragment));
		fragment->type = htonl(type);
		fragment->index = htonl(index++);
		fragment->total_size = htonl(message_size);
		fragment->checksum = 0							// TODO
		
		q_message = malloc(sizeof(Queue_Message));
		q_message->id = sockfd;
		q_message->size = INITIAL_SIZE + next_size;
		q_message->fragment = fragment;
		
		next_size = (message_size - (bytes_sent + next_size)) < NEXT_PAYLOAD_SIZE ? (message_size - (bytes_sent + next_size)) : NEXT_PAYLOAD_SIZE;
		memcpy(fragment->buffer, message + bytes_sent, num_fragment_bytes);

		enqueue(listener_send_queue, &listener_send_queue_lock, q_message);
		bytes_sent += this_size;
	}
	
	free(message);
	return 0;
}

void *listener_handler_thread(void *vargp){
	Connected_Agents* CA = vargp;

	while(!all_sigint){
		if (listener_handle_message(CA) != RET_OK){
			printf("ERROR listener_handler_thread\n");
			all_sigint = true;
		}
	}

	debug_print("%s\n", "listener_handler_thread done");
	return 0;
}