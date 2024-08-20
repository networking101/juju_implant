#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "queue.h"
#include "implant.h"
#include "utility.h"
#include "agent_message_handler.h"

// Globals
extern Queue* agent_receive_queue;
extern pthread_mutex_t agent_receive_queue_lock;
extern Queue* agent_send_queue;
extern pthread_mutex_t agent_send_queue_lock;

STATIC int agent_handle_command(void){
	return 0;
}

STATIC int agent_handle_put_file(void){
	return 0;
}

STATIC int agent_handle_get_file(void){
	return 0;
}

STATIC int parse_first_fragment(Queue_Message* q_message, Assembled_Message* a_message){
	Fragment* fragment = q_message->fragment;
	
	// check if we are receiving the first index of a message
	if (fragment->index != 0){
		return RET_ERROR;
	}
	a_message->last_fragment_index = fragment->index;
	
	// assign type
	a_message->type = fragment->type;
	
	// get payload size
	a_message->total_message_size = fragment->first_payload.total_size;
	
	// allocate memory and null
	a_message->complete_message = malloc(a_message->total_message_size);
	memset(a_message->complete_message, 0, a_message->total_message_size);
	
	// get the size of payload in this fragment
	int this_payload_size = q_message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(fragment->first_payload.total_size);
	
	// copy payload (not including total message size) to message buffer
	memcpy(a_message->complete_message, fragment->first_payload.actual_payload, this_payload_size);
	
	// record current size of message
	a_message->current_message_size = this_payload_size;
	
	debug_print("first message:\t total_size: %d, current_message_size: %d, type: %d\n", a_message->total_message_size, a_message->current_message_size, a_message->type);
	return RET_OK;
}

STATIC int parse_next_fragment(Queue_Message* q_message, Assembled_Message* a_message){
	Fragment* fragment = q_message->fragment;
	
	// check if we are receiving the next expected index of a message
	if (fragment->index != a_message->last_fragment_index + 1 || fragment->type != a_message->type){
		a_message->last_fragment_index = -1;
		return RET_ERROR;
	}
	a_message->last_fragment_index = fragment->index;
	
	// get the size of payload in this fragment
	int this_payload_size = q_message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index);
	
	// copy payload to end of message buffer
	memcpy(a_message->complete_message + a_message->current_message_size, fragment->next_payload, this_payload_size);
	
	// update current size of message
	a_message->current_message_size += this_payload_size;
	
	debug_print("next message:\tcurrent_message_size: %d, type: %d\n", a_message->current_message_size, a_message->type);
	return RET_OK;
}


void* agent_handle_message(void*){
	Queue_Message* message;
	Fragment* fragment;
	Assembled_Message assembled_message = {0};
	
	assembled_message.last_fragment_index = -1;
	
	for (;;){		
		if (!(message = dequeue(agent_receive_queue, &agent_receive_queue_lock))) continue;;
		
		fragment = message->fragment;
		if (assembled_message.last_fragment_index == -1){
			
			int retval = parse_first_fragment(message, &assembled_message);
			if (!retval){
				printf("ERROR message order\n");
				exit(RET_ERROR);
			}
		}
		else{
			int retval = parse_next_fragment(message, &assembled_message);
			if (!retval){
				printf("ERROR message order\n");
				exit(RET_ERROR);
			}
		}
		
		if (assembled_message.total_message_size == assembled_message.current_message_size){
			assembled_message.last_fragment_index = -1;
			if (assembled_message.type == TYPE_COMMAND){
				agent_handle_command();
			}
			else if (assembled_message.type == TYPE_PUT_FILE){
				agent_handle_put_file();
			}
			else if (assembled_message.type == TYPE_GET_FILE){
				agent_handle_get_file();
			}
			else{
				printf("ERROR unknown fragment type\n");
				exit(-1);
			}
		}
	}
	return 0;
}