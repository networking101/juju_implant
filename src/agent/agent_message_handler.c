#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "queue.h"

// Globals
extern Queue* receive_queue;
extern pthread_mutex_t receive_queue_lock;
extern Queue* send_queue;
extern pthread_mutex_t send_queue_lock;

static int agent_handle_command(void){
	return 0;
}

static int agent_handle_put_file(void){
	return 0;
}

static int agent_handle_get_file(void){
	return 0;
}

void* agent_handle_message(void*){
	Queue_Message* message;
	Fragment* fragment;
	
	int type = 0;
	int last_fragment_index = -1;
	int total_message_size = 0;
	int current_message_size = 0;
	char* complete_message;
	
	for (;;){
		if (isEmpty(receive_queue))
			continue;
		
		pthread_mutex_lock(&receive_queue_lock);
		message = dequeue(receive_queue);
		pthread_mutex_unlock(&receive_queue_lock);
		
		fragment = message->fragment;
		if (last_fragment_index == -1 && fragment->index == 0){
			// get payload size
			total_message_size = (fragment->payload[0] && 0xFF) << 24;
			total_message_size += (fragment->payload[1] && 0xFF) << 16;
			total_message_size += (fragment->payload[2] && 0xFF) << 8;
			total_message_size += fragment->payload[3] && 0xFF;
			
			// set last fragment index
			last_fragment_index = 0;
			
			// allocate memory and null
			complete_message = malloc(total_message_size);
			memset(complete_message, 0, total_message_size);
			
			int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index) - sizeof(total_message_size);
			
			// copy payload (not including total message size) to message buffer
			memcpy(complete_message, fragment->payload + sizeof(total_message_size), this_payload_size);
			
			// record current size of message
			current_message_size = this_payload_size;
			type = fragment->type;
			
			printf("DEBUG first message:\t current_message_size: %d, type: %d", current_message_size, fragment->type);
		}
		else if (last_fragment_index == fragment->index - 1 && type == fragment->type){
			int this_payload_size = message->fragment_size - sizeof(fragment->type) - sizeof(fragment->index);
			
			// copy payload to end of message buffer
			memcpy(complete_message, fragment->payload, this_payload_size);
			
			// update current size of message
			current_message_size += this_payload_size;
			
			printf("DEBUG next message:\tcurrent_message_size: %d, type: %d", current_message_size, fragment->type);
		}
		else{
			printf("error message order\n");
			exit(-1);
		}
		
		if (total_message_size == current_message_size){
			last_fragment_index = -1;
			if (type == TYPE_COMMAND){
				agent_handle_command();
			}
			if (type == TYPE_PUT_FILE){
				agent_handle_put_file();
			}
			if (type == TYPE_GET_FILE){
				agent_handle_get_file();
			}
		}
	}
	return 0;
}